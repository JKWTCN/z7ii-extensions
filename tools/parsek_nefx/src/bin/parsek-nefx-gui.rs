#![cfg_attr(windows, windows_subsystem = "windows")]

#[cfg(windows)]
#[path = "../gui.rs"]
mod gui;

#[cfg(windows)]
fn main() {
    gui::run();
}

#[cfg(not(windows))]
fn main() {
    eprintln!("The Parsek NEFX graphical interface requires Windows.");
    std::process::exit(1);
}
