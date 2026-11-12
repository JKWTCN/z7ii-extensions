use build_master::{
    firmware::{self, BuildError, BuildReport, Stage},
    language::Language,
};
use std::{
    cell::RefCell,
    ffi::OsString,
    io,
    mem::{size_of, zeroed},
    os::windows::ffi::{OsStrExt, OsStringExt},
    path::PathBuf,
    ptr::{null, null_mut},
    sync::mpsc::{self, Receiver, TryRecvError},
    thread,
};
use windows_sys::Win32::{
    Foundation::{HWND, LPARAM, LRESULT, RECT, WPARAM},
    Graphics::Gdi::{COLOR_BTNFACE, CreateFontW, DEFAULT_CHARSET, DeleteObject, HFONT},
    System::LibraryLoader::GetModuleHandleW,
    UI::{
        Controls::{
            Dialogs::*, ICC_PROGRESS_CLASS, INITCOMMONCONTROLSEX, InitCommonControlsEx, PBM_SETPOS,
            PBM_SETRANGE32,
        },
        HiDpi::{
            DPI_AWARENESS_CONTEXT_SYSTEM_AWARE, GetDpiForSystem, SetProcessDpiAwarenessContext,
        },
        Input::KeyboardAndMouse::EnableWindow,
        WindowsAndMessaging::*,
    },
};

const INPUT: usize = 101;
const PICK_INPUT: usize = 102;
const OUTPUT: usize = 103;
const PICK_OUTPUT: usize = 104;
const BUILD: usize = 105;
const STATUS: usize = 106;
const PROGRESS: usize = 107;

enum Event {
    Progress(Stage),
    Done(Result<BuildReport, BuildError>),
}

struct App {
    language: Language,
    font: HFONT,
    receiver: Option<Receiver<Event>>,
}

fn wide(text: &str) -> Vec<u16> {
    text.encode_utf16().chain(Some(0)).collect()
}

fn set_text(window: HWND, text: &str) {
    unsafe {
        SetWindowTextW(window, wide(text).as_ptr());
    }
}

fn read_path(window: HWND) -> PathBuf {
    unsafe {
        let size = GetWindowTextLengthW(window) as usize;
        let mut text = vec![0; size + 1];
        let length = GetWindowTextW(window, text.as_mut_ptr(), text.len() as i32);
        PathBuf::from(OsString::from_wide(&text[..length.max(0) as usize]))
    }
}

fn path_text(window: HWND, path: &std::path::Path) {
    let text: Vec<_> = path.as_os_str().encode_wide().chain(Some(0)).collect();
    unsafe {
        SetWindowTextW(window, text.as_ptr());
    }
}

// Keep the native control's geometry, style and font together at each call site.
#[allow(clippy::too_many_arguments)]
fn control(
    parent: HWND,
    class: &str,
    label: &str,
    id: usize,
    style: u32,
    area: [i32; 4],
    dpi: u32,
    font: HFONT,
) -> io::Result<HWND> {
    unsafe {
        let [x, y, w, h] = area.map(|n| n * dpi as i32 / 96);
        let handle = CreateWindowExW(
            0,
            wide(class).as_ptr(),
            wide(label).as_ptr(),
            WS_CHILD | WS_VISIBLE | style,
            x,
            y,
            w,
            h,
            parent,
            id as _,
            null_mut(),
            null(),
        );
        if handle.is_null() {
            return Err(io::Error::last_os_error());
        }
        SendMessageW(handle, WM_SETFONT, font as usize, 1);
        Ok(handle)
    }
}

