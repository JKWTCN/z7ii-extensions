# FTP 上传 / FTP upload

简体中文 | [English](#english)

Nikon Z 7II C 1.70 安装 SD 加载宿主后，可通过设定菜单的「FTP 上传」将 SD 卡上的 JPEG 和 NEF 传到 FTP 服务器。更换组合模块时，SD 卡根目录已有的 `Z72APP.CFG` 和 FTP 配置可以保留。

「FTP 上传」已在 Z 7II C 1.70 上通过 `v0.1.0-preview.24` 组合 SD 模块实机验证。

## 连接相机和电脑

- **相机热点（AP）**：在相机开启用于连接智能设备或 PC 的 Wi‑Fi 热点，让电脑连上它。运行 `ipconfig`，查看电脑 WLAN 在该网络中的 IPv4 地址。下文以电脑 `192.168.1.3`、相机 `192.168.1.1` 为例。
- **加入路由器（STA）**：通过相机「连接到 PC → 网络设定」让相机加入路由器。电脑接入同一局域网，可用 Wi‑Fi 或网线。FTP 配置填电脑在该网络中的 IPv4 地址，防火墙规则的远程地址填路由器给相机分配的 IPv4 地址。

可以先连接网络，也可以先从 FTP 菜单开始手动上传。相机显示「等待 Wi‑Fi 连接」时，可在等待期间完成网络连接。如果已经超时，连接后在「传输状态」选择「发送待传影像」。

打开 FTP 菜单、正在上传或启用自动发送时，模块会延后相机原厂的 Wi‑Fi 空闲断开。仍可手动关闭 Wi‑Fi。关闭自动发送、结束上传并退出 FTP 菜单后，原厂空闲计时继续生效。

## 配置 FileZilla Server

模块使用 IPv4 被动模式和明文 FTP，用户名、密码和照片通过未加密的连接发送。请在自己信任的局域网或相机热点上使用。

1. 打开 FileZilla Server 管理窗口，进入 **Server → Configure → Server listeners**。为电脑连接相机的 IPv4 地址设置 TCP `21` 监听，也可用 `0.0.0.0:21` 监听本机所有 IPv4 网卡。协议选择 **Explicit FTP over TLS and insecure plain FTP**。[FileZilla 监听设置](https://filezillapro.com/docs/server/advanced-options/setting-up-ftp-listeners-and-connection-security/)
2. 在 **Protocol settings → FTP and FTP over TLS (FTPS) → Passive mode** 勾选 **Use custom port range**，例如填写 `49160` 到 `49169`。防火墙使用同一范围。[FileZilla 被动模式](https://filezillapro.com/docs/server/advanced-options/filezilla-server-passive-mode/)
3. 在 **Rights management → Users** 启用一个带密码的账号。在 **Mount points** 将虚拟路径 `/` 映射到电脑上的接收目录，例如 `D:Nikon-Uploads`，权限选择 **Read + Write**，并确保 FileZilla Server 服务可以写入该目录。[FileZilla 用户设置](https://filezillapro.com/docs/server/advanced-options/filezilla-server-users-panel/) · [挂载目录](https://filezillapro.com/docs/server/advanced-options/how-to-edit-mount-points-virtual-native-paths/)

## 将 Windows 网络设为专用并放行端口

1. 在 Windows 11 打开「设置 → 网络和 Internet → Wi‑Fi」，选当前连接的网络，将「网络配置文件类型」设为「专用网络」。电脑用网线时，在「以太网」中选择相同的网络配置文件。[Microsoft 网络设置](https://support.microsoft.com/en-us/windows/experience/connectivity-networking/essential-network-settings-and-tasks-in-windows)
2. 在管理员 PowerShell 中添加入站规则，放行 **TCP 控制端口和被动数据端口**。下面以 FileZilla Server 的默认安装路径和相机热点为例。按实际情况修改程序路径、网卡名、两个 IPv4 地址及端口。[Microsoft 防火墙命令参考](https://learn.microsoft.com/en-us/powershell/module/netsecurity/new-netfirewallrule?view=windowsserver2025-ps)

```powershell
$adapter = 'WLAN'
$pcIp = '192.168.1.3'
$cameraIp = '192.168.1.1'
$server = 'C:\Program Files\FileZilla Server\filezilla-server.exe'

New-NetFirewallRule -DisplayName 'Nikon Z 7II FileZilla FTP' -Direction Inbound `
    -Action Allow -Profile Private -InterfaceAlias $adapter -Program $server `
    -Protocol TCP -LocalAddress $pcIp -RemoteAddress $cameraIp `
    -LocalPort @('21', '49160-49169')
```

也可通过「Windows Defender 防火墙（高级安全）→ 入站规则 → 新建规则 → 自定义」配置同一组条件。改用另一张网卡或更换网络后，检查规则中的地址和网卡名是否仍匹配。

## 在相机上设置和发送

1. 打开「设定菜单（扳手）→ FTP 上传 → 网络设定」，选配置 1、2 或 3。再次进入「网络设定 → 编辑当前配置」。
2. 在「地址」填写 `IPv4:端口`，例如 `192.168.1.3:21`，再填用户名和密码。目标文件夹留空时使用登录目录。需要子目录时，填写服务器的虚拟路径，例如 `/photos`，并先在服务器上建好目录。选「保存配置」。
3. 在「选择影像」点选照片，带 `*` 的照片已加入待传队列。也可在回放 i 菜单选择「选择以 FTP 发送／取消」标记当前照片。回到「FTP 上传」，选择「开始上传」。通过「传输状态」查看进度、更新状态、取消当前传输或发送剩余影像。

配置文件保存在 SD 卡根目录，分别为 `Z72FTP.CFG`、`Z72FTP2.CFG`、`Z72FTP3.CFG`。密码以明文保存在卡上，分享 SD 卡内容前请留意这些配置文件。

| 选项 | 行为 |
| --- | --- |
| 自动发送 | 将开启后写入 SD 卡的新照片加入待传队列，使用开启时选定的配置。关闭后停止加入新照片，已经入队的照片仍会发送。 |
| 传送文件为 | 「NEF + JPEG」会同时发送同一文件夹内同名的一对照片。「仅 JPEG」在存在配对 JPEG 时省略对应 NEF，单独拍摄的 NEF 仍可上传。 |
| 发送文件夹 | 选择 SD 卡中的拍摄文件夹，将支持的照片加入队列并开始上传。 |
| 取消全部选择？ | 确认后清空尚待发送的照片，并取消正在进行的上传。 |
| 同名时重写 | 开启后允许覆盖服务器上的同名文件。关闭时，服务器有同名文件便尝试使用编号后缀。 |

FTP 上传使用 SD 卡中的拍摄目录，支持每个文件最多 256 MiB。

## 确认传输

FileZilla Server 收到 `STOR` 并回复 `226` 表示上传完成。若先看到 `SIZE` 返回 `550`，随后仍有 `STOR` 和 `226`，这是新文件上传时的同名检查。若登录成功但只到 `EPSV`／`229`，请核对 Windows 的被动端口规则。若连会话记录都没有，先检查相机填写的地址和端口、FileZilla 的监听地址及控制端口规则。

![FileZilla Server 显示多次 FTP 上传成功，Windows 资源管理器中有四个收到的 NEF 文件](images/z7ii-ftp-filezilla-nef.png)

实机连续上传 NEF 到 FileZilla Server 的记录。

## English

The SD extension adds **Setup menu → FTP upload** on the Nikon Z 7II C 1.70 with the SD loader installed. It uploads JPEG and NEF photos from the SD card, up to 256 MiB per file.

FTP upload was verified on a Z 7II running C 1.70 with the `v0.1.0-preview.24` combined SD module.

1. Join the camera hotspot from the PC, or connect both devices to the same LAN using the camera's **Connect to PC → Network settings**. Run `ipconfig` on the PC and use the IPv4 address of the adapter on that LAN. You can connect before or after starting a manual upload. The camera waits for Wi‑Fi and lets you retry pending photos if the connection takes too long.
2. In FileZilla Server, allow plain FTP in **Server → Configure → Server listeners** on TCP port `21` or the port you choose. Under **Protocol settings → FTP and FTP over TLS (FTPS) → Passive mode**, set a custom range such as `49160–49169`. Enable a password-protected user and map the virtual path `/` to a local upload folder with **Read + Write** access. Use IPv4 on a trusted LAN because plain FTP sends credentials and files without encryption. [FileZilla listeners](https://filezillapro.com/docs/server/advanced-options/setting-up-ftp-listeners-and-connection-security/) · [Passive mode](https://filezillapro.com/docs/server/advanced-options/filezilla-server-passive-mode/) · [Mount points](https://filezillapro.com/docs/server/advanced-options/how-to-edit-mount-points-virtual-native-paths/)
3. In Windows **Settings → Network & internet → Wi‑Fi** (or **Ethernet**), set the connected network profile to **Private**. Run the [PowerShell example](#将-windows-网络设为专用并放行端口) as administrator. Set the adapter, local PC address, remote camera address, program path, control port and passive TCP range to match your setup. [Microsoft network settings](https://support.microsoft.com/en-us/windows/experience/connectivity-networking/essential-network-settings-and-tasks-in-windows) · [Firewall rule reference](https://learn.microsoft.com/en-us/powershell/module/netsecurity/new-netfirewallrule?view=windowsserver2025-ps)
4. On the camera, choose one of the three profiles under **FTP upload → Network settings**, then **Edit active profile**. Enter the server as `IPv4:port`, the user name, password and optional virtual destination folder, then save. Profiles are stored on the SD card, including their passwords.
5. Mark files with **Select photos** or **Mark/unmark for FTP** in the playback i menu, then choose **Start upload**. **Transfer status** shows progress and lets you cancel or send queued files. **Options** lets you send an entire SD folder, clear pending selections, choose whether to replace files with the same name, and send newly saved photos automatically. **JPEG only** omits the NEF from a RAW+JPEG pair and still allows standalone NEFs.

The extension defers the camera's idle Wi‑Fi disconnect while the FTP menu is open, an upload is active, or auto send is enabled. Wi‑Fi can still be turned off manually. On the server, `STOR` followed by `226` confirms a completed upload. If the session stops after `229`, check the passive data port range and inbound rule.
