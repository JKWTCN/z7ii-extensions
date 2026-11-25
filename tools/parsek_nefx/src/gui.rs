use native_windows_gui as nwg;
use parsek_nefx::{
    Language, Options, Update, calculate_alignment, can_calculate, dimensions, extension_is, merge,
};
use std::cell::{Cell, RefCell};
use std::panic::{AssertUnwindSafe, catch_unwind};
use std::path::{Path, PathBuf};
use std::rc::Rc;
use std::sync::mpsc::{Receiver, TryRecvError, channel};
use std::thread;
use windows_sys::Win32::Globalization::GetUserDefaultUILanguage;

const MARGIN: i32 = 20;
const LIST_TOP: i32 = 104;

fn user_language() -> Language {
    // Windows encodes the primary language in the low ten bits of a LANGID.
    if (unsafe { GetUserDefaultUILanguage() } & 0x03ff) == 0x04 {
        Language::Chinese
    } else {
        Language::English
    }
}

enum Message {
    Update(Update),
    Finished(anyhow::Result<&'static str>),
}

#[derive(Default)]
struct Controls {
    window: nwg::Window,
    instructions: nwg::Label,
    add: nwg::Button,
    reference: nwg::Button,
    remove: nwg::Button,
    remove_all: nwg::Button,
    list: nwg::ListBox<String>,
    summary: nwg::Label,
    output_label: nwg::Label,
    output: nwg::TextInput,
    browse: nwg::Button,
    motion: nwg::CheckBox,
    correct_ca: nwg::CheckBox,
    calculate: nwg::Button,
    merge: nwg::Button,
    status: nwg::Label,
    progress: nwg::ProgressBar,
    log: nwg::TextBox,
    open_dialog: nwg::FileDialog,
    save_dialog: nwg::FileDialog,
    notice: nwg::Notice,
}

struct App {
    ui: Controls,
    language: Language,
    files: RefCell<Vec<PathBuf>>,
    messages: RefCell<Option<Receiver<Message>>>,
    close_after_task: Cell<bool>,
}

fn suggested_output(reference: Option<&PathBuf>) -> Option<PathBuf> {
    let reference = reference?;
    let mut name = reference.file_stem()?.to_os_string();
    name.push("_parsek.NEFX");
    Some(reference.with_file_name(name))
}

impl App {
    fn build(language: Language) -> Result<Self, nwg::NwgError> {
        let t = |english: &'static str, chinese: &'static str| language.text(english, chinese);
        let mut ui = Controls::default();
        nwg::Window::builder()
            .title(t(
                "Parsek NEFX — Pixel Shift Merge",
                "Parsek NEFX · 像素变化合成",
            ))
            .size((730, 590))
            .flags(nwg::WindowFlags::MAIN_WINDOW)
            .center(true)
            .accept_files(true)
            .build(&mut ui.window)?;
        nwg::Label::builder()
            .parent(&ui.window)
            .text(t(
                "Add or drop full-size Nikon Z7 II 14-bit NEFs. The first is the reference.\r\nMerge 4, 8, 16 or 32 files, or calculate alignment for 2 to 32.",
                "添加或拖入尼康 Z7 II 全画幅 14 位 NEF，首张作为参考原片。\r\n合成支持 4、8、16 或 32 张，仅计算支持 2 至 32 张。",
            ))
            .position((MARGIN, 17))
            .build(&mut ui.instructions)?;
        for (button, caption, x, width) in [
            (&mut ui.add, t("Add NEF files…", "添加 NEF…"), MARGIN, 132),
            (
                &mut ui.reference,
                t("Use as reference", "设为参考原片"),
                MARGIN + 142,
                148,
            ),
            (&mut ui.remove, t("Remove", "移除"), MARGIN + 300, 100),
            (
                &mut ui.remove_all,
                t("Remove all", "移除全部"),
                MARGIN + 410,
                110,
            ),
        ] {
            nwg::Button::builder()
                .parent(&ui.window)
                .text(caption)
                .position((x, 65))
                .size((width, 30))
                .build(button)?;
        }
        nwg::ListBox::builder()
            .parent(&ui.window)
            .flags(nwg::ListBoxFlags::VISIBLE | nwg::ListBoxFlags::TAB_STOP)
            .build(&mut ui.list)?;
        nwg::Label::builder()
            .parent(&ui.window)
            .build(&mut ui.summary)?;
        nwg::Label::builder()
            .parent(&ui.window)
            .text(t("Output NEFX", "输出 NEFX"))
            .build(&mut ui.output_label)?;
        nwg::TextInput::builder()
            .parent(&ui.window)
            .build(&mut ui.output)?;
        nwg::Button::builder()
            .parent(&ui.window)
            .text(t("Browse…", "浏览…"))
            .build(&mut ui.browse)?;
        nwg::CheckBox::builder()
            .parent(&ui.window)
            .text(t(
                "Favor the reference photo in moving areas",
                "运动区域优先保留参考原片",
            ))
            .build(&mut ui.motion)?;
        nwg::CheckBox::builder()
            .parent(&ui.window)
            .text(t(
                "Correct lateral chromatic aberration",
                "色差矫正（横向）",
            ))
            .check_state(nwg::CheckBoxState::Checked)
            .build(&mut ui.correct_ca)?;
        nwg::Button::builder()
            .parent(&ui.window)
            .text(t("Calculate only", "仅计算"))
            .build(&mut ui.calculate)?;
        nwg::Button::builder()
            .parent(&ui.window)
            .text(t("Merge to NEFX", "合成 NEFX"))
            .build(&mut ui.merge)?;
        nwg::Label::builder()
            .parent(&ui.window)
            .build(&mut ui.status)?;
        nwg::ProgressBar::builder()
            .parent(&ui.window)
            .range(0..100)
            .build(&mut ui.progress)?;
        nwg::TextBox::builder()
            .parent(&ui.window)
            .flags(
                nwg::TextBoxFlags::VISIBLE
                    | nwg::TextBoxFlags::VSCROLL
                    | nwg::TextBoxFlags::TAB_STOP,
            )
            .readonly(true)
            .build(&mut ui.log)?;
        nwg::FileDialog::builder()
            .title(t("Select the source NEFs", "选择原始 NEF"))
            .action(nwg::FileDialogAction::Open)
            .filters(t("Nikon RAW files (*.NEF)", "尼康 RAW 文件 (*.NEF)"))
            .multiselect(true)
            .build(&mut ui.open_dialog)?;
        nwg::FileDialog::builder()
            .title(t("Save the merged NEFX", "保存合成后的 NEFX"))
            .action(nwg::FileDialogAction::Save)
            .filters(t("Nikon NEFX files (*.NEFX)", "尼康 NEFX 文件 (*.NEFX)"))
            .build(&mut ui.save_dialog)?;
        nwg::Notice::builder()
            .parent(&ui.window)
            .build(&mut ui.notice)?;

        let app = Self {
            ui,
            language,
            files: RefCell::default(),
            messages: RefCell::default(),
            close_after_task: Cell::default(),
        };
        app.resize();
        app.refresh(None);
        app.ui
            .status
            .set_text(t("Waiting for NEF files", "等待添加 NEF"));
        Ok(app)
    }

