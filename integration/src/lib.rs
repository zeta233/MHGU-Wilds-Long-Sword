//! Clean host shim: no legacy controller, private motion data or hit rewrites.
#![no_std]
#[cfg(all(ml_ascendance_prototype, ml_world_presentation, not(ml_switch_no_overlay)))]
mod radiance;
#[cfg(all(ml_ascendance_prototype, not(ml_switch_no_overlay)))]
mod boost_hud;
#[cfg(all(ml_world_combat, not(ml_switch_no_overlay)))]
mod wound_fx;
use core::sync::atomic::{AtomicU32,AtomicU64,Ordering};
use forge::log::prelude::*;
static BASE:AtomicU32=AtomicU32::new(0);
unsafe extern "C" {fn ml_install(base:u32)->u32;fn ml_pad_refresh();}
#[unsafe(no_mangle)]
extern "C" fn ml_startup_guard(offset:u32, observed:u32, expected:u32) {
    error!("[ml-rebuild] startup guard at main+{offset:#010x}: found {observed:#010x}, expected {expected:#010x}");
}
#[cfg(ml_switch_guard_diagnostics)]
#[unsafe(no_mangle)]
extern "C" fn ml_switch_action_redirect(destination: u32) {
    error!("[ml-rebuild] action request already redirected; existing destination={destination:#010x}");
}
#[cfg(ml_switch_viewport)]
#[unsafe(no_mangle)]
extern "C" fn ml_shared_damage_hook(offset:u32) {
    info!("[ml-rebuild] retaining existing Forge damage hook at main+{offset:#010x}");
}
#[cfg(ml_switch_guard_diagnostics)]
#[unsafe(no_mangle)]
extern "C" fn ml_switch_guard_stage(stage: u32, action: u32, source: u32, detail: u32) {
    let label = match stage {
        0 => "request begin", 1 => "request end",
        2 => "reset begin", 3 => "layer begin", 4 => "layer end",
        5 => "physics end", 6 => "detector end",
        7 => "sound begin", 8 => "sound end",
        9 => "effect begin", 10 => "effect end",
        _ => "unknown",
    };
    info!("[ml-rebuild] Switch guard: {label}; move={action} source={source} detail={detail:#x}");
}
#[cfg(ml_swap_r_zr)]
unsafe extern "C" {fn ml_shoulder_swap_installed()->u32;}

#[forge::hook(offset=0x003D_E064)]
fn sample_pad(object:u32)->u32{
    let value=original!(object);
    if BASE.load(Ordering::Relaxed)!=0 {unsafe{ml_pad_refresh();}}
    value
}
#[forge::entry]
fn main(){
    let base=forge::mem::text_addr();
    if let Some(version) = option_env!("ML_PACKAGE_VERSION") {
        info!("[ml-rebuild] package {version}");
    }
    #[cfg(ml_switch_guard_diagnostics)]
    info!("[ml-rebuild] Switch diagnostic variant; checking native action entry before activation");
    #[cfg(all(not(ml_animation_preview),not(ml_world_combat)))]
    info!("[ml-rebuild] build=rebuild-b4 native_thrust_transitions=1 legacy_assets=0 scope=fade_step");
    #[cfg(ml_world_combat)]
    info!("[ml-rebuild] build=wilds-combat-v0.1 scope=world_and_wilds native_collision=1");
    #[cfg(ml_world_audio_probe)]
    info!("[ml-rebuild] audio-probe-only=1 independent_entry_deploy=0");
    #[cfg(ml_world_fx_probe)]
    info!("[ml-rebuild] fx-probe-only=1 independent_entry_deploy=0");
    #[cfg(ml_world_presentation)]
    info!("[ml-rebuild] presentation=world-layers-v102 world_audio=1 probe_mailbox=0");
    #[cfg(all(ml_animation_preview,not(ml_attachment_preview)))]
    info!("[ml-rebuild] build=body-preview-1 scope=animation_probe combat_ready=0");
    #[cfg(ml_attachment_preview)]
    info!("[ml-rebuild] build=attachment-preview-1 scope=attachment_probe combat_ready=0");
    let installed=unsafe{ml_install(base)};
    if installed==0 {error!("[ml-rebuild] native guards rejected; controller remains inactive");return;}
    #[cfg(ml_switch_no_overlay)]
    info!("[ml-rebuild] Switch compatibility: mod overlay disabled");
    #[cfg(ml_switch_native_focus_pose)]
    info!("[ml-rebuild] Switch compatibility: native Focus pivot and gait");
    #[cfg(ml_switch_guard_diagnostics)]
    info!("[ml-rebuild] Switch compatibility: guard diagnostics, native view matrix");
    BASE.store(base,Ordering::Relaxed);
    forge::install_hook!(base,sample_pad);
    #[cfg(ml_swap_r_zr)]
    if unsafe { ml_shoulder_swap_installed() } != 0 {
        info!("[ml-rebuild] R/ZR combat swap enabled; physical R sprints");
        return;
    } else {
        error!("[ml-rebuild] R/ZR swap unavailable; using default controls");
    }
    info!("[ml-rebuild] installed; all six hunting styles; native collision retained");
}

