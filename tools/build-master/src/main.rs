#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod ui;

fn main() {
    if let Err(error) = ui::run() {
        ui::startup_error(&error);
    }
}