    fn t<'a>(&self, english: &'a str, chinese: &'a str) -> &'a str {
        self.language.text(english, chinese)
    }

    fn resize(&self) {
        let (width, height) = self.ui.window.size();
        let width = (width as i32 - 2 * MARGIN).max(1);
        let height = height as i32;
        let list_height = ((height - 340) / 2).max(74);
        let bottom = LIST_TOP + list_height;
        self.ui.instructions.set_size(width as u32, 44);
        self.ui.list.set_position(MARGIN, LIST_TOP);
        self.ui.list.set_size(width as u32, list_height as u32);
        self.ui.summary.set_position(MARGIN, bottom + 7);
        self.ui.summary.set_size(width as u32, 24);
        self.ui.output_label.set_position(MARGIN, bottom + 34);
        self.ui.output_label.set_size(180, 20);
        self.ui.output.set_position(MARGIN, bottom + 56);
        self.ui.output.set_size((width - 110).max(1) as u32, 30);
        self.ui
            .browse
            .set_position(MARGIN + width - 100, bottom + 56);
        self.ui.browse.set_size(100, 30);
        self.ui.motion.set_position(MARGIN, bottom + 96);
        self.ui.motion.set_size(width as u32, 30);
        self.ui.correct_ca.set_position(MARGIN, bottom + 126);
        self.ui.correct_ca.set_size((width - 282).max(1) as u32, 30);
        self.ui
            .calculate
            .set_position(MARGIN + width - 272, bottom + 126);
        self.ui.calculate.set_size(122, 30);
        self.ui
            .merge
            .set_position(MARGIN + width - 140, bottom + 126);
        self.ui.merge.set_size(140, 30);
        self.ui.status.set_position(MARGIN, bottom + 171);
        self.ui.status.set_size(width as u32, 20);
        self.ui.progress.set_position(MARGIN, bottom + 193);
        self.ui.progress.set_size(width as u32, 12);
        self.ui.log.set_position(MARGIN, bottom + 215);
        self.ui
            .log
            .set_size(width as u32, (height - bottom - 215 - MARGIN).max(1) as u32);
    }

