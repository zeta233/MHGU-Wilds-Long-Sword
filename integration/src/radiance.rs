use core::sync::atomic::{AtomicU64, Ordering};
use forge::imgui::{color::color, draw::DrawList};
use forge::sys::imgui::ImVec2;

static LAYERS: [AtomicU64; 4] = [const { AtomicU64::new(0) }; 4];

#[unsafe(no_mangle)]
extern "C" fn ml_radiance_screen(layer: u32, start: u32, end: u32, age: u32) {
    if let Some(entry) = LAYERS.get(layer as usize) {
        entry.store(u64::from(start) | u64::from(end) << 24 | u64::from(age) << 48, Ordering::Relaxed);
    }
}

fn point(bits: u32, size: ImVec2) -> ImVec2 {
    let x = ((bits & 4095) as f32 - 1.0) / 4093.0;
    let y = (((bits >> 12) & 4095) as f32 - 1.0) / 4093.0;
    ImVec2::new((1.5*x - 0.25)*size.x, (1.5*y - 0.25)*size.y)
}

fn smooth(t: f32) -> f32 {
    let t = t.clamp(0.0, 1.0);
    t*t*(3.0-2.0*t)
}

fn lens(dl: &DrawList, origin: ImVec2, axis: ImVec2, length: f32, width: f32, rgba: (u8,u8,u8,u8)) {
    streak(dl, origin, axis, length, width, 0.0, rgba);
}

fn streak(dl: &DrawList, origin: ImVec2, axis: ImVec2, length: f32, width: f32, bias: f32, rgba: (u8,u8,u8,u8)) {
    if rgba.3 == 0 { return; }
    let p = |along: f32, across: f32| ImVec2::new(
        origin.x + axis.x*along - axis.y*across,
        origin.y + axis.y*along + axis.x*across);
    // ImGui's antialias fringe requires clockwise vertices in screen coordinates.
    let shape = [
        p(-length,0.0), p(-0.55*length,-0.65*width), p(bias*length,-width),
        p(0.55*length,-0.65*width), p(length,0.0), p(0.55*length,0.65*width),
        p(bias*length,width), p(-0.55*length,0.65*width),
    ];
    crate::fill_overlay_polygon(dl, &shape, color(rgba.0,rgba.1,rgba.2,rgba.3));
}

