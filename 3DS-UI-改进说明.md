# CPyMO 3DS 双屏 UI 改进工作区

本工作区基于 Strrationalism/CPyMO 的 3DS 后端，保留原引擎与按键逻辑，仅改进启动器和双屏呈现。

## 界面与操作

- 游戏选择：上屏 400×240 显示当前游戏图标、完整标题、路径及状态；下屏 320×240 显示四行游戏列表（仅显示首个换行符前的游戏名）、操作说明和时间。上屏支持 3D 前后景视差：背景及辅助信息靠后，标题栏、图标和完整游戏名靠前。
- 四种游玩显示模式，不提供上下屏交换：
  1. 上屏等比填充并裁剪，台词与画面同在上屏；下屏星空、时间和完整按键说明。
  2. 上屏等比填充并裁剪，仅显示游戏画面；下屏星空、时间、台词或菜单和简要提示。
  3. 上屏等比缩放并保留左右留边，台词与画面同在上屏；下屏同模式 1。
  4. 上屏等比缩放并保留左右留边，仅显示游戏画面；下屏同模式 2。
- SELECT 按 1 → 2 → 3 → 4 顺序循环切换，选择会保存，并兼容原版显示模式存档；没有显示模式存档的游戏默认使用模式 2。
- 游戏内按 START 或选择退出会保存并返回游戏选择器；仅在选择器按 START 才退出整个程序。返回后即使只有一款游戏也不会自动重新启动。
- 原有游戏按键不变：A / Y / ZL / ZR / ↓ 继续，↑ 历史对话，X / B 菜单或返回，按住 L 隐藏文字，R 快进；START 返回游戏选择器。
- 双屏台词模式保留触摸输入。

## 主要改动

- `cpymo-backends/3ds/ui_3ds.c`、`ui_3ds.h`：原生 Citro2D 双屏 UI。
- `cpymo-backends/3ds/romfs/night-sky.png`：内置 320×240 夜空资源。
- `main.c`：选择器覆盖绘制、三种显示模式、绘制分派和资源释放。
- 图像后端增加物理像素绘制入口；文字后端共享已加载字体。
- 游戏选择器保存原始 UTF-8 游戏标题供原生启动器使用。

## 构建

标准 devkitPro 环境需包含 devkitARM、libctru、Citro3D、Citro2D、3dstools，以及 CIA 打包用的 3dstool 和 makerom。

本目录的 `cpymo-backends/3ds/build.FFmpeg.3DS` 已包含本次构建所用的精简 FFmpeg 5.1 静态库和头文件。进入 `cpymo-backends/3ds` 后运行：

```sh
make -j2
3dstool -cvtf romfs build/cpymo.romfs --romfs-dir romfs
makerom -f cia -o cpymo.cia -elf ./build/cpymo.elf \
  -rsf cpymo.rsf -romfs build/cpymo.romfs \
  -exefslogo -target t -icon icon -banner banner
```

二进制生成于 `cpymo-backends/3ds/cpymo.3dsx` 和 `cpymo.cia`。游戏文件不内置，仍按原项目方式放置于 SD 卡 `/pymogames/`。

## 验证范围

已完成 ARM 交叉编译、ELF 链接、3DSX/SMDH 生成、CIA 打包及文件格式检查；另将 CIA 转换为 CCI 后解包，逐字节比对确认其中的 `font.bcfnt` 与新版 `night-sky.png` 和源资源一致。未在 Nintendo 3DS 实机或模拟器中执行视觉与交互测试；首次实机使用建议先测试游戏选择、SELECT 四种模式切换、菜单、存档及音视频播放。
