# 光辉闪斩后处理 · Windows MHGU Recomp 0.17.9

本补丁让光辉闪斩和追击二刀在 Vulkan 显示阶段使用径向模糊与局部折射。补丁基于 [MHGU Recomp 0.17.9](https://github.com/morerokk/MHGURecomp/tree/0.17.9)，上游提交为 `1b405eef2635db871fd021b9d3a7d6d06a25876a`。

v0.261 起为超宽屏提供原生 HUD 的边缘锚点，配合对应的太刀模组包修正 `R3` 提示位置。补丁也包含闪斩径向模糊、命中折射和追击二刀的局部柔光。v0.262 沿用 v0.261 后处理程序。

已经安装 Windows 版 MHGU Recomp 0.17.9 的玩家可在退出游戏后备份原 `mhgurecomp.exe`，再换成发布页单独提供的同名程序。继续使用原来的 DLL、存档和游戏数据。恢复原程序即可回退。环境变量 `MHGU_RADIANCE_PRESENT=0` 可关闭屏幕取样效果。

## 从源码构建

检出上游 `0.17.9` 并按上游说明准备 Windows 构建环境和依赖。在上游源码根目录应用本目录的补丁，然后运行上游构建命令。

```sh
git apply /path/to/radiance.patch
make -j
```

上游许可证和第三方软件声明随补丁提供。发布包附有各依赖的许可证与源码出处。
