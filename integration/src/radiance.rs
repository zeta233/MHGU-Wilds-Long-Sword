use core::sync::atomic::{AtomicU64, Ordering};
use forge::imgui::{color::color, draw::DrawList};
use forge::sys::imgui::ImVec2;

static LAYERS: [AtomicU64; 3] = [const { AtomicU64::new(0) }; 3];

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
    if rgba.3 == 0 { return; }
    let p = |along: f32, across: f32| ImVec2::new(
        origin.x + axis.x*along - axis.y*across,
        origin.y + axis.y*along + axis.x*across);
    // ImGui's antialias fringe requires clockwise vertices in screen coordinates.
    let shape = [
        p(-length,0.0), p(-0.55*length,-0.65*width), p(0.0,-width),
        p(0.55*length,-0.65*width), p(length,0.0), p(0.55*length,0.65*width),
        p(0.0,width), p(-0.55*length,0.65*width),
    ];
    dl.add_convex_poly_filled(&shape, color(rgba.0,rgba.1,rgba.2,rgba.3));
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
                // Preserve the world anchor. A foreshortened blade uses a
                // screen diagonal so the foreground still sweeps across the view.
                let projected_y = if axis.x > 0.0 { -axis.y } else { axis.y };
                let projected_weight = smooth((axis.x.abs()-0.30)/0.32);
                let slope = (-0.42*(1.0-projected_weight)+projected_y*projected_weight)
                    .clamp(-0.53,0.53);
                axis = ImVec2::new(-1.0,slope);
                let half = (span*5.7).clamp(size.x*0.44,size.x*0.62);
                let reveal = 0.19 + 0.81*smooth(age/0.093);
                let start = ImVec2::new(center.x-axis.x*half*(1.0-reveal),
                    center.y-axis.y*half*(1.0-reveal));
                let fade = smooth((age+0.012)/0.030)*(1.0-smooth((age-0.095)/0.093));
                let blur = (1.0-smooth(age/0.21))*smooth((age+0.010)/0.028);
                let core = (0.57+0.43*smooth(age/0.085))*(1.0-0.4*smooth((age-0.10)/0.05));
                lens(dl,start,axis,half*reveal*1.08,26.0*scale,(241,33,43,strength(38.0*blur)));
                lens(dl,start,axis,half*reveal,14.5*scale,(255,24,36,strength(106.0*fade)));
                lens(dl,start,axis,half*reveal*0.99,10.0*core*scale,(255,46,60,strength(157.0*fade)));
                lens(dl,start,axis,half*reveal*0.96,7.0*core*scale,(255,159,161,strength(205.0*fade)));
                lens(dl,start,axis,half*reveal*0.94,4.8*core*scale,(255,248,239,strength(255.0*fade)));
            }
            1 => {
                let fade = (1.0-smooth(age/0.36))*smooth((age+0.008)/0.023);
                let half = (span*1.05).clamp(46.0*scale, 90.0*scale);
                for (distance,width,alpha) in [(0.0,25.0,21.0),(0.11,15.0,30.0),(0.22,6.0,40.0)] {
                    let ghost = ImVec2::new(center.x-axis.x*half*distance,center.y-axis.y*half*distance);
                    lens(dl,ghost,axis,half*(1.0+age),width*scale,(255,108,98,strength(alpha*fade)));
                }
            }
            _ => {
                let light = smooth((age+0.013)/0.035)*(1.0-smooth((age-0.064)/0.166));
                let travel = 0.75 + 0.25*smooth(age/0.075);
                let half = (span*0.79).clamp(32.0*scale,57.0*scale)*travel;
                let rays = [(0.65,-0.76),(0.91,-0.42),(1.0,0.0),(0.91,0.42),(0.65,0.76)];
                for (index,(cos,sin)) in rays.into_iter().enumerate() {
                    let ray = ImVec2::new(axis.x*cos-axis.y*sin, axis.x*sin+axis.y*cos);
                    let length = half*if index == 2 {1.0} else {0.86};
                    let origin = ImVec2::new(center.x+ray.x*length*0.88,center.y+ray.y*length*0.88);
                    lens(dl,origin,ray,length,7.8*scale,(237,27,37,strength(127.0*light)));
                    lens(dl,origin,ray,length*0.95,3.1*scale,(255,119,113,strength(180.0*light)));
                    lens(dl,origin,ray,length*0.90,1.2*scale,(255,237,214,strength(229.0*light)));
                }
            }
        }
    }
}