    fn busy(&self) -> bool {
        self.messages.borrow().is_some()
    }

    fn update_buttons(&self) {
        let idle = !self.busy();
        let selected = self.ui.list.selection();
        let count = self.files.borrow().len();
        self.ui.add.set_enabled(idle);
        self.ui
            .reference
            .set_enabled(idle && selected.is_some_and(|i| i > 0));
        self.ui.remove.set_enabled(idle && selected.is_some());
        self.ui.remove_all.set_enabled(idle && count > 0);
        self.ui.browse.set_enabled(idle);
        self.ui.output.set_readonly(!idle);
        self.ui.motion.set_enabled(idle);
        self.ui.correct_ca.set_enabled(idle);
        self.ui.calculate.set_enabled(idle && can_calculate(count));
        self.ui
            .merge
            .set_enabled(idle && dimensions(count).is_some());
    }

    fn refresh(&self, selected: Option<usize>) {
        let (count, listing) = {
            let files = self.files.borrow();
            let listing = files
                .iter()
                .enumerate()
                .map(|(index, file)| {
                    let name = file
                        .file_name()
                        .unwrap_or(file.as_os_str())
                        .to_string_lossy();
                    let reference = if index == 0 {
                        self.t("  (reference)", "  （参考）")
                    } else {
                        ""
                    };
                    let folder = file.parent().unwrap_or(Path::new(""));
                    format!("{name}{reference}    {}", folder.display())
                })
                .collect();
            (files.len(), listing)
        };
        self.ui.list.set_collection(listing);
        self.ui.list.set_selection(selected);
        let mut summary = match self.language {
            Language::English => format!("NEF files selected  {count}"),
            Language::Chinese => format!("已选 NEF  {count} 张"),
        };
        if let Some((width, height)) = dimensions(count) {
            summary.push_str(&format!("    ·    {width} × {height}"));
        }
        self.ui.summary.set_text(&summary);
        self.update_buttons();
    }

    fn edit_files(&self, edit: impl FnOnce(&mut Vec<PathBuf>) -> Option<usize>) {
        let (selected, previous, suggested) = {
            let mut files = self.files.borrow_mut();
            let previous = suggested_output(files.first());
            let selected = edit(&mut files);
            (selected, previous, suggested_output(files.first()))
        };
        let current = self.ui.output.text();
        if current.is_empty() || previous.as_deref() == Some(Path::new(&current)) {
            self.ui.output.set_text(
                &suggested
                    .as_deref()
                    .unwrap_or(Path::new(""))
                    .to_string_lossy(),
            );
        }
        self.ui.progress.set_pos(0);
        self.ui.log.set_text("");
        self.ui.status.set_text(if suggested.is_some() {
            ""
        } else {
            self.t("Waiting for NEF files", "等待添加 NEF")
        });
        self.refresh(selected);
    }

    fn add_files(&self, mut paths: Vec<PathBuf>) {
        if self.busy() || paths.is_empty() {
            return;
        }
        let count = paths.len();
        paths.retain(|path| extension_is(path, "nef"));
        if paths.is_empty() {
            self.ui
                .status
                .set_text(self.t("Select NEF files to merge", "请选择要合成的 NEF 文件"));
            return;
        }
        let ignored = count - paths.len();
        paths.sort_by(|a, b| a.file_name().cmp(&b.file_name()).then_with(|| a.cmp(b)));
        self.edit_files(|files| {
            for path in paths {
                if !files.contains(&path) {
                    files.push(path);
                }
            }
            Some(0)
        });
        if ignored > 0 {
            self.ui.status.set_text(self.t(
                "Skipped files without a .NEF extension",
                "已跳过非 NEF 文件",
            ));
        }
    }