fn create_controls(window: HWND, app: &mut App) -> io::Result<()> {
    let lang = app.language;
    unsafe {
        let dpi = GetDpiForSystem().max(96);
        app.font = CreateFontW(
            -16 * dpi as i32 / 96,
            0,
            0,
            0,
            400,
            0,
            0,
            0,
            DEFAULT_CHARSET as u32,
            0,
            0,
            0,
            0,
            wide("Segoe UI").as_ptr(),
        );
        if app.font.is_null() {
            return Err(io::Error::last_os_error());
        }
        control(
            window,
            "STATIC",
            "Nikon Z 7II C 1.70   ·   1 MiB / 4 MiB",
            0,
            0,
            [24, 20, 712, 26],
            dpi,
            app.font,
        )?;
        control(
            window,
            "STATIC",
            lang.text("原厂固件位置", "Original firmware"),
            0,
            0,
            [24, 66, 712, 24],
            dpi,
            app.font,
        )?;
        control(
            window,
            "EDIT",
            "",
            INPUT,
            WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL as u32,
            [24, 94, 596, 30],
            dpi,
            app.font,
        )?;
        control(
            window,
            "BUTTON",
            lang.text("选择文件…", "Browse…"),
            PICK_INPUT,
            WS_TABSTOP,
            [632, 93, 104, 32],
            dpi,
            app.font,
        )?;
        control(
            window,
            "STATIC",
            lang.text("宿主固件输出位置", "Host firmware output"),
            0,
            0,
            [24, 146, 712, 24],
            dpi,
            app.font,
        )?;
        control(
            window,
            "EDIT",
            "",
            OUTPUT,
            WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL as u32,
            [24, 174, 596, 30],
            dpi,
            app.font,
        )?;
        control(
            window,
            "BUTTON",
            lang.text("选择位置…", "Save as…"),
            PICK_OUTPUT,
            WS_TABSTOP,
            [632, 173, 104, 32],
            dpi,
            app.font,
        )?;
        control(
            window,
            "BUTTON",
            lang.text("生成宿主固件", "Build host firmware"),
            BUILD,
            WS_TABSTOP | BS_DEFPUSHBUTTON as u32,
            [24, 228, 216, 36],
            dpi,
            app.font,
        )?;
        control(
            window,
            "msctls_progress32",
            "",
            PROGRESS,
            0,
            [24, 282, 712, 18],
            dpi,
            app.font,
        )?;
        SendMessageW(GetDlgItem(window, PROGRESS as i32), PBM_SETRANGE32, 0, 6);
        control(
            window,
            "EDIT",
            lang.text(
                "请选择原版 C 1.70 固件和输出位置。",
                "Select the original C 1.70 firmware and an output location.",
            ),
            STATUS,
            WS_BORDER
                | ES_MULTILINE as u32
                | ES_READONLY as u32
                | ES_AUTOVSCROLL as u32
                | WS_VSCROLL,
            [24, 318, 712, 128],
            dpi,
            app.font,
        )?;
        if SetTimer(window, 1, 100, None) == 0 {
            return Err(io::Error::last_os_error());
        }
    }
    Ok(())
}

fn choose_path(window: HWND, lang: Language, save: bool) -> io::Result<Option<PathBuf>> {
    unsafe {
        let mut filename = vec![0u16; 32768];
        if save {
            let default = wide("Z7_2_0170.bin");
            filename[..default.len()].copy_from_slice(&default);
        }
        let filter = wide(lang.text(
            "固件文件 (*.bin)\0*.bin\0所有文件\0*.*\0",
            "Firmware (*.bin)\0*.bin\0All files\0*.*\0",
        ));
        let title = wide(if save {
            lang.text("选择宿主固件输出位置", "Save host firmware")
        } else {
            lang.text("选择原版 C 1.70 固件", "Select original C 1.70 firmware")
        });
        let extension = wide("bin");
        let mut dialog: OPENFILENAMEW = zeroed();
        dialog.lStructSize = size_of::<OPENFILENAMEW>() as u32;
        dialog.hwndOwner = window;
        dialog.lpstrFilter = filter.as_ptr();
        dialog.nFilterIndex = 1;
        dialog.lpstrFile = filename.as_mut_ptr();
        dialog.nMaxFile = filename.len() as u32;
        dialog.lpstrTitle = title.as_ptr();
        dialog.lpstrDefExt = extension.as_ptr();
        dialog.Flags = OFN_EXPLORER
            | OFN_NOCHANGEDIR
            | OFN_PATHMUSTEXIST
            | if save { 0 } else { OFN_FILEMUSTEXIST };
        let accepted = if save {
            GetSaveFileNameW(&mut dialog)
        } else {
            GetOpenFileNameW(&mut dialog)
        };
        if accepted == 0 {
            let error = CommDlgExtendedError();
            return if error == 0 {
                Ok(None)
            } else {
                Err(io::Error::other(format!(
                    "{} {error:#x}",
                    lang.text("文件对话框错误", "File dialog error")
                )))
            };
        }
        let length = filename
            .iter()
            .position(|value| *value == 0)
            .unwrap_or(filename.len());
        Ok(Some(PathBuf::from(OsString::from_wide(
            &filename[..length],
        ))))
    }
}

