#![windows_subsystem = "windows"]

mod dng;
mod mov;
mod zrv;

use native_windows_gui as nwg;
use std::cell::{Cell, RefCell};
use std::io;
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::mpsc::{self, Receiver, TryRecvError};
use zrv::{ClipInfo, ClipSequence, MovieFrameCounts};

const TITLE: &str = "ZRV 转 CinemaDNG";
const WIDTH: i32 = 736;
const MARGIN: i32 = 24;
const CONTENT: i32 = WIDTH - 2 * MARGIN;
const TIMEOUT_LABEL_WIDTH: i32 = 94;

enum Update {
    Progress(u32, u32),
    Finished(io::Result<PathBuf>),
}

struct Job {
    receiver: Receiver<Update>,
    cancel: Arc<AtomicBool>,
}

#[derive(Clone)]
struct MovieReference {
    path: PathBuf,
    frames: u32,
    counts: MovieFrameCounts,
}

impl MovieReference {
    fn open(sequence: &ClipSequence, source: &Path, path: PathBuf) -> io::Result<Self> {
        let (positions, frames) = dng::movie_timeline(sequence, source, &path)?;
        let counts = sequence.movie_frame_counts(&positions, frames);
        Ok(Self { path, frames, counts })
    }
}

fn duration(frames: u32, info: ClipInfo) -> String {
    format!("{frames} 帧 / {:.2} 秒", f64::from(frames) / info.fps())
}

fn movie_frame_summary(counts: MovieFrameCounts) -> String {
    let mut text = format!("超时帧 {} · 同步帧 {}", counts.timeout, counts.sync);
    if counts.unknown != 0 {
        text.push_str(&format!(" · 类型待确认 {}", counts.unknown));
    }
    text
}

#[derive(Default)]
struct App {
    window: nwg::Window,
    heading_font: nwg::Font,
    labels: [nwg::Label; 6],
    source_path: nwg::TextInput,
    output_path: nwg::TextInput,
    black_level: nwg::TextInput,
    black_level_source: nwg::Label,
    missing_frame: nwg::ComboBox<&'static str>,
    sync_movie: nwg::CheckBox,
    movie_label: nwg::Label,
    movie_button: nwg::Button,
    source_button: nwg::Button,
    output_button: nwg::Button,
    convert_button: nwg::Button,
    cancel_button: nwg::Button,
    open_button: nwg::Button,
    drop_report_button: nwg::Button,
    clip_info: nwg::Label,
    progress: nwg::ProgressBar,
    status: nwg::Label,
    file_dialog: nwg::FileDialog,
    folder_dialog: nwg::FileDialog,
    drop_report_dialog: nwg::FileDialog,
    movie_dialog: nwg::FileDialog,
    notice: nwg::Notice,
    source: RefCell<Option<(PathBuf, ClipInfo)>>,
    movie: RefCell<Option<MovieReference>>,
    movie_supported: Cell<bool>,
    output_parent: RefCell<Option<PathBuf>>,
    completed: RefCell<Option<PathBuf>>,
    job: RefCell<Option<Job>>,
    closing: Cell<bool>,
}