    fn choose_inputs(&self) {
        if self.ui.open_dialog.run(Some(&self.ui.window)) {
            match self.ui.open_dialog.get_selected_items() {
                Ok(paths) => self.add_files(paths.into_iter().map(PathBuf::from).collect()),
                Err(error) => self.show_error(
                    self.t("Cannot read selected files", "无法读取所选文件"),
                    &error,
                ),
            }
        }
    }

    fn choose_output(&self) {
        if let Some(folder) = Path::new(&self.ui.output.text())
            .parent()
            .and_then(Path::to_str)
        {
            let _ = self.ui.save_dialog.set_default_folder(folder);
        }
        if self.ui.save_dialog.run(Some(&self.ui.window)) {
            match self.ui.save_dialog.get_selected_item() {
                Ok(path) => self.ui.output.set_text(&path.to_string_lossy()),
                Err(error) => self.show_error(
                    self.t("Cannot read the output path", "无法读取输出路径"),
                    &error,
                ),
            }
        }
    }

    fn show_error(&self, title: &str, error: &impl std::fmt::Display) {
        nwg::modal_error_message(&self.ui.window, title, &format!("{error:#}"));
    }

    fn start(&self, calculate_only: bool) {
        if self.busy() {
            return;
        }
        let files = self.files.borrow().clone();
        let target = if calculate_only {
            if !can_calculate(files.len()) {
                return;
            }
            None
        } else {
            if dimensions(files.len()).is_none() {
                return;
            }
            let name = self.ui.output.text();
            if name.trim().is_empty() {
                self.ui
                    .status
                    .set_text(self.t("Choose an output file", "请选择输出文件"));
                self.ui.output.set_focus();
                return;
            }
            let mut output = PathBuf::from(name.trim());
            if output.extension().is_none() {
                output.set_extension("NEFX");
            }
            self.ui.output.set_text(&output.to_string_lossy());
            Some((
                output,
                Options {
                    reference_motion: self.ui.motion.check_state() == nwg::CheckBoxState::Checked,
                    correct_ca: self.ui.correct_ca.check_state() == nwg::CheckBoxState::Checked,
                },
            ))
        };
        let notice = self.ui.notice.sender();
        let language = self.language;
        let (send, receive) = channel();
        self.messages.replace(Some(receive));
        self.update_buttons();
        self.ui.log.set_text("");
        self.ui.progress.set_pos(0);
        self.ui.status.set_text(if calculate_only {
            self.t("Preparing alignment…", "正在准备配准…")
        } else {
            self.t("Preparing merge…", "正在准备合成…")
        });

        let worker = thread::Builder::new().spawn(move || {
            let post = |message| {
                if send.send(message).is_ok() {
                    notice.notice();
                }
            };
            let result = catch_unwind(AssertUnwindSafe(|| {
                let update = |event| post(Message::Update(event));
                if let Some((output, options)) = target {
                    merge(&files, &output, options, language, update)
                        .map(|()| language.text("NEFX saved", "NEFX 已保存"))
                } else {
                    calculate_alignment(&files, language, update)
                        .map(|()| language.text("Alignment calculated", "配准计算完成"))
                }
            }))
            .unwrap_or_else(|_| {
                Err(anyhow::anyhow!(
                    "{}",
                    language.text("The task stopped unexpectedly", "任务意外中断")
                ))
            });
            post(Message::Finished(result));
        });
        if let Err(error) = worker {
            self.finish(Err(
                anyhow::Error::new(error).context(self.t("Cannot start task", "无法启动任务"))
            ));
        }
    }

    fn finish(&self, result: anyhow::Result<&str>) {
        self.messages.replace(None);
        self.update_buttons();
        match result {
            Ok(status) => {
                self.ui.progress.set_pos(100);
                self.ui.status.set_text(status);
            }
            Err(error) => {
                self.ui.progress.set_pos(0);
                self.ui.status.set_text(self.t(
                    "Task failed. See the details below.",
                    "任务失败，详情见下方记录。",
                ));
                self.ui.log.appendln(&format!("{error:#}"));
            }
        }
        if self.close_after_task.get() {
            self.ui.window.close();
        }
    }

