#![windows_subsystem = "windows"]

mod repair;

use native_windows_gui as nwg;
use std::cell::{Cell, RefCell};
use std::path::PathBuf;
use std::rc::Rc;
use std::sync::mpsc::{Receiver, channel};
use std::thread;

const TITLE: &str = "Z7 II NEFX 修复工具";
const MARGIN: u32 = 20;
const LOG_TOP: u32 = 142;

enum Update {
    Status(String),
    Log(String),
    Done(String),
}

#[derive(Default)]
struct App {
    window: nwg::Window,
    instructions: nwg::Label,
    files: nwg::Button,
    folder: nwg::Button,
    status: nwg::Label,
    log: nwg::TextBox,
    file_dialog: nwg::FileDialog,
    folder_dialog: nwg::FileDialog,
    notice: nwg::Notice,
    updates: RefCell<Option<Receiver<Update>>>,
    close_after_batch: Cell<bool>,
}

impl App {
    fn build() -> Result<Self, nwg::NwgError> {
        let mut app = Self::default();
        let left = MARGIN as i32;
        nwg::Window::builder()
            .title(TITLE)
            .size((680, 400))
            .flags(nwg::WindowFlags::MAIN_WINDOW)
            .center(true)
            .accept_files(true)
            .build(&mut app.window)?;
        nwg::Label::builder()
            .parent(&app.window)
            .text("选择或拖入尼康工坊合成的 NEFX 文件或文件夹。\r\n修复副本保存在原目录，文件名带有 _NX_compatible 后缀。")
            .position((left, 18))
            .size((640, 48))
            .build(&mut app.instructions)?;
        nwg::Button::builder()
            .parent(&app.window)
            .text("选择 NEFX 文件")
            .position((left, 72))
            .size((144, 34))
            .build(&mut app.files)?;
        nwg::Button::builder()
            .parent(&app.window)
            .text("选择文件夹")
            .position((left + 156, 72))
            .size((144, 34))
            .build(&mut app.folder)?;
        nwg::Label::builder()
            .parent(&app.window)
            .text("等待选择文件")
            .position((left, 118))
            .size((640, 24))
            .build(&mut app.status)?;
        nwg::TextBox::builder()
            .parent(&app.window)
            .position((left, LOG_TOP as i32))
            .flags(
                nwg::TextBoxFlags::VISIBLE
                    | nwg::TextBoxFlags::VSCROLL
                    | nwg::TextBoxFlags::TAB_STOP,
            )
            .readonly(true)
            .limit(0x7fff_ffff)
            .build(&mut app.log)?;
        nwg::FileDialog::builder()
            .title("选择合成后的 NEFX 文件")
            .action(nwg::FileDialogAction::Open)
            .filters("NEFX 文件(*.NEFX)")
            .multiselect(true)
            .build(&mut app.file_dialog)?;
        nwg::FileDialog::builder()
            .title("选择 NEFX 所在的文件夹")
            .action(nwg::FileDialogAction::OpenDirectory)
            .build(&mut app.folder_dialog)?;
        nwg::Notice::builder()
            .parent(&app.window)
            .build(&mut app.notice)?;
        app.resize();
        Ok(app)
    }

    fn resize(&self) {
        let (width, height) = self.window.size();
        let width = width.saturating_sub(2 * MARGIN);
        self.instructions.set_size(width, 48);
        self.status.set_size(width, 24);
        self.log
            .set_size(width, height.saturating_sub(LOG_TOP + MARGIN));
    }

    fn select(&self, folder: bool) {
        let dialog = if folder {
            &self.folder_dialog
        } else {
            &self.file_dialog
        };
        if !dialog.run(Some(&self.window)) {
            return;
        }
        let selection = if folder {
            dialog.get_selected_item().map(|path| vec![path])
        } else {
            dialog.get_selected_items()
        };
        match selection {
            Ok(paths) => self.start(paths.into_iter().map(PathBuf::from).collect()),
            Err(error) => {
                nwg::modal_error_message(&self.window, "无法读取所选路径", &error.to_string());
            }
        }
    }