impl App {
    fn build() -> Result<Self, nwg::NwgError> {
        let mut app = Self::default();
        nwg::Window::builder()
            .size((WIDTH, 491))
            .title(TITLE)
            .flags(nwg::WindowFlags::WINDOW | nwg::WindowFlags::MINIMIZE_BOX)
            .center(true)
            .accept_files(true)
            .build(&mut app.window)?;
        nwg::Font::builder()
            .family("Microsoft YaHei UI")
            .size(21)
            .weight(600)
            .build(&mut app.heading_font)?;
        nwg::Label::builder()
            .parent(&app.window)
            .font(Some(&app.heading_font))
            .text(TITLE)
            .position((MARGIN, 20))
            .size((CONTENT, 40))
            .build(&mut app.labels[0])?;
        for (label, text, y) in [
            (1, "拖入 .ZRV 首段自动合并同名续段，也可单独选择续段", 65),
            (2, "源文件", 99),
            (3, "输出序列目录", 203),
            (4, "黑电平", 271),
            (5, "超时帧补齐", 309),
        ] {
            nwg::Label::builder()
                .parent(&app.window)
                .text(text)
                .position((MARGIN, y))
                .size((if label == 5 { TIMEOUT_LABEL_WIDTH } else { CONTENT }, 24))
                .build(&mut app.labels[label])?;
        }
        for (field, y) in [(&mut app.source_path, 123), (&mut app.output_path, 227)] {
            nwg::TextInput::builder()
                .parent(&app.window)
                .readonly(true)
                .position((MARGIN, y))
                .size((CONTENT - 132, 28))
                .build(field)?;
        }
        nwg::TextInput::builder()
            .parent(&app.window)
            .flags(
                nwg::TextInputFlags::VISIBLE
                    | nwg::TextInputFlags::NUMBER
                    | nwg::TextInputFlags::TAB_STOP,
            )
            .text(&dng::DEFAULT_BLACK_LEVEL.to_string())
            .position((MARGIN + 70, 267))
            .size((78, 28))
            .limit(4)
            .build(&mut app.black_level)?;
        nwg::Label::builder()
            .parent(&app.window)
            .position((MARGIN + 162, 271))
            .size((CONTENT - 162, 24))
            .build(&mut app.black_level_source)?;
        nwg::ComboBox::builder()
            .parent(&app.window)
            .position((MARGIN + TIMEOUT_LABEL_WIDTH, 305))
            .size((172, 95))
            .collection(vec!["复制前一帧", "插入黑帧"])
            .selected_index(Some(0))
            .build(&mut app.missing_frame)?;
        nwg::CheckBox::builder()
            .parent(&app.window)
            .text("同步到 MOV")
            .position((MARGIN, 343))
            .size((132, 28))
            .build(&mut app.sync_movie)?;
        nwg::Label::builder()
            .parent(&app.window)
            .position((MARGIN + 136, 346))
            .size((CONTENT - 268, 25))
            .build(&mut app.movie_label)?;
        let buttons = [
            (
                &mut app.source_button,
                "选择文件",
                (WIDTH - MARGIN - 118, 122),
                118,
            ),
            (
                &mut app.output_button,
                "选择位置",
                (WIDTH - MARGIN - 118, 226),
                118,
            ),
            (&mut app.movie_button, "选择 MOV", (WIDTH - MARGIN - 118, 340), 118),
            (&mut app.convert_button, "开始转换", (MARGIN, 440), 120),
            (&mut app.cancel_button, "取消", (MARGIN + 132, 440), 78),
            (
                &mut app.open_button,
                "打开输出目录",
                (MARGIN + 222, 440),
                138,
            ),
            (
                &mut app.drop_report_button,
                "导出超时帧表",
                (WIDTH - MARGIN - 138, 440),
                138,
            ),
        ];
        for (button, text, position, width) in buttons {
            nwg::Button::builder()
                .parent(&app.window)
                .position(position)
                .size((width, 32))
                .text(text)
                .build(button)?;
        }
        nwg::Label::builder()
            .parent(&app.window)
            .position((MARGIN, 157))
            .size((CONTENT, 44))
            .build(&mut app.clip_info)?;
        nwg::ProgressBar::builder()
            .parent(&app.window)
            .position((MARGIN, 386))
            .size((CONTENT, 15))
            .build(&mut app.progress)?;
        nwg::Label::builder()
            .parent(&app.window)
            .text("等待选择 ZRV 文件")
            .position((MARGIN, 408))
            .size((CONTENT, 24))
            .build(&mut app.status)?;
        nwg::FileDialog::builder()
            .title("选择 ZRV 文件")
            .filters("ZRV 录像和分段 (*.zrv;*.z0?*;*.z1?*;*.z2?*;*.z3?*;*.z4?*;*.z5?*;*.z6?*;*.z7?*;*.z8?*;*.z9?*)")
            .action(nwg::FileDialogAction::Open)
            .build(&mut app.file_dialog)?;
        nwg::FileDialog::builder()
            .title("选择保存 CinemaDNG 的位置")
            .action(nwg::FileDialogAction::OpenDirectory)
            .build(&mut app.folder_dialog)?;
        nwg::FileDialog::builder()
            .title("选择帧表保存文件夹")
            .action(nwg::FileDialogAction::OpenDirectory)
            .build(&mut app.drop_report_dialog)?;
        nwg::FileDialog::builder()
            .title("选择同期 MOV 或 MP4")
            .filters("普通视频 (*.mov;*.mp4)")
            .action(nwg::FileDialogAction::Open)
            .build(&mut app.movie_dialog)?;
        nwg::Notice::builder()
            .parent(&app.window)
            .build(&mut app.notice)?;
        app.refresh_actions(false);
        Ok(app)
    }