fn busy(window: HWND, active: bool) {
    unsafe {
        for id in [INPUT, PICK_INPUT, OUTPUT, PICK_OUTPUT, BUILD] {
            EnableWindow(GetDlgItem(window, id as i32), (!active) as i32);
        }
        EnableMenuItem(
            GetSystemMenu(window, 0),
            SC_CLOSE,
            MF_BYCOMMAND | if active { MF_GRAYED } else { MF_ENABLED },
        );
    }
}

fn start_build(window: HWND, app: &mut App) {
    if app.receiver.is_some() {
        return;
    }
    let input = read_path(unsafe { GetDlgItem(window, INPUT as i32) });
    let output = read_path(unsafe { GetDlgItem(window, OUTPUT as i32) });
    if input.as_os_str().is_empty() || output.as_os_str().is_empty() {
        set_text(
            unsafe { GetDlgItem(window, STATUS as i32) },
            app.language.text(
                "请先选择原厂固件和输出位置。",
                "Choose the original firmware and an output location first.",
            ),
        );
        return;
    }
    let (sender, receiver) = mpsc::channel();
    app.receiver = Some(receiver);
    busy(window, true);
    unsafe {
        SendMessageW(GetDlgItem(window, PROGRESS as i32), PBM_SETPOS, 0, 0);
    }
    thread::spawn(move || {
        let result = firmware::build(&input, &output, |stage| {
            let _ = sender.send(Event::Progress(stage));
        });
        let _ = sender.send(Event::Done(result));
    });
}

fn poll(window: HWND, app: &mut App) {
    loop {
        let Some(receiver) = &app.receiver else {
            return;
        };
        let event = match receiver.try_recv() {
            Ok(event) => event,
            Err(TryRecvError::Empty) => return,
            Err(TryRecvError::Disconnected) => {
                set_text(
                    unsafe { GetDlgItem(window, STATUS as i32) },
                    app.language.text(
                        "构建任务意外结束。请重新尝试。",
                        "The build task stopped unexpectedly. Please try again.",
                    ),
                );
                app.receiver = None;
                busy(window, false);
                return;
            }
        };
        match event {
            Event::Progress(stage) => {
                set_text(
                    unsafe { GetDlgItem(window, STATUS as i32) },
                    app.language.stage(stage),
                );
                unsafe {
                    SendMessageW(
                        GetDlgItem(window, PROGRESS as i32),
                        PBM_SETPOS,
                        stage as usize,
                        0,
                    );
                }
            }
            Event::Done(result) => {
                let text = match result {
                    Ok(report) => {
                        unsafe {
                            SendMessageW(GetDlgItem(window, PROGRESS as i32), PBM_SETPOS, 6, 0);
                        }
                        app.language.success(&report)
                    }
                    Err(error) => app.language.error(&error),
                };
                set_text(unsafe { GetDlgItem(window, STATUS as i32) }, &text);
                app.receiver = None;
                busy(window, false);
                return;
            }
        }
    }
}

