use crate::firmware::{BuildError, BuildReport, Stage};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Language {
    Chinese,
    English,
}

impl Language {
    pub fn from_langid(id: u16) -> Self {
        if id & 0x3ff == 0x04 {
            Self::Chinese
        } else {
            Self::English
        }
    }

    pub fn system() -> Self {
        #[cfg(windows)]
        unsafe {
            Self::from_langid(windows_sys::Win32::Globalization::GetUserDefaultUILanguage())
        }
        #[cfg(not(windows))]
        Self::English
    }

    pub fn text<'a>(self, chinese: &'a str, english: &'a str) -> &'a str {
        match self {
            Self::Chinese => chinese,
            Self::English => english,
        }
    }

    pub fn title(self) -> &'static str {
        self.text("Z7II 宿主固件生成器", "Z7II Host Firmware Builder")
    }

    pub fn stage(self, stage: Stage) -> &'static str {
        match stage {
            Stage::Reading => self.text("正在读取原厂固件…", "Reading original firmware…"),
            Stage::Checking => self.text("正在校验原厂固件…", "Checking original firmware…"),
            Stage::Patching => self.text("正在应用宿主补丁…", "Applying host patches…"),
            Stage::Packing => self.text("正在生成升级包…", "Building the update package…"),
            Stage::Verifying => self.text("正在核对升级包…", "Verifying the update package…"),
            Stage::Writing => self.text(
                "正在保存固件和校验报告…",
                "Saving firmware and verification report…",
            ),
        }
    }

    pub fn error(self, error: &BuildError) -> String {
        match error {
            BuildError::InvalidInput => self.text(
                "原厂固件校验失败。请选择原版 Nikon Z 7II C 1.70 升级包 Z7_2_0170.bin。",
                "Original firmware verification failed. Select the original Nikon Z 7II C 1.70 update, Z7_2_0170.bin."
            ).to_owned(),
            BuildError::InvalidContainer => self.text("固件容器校验失败。", "Firmware container verification failed.").to_owned(),
            BuildError::InvalidMaster => self.text("宿主补丁校验失败。", "Host patch verification failed.").to_owned(),
            BuildError::InvalidPackage => self.text("升级包与已验证版本不一致。", "The update package does not match the verified release.").to_owned(),
            BuildError::OutputExists(path) => format!("{}\r\n{}", self.text(
                "输出固件或报告已存在。请选择新的输出位置。",
                "The output firmware or report already exists. Choose a new output location."
            ), path.display()),
            BuildError::Io(error) => format!("{}\r\n{error}", self.text(
                "读取或保存文件失败。请检查路径、访问权限和磁盘空间。",
                "Could not read or save a file. Check the path, access permissions and disk space."
            )),
        }
    }

    pub fn success(self, report: &BuildReport) -> String {
        format!(
            "{}\r\n{}\r\n{}\r\n{}\r\nSHA-256\r\n{}",
            self.text("生成成功", "Build completed"),
            report.output.display(),
            self.text("校验报告", "Verification report"),
            report.manifest.display(),
            report.sha256
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn matches_windows_display_languages() {
        for id in [0x0804, 0x0404, 0x0c04] {
            assert_eq!(Language::from_langid(id), Language::Chinese);
        }
        for id in [0x0409, 0x0809, 0x0411, 0x0407] {
            assert_eq!(Language::from_langid(id), Language::English);
        }
        assert_eq!(Language::Chinese.title(), "Z7II 宿主固件生成器");
        assert_eq!(
            Language::English.stage(Stage::Writing),
            "Saving firmware and verification report…"
        );
    }
}
