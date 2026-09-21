# BetaRobot

### 快速开始：

下载脚本并运行（脚本会拉主仓库、拉 app 模板仓库、生成 `user_cfg.h`）：

`Linux` / `macOS`：

```bash
curl -fsSLO https://raw.githubusercontent.com/itgz123/BetaRobot/main/scripts/setup_project.sh && bash setup_project.sh
```

`Windows`（`PowerShell`，Win10/11 默认）：

```powershell
curl.exe -fsSLO https://raw.githubusercontent.com/itgz123/BetaRobot/main/scripts/setup_project.bat
.\setup_project.bat
```

`Windows`（`cmd`）：

```bat
curl.exe -fsSLO https://raw.githubusercontent.com/itgz123/BetaRobot/main/scripts/setup_project.bat && setup_project.bat
```

> 两个终端有三点差别，混用会报错：`PowerShell` 里 `curl` 是 `Invoke-WebRequest` 的别名（必须写 `curl.exe`）、`&&` 不是合法语句分隔符（得分两行）、当前目录的脚本要加 `.\` 前缀。`cmd` 才可以一行写完。

> 地址必须用 `raw.githubusercontent.com`（直接返回文件内容），不能写成 `github.com/.../blob/...` —— 那是网页地址，`curl` 下来是一份 HTML。
> `raw.githubusercontent.com` 不通时可以用 jsDelivr CDN 兜底：把 `https://raw.githubusercontent.com/itgz123/BetaRobot/main/` 换成 `https://cdn.jsdelivr.net/gh/itgz123/BetaRobot@main/`，其余不变。代价是分支引用有 12 小时边缘缓存，改了脚本不会立刻生效。

> 仓库是公开的，脚本克隆时依次尝试 `SSH(22)` → `SSH(443)` → `HTTPS`，任一成功即停，所以没配 SSH 密钥也能用。
> 若连 `github.com` 整个不通，用镜像前缀重跑（三选一）：
>
> - `cmd`：`set BETAROBOT_GIT_MIRROR=https://<镜像>/`，再运行 `setup_project.bat`
> - `PowerShell`：`$env:BETAROBOT_GIT_MIRROR='https://<镜像>/'`，再运行 `.\setup_project.bat`
> - `bash`：`BETAROBOT_GIT_MIRROR=https://<镜像>/ bash setup_project.sh`

### 文档：

[GitHub Pages](https://itgz123.github.io/BetaRobot/)
