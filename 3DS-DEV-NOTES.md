# CPyMO 3DS 分支 开发备忘（长期记录）

本文件是本 fork（`TQHYG/CPyMO`）3DS 后端的工作记录，供后续会话/开发者快速恢复上下文。
**修改前请先读本文件，修改后请更新本文件。**

- 上游：`Strrationalism/CPyMO`（remote `upstream`），fork：`TQHYG/CPyMO`（remote `origin`）
- 分支：`main` 与上游一致；3DS 改动在 `3ds-ui-improvements`
- 工作区：`D:\tqhyg\Project\cpymo-work`
  - `cpymo-3ds/`：git 仓库（本文件所在仓库）
  - `extras/`：**不入库**，放本机构建工具（makerom/3dstool/ctrtool）与产物；见 `extras/README.md`

## 构建

```powershell
C:\dev\devkitPro\msys2\usr\bin\bash.exe -lc 'bash /d/tqhyg/Project/cpymo-work/extras/build-3ds.sh'
```

- devkitPro 在 `C:\dev\devkitPro`，**不在** Windows PATH，必须用它自带的 MSYS2 bash（提供 `DEVKITPRO`/`DEVKITARM`）。
- devkitARM 16.1.0 / libctru / citro2d+citro3d。
- FFmpeg 静态库来自仓库内被忽略的 `cpymo-backends/3ds/build.FFmpeg.3DS/`（`git clean -xdf` 会删掉它）。
- `makerom`/`3dstool` 不是 devkitPro 组件，用 `extras/tools` 里下载的那份。
- 产物：`cpymo-backends/3ds/cpymo.3dsx`、`cpymo.cia`，脚本会复制到 `extras/dist/`。

## 硬性不变量（勿破坏）

1. **不要改存档格式**：`<gamedir>/save/*.csav`（`config.csav`、`global.csav`、槽位存档）保持原样，
   旧存档必须能读。新增设置一律写到下面的全局文本文件里。
2. **全局设置文件**：`sdmc:/pymogames/settings.txt`（键值文本，容忍未知行/缺行）。只存全局偏好，
   不写入任何游戏存档目录。
3. **ZL/ZR 是 L/R 的镜像**，不单独配置、不在任何 UI 文案中出现。
4. 只做体验/UI 改动，**不要改 gameplay 语义**（脚本解释器、自动/快进判定、存档读写时机）。
5. 默认按键（全局）：`下/L/A = 下一句`，`R = 快进`，`Y = 菜单`，`B = 自动播放`，
   `X = 隐藏对话框(按住)`，`上 = 对话历史`，`左/右 = 未定义`。
   每个按键可选动作：自动 / 存档 / 读档 / 快进 / 隐藏对话框(按住) / 对话历史 / 设置 / 重启游戏 / 未定义。

## 3DS 后端结构

| 位置 | 作用 |
| --- | --- |
| `cpymo-backends/3ds/main.c` | 入口；屏幕渲染目标 `screen1/2/3`；显示模式 `display_mode`；选择器回调；调试模式 |
| `cpymo-backends/3ds/ui_3ds.c/.h` | 原生 Citro2D 启动器/帮助/下屏星空/时钟/按键说明 |
| `cpymo-backends/3ds/cpymo_backend_image.c` | 图像后端；`enhanced_3ds_display_mode_select()` 决定每类绘制去上屏还是下屏；`cpymo_3ds_draw_image_raw()` 物理像素直绘 |
| `cpymo-backends/3ds/cpymo_backend_input.c` | `cpymo_input_snapshot()`：按键→`cpymo_input` |
| `cpymo-backends/3ds/cpymo_backend_text.c` | 字体（`cpymo_backend_text_3ds_font()` 暴露给 ui_3ds） |

### 显示模式（`display_mode`，0..3）

- bit0 = 台词/UI 在下屏（“增强模式”，`enhanced_3ds_display_mode`）
- bit1 = 等比留边（否则等比裁剪铺满，`fill_screen = !(bit1)`）
- SELECT 循环切换，存 `<gamedir>/save/3ds-display-mode.csav`，格式 `0x80 | display_mode`；
  读到旧格式（bit1=fill、bit0=enhanced）时按语义换算，不按位互换。
- 未存档时用全局设置里的默认显示模式。

### 下屏绘制分派

`cpymo_engine_draw()` 对上屏、下屏各调用一次；后端用 `draw_type` 过滤。UI 元素走
`ui_element*`，对话文字走 `text_say*`，其余（bg/chara/sel_img）走上屏。

## 调试模式（按住 L 选游戏）

- `before_select_game` 检测 L 按住 → 删除 `screen3`、`consoleInit(GFX_BOTTOM)`、
  `cpymo_3ds_debug_mode = true`。
- 调试模式下：下屏完全交给 CPyMO 控制台（不画星空/帮助/台词/任何下屏 UI），
  台词强制在上屏，START 不再生效（既不返回选择器也不退出），不写显示模式存档。
- 该模式一直持续到进程退出（libctru 无 `consoleExit`）。

## 进度

- [x] 修复：旧存档显示模式映射、`screen3` 泄漏、初始化失败 double free、stbi 内存泄漏、按键优先级括号
- [x] 调试模式（按住 L 启动）
- [ ] 全局设置文件 + 键位自定义 + 设置页面
- [ ] 菜单打开时保留立绘；下屏语音/自动播放进度条
- [ ] 下屏自定义背景图（`sdmc:/pymogames/bg.jpg|png`）