#[cfg(ml_world_combat)]
static GAUGE_GLOW: AtomicU32 = AtomicU32::new(0);
#[cfg(ml_world_combat)]
static WILDS_CHARGE: AtomicU32 = AtomicU32::new(0);
#[cfg(ml_world_combat)]
static WILDS_FOCUS: AtomicU32 = AtomicU32::new(0);
#[cfg(ml_ascendance_prototype)]
static ASCENDANCE_GAUGE: AtomicU32 = AtomicU32::new(0);

#[cfg(ml_ascendance_prototype)]
#[unsafe(no_mangle)]
extern "C" fn ml_ascendance_gauge(value: u32) { ASCENDANCE_GAUGE.store(value, Ordering::Relaxed); }
#[cfg(ml_world_combat)]
static WILDS_WOUNDS: [AtomicU64; 12] = [const { AtomicU64::new(0) }; 12];
#[cfg(ml_world_combat)]
static WILDS_LOCK_HINT: AtomicU32 = AtomicU32::new(0);
#[cfg(all(ml_world_combat, ml_wilds_moves, ml_recomp_runtime))]
unsafe extern "C" { fn ml_hud_right_offset() -> u32; }
#[cfg(all(ml_world_combat, ml_wilds_moves, ml_switch_viewport, not(ml_switch_no_overlay)))]
unsafe extern "C" { fn ml_hud_native_crop() -> u32; }
#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_wilds_charge(value: u32) { WILDS_CHARGE.store(value, Ordering::Relaxed); }

#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_wilds_focus(value: u32) {
    #[cfg(ml_switch_no_overlay)]
    if WILDS_FOCUS.swap(value, Ordering::Relaxed) != value {
        info!("[ml-rebuild] Switch drawn focus={value}; mod overlay disabled");
    }
    #[cfg(not(ml_switch_no_overlay))]
    WILDS_FOCUS.store(value, Ordering::Relaxed);
}

#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_wilds_wound(slot: u32, value: u32, detail: u32) {
    if let Some(marker) = WILDS_WOUNDS.get(slot as usize) {
        marker.store(((detail as u64) << 32) | value as u64, Ordering::Relaxed);
    }
}

#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_wilds_lock_hint(value: u32) {
    WILDS_LOCK_HINT.store(value, Ordering::Relaxed);
}

#[cfg(ml_world_combat)]
#[unsafe(no_mangle)]
extern "C" fn ml_gauge_glow(value: u32) {
    GAUGE_GLOW.store(value, Ordering::Relaxed);
}

#[cfg(all(any(ml_ascendance_prototype, ml_world_combat), not(ml_switch_no_overlay)))]
fn fill_overlay_polygon(dl: &forge::imgui::draw::DrawList, points: &[forge::sys::imgui::ImVec2], color: u32) {
    // Some Android Forge builds mirror just the first guest array element.
    // Build the path on the host through the by-value point API.
    #[cfg(ml_android_recomp)]
    {
        dl.path_clear();
        for &point in points {
            dl.path_line_to(point);
        }
        dl.path_fill_convex(color);
    }
    #[cfg(not(ml_android_recomp))]
    dl.add_convex_poly_filled(points, color);
}