pub fn draw(dl: &DrawList, size: ImVec2, scale: f32) {
    for (layer, entry) in LAYERS.iter().enumerate() {
        let state = entry.swap(0, Ordering::Relaxed);
        if state == 0 { continue; }
        let age = (state >> 48) as f32 / 480.0;
        let center = point(state as u32, size);
        let end = (state >> 24) as u32 & 0xff_ffff;
        let tip = point(end, size);
        let mut axis = ImVec2::new(tip.x-center.x, tip.y-center.y);
        let major = axis.x.abs().max(axis.y.abs());
        let minor = axis.x.abs().min(axis.y.abs());
        let span = major + 0.414*minor;
        if end == 0 || span < 4.0*scale {
            axis = ImVec2::new(0.92,-0.39);
        } else {
            axis.x /= span;
            axis.y /= span;
        }
        let strength = |value: f32| value.clamp(0.0,255.0) as u8;
        match layer {
            0 => {
                // The right tip enters first and stays pinned while the left
                // tip crosses the view. Keep some camera-projected tilt.
                let projected_slope = (axis.y / axis.x.abs().max(0.40))
                    * if axis.x >= 0.0 { 1.0 } else { -1.0 };
                axis = ImVec2::new(-1.0, -(0.37 + 0.13*projected_slope).clamp(0.28,0.48));
                let right = (center.x + 0.44*size.x).clamp(0.87*size.x,1.04*size.x);
                let half = size.x*(0.13 + 0.43*smooth(age/0.096));
                let remaining = 1.0-smooth((age-0.087)/0.097);
                let weight = (0.26 + 0.74*smooth(age/0.083))*remaining;
                let origin = ImVec2::new(right-half,
                    center.y+(right-center.x-half)*-axis.y);
                streak(dl,origin,axis,half*1.04,29.0*scale,0.27,(244,37,47,strength(54.0*remaining)));
                streak(dl,origin,axis,half,16.5*scale,0.27,(255,29,42,strength(135.0*remaining)));
                streak(dl,origin,axis,half*0.99,13.0*weight*scale,0.27,(255,110,115,strength(222.0*remaining)));
                streak(dl,origin,axis,half*0.98,9.3*weight*scale,0.27,(255,248,238,strength(255.0*remaining)));
            }
            1 => {
                let fade = (1.0-smooth(age/0.36))*smooth((age+0.008)/0.023);
                let half = (span*1.05).clamp(46.0*scale, 90.0*scale);
                for (distance,width,alpha) in [(0.0,25.0,21.0),(0.11,15.0,30.0),(0.22,6.0,40.0)] {
                    let ghost = ImVec2::new(center.x-axis.x*half*distance,center.y-axis.y*half*distance);
                    lens(dl,ghost,axis,half*(1.0+age),width*scale,(255,108,98,strength(alpha*fade)));
                }
            }
            2 => {
                let fade = 1.0-smooth((age-0.072)/0.108);
                let light = (0.28+0.72*smooth(age/0.017))*fade;
                let spread = smooth(age/0.035);
                let slope = (0.34+0.07*axis.y*(if axis.x >= 0.0 {1.0} else {-1.0})).clamp(0.28,0.41);
                let slash = ImVec2::new(-1.0,slope);
                let root_x = center.x.clamp(0.28*size.x,0.75*size.x);
                let right = 1.13*size.x;
                let left = root_x+(-0.16*size.x-root_x)*spread;
                let half = (right-left)*0.5;
                let mid = right-half;
                let origin = ImVec2::new(mid,center.y-(mid-root_x)*slope);
                let width = (4.0+9.0*spread)*scale;
                streak(dl,origin,slash,half,2.0*width,-0.14,(204,17,40,strength(89.0*light)));
                streak(dl,origin,slash,half,1.47*width,-0.14,(245,39,59,strength(164.0*light)));
                streak(dl,origin,slash,half,1.08*width,-0.14,(255,122,135,strength(222.0*light)));
                streak(dl,origin,slash,half,0.84*width,-0.14,(255,246,239,strength(249.0*light)));
                // The softer fan expands beneath the dominant white sheet.
                for (tilt, reach) in [(0.19,0.36),(0.35,0.27),(0.52,0.19)] {
                    let ray = ImVec2::new(1.0,-slope+tilt);
                    let length = size.x*reach*(0.53+0.47*spread)*0.5;
                    let start = ImVec2::new(root_x,center.y);
                    let origin = ImVec2::new(start.x+ray.x*length,start.y+ray.y*length);
                    lens(dl,origin,ray,length,9.0*scale,(233,75,72,strength(42.0*light)));
                }
            }
            _ if end != 0 && span >= 4.0*scale => {
                let gain = (state >> 48) as f32 / 255.0;
                let length = span*0.54;
                let mid = ImVec2::new((center.x+tip.x)*0.5,(center.y+tip.y)*0.5);
                let flicker = (forge::imgui::item::get_time()*16.0) as f32;
                let wave = (1.0-2.0*(flicker-flicker as u32 as f32)).abs();
                streak(dl,mid,axis,length,8.5*scale,0.21,(234,69,35,strength(64.0*gain)));
                streak(dl,mid,axis,length*0.97,(4.4+1.3*wave)*scale,0.21,(255,133,48,strength(104.0*gain)));
                streak(dl,mid,axis,length*0.96,2.0*scale,0.21,(255,221,142,strength(125.0*gain)));
            }
            _ => {}
        }
    }
}