    fn error(&self, title: &str, error: &str) {
        nwg::modal_error_message(&self.window, title, error);
    }

    fn choose(&self, folder: bool) {
        let dialog = if folder {
            &self.folder_dialog
        } else {
            &self.file_dialog
        };
        if let Some(parent) = self.output_parent.borrow().as_ref() {
            let _ = dialog.set_default_folder(&parent.to_string_lossy());
        }
        if dialog.run(Some(&self.window)) {
            match dialog.get_selected_item() {
                Ok(path) if folder => {
                    self.output_parent.replace(Some(path.into()));
                    self.update_output_path();
                }
                Ok(path) => self.load_source(path.into()),
                Err(error) => self.error("无法取得所选路径", &error.to_string()),
            }
        }
    }

    fn load_source(&self, path: PathBuf) {
        if self.job.borrow().is_some() {
            return;
        }
        let sequence = match ClipSequence::open(&path) {
            Ok(sequence) => sequence,
            Err(error) => {
                self.error("无法打开 ZRV", &format!("{}\n{error}", path.display()));
                return;
            }
        };
        let clip = sequence.info;
        let parts = sequence.part_count();
        if self.output_parent.borrow().is_none() {
            self.output_parent.replace(path.parent().map(|p| {
                if p.as_os_str().is_empty() {
                    PathBuf::from(".")
                } else {
                    p.to_owned()
                }
            }));
        }
        self.source_path.set_text(&path.to_string_lossy());
        let level = clip.black_level.unwrap_or(dng::DEFAULT_BLACK_LEVEL);
        self.black_level.set_text(&level.to_string());
        self.black_level_source.set_text(&match clip.black_level {
            Some(reference) => format!("ZRV 内记录 {reference}（暗场参考值）"),
            None => format!("估计值 {level} · 此 ZRV 无黑电平记录"),
        });
        let fps = clip.fps();
        let sound = match clip.audio {
            Some(track) if track.status == 0 => " · 双声道 PCM",
            Some(track) if track.has_samples() => " · 部分 PCM",
            Some(_) => " · 未取得 PCM",
            None => "",
        };
        let missing = if clip.tracks_dropped_frames || clip.dropped_frames > 0 {
            format!(" · ZRV 超时帧 {}", clip.dropped_frames)
        } else {
            " · 无超时帧明细".to_owned()
        };
        let parts = if parts > 1 { format!(" · {parts} 段") } else { String::new() };
        self.clip_info.set_text(&format!(
            "{} × {}  ·  RAW12 RGGB  ·  {fps:.2} fps  ·  ZRV 源片 {}{parts}\r\n{}{sound}{missing}",
            clip.width,
            clip.height,
            duration(clip.frames, clip),
            clip.end_label()
        ));
        self.missing_frame.set_selection(Some(usize::from(clip.prefer_black)));
        self.sync_movie.set_check_state(nwg::CheckBoxState::Unchecked);
        self.movie.replace(None);
        let is_head = path.extension().is_some_and(|ext| ext.eq_ignore_ascii_case("zrv"));
        let movie_map = sequence.movie_positions(None);
        self.movie_supported.set(is_head && movie_map.is_ok());
        let movie = if self.movie_supported.get() {
            [path.with_extension("MOV"), path.with_extension("MP4")].into_iter().find(|p| p.is_file())
        } else { None };
        match movie {
            Some(movie) => match MovieReference::open(&sequence, &path, movie) {
                Ok(reference) => self.set_movie_reference(reference, clip),
                Err(error) => self.movie_label.set_text(&format!("同步检查失败 · {error}")),
            },
            None if self.movie_supported.get() => self.movie_label.set_text("请选择同名 MOV 或 MP4"),
            None if !is_head => self.movie_label.set_text("请打开 .ZRV 首段以同步 MOV"),
            None => self.movie_label.set_text(&movie_map.unwrap_err().to_string()),
        }
        self.source.replace(Some((path, clip)));
        self.update_output_path();
    }