#[cfg(all(ml_world_combat, not(ml_switch_no_overlay)))]
#[forge::imgui_free_render]
fn draw_gauge_glow() {
    use forge::imgui::{color::color, context, draw::DrawList};
    use forge::sys::imgui::ImVec2;
    let value = GAUGE_GLOW.swap(0, Ordering::Relaxed);
    let charge = WILDS_CHARGE.swap(0, Ordering::Relaxed);
    let focus = WILDS_FOCUS.swap(0, Ordering::Relaxed);
    let lock_hint = WILDS_LOCK_HINT.swap(0, Ordering::Relaxed);
    let wounds = core::array::from_fn::<_, 12, _>(|slot| WILDS_WOUNDS[slot].swap(0, Ordering::Relaxed));
    #[cfg(ml_ascendance_prototype)]
    let boost = boost_hud::render_state(ASCENDANCE_GAUGE.load(Ordering::Relaxed));
    #[cfg(not(ml_ascendance_prototype))]
    let boost = 0;
    let size = context::get_io().DisplaySize;
    #[cfg(all(ml_wilds_moves, ml_switch_viewport))]
    let size = {
        let crop = unsafe { ml_hud_native_crop() };
        if crop != 0 {
            let width = (crop & 0xffff) as f32;
            let height = (crop >> 16) as f32;
            ImVec2::new(width.min(size.x), height.min(size.y))
        } else {
            size
        }
    };
    let scale = (size.x / 640.0).min(size.y / 360.0);
    let left = (size.x - 640.0 * scale) * 0.5;
    let top = (size.y - 360.0 * scale) * 0.5;
    let width = 48.0 * (value >> 8) as f32 / 100.0;
    let dl = DrawList::foreground();
    #[cfg(all(ml_ascendance_prototype, ml_world_presentation))]
    radiance::draw(&dl, size, scale);
    if value == 0 && charge == 0 && focus == 0 && lock_hint == 0 && boost == 0 && wounds.iter().all(|&wound| wound == 0) { return; }
    wound_fx::draw(&dl, size, scale, &wounds);
    if lock_hint != 0 {
        #[cfg(all(ml_wilds_moves, ml_recomp_runtime))]
        let right_offset = if size.x > size.y * (16.0 / 9.0) {
            let offset = unsafe { ml_hud_right_offset() } as f32 * (size.y / 65536.0);
            offset.min(left)
        } else { 0.0 };
        #[cfg(not(all(ml_wilds_moves, ml_recomp_runtime)))]
        let right_offset = 0.0;
        let x = left + 600.0 * scale + right_offset;
        let y = top + 41.5 * scale;
        dl.add_rect_filled(ImVec2::new(x - 7.0 * scale, y - 5.5 * scale),
            ImVec2::new(x + 7.0 * scale, y + 5.5 * scale), color(23, 29, 35, 245), 2.0 * scale);
        dl.add_rect_filled(ImVec2::new(x - 6.0 * scale, y - 4.5 * scale),
            ImVec2::new(x + 6.0 * scale, y + 4.5 * scale), color(249, 247, 229, 255), 1.5 * scale);
        let letters = [[7u8, 5, 7, 6, 5], [7u8, 1, 7, 1, 7]];
        for (letter, rows) in letters.iter().enumerate() {
            for (row, bits) in rows.iter().enumerate() {
                for col in 0..3 {
                    if bits & (4 >> col) == 0 { continue; }
                    let px = x + (-3.5 + (4 * letter + col) as f32) * scale;
                    let py = y + (-2.5 + row as f32) * scale;
                    dl.add_rect_filled(ImVec2::new(px, py), ImVec2::new(px + scale, py + scale),
                        color(19, 24, 29, 255), 0.0);
                }
            }
        }
    }
    if focus != 0 {
        let x = size.x * 0.5;
        let y = size.y * 0.5;
        let tint = color(245, 238, 203, 235);
        let bars = [(-8.0, -0.5, -3.0, 0.5), (3.0, -0.5, 8.0, 0.5),
                    (-0.5, -8.0, 0.5, -3.0), (-0.5, 3.0, 0.5, 8.0)];
        for (x1, y1, x2, y2) in bars {
            dl.add_rect_filled(ImVec2::new(x + x1 * scale, y + y1 * scale),
                ImVec2::new(x + x2 * scale, y + y2 * scale), tint, 0.0);
        }
    }
    if value > 0 {
    dl.add_rect_filled(ImVec2::new(left+64.0*scale, top+38.0*scale),
        ImVec2::new(left+(64.0+width)*scale, top+40.0*scale),
        color(248,235,223,value as u8), 0.0);
    }
    #[cfg(ml_ascendance_prototype)]
    if boost != 0 { boost_hud::draw(&dl, left, top, scale, boost); }
    if charge > 0 {
        let progress = (charge-1).min(100) as f32 / 100.0;
        let start = ImVec2::new(left+64.0*scale, top+43.0*scale);
        dl.add_rect_filled(start, ImVec2::new(left+112.0*scale,top+46.0*scale),color(20,25,32,210),0.0);
        let tint = if charge >= 101 {color(255,205,75,255)} else {color(90,185,245,245)};
        dl.add_rect_filled(start,ImVec2::new(left+(64.0+48.0*progress)*scale,top+46.0*scale),tint,0.0);
    }
}