unsafe extern "system" fn window_proc(
    window: HWND,
    message: u32,
    wparam: WPARAM,
    lparam: LPARAM,
) -> LRESULT {
    // The application state and native controls are owned by this UI thread.
    unsafe {
        if message == WM_NCCREATE {
            let create = &*(lparam as *const CREATESTRUCTW);
            SetWindowLongPtrW(window, GWLP_USERDATA, create.lpCreateParams as isize);
        }
        let state = GetWindowLongPtrW(window, GWLP_USERDATA) as *mut RefCell<App>;
        if !state.is_null() {
            // Native dialogs and control messages can reenter this callback.
            let Ok(mut app) = (*state).try_borrow_mut() else {
                return if message == WM_CLOSE {
                    0
                } else {
                    DefWindowProcW(window, message, wparam, lparam)
                };
            };
            match message {
                WM_COMMAND => match wparam & 0xffff {
                    PICK_INPUT | PICK_OUTPUT => {
                        let save = wparam & 0xffff == PICK_OUTPUT;
                        match choose_path(window, app.language, save) {
                            Ok(Some(path)) => path_text(
                                GetDlgItem(window, if save { OUTPUT } else { INPUT } as i32),
                                &path,
                            ),
                            Ok(None) => {}
                            Err(error) => set_text(
                                GetDlgItem(window, STATUS as i32),
                                &app.language.error(&BuildError::Io(error)),
                            ),
                        }
                        return 0;
                    }
                    BUILD => {
                        start_build(window, &mut app);
                        return 0;
                    }
                    _ => {}
                },
                WM_TIMER => {
                    poll(window, &mut app);
                    return 0;
                }
                WM_CLOSE if app.receiver.is_some() => return 0,
                WM_DESTROY => {
                    PostQuitMessage(0);
                    return 0;
                }
                _ => {}
            }
        }
        DefWindowProcW(window, message, wparam, lparam)
    }
}

pub fn run() -> io::Result<()> {
    unsafe {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        let language = Language::system();
        let app = Box::new(RefCell::new(App {
            language,
            font: null_mut(),
            receiver: None,
        }));
        let instance = GetModuleHandleW(null());
        let class_name = wide("Z7IIHostBuilder");
        let class = WNDCLASSW {
            lpfnWndProc: Some(window_proc),
            hInstance: instance,
            lpszClassName: class_name.as_ptr(),
            hCursor: LoadCursorW(null_mut(), IDC_ARROW),
            hbrBackground: (COLOR_BTNFACE + 1) as _,
            ..zeroed()
        };
        if RegisterClassW(&class) == 0 {
            return Err(io::Error::last_os_error());
        }
        let controls = INITCOMMONCONTROLSEX {
            dwSize: size_of::<INITCOMMONCONTROLSEX>() as u32,
            dwICC: ICC_PROGRESS_CLASS,
        };
        if InitCommonControlsEx(&controls) == 0 {
            return Err(io::Error::last_os_error());
        }
        let dpi = GetDpiForSystem().max(96) as i32;
        let style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        let mut rect = RECT {
            left: 0,
            top: 0,
            right: 760 * dpi / 96,
            bottom: 470 * dpi / 96,
        };
        AdjustWindowRect(&mut rect, style, 0);
        let window = CreateWindowExW(
            0,
            class_name.as_ptr(),
            wide(language.title()).as_ptr(),
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            null_mut(),
            null_mut(),
            instance,
            (app.as_ref() as *const RefCell<App>).cast(),
        );
        if window.is_null() {
            return Err(io::Error::last_os_error());
        }
        if let Err(error) = create_controls(window, &mut app.borrow_mut()) {
            DestroyWindow(window);
            return Err(error);
        }
        ShowWindow(window, SW_SHOW);
        let mut message = zeroed();
        loop {
            match GetMessageW(&mut message, null_mut(), 0, 0) {
                -1 => return Err(io::Error::last_os_error()),
                0 => break,
                _ => {
                    if IsDialogMessageW(window, &message) == 0 {
                        TranslateMessage(&message);
                        DispatchMessageW(&message);
                    }
                }
            }
        }
        DeleteObject(app.borrow().font);
    }
    Ok(())
}

pub fn startup_error(error: &io::Error) {
    let lang = Language::system();
    let message = format!(
        "{}\n{error}",
        lang.text("无法启动程序。", "Could not start the application.")
    );
    unsafe {
        MessageBoxW(
            null_mut(),
            wide(&message).as_ptr(),
            wide(lang.title()).as_ptr(),
            MB_OK | MB_ICONERROR,
        );
    }
}
