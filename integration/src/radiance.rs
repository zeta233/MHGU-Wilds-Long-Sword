use core::sync::atomic::{AtomicU64, Ordering};
use forge::imgui::{color::color, draw::DrawList};
use forge::sys::imgui::ImVec2;

static LAYERS: [AtomicU64; 16] = [const { AtomicU64::new(0) }; 16];

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
                let slope = (0.36 + 0.13*projected_slope).clamp(0.27,0.47)
                    * size.y/size.x*(16.0/9.0);
                axis = ImVec2::new(-1.0, -slope);
                let right = (center.x + 0.44*size.x).clamp(0.87*size.x,1.04*size.x);
                let half = size.x*(0.19 + 0.43*smooth(age/0.083));
                let remaining = 1.0-smooth((age-0.105)/0.265);
                let weight = (0.42 + 0.58*smooth(age/0.064))*remaining;
                let origin = ImVec2::new(right-half,
                    center.y+(right-center.x-half)*-axis.y);
                streak(dl,origin,axis,half*1.04,25.0*scale,0.27,(244,32,54,strength(51.0*remaining)));
                streak(dl,origin,axis,half,16.5*scale,0.27,(255,25,48,strength(172.0*remaining)));
                streak(dl,origin,axis,half*0.99,13.5*weight*scale,0.27,(255,116,130,strength(222.0*remaining)));
                streak(dl,origin,axis,half*0.98,10.2*weight*scale,0.27,(255,248,238,strength(255.0*remaining)));
            }
            1 => {
                let fade = (1.0-smooth(age/0.24))*smooth((age+0.008)/0.023);
                let radius = (span*0.22).clamp(10.0*scale, 33.0*scale);
                for (reach,width,alpha) in [(1.0,0.62,38.0),(0.64,0.40,64.0),(0.34,0.22,84.0)] {
                    lens(dl,center,axis,radius*reach*(1.0+age),radius*width,
                         (255,164,102,strength(alpha*fade)));
                }
            }
            2 => {
                let fade = 1.0-smooth((age-0.13)/0.39);
                let light = (0.78+0.22*smooth(age/0.065))*fade;
                let spread = smooth(age/0.095);
                // Follow through from a shallow swipe into an ascending cut.
                let projected = axis.y * if axis.x >= 0.0 { 1.0 } else { -1.0 };
                let high = (0.44-0.05*projected).clamp(0.39,0.49);
                let slope = 0.06+(high-0.06)*smooth(age/0.10);
                let slash = ImVec2::new(-1.0,slope*size.y/size.x*(16.0/9.0));
                let root_x = center.x.clamp(0.28*size.x,0.75*size.x);
                let right = 1.13*size.x;
                let left = root_x+(0.06*size.x-root_x)*(0.65+0.35*spread)-0.22*size.x*spread;
                let half = (right-left)*0.5;
                let mid = right-half;
                let origin = ImVec2::new(mid,center.y-(mid-root_x)*slash.y);
                let width = (5.0+17.0*spread)*scale;
                streak(dl,origin,slash,half,2.0*width,-0.14,(204,17,40,strength(89.0*light)));
                streak(dl,origin,slash,half,1.47*width,-0.14,(245,39,59,strength(164.0*light)));
                streak(dl,origin,slash,half,1.08*width,-0.14,(255,122,135,strength(222.0*light)));
                streak(dl,origin,slash,half,0.84*width,-0.14,(255,246,239,strength(249.0*light)));
                // The softer fan expands beneath the dominant white sheet.
                for (tilt, reach) in [(0.10,0.36),(0.18,0.27),(0.25,0.19)] {
                    let ray = ImVec2::new(1.0,(-slope+tilt*spread)*size.y/size.x*(16.0/9.0));
                    let length = size.x*reach*(0.53+0.47*spread)*0.5;
                    let start = ImVec2::new(root_x,center.y);
                    let origin = ImVec2::new(start.x+ray.x*length,start.y+ray.y*length);
                    lens(dl,origin,ray,length,7.0*scale,(233,75,72,strength(34.0*light*spread)));
                }
            }
            3 if end != 0 && span >= 4.0*scale => {
                let gain = (state >> 48) as f32 / 255.0;
                let length = span*0.54;
                let mid = ImVec2::new((center.x+tip.x)*0.5,(center.y+tip.y)*0.5);
                let flicker = (forge::imgui::item::get_time()*16.0) as f32;
                let wave = (1.0-2.0*(flicker-flicker as u32 as f32)).abs();
                streak(dl,mid,axis,length,8.5*scale,0.21,(234,69,35,strength(64.0*gain)));
                streak(dl,mid,axis,length*0.97,(4.4+1.3*wave)*scale,0.21,(255,133,48,strength(104.0*gain)));
                streak(dl,mid,axis,length*0.96,2.0*scale,0.21,(255,221,142,strength(125.0*gain)));
            }
            4..=15 if end != 0 => {
                let gain = (state >> 48) as f32 / 255.0;
                let length = (span*0.72).clamp(1.7*scale, 7.0*scale);
                lens(dl,center,axis,length,1.6*scale,(247,112,33,strength(128.0*gain)));
                lens(dl,center,axis,length*0.82,0.73*scale,(255,221,145,strength(246.0*gain)));
            }
            _ => {}
        }
    }
}
