# BetaRobot

### 快速开始：

下载脚本并运行（脚本会拉主仓库、拉 app 模板仓库、生成 `user_cfg.h`）：

`Linux` / `macOS`：

```bash
curl -fsSLO https://github.com/itgz123/BetaRobot/releases/download/config_project/setup_project.sh && bash setup_project.sh
```

`Windows`：

```bat
curl.exe -fsSLO https://github.com/itgz123/BetaRobot/releases/download/config_project/setup_project.bat && setup_project.bat
```

> 上面这条给 `cmd` 用。`PowerShell 5.1` 里 `curl` 是 `Invoke-WebRequest` 的别名、`&&` 也不是合法语法，得分两步：先 `curl.exe -fsSLO <上面的地址>`，再 `.\setup_project.bat`（当前目录的脚本要带 `.\`）。

### 文档：

[GitHub Pages](https://itgz123.github.io/BetaRobot/)