    fn start(&self, paths: Vec<PathBuf>) {
        if paths.is_empty() || self.updates.borrow().is_some() {
            return;
        }

        let (send, receive) = channel();
        self.updates.replace(Some(receive));
        self.files.set_enabled(false);
        self.folder.set_enabled(false);
        self.log.set_text("");
        self.status.set_text("正在查找 NEFX 文件……");

        let notice = self.notice.sender();
        let worker = thread::Builder::new().spawn(move || {
            process(paths, |update| {
                if send.send(update).is_ok() {
                    notice.notice();
                }
            });
        });
        if let Err(error) = worker {
            self.log.appendln(&error.to_string());
            self.finish("无法启动修复任务");
        }
    }

    fn finish(&self, status: &str) {
        self.updates.replace(None);
        self.status.set_text(status);
        self.files.set_enabled(true);
        self.folder.set_enabled(true);
        if self.close_after_batch.get() {
            self.window.close();
        } else {
            self.files.set_focus();
        }
    }

    fn receive_updates(&self) {
        loop {
            let update = self.updates.borrow().as_ref().map(Receiver::try_recv);
            match update {
                Some(Ok(Update::Status(text))) => {
                    if !self.close_after_batch.get() {
                        self.status.set_text(&text);
                    }
                }
                Some(Ok(Update::Log(text))) => self.log.appendln(&text),
                Some(Ok(Update::Done(text))) => self.finish(&text),
                Some(Err(std::sync::mpsc::TryRecvError::Disconnected)) => {
                    self.finish("修复任务已中断");
                }
                _ => break,
            }
        }
    }

    fn event(&self, event: nwg::Event, data: nwg::EventData, handle: nwg::ControlHandle) {
        use nwg::Event::*;
        match event {
            OnButtonClick if handle == self.files.handle => self.select(false),
            OnButtonClick if handle == self.folder.handle => self.select(true),
            OnFileDrop => self.start(
                data.on_file_drop()
                    .files()
                    .into_iter()
                    .map(PathBuf::from)
                    .collect(),
            ),
            OnNotice if handle == self.notice.handle => self.receive_updates(),
            OnResize if handle == self.window.handle => self.resize(),
            OnMinMaxInfo if handle == self.window.handle => {
                data.on_min_max().set_min_size(500, 300)
            }
            OnWindowClose if handle == self.window.handle => {
                if self.updates.borrow().is_some() {
                    if let nwg::EventData::OnWindowClose(close) = data {
                        close.close(false);
                    }
                    self.close_after_batch.set(true);
                    self.status.set_text("当前批次处理完成后关闭……");
                } else {
                    nwg::stop_thread_dispatch();
                }
            }
            _ => {}
        }
    }
}

fn process(paths: Vec<PathBuf>, update: impl Fn(Update)) {
    let mut sources = Vec::new();
    let mut failed = 0;
    for path in paths {
        match repair::source_files(&path) {
            Ok(files) if files.is_empty() => {
                failed += 1;
                update(Update::Log(format!(
                    "失败  {}\n  文件夹中没有可处理的 NEFX",
                    path.display()
                )));
            }
            Ok(files) => sources.extend(files),
            Err(error) => {
                failed += 1;
                update(Update::Log(format!("失败  {}\n  {error}", path.display())));
            }
        }
    }

    let mut created = 0;
    let mut compatible = 0;
    let total = sources.len();
    for (index, source) in sources.into_iter().enumerate() {
        update(Update::Status(format!("正在处理 {}/{total}…", index + 1)));
        let result = match repair::repair(&source) {
            Ok(Some(output)) => {
                created += 1;
                format!("已生成  {}", output.display())
            }
            Ok(None) => {
                compatible += 1;
                format!("已兼容  {}", source.display())
            }
            Err(error) => {
                failed += 1;
                format!("失败  {}\n  {error}", source.display())
            }
        };
        update(Update::Log(result));
    }
    update(Update::Done(format!(
        "完成  ·  已生成 {created}  ·  已兼容 {compatible}  ·  失败 {failed}"
    )));
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
    app.files.set_focus();
    app.start(std::env::args_os().skip(1).map(PathBuf::from).collect());
    nwg::dispatch_thread_events();
    nwg::unbind_event_handler(&handler);
    Ok(())
}

fn main() {
    if let Err(error) = run() {
        nwg::fatal_message(TITLE, &error.to_string());
    }
}