    fn update_output_path(&self) {
        self.completed.replace(None);
        if let (Some((source, info)), Some(parent)) = (
            self.source.borrow().as_ref(),
            self.output_parent.borrow().as_ref(),
        ) {
            self.output_path
                .set_text(&dng::destination(source, parent, self.sync_enabled()).to_string_lossy());
            let status = match self.selected_movie() {
                Some(movie) => format!("将按 MOV 同步输出 · {} · {}",
                    duration(movie.frames, *info), movie_frame_summary(movie.counts)),
                None => format!("将按 ZRV 源片输出 · {}", duration(info.frames, *info)),
            };
            self.status.set_text(&status);
        }
        self.progress.set_pos(0);
        self.refresh_actions(false);
    }

    fn refresh_actions(&self, busy: bool) {
        self.source_button.set_enabled(!busy);
        self.output_button.set_enabled(!busy);
        self.black_level.set_enabled(!busy);
        self.movie_button.set_enabled(!busy && self.movie_supported.get());
        self.sync_movie.set_enabled(!busy && self.movie.borrow().is_some());
        let synced = self.sync_enabled();
        self.drop_report_button.set_text(if synced { "导出帧表" } else { "导出超时帧表" });
        if let Some((_, info)) = self.source.borrow().as_ref() {
            let has_timeouts = if synced {
                self.movie.borrow().as_ref().is_some_and(|movie| movie.counts.timeout > 0)
            } else {
                info.dropped_frames > 0
            };
            self.missing_frame.set_enabled(!busy && has_timeouts);
            self.drop_report_button.set_enabled(!busy && (info.tracks_dropped_frames || info.dropped_frames > 0));
        } else {
            self.missing_frame.set_enabled(false);
            self.drop_report_button.set_enabled(false);
        }
        self.convert_button.set_enabled(
            !busy && self.source.borrow().is_some() && self.output_parent.borrow().is_some(),
        );
        self.cancel_button.set_enabled(busy);
        self.open_button
            .set_enabled(!busy && self.completed.borrow().is_some());
    }

    fn sync_enabled(&self) -> bool {
        self.movie.borrow().is_some() && self.sync_movie.check_state() == nwg::CheckBoxState::Checked
    }

    fn selected_movie(&self) -> Option<MovieReference> {
        if self.sync_enabled() { self.movie.borrow().clone() } else { None }
    }

    fn set_movie_reference(&self, reference: MovieReference, info: ClipInfo) {
        self.movie_label.set_text(&format!("{} · {}",
            reference.path.file_name().unwrap_or_default().to_string_lossy(), duration(reference.frames, info)));
        self.movie.replace(Some(reference));
        self.sync_movie.set_check_state(nwg::CheckBoxState::Checked);
    }

    fn choose_movie(&self) {
        let Some((source, info)) = self.source.borrow().clone() else { return };
        if let Some(parent) = source.parent() {
            let _ = self.movie_dialog.set_default_folder(&parent.to_string_lossy());
        }
        if !self.movie_dialog.run(Some(&self.window)) { return; }
        let movie = match self.movie_dialog.get_selected_item() {
            Ok(path) => PathBuf::from(path),
            Err(error) => { self.error("无法读取 MOV 路径", &error.to_string()); return; }
        };
        match ClipSequence::open(&source).and_then(|clip| MovieReference::open(&clip, &source, movie)) {
            Ok(reference) => self.set_movie_reference(reference, info),
            Err(error) => { self.error("无法同步到所选视频", &error.to_string()); return; }
        }
        self.update_output_path();
    }