    fn receive(&self) {
        loop {
            let message = self.messages.borrow().as_ref().map(Receiver::try_recv);
            match message {
                Some(Ok(Message::Update(Update::Status(percent, status)))) => {
                    self.ui.progress.set_pos(percent);
                    if !self.close_after_task.get() {
                        self.ui.status.set_text(&status);
                    }
                }
                Some(Ok(Message::Update(Update::Log(line)))) => self.ui.log.appendln(&line),
                Some(Ok(Message::Finished(result))) => self.finish(result),
                Some(Err(TryRecvError::Disconnected)) => {
                    self.finish(Err(anyhow::anyhow!(
                        "{}",
                        self.t("The task stopped unexpectedly", "任务意外中断")
                    )));
                }
                _ => break,
            }
        }
    }

    fn event(&self, event: nwg::Event, data: nwg::EventData, handle: nwg::ControlHandle) {
        use nwg::Event::*;
        match event {
            OnButtonClick if handle == self.ui.add.handle => self.choose_inputs(),
            OnButtonClick if handle == self.ui.browse.handle => self.choose_output(),
            OnButtonClick if handle == self.ui.merge.handle => self.start(false),
            OnButtonClick if handle == self.ui.calculate.handle => self.start(true),
            OnButtonClick if handle == self.ui.remove_all.handle => {
                self.edit_files(|files| {
                    files.clear();
                    None
                });
                self.ui.add.set_focus();
            }
            OnButtonClick if handle == self.ui.reference.handle => {
                if let Some(index) = self.ui.list.selection() {
                    self.edit_files(|files| {
                        files[..=index].rotate_right(1);
                        Some(0)
                    });
                }
            }
            OnButtonClick if handle == self.ui.remove.handle => {
                if let Some(index) = self.ui.list.selection() {
                    self.edit_files(|files| {
                        files.remove(index);
                        files.len().checked_sub(1).map(|last| index.min(last))
                    });
                }
            }
            OnListBoxSelect if handle == self.ui.list.handle => self.update_buttons(),
            OnFileDrop => self.add_files(
                data.on_file_drop()
                    .files()
                    .into_iter()
                    .map(PathBuf::from)
                    .collect(),
            ),
            OnNotice if handle == self.ui.notice.handle => self.receive(),
            OnResize if handle == self.ui.window.handle => self.resize(),
            OnMinMaxInfo if handle == self.ui.window.handle => {
                data.on_min_max().set_min_size(650, 484)
            }
            OnWindowClose if handle == self.ui.window.handle => {
                if self.busy() {
                    if let nwg::EventData::OnWindowClose(close) = data {
                        close.close(false);
                    }
                    self.close_after_task.set(true);
                    self.ui.status.set_text(
                        self.t("Closing after the task finishes…", "任务完成后关闭窗口…"),
                    );
                } else {
                    nwg::stop_thread_dispatch();
                }
            }
            _ => {}
        }
    }
}

fn show(language: Language) -> Result<(), nwg::NwgError> {
    nwg::init()?;
    nwg::Font::set_global_family(language.text("Segoe UI", "Microsoft YaHei UI"))?;
    let app = Rc::new(App::build(language)?);
    let weak = Rc::downgrade(&app);
    let handler =
        nwg::full_bind_event_handler(&app.ui.window.handle, move |event, data, handle| {
            if let Some(app) = weak.upgrade() {
                app.event(event, data, handle);
            }
        });
    app.add_files(std::env::args_os().skip(1).map(PathBuf::from).collect());
    app.ui.window.set_visible(true);
    app.ui.add.set_focus();
    nwg::dispatch_thread_events();
    nwg::unbind_event_handler(&handler);
    Ok(())
}

pub fn run() {
    let language = user_language();
    if let Err(error) = show(language) {
        nwg::fatal_message(
            language.text("Cannot start Parsek NEFX", "无法启动 Parsek NEFX"),
            &error.to_string(),
        );
    }
}
