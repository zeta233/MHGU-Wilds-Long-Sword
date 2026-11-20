# 荒野太刀 · MHGU Recomp

将《怪物猎人 荒野》的太刀动作移植到 Windows 版 MHGU Recomp，支持公会与勇气风格。公会和勇气普通模式可使用特殊纳刀、居合和见切。勇气蓝刃可以使用刚气刃 GP、赤刃连段和消耗勇气槽的红刃见切。

## 安装与按键

保存并退出游戏，准备 Switch Forge v1.5.1 前置。下载完整安装包，将其中的 Wilds-LS-World 文件夹解压到游戏的 mods 目录。更新前可将旧版移出 mods 备份。招式输入、集中模式、更新和回退步骤见随包说明。

[下载完整包](https://github.com/zeta233/MHGU-Wilds-Long-Sword/releases/latest) · [安装与按键说明](安装与按键说明.txt)

## v0.3.82 更新

公会与勇气风格太刀更新至 v0.3.82。特殊纳刀默认约 0.8 秒，收刀术不再叠加。短按 R 的普通气刃连段按当前练气槽判断能否续刀。集中突刺的伤口爆炸移到方向收尾之后。

练气解放斩扩大前三刀的高度判定。收尾十二段按挥刀顺序分配到实际命中的部位。三刀全中时每刀四段，全部挥空时不触发追击。

## v0.3.71 更新

集中突刺破坏伤口后，在空中三刀、回身或方向收尾期间再次按住或点按 R＋Y，均不会自动发动第二次突刺。收招后可重新输入。已在 GU 内用固定目标验证公会与勇气状态的派生。

自上次公开版 v0.3.50 起，集中突刺追加了方向收尾，调整了三刀的横移与衔接。赤刃斩增加刀光，练气解放斩增强命中反馈；勇气模式也加入了可用集中模式调整两刀方向的移动刚气刃斩三。

## v0.3.50 勇气普通模式

长按 Y 进入纳刀姿势后，一字斩、气刃踏步斩、后撤斩、左移动斩和右移动斩均可在收招时接 R＋A 普通见切或 R＋B 特殊纳刀。五种攻击的两个派生都在 Windows 版游戏内验证。气刃踏步斩后单按 R 仍可继续气刃连段。

## 编译插件

运行环境为 Windows PowerShell。准备 Rust nightly 和 rust-src、cargo-forge（MHGU Switch Forge 工具链），并将 arm-none-eabi-gcc 和 cargo 加入 PATH。依赖版本记录于 integration/Cargo.lock。本次构建使用 xPack Arm GNU Toolchain 15.2.1。

在仓库根目录运行：

```powershell
.\scripts\build.ps1
```

输出为 integration/mhw_ls_rebuild.nro，安装位置为 Wilds-LS-World/romfs/nativeNX/plugins/。integration/c 包含战斗与输入逻辑和编译所需的生成表，integration/src 包含 Rust 入口。

wilds-port/tools 提供动作、战斗表和音效转换工具。重新制作资源时需另备相应游戏数据与模型，工具的默认目录结构见 portkit.py，可用 MHGU_DATA_ROOT 指定数据根目录。编译插件时已包含所需表格。

## 来源

项目基于 Akumi-xu 的公会太刀世界招式移植。动作转换工具参考 RevilLib、MHGU-blenderImporter 和 REasy；REasy 参考数据附有原许可。游戏及动作、音效素材归各自权利人所有。

[Akumi-xu 原项目](https://github.com/Akumi-xu/MHW-Long-Sword-Moveset-for-MHGU)
