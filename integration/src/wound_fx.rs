use forge::imgui::{color::color, draw::DrawList};
use forge::sys::imgui::ImVec2;

const CUT: [(f32, f32, f32); 7] = [
    (-1.12, 0.07, 0.04),
    (-0.79, -0.23, 0.38),
    (-0.43, 0.27, 0.88),
    (-0.02, -0.14, 0.55),
    (0.34, 0.18, 0.74),
    (0.72, -0.27, 0.36),
    (1.06, 0.06, 0.03),
];

fn draw_cut(dl: &DrawList, center: ImVec2, size: ImVec2, scale: f32, axes: u32, style: u32) {
    let component = |shift| ((axes >> shift) as u8 as i8) as f32 * 0.25;
    let mut along = ImVec2::new(component(0), component(8));
    let mut across = ImVec2::new(component(16), component(24));
    let length = along.x.abs().max(along.y.abs()) + 0.414 * along.x.abs().min(along.y.abs());
    if length < 0.75 { return; }
    let focused = style & 3 == 2;
    // Keep distant wounds legible and retain the angle and foreshortening.
    let factor = length.clamp(if focused { 6.0 } else { 4.0 }, 16.0) / length;
    along.x *= factor * size.x / 640.0;
    along.y *= factor * size.y / 360.0;
    across.x *= factor * size.x / 640.0;
    across.y *= factor * size.y / 360.0;
    if along.x * across.y - along.y * across.x < 0.0 {
        across.x = -across.x;
        across.y = -across.y;
    }
    let seed = (style >> 2) & 3;
    let visibility = (style >> 4) as f32 / 7.0;
    let tint = |r, g, b, alpha: u8| color(r, g, b, (alpha as f32 * visibility) as u8);
    let point = |u, v| ImVec2::new(center.x + along.x * u + across.x * v,
        center.y + along.y * u + across.y * v);
    let section = |i: usize, width: f32| {
        let (u, bend, thickness) = CUT[i];
        let bend = if seed & 1 == 0 { bend } else { -bend };
        let ragged = ((seed as usize * 7 + i * 9) % 5) as f32 * 0.05;
        [point(u, bend - thickness * width), point(u, bend + thickness * width * (0.74 + ragged))]
    };
    // A dark recess separates the red cut from both hot armor and pale hide.
    let bands = if focused {
        [(1.55, tint(116, 22, 33, 62)), (1.25, tint(72, 19, 29, 196)),
            (1.00, tint(25, 9, 16, 247)), (0.51, tint(205, 35, 43, 228))]
    } else {
        [(1.39, tint(99, 24, 32, 49)), (1.21, tint(79, 25, 32, 172)),
            (1.00, tint(28, 11, 18, 236)), (0.47, tint(161, 35, 43, 211))]
    };
    for (width, tint) in bands {
        for i in 0..CUT.len() - 1 {
            let [a, b] = section(i, width);
            let [c, d] = section(i + 1, width);
            crate::fill_overlay_polygon(dl, &[a, c, d, b], tint);
        }
    }

    // Broken glints sit inside the recess so they retain contrast on white hide.
    let edge = if focused { tint(255, 239, 215, 240) } else { tint(255, 225, 199, 185) };
    let weight = if focused { 1.15 } else { 0.9 };
    for (i, opposite, fraction) in [(1, false, 0.68), (2, false, 0.48), (3, true, 0.66), (4, true, 0.38)] {
        let start = section(i, 0.76)[opposite as usize];
        let end = section(i + 1, 0.76)[opposite as usize];
        let end = ImVec2::new(start.x + (end.x - start.x) * fraction,
            start.y + (end.y - start.y) * fraction);
        dl.add_line(start, end, edge, weight * scale);
    }
}

fn draw_break(dl: &DrawList, center: ImVec2, scale: f32, age: f32, mut amount: u32) {
    let flash = (1.0 - age * 12.0).max(0.0);
    if flash > 0.0 {
        dl.add_circle_filled(center, (1.2 + 2.2 * flash) * scale,
            color(255, 240, 216, (218.0 * flash) as u8), 8);
    }
    let spark = (1.0 - age * 3.3).max(0.0);
    if spark > 0.0 {
        for (x, y, speed) in [(-0.92, -0.51, 0.9), (-0.42, -1.0, 1.1), (0.37, -0.83, 1.3),
            (1.0, -0.34, 1.4), (0.76, 0.35, 1.0), (-0.63, 0.66, 0.7)] {
            let distance = (2.5 + 60.0 * age) * speed * scale;
            let pos = ImVec2::new(center.x + distance * x, center.y + distance * y);
            let tail = ImVec2::new(pos.x - x * 3.0 * spark * scale, pos.y - y * 3.0 * spark * scale);
            dl.add_line(tail, pos, color(255, 99, 57, (160.0 * spark) as u8), 2.1 * spark * scale);
            dl.add_circle_filled(pos, 1.05 * spark * scale, color(255, 217, 150, (255.0 * spark) as u8), 6);
        }
    }
    if amount == 0 { return; }
    let mut digits = [b'0'; 10];
    let mut start = digits.len();
    while amount > 0 {
        start -= 1;
        digits[start] += (amount % 10) as u8;
        amount /= 10;
    }
    if let Ok(label) = core::str::from_utf8(&digits[start..]) {
        let alpha = ((1.0 - age) * 4.0).clamp(0.0, 1.0);
        let pos = ImVec2::new(center.x + 6.0 * scale, center.y - (17.0 + 9.0 * age) * scale);
        dl.add_text(ImVec2::new(pos.x + scale, pos.y + scale), color(32, 14, 6, (235.0 * alpha) as u8), label);
        dl.add_text(pos, color(255, 208, 99, (255.0 * alpha) as u8), label);
    }
}

pub fn draw(dl: &DrawList, size: ImVec2, scale: f32, wounds: &[u64]) {
    for &wound in wounds {
        if wound == 0 { continue; }
        let center = ImVec2::new(
            (wound & 4095) as f32 / 4094.0 * size.x,
            (1.0 - ((wound >> 12) & 4095) as f32 / 4094.0) * size.y,
        );
        let style = (wound as u32) >> 24;
        let detail = (wound >> 32) as u32;
        if style & 0x80 != 0 {
            draw_break(dl, center, scale, (style & 127) as f32 / 127.0, detail);
        } else {
            draw_cut(dl, center, size, scale, detail, style);
        }
    }
}