    fn save_drop_report(&self) {
        let Some((source, _)) = self.source.borrow().clone() else { return };
        let movie = self.selected_movie();
        if let Some(parent) = self.output_parent.borrow().as_ref() {
            let _ = self.drop_report_dialog.set_default_folder(&parent.to_string_lossy());
        }
        if !self.drop_report_dialog.run(Some(&self.window)) {
            return;
        }
        let folder = match self.drop_report_dialog.get_selected_item() {
            Ok(folder) => PathBuf::from(folder),
            Err(error) => {
                self.error("无法取得保存文件夹", &error.to_string());
                return;
            }
        };
        let result = Self::write_drop_report(&source, &folder, movie.as_ref().map(|movie| movie.path.as_path()));
        match result {
            Ok(status) => self.status.set_text(&status),
            Err(error) => self.error("无法导出帧表", &error.to_string()),
        }
    }

    fn write_drop_report(source: &Path, folder: &Path, movie: Option<&Path>) -> io::Result<String> {
        let clip = ClipSequence::open(source)?;
        let mapping = movie.map(|movie| dng::movie_timeline(&clip, source, movie)).transpose()?;
        for number in 1..=u32::MAX {
            let path = dng::frame_report_path(source, folder, mapping.is_some(), number);
            if path.try_exists()? { continue; }
            let result = if let Some((positions, total)) = &mapping {
                clip.export_movie_frame_report(&path, positions, *total).map(movie_frame_summary)
            } else {
                match clip.export_drop_report(&path) {
                    Ok(false) => return Ok("此文件格式没有超时帧明细".to_owned()),
                    result => result.map(|_| format!("超时帧 {}", clip.info.dropped_frames)),
                }
            };
            match result {
                Ok(summary) => {
                    let filename = path.file_name().unwrap_or_default().to_string_lossy();
                    return Ok(if mapping.is_some() {
                        format!("已保存 3 个 CSV · {filename} · {summary}")
                    } else {
                        format!("已保存 {filename} · {summary}")
                    });
                }
                Err(error) if error.kind() == io::ErrorKind::AlreadyExists => continue,
                Err(error) => return Err(error),
            }
        }
        Err(io::Error::other("无法分配可用的帧表文件名"))
    }

    fn start(&self) {
        if self.job.borrow().is_some() {
            return;
        }
        let (Some((source, info)), Some(parent)) = (
            self.source.borrow().clone(),
            self.output_parent.borrow().clone(),
        ) else {
            return;
        };
        let Ok(black_level) = self.black_level.text().parse::<u16>() else {
            self.error("黑电平无效", "请输入 0–4094 之间的整数。");
            return;
        };
        if u32::from(black_level) >= dng::WHITE_LEVEL {
            self.error("黑电平无效", "请输入 0–4094 之间的整数。");
            return;
        }
        let missing_frame = if self.missing_frame.selection() == Some(1) {
            dng::MissingFrame::Black
        } else {
            dng::MissingFrame::Previous
        };
        let movie = self.selected_movie();
        self.completed.replace(None);
        self.refresh_actions(true);
        self.progress.set_range(0..movie.as_ref().map_or(info.frames, |movie| movie.frames));
        self.progress.set_pos(0);
        self.status.set_text("正在转换 CinemaDNG…");
        let (send, receiver) = mpsc::channel();
        let cancel = Arc::new(AtomicBool::new(false));
        let cancelled = Arc::clone(&cancel);
        let notice = self.notice.sender();
        match std::thread::Builder::new().spawn(move || {
            let result = dng::export(&source, &parent, black_level, missing_frame, movie.as_ref().map(|movie| movie.path.as_path()), &cancelled, |done, total| {
                if (done.is_multiple_of(8) || done == total)
                    && send.send(Update::Progress(done, total)).is_ok()
                {
                    notice.notice();
                }
            });
            if send.send(Update::Finished(result)).is_ok() {
                notice.notice();
            }
        }) {
            Ok(_) => {
                self.job.replace(Some(Job { receiver, cancel }));
            }
            Err(error) => self.finish(Err(error)),
        }
    }

