import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
PLUGIN = 'romfs/nativeNX/plugins/mhw_ls_rebuild.nro'
MOTION = 'romfs/nativeNX/arc/player/quest/w07.arc'
MOD = 'Wilds-LS-World'
TITLE = '0100770008DD8000'
CONTROLS = '安装与按键说明.txt'
ANDROID_GUIDE = '安卓Recomp安装说明.txt'
SWITCH_GUIDE = '模拟器与Switch安装说明.txt'
PLATFORMS = (
    ('Windows-Recomp', 'Recomp', MOD, None),
    ('Android-Recomp', 'Android', MOD, ANDROID_GUIDE),
    ('Eden', 'Switch', MOD, SWITCH_GUIDE),
    ('Switch-Atmosphere', 'Switch', f'atmosphere/contents/{TITLE}', SWITCH_GUIDE),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_zip(path, entries):
    with ZipFile(path, 'w', ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    with ZipFile(path) as archive:
        assert archive.testzip() is None and set(archive.namelist()) == set(entries)
        assert all(digest(archive.read(name)) == digest(data) for name, data in entries.items())
    print(f'| `{path.name}` | `{digest(path.read_bytes())}` |', flush=True)


def read_plugin(path, swap):
    data = path.read_bytes()
    assert data[16:20] == b'NRO0', path
    assert (b'R/ZR combat swap enabled' in data) == swap, path
    return data


def patch_guide(version, platform, prefix):
    destination = f'mods/{prefix}'
    if platform == 'Eden':
        destination = f'Eden 的 GU 模组数据目录/{prefix}'
    elif platform == 'Switch-Atmosphere':
        destination = f'SD 卡/{prefix}'
    return f'''荒野太刀 · 凌越扩展 v{version}｜{platform} 可选 R／ZR 互换补丁

适用于同平台、同版本完整包的六种狩猎风格的太刀。
保存并退出游戏，解压本包，用「启用补丁」里的 mhw_ls_rebuild.nro 覆盖以下位置，再重启游戏。
{destination}/{PLUGIN}
要恢复默认键位，用「恢复默认」中的同名文件覆盖同一位置，再重启。动作、音效等资源沿用完整包。

• 收刀时方向＋R 奔跑。方向＋ZR 走路。
• 气刃斩、蓄力气刃斩、勇气刚气刃斩等改用 ZR，原 R 组合也改用 ZR。
• 方向＋ZR＋X＋A 可拔刀出移动气刃斩。公会 ZR＋A 见切，ZR＋B 特殊纳刀。
• 勇气刚一 GP 成功后，逐次按 ZR 接刚二和练气解放斩。未成功时，先用 ZR 接刚三，再按 ZR 接练气解放斩。
• 公会轻按 R 后松开切换增幅，R＋X＋A 进光辉纳刀。
• 勇气蓝刃收刀时按 R＋X＋A 直接拔刀出光辉闪斩，需要一格增幅护腕储备。
• 武士道精准回避后按 X 出反击斩，再按 ZR 接练气解放斩，消耗一档刃色。
• 武士道大居合成功反击后点按 ZR 出追斩，松开再按 ZR 接无双斩。
• 空战踩怪后按 ZR 出空中气刃三，松开再按 ZR 接落地大回旋。空中可先按 ZR＋B 接落地特殊纳刀，集中模式下可先按 ZR＋Y 接落地集中突刺。
• 空战地面拔刀后按 ZR＋B 特殊纳刀，架势就绪后按 X 或 ZR 出小居合或大居合。攻击收招窗口按 ZR＋A 出见切。单机增幅键为 R。
• 强袭和炼金沿用公会新增招式键位。炼金翻滚中 ZR＋Y 用于炼金桶，站定或挥刀时按住集中键再按 ZR＋Y 出集中突刺。
• Xbox 键名 R＝RB、ZR＝RT。集中模式仍用 ZL，操作类型 0 的狩技仍用方向键。
'''.encode('utf-8-sig')


def package(args):
    args.output.mkdir(parents=True, exist_ok=True)
    controls = (ROOT / CONTROLS).read_text(encoding='utf-8-sig')
    android_arc = args.android_arc.read_bytes()
    assert android_arc[:4] in (b'ARC\0', b'ARCC'), args.android_arc
    builds = {}
    for target in dict.fromkeys(row[1] for row in PLATFORMS):
        default = read_plugin(args.binaries / f'{target}.nro', swap=False)
        swapped = read_plugin(args.binaries / f'{target}-SwapRZR.nro', swap=True)
        builds[target] = (default, swapped)
    assert len({digest(data) for pair in builds.values() for data in pair}) == 6

    for (platform, target, prefix, guide), template in zip(PLATFORMS, args.templates):
        default, swapped = builds[target]
        start = f'{prefix}/romfs/'
        with ZipFile(template) as archive:
            entries = {name: archive.read(name) for name in archive.namelist() if name.startswith(start)}
        assert f'{prefix}/{PLUGIN}' in entries and f'{prefix}/{MOTION}' in entries, template
        entries[f'{prefix}/{PLUGIN}'] = default
        if target == 'Android':
            entries[f'{prefix}/{MOTION}'] = android_arc
        else:
            assert entries[f'{prefix}/{MOTION}'] != android_arc, template

        control_path = f'{prefix}/{CONTROLS}' if target == 'Android' else CONTROLS
        if guide:
            heading = f'荒野太刀 · 凌越扩展 v{args.version}｜{platform} 手柄按键\n\n安装见《{guide}》。\n\n'
            gameplay = controls[controls.index('凌越扩展（公会风格）'):]
            entries[control_path] = (heading + gameplay.replace('RT／ZR', 'ZR').replace(' LT ', ' ZL ')).encode('utf-8-sig')
            guide_path = f'{prefix}/{guide}' if target == 'Android' else guide
            entries[guide_path] = (ROOT / guide).read_bytes()
        else:
            entries[control_path] = controls.encode('utf-8-sig')

        manifest = {'version': args.version, 'platform': platform, 'title_id': TITLE, 'controls': 'default',
                    'files': {name: digest(data) for name, data in entries.items()}}
        entries[f'{prefix}/版本与验证.json'] = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode()
        name = f'MHGU-Wilds-Long-Sword-v{args.version}-{platform}'
        write_zip(args.output / f'{name}.zip', entries)
        write_zip(args.output / f'{name}-R-ZR-Swap.zip', {
            '启用补丁/mhw_ls_rebuild.nro': swapped,
            '恢复默认/mhw_ls_rebuild.nro': default,
            CONTROLS: patch_guide(args.version, platform, prefix),
        })


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Build and verify the four platform release ZIPs and optional R/ZR patches.')
    parser.add_argument('version', help='Release version without the v prefix')
    parser.add_argument('--templates', nargs=4, required=True, type=Path, metavar=('WINDOWS', 'ANDROID', 'EDEN', 'SWITCH'))
    parser.add_argument('--binaries', required=True, type=Path)
    parser.add_argument('--android-arc', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    package(parser.parse_args())
