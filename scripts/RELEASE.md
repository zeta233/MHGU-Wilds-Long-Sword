# 四平台发布流程

Windows Recomp、Android Recomp、Eden 和 Switch Atmosphère 默认共用一个发布版本号。Eden 和 Switch 共用 `Switch` 插件构建，各自打包。完整包使用默认键位。R／ZR 互换是配套的可选包。

在 PowerShell 里配置好 README 所列的编译工具，然后从仓库根目录分别构建六个插件。

```powershell
New-Item -ItemType Directory -Force ./release-binaries
foreach ($platform in 'Recomp', 'Android', 'Switch') {
    ./scripts/build.ps1 -Platform $platform -OutputPath "./release-binaries/$platform.nro"
    ./scripts/build.ps1 -Platform $platform -SwapRZR -OutputPath "./release-binaries/$platform-SwapRZR.nro"
}
```

更新三份安装说明里的版本与验证状态。用四个平台的上一版完整 ZIP 作为资源模板，将新安卓 `w07.arc` 传给 `--android-arc`。若有其他新增或变更的资源，先将它们同步到对应平台的资源模板。

```powershell
python ./scripts/package-release.py $version `
    --templates $windowsZip $androidZip $edenZip $switchZip `
    --binaries ./release-binaries --android-arc $androidArc --output ./release-output
```

核对四个完整 ZIP 和四个可选 ZIP。至少在可用的环境验证插件加载、普通与互换两套键位、R 奔跑和还原默认。记录实测平台及构建校验平台。Windows 后处理运行时有改动时随版本发布，沿用时在发布说明链接兼容版本。推送源码与版本标签，上传八个 ZIP，核对发布附件 SHA-256 和公开下载链接。