    fn finish(&self, result: io::Result<PathBuf>) {
        self.job.replace(None);
        if self.closing.get() {
            self.window.close();
            return;
        }
        match result {
            Ok(path) => {
                let status = match (self.selected_movie(), self.source.borrow().as_ref()) {
                    (Some(movie), Some((_, info))) => format!("已同步到 MOV · {} · {}",
                        duration(movie.frames, *info), movie_frame_summary(movie.counts)),
                    _ => "转换完成 · 文件中已保存的画面已全部导出为 DNG".to_owned(),
                };
                self.status.set_text(&status);
                self.completed.replace(Some(path));
            }
            Err(error) if error.kind() == io::ErrorKind::Interrupted => {
                self.progress.set_pos(0);
                self.status.set_text("转换已取消");
            }
            Err(error) => {
                self.progress.set_pos(0);
                self.status.set_text("转换失败");
                self.error("无法导出 CinemaDNG", &error.to_string());
            }
        }
        self.refresh_actions(false);
    }

    fn receive_updates(&self) {
        loop {
            let message = self
                .job
                .borrow()
                .as_ref()
                .map(|job| job.receiver.try_recv());
            match message {
                Some(Ok(Update::Progress(done, total))) if self.cancel_button.enabled() => {
                    self.progress.set_range(0..total);
                    self.progress.set_pos(done);
                    self.status
                        .set_text(&format!("正在转换 · {done} / {total} 帧"));
                }
                Some(Ok(Update::Progress(..))) => {}
                Some(Ok(Update::Finished(result))) => self.finish(result),
                Some(Err(TryRecvError::Disconnected)) => {
                    self.finish(Err(io::Error::other("转换任务意外中断")))
                }
                _ => break,
            }
        }
    }

    fn cancel(&self) {
        if let Some(job) = self.job.borrow().as_ref() {
            job.cancel.store(true, Ordering::Relaxed);
            self.cancel_button.set_enabled(false);
            self.status.set_text("正在取消并清理临时文件…");
        }
    }

    fn event(&self, event: nwg::Event, data: nwg::EventData, handle: nwg::ControlHandle) {
        use nwg::Event::*;
        match event {
            OnButtonClick if handle == self.source_button.handle => self.choose(false),
            OnButtonClick if handle == self.output_button.handle => self.choose(true),
            OnButtonClick if handle == self.movie_button.handle => self.choose_movie(),
            OnButtonClick if handle == self.sync_movie.handle => self.update_output_path(),
            OnButtonClick if handle == self.convert_button.handle => self.start(),
            OnButtonClick if handle == self.drop_report_button.handle => self.save_drop_report(),
            OnButtonClick if handle == self.cancel_button.handle => self.cancel(),
            OnButtonClick if handle == self.open_button.handle => {
                if let Some(path) = self.completed.borrow().as_ref()
                    && let Err(error) = std::process::Command::new("explorer.exe").arg(path).spawn()
                {
                    self.error("无法打开输出目录", &error.to_string());
                }
            }
            OnFileDrop if self.job.borrow().is_none() => {
                let files = data.on_file_drop().files();
                if let [path] = files.as_slice() {
                    self.load_source(path.into());
                } else {
                    self.error("请选择一个文件", "拖入 .ZRV 首段即可合并同名续段。");
                }
            }
            OnNotice if handle == self.notice.handle => self.receive_updates(),
            OnWindowClose if handle == self.window.handle => {
                if self.job.borrow().is_some() {
                    if let nwg::EventData::OnWindowClose(close) = data {
                        close.close(false);
                    }
                    self.closing.set(true);
                    self.cancel();
                } else {
                    nwg::stop_thread_dispatch();
                }
            }
            _ => {}
        }
    }
}

fn run() -> Result<(), nwg::NwgError> {
    nwg::init()?;
    nwg::Font::set_global_family("Microsoft YaHei UI")?;
    let app = Rc::new(App::build()?);
    let weak = Rc::downgrade(&app);
    let handler = nwg::full_bind_event_handler(&app.window.handle, move |event, data, handle| {
        if let Some(app) = weak.upgrade() {
            app.event(event, data, handle);
        }
    });
    app.window.set_visible(true);
    app.source_button.set_focus();
    if let Some(path) = std::env::args_os().nth(1) {
        app.load_source(path.into());
    }
    nwg::dispatch_thread_events();
    nwg::unbind_event_handler(&handler);
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        nwg::fatal_message(TITLE, &error.to_string());
    }
}
