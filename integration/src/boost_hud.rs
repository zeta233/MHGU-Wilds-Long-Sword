use forge::imgui::{color::color, draw::DrawList, font::ImFont, item};
use forge::sys::imgui::{ImDrawFlags, ImVec2};

const METAL: u32 = 0xff829bb7;
const RIM: u32 = 0xffbef1fa;
const RECESS: u32 = 0xf5272019;

fn border(dl: &DrawList, edge: &[ImVec2], tint: u32, width: f32) {
    for pair in edge.windows(2) {
        dl.add_line(pair[0], pair[1], tint, width);
    }
    dl.add_line(edge[edge.len()-1], edge[0], tint, width);
}

fn fill_crystal(dl: &DrawList, crystal: &[ImVec2; 6], floor: f32, tint: u32) {
    let mut visible = [crystal[0]; 7];
    let mut count = 0;
    let mut previous = crystal[5];
    for &current in crystal {
        let inside = current.y >= floor;
        if inside != (previous.y >= floor) {
            let t = (floor-previous.y)/(current.y-previous.y);
            visible[count] = ImVec2::new(previous.x+t*(current.x-previous.x), floor);
            count += 1;
        }
        if inside {
            visible[count] = current;
            count += 1;
        }
        previous = current;
    }
    if count>=3 { dl.add_convex_poly_filled(&visible[..count], tint); }
}

pub fn draw(dl: &DrawList, left: f32, top: f32, scale: f32, state: u32) {
    let active = state & 2 != 0;
    let enhanced = state & 4 != 0;
    let charging = state & 8 != 0;
    let chase = state & 16 != 0;
    let musou = (state >> 5) & 3;
    let fraction = ((state >> 8) & 255).min(100) as f32 / 100.0;
    let reserve = (state >> 16).min(300) as f32 / 100.0;
    let ready = !active && reserve >= 1.0;
    let warning = active && fraction < 0.23;
    let phase = (item::get_time()*1.8) as f32;
    let wave = 1.0-(2.0*(phase-phase as u32 as f32)-1.0).abs();
    let origin = ImVec2::new(left+137.0*scale,top+46.0*scale);
    let scale = 0.82*scale;
    let point = |x: f32, y: f32| ImVec2::new(origin.x+x*scale,origin.y+y*scale);
    let core = point(26.0,22.0);
    let outline = [
        point(3.0,27.0), point(17.0,13.0), point(37.0,12.0),
        point(56.0,1.0), point(132.0,1.0), point(143.0,10.0),
        point(132.0,25.0), point(54.0,25.0), point(36.0,36.0), point(13.0,36.0),
    ];
    let body_tint = color(19,22,27,244);
    dl.add_convex_poly_filled(&outline[2..8],body_tint);
    dl.add_convex_poly_filled(&[outline[0],outline[1],outline[2],outline[7],outline[8],outline[9]],body_tint);
    border(dl,&outline,METAL,0.9*scale);
    dl.add_line(point(57.0,3.0),point(129.0,3.0),color(252,230,184,132),0.7*scale);
    dl.add_line(point(56.0,23.0),point(130.0,23.0),color(100,85,65,220),0.8*scale);

    dl.add_circle_filled(core,13.0*scale,RECESS,24);
    let signal = active || ready || enhanced || charging || chase || musou != 0;
    let (main, glare) = if active {
        (color(189,64,20,255),color(255,215,119,245))
    } else if signal {
        (color(40,105,174,255),color(162,231,255,245))
    } else {
        (color(29,52,76,255),color(95,150,192,210))
    };
    if active || ready {
        let glow = (38.0+32.0*wave) as u8;
        let halo = if warning { color(235,61,38,glow) } else if active {
            color(247,145,40,glow)
        } else { color(101,195,247,glow) };
        dl.add_circle(core,14.0*scale,halo,28,1.8*scale);
    }
    dl.add_circle_filled(core,9.2*scale,main,24);
    dl.add_convex_poly_filled(&[
        point(20.0,22.0),point(24.0,16.0),point(31.0,17.0),
        point(34.0,21.0),point(29.0,27.0),point(22.0,27.0),
    ],color(28,33,53,122));
    dl.add_convex_poly_filled(&[
        point(25.5,16.0),point(31.0,20.9),point(27.0,27.6),point(21.3,22.5),
    ],glare);
    dl.add_triangle_filled(point(25.5,16.0),point(27.0,24.5),point(21.3,22.5),color(255,244,224,104));
    dl.add_circle(core,10.5*scale,METAL,28,1.1*scale);
    dl.add_circle(core,12.6*scale,color(85,76,64,255),28,0.68*scale);
    if enhanced {
        dl.add_line(point(19.8,33.2),point(31.8,33.2),color(115,215,255,240),1.05*scale);
    }

    // The arc and crystals use the same timer while Boost is active.
    let arc = [
        (-16.0,0.0),(-15.7,-3.1),(-14.8,-6.1),(-13.3,-8.9),(-11.3,-11.3),
        (-8.9,-13.3),(-6.1,-14.8),(-3.1,-15.7),(0.0,-16.0),
        (3.1,-15.7),(6.1,-14.8),(8.9,-13.3),(11.3,-11.3),(13.3,-8.9),
        (14.8,-6.1),(15.7,-3.1),(16.0,0.0),
    ];
    for (index,pair) in arc.windows(2).enumerate() {
        let (x,y)=pair[0];
        let (next_x,next_y)=pair[1];
        let start=point(26.0+x,22.0+y);
        dl.add_line(start,point(26.0+next_x,22.0+next_y),color(83,66,52,225),2.5*scale);
        if active {
            let remainder=fraction*16.0-index as f32;
            let part=remainder.clamp(0.0,1.0);
            if part>0.0 {
                let end=point(26.0+x+(next_x-x)*part,22.0+y+(next_y-y)*part);
                let hot=if warning { color(255,(66.0+27.0*wave) as u8,48,255) }
                    else { color(245,153,47,255) };
                dl.add_line(start,end,hot,2.8*scale);
                dl.add_line(start,end,glare,0.75*scale);
                if remainder<=1.0 {
                    dl.add_circle_filled(end,1.8*scale,hot,12);
                    dl.add_circle_filled(end,0.75*scale,glare,8);
                }
            }
        }
    }
    for cell in 0..3 {
        let x=61.0+cell as f32*23.0;
        let trim=[
            point(x+5.0,6.0),point(x+18.0,6.0),point(x+19.5,14.0),
            point(x+12.0,20.0),point(x+2.0,17.0),point(x,12.0),
        ];
        dl.add_convex_poly_filled(&trim,RECESS);
        let stored = (reserve-cell as f32).clamp(0.0,1.0);
        let part = if active { (fraction*3.0-cell as f32).clamp(0.0,1.0) } else { stored };
        let rim = if !active && stored>=1.0 { RIM } else if active && part>0.0 {
            color(226,157,78,240)
        } else { METAL };
        border(dl,&trim,rim,0.66*scale);
        if part>0.0 {
            let crystal=[
                point(x+6.0,8.0),point(x+16.5,8.0),point(x+17.5,13.5),
                point(x+12.0,18.0),point(x+3.5,15.5),point(x+2.5,12.2),
            ];
            let (recess, fill, facet) = if active {
                let fill = if warning { color(242,76,43,255) } else { color(240,154,55,255) };
                (color(89,47,24,235),fill,color(255,228,154,235))
            } else {
                (color(31,59,90,235),color(59,157,220,255),color(188,234,255,225))
            };
            dl.add_convex_poly_filled(&crystal,recess);
            fill_crystal(dl,&crystal,point(x,18.0-10.0*part).y,fill);
            if part>=0.995 {
                dl.add_triangle_filled(point(x+6.5,8.5),point(x+15.2,8.5),
                    point(x+10.0,13.8),facet);
            }
        }
        if active {
            dl.add_rect_filled(point(x+1.0,21.0),point(x+19.0,23.0),color(31,59,90,235),0.0);
            if stored>0.0 {
                let tint = if stored>=1.0 { color(119,210,255,255) } else { color(59,157,220,255) };
                dl.add_rect_filled(point(x+1.0,21.0),point(x+1.0+18.0*stored,23.0),tint,0.0);
            }
        }
    }

    let key = if musou == 1 { "R" } else if chase { "X" } else if ready {
        if state & 128 != 0 { "RT" } else { "L3" }
    } else { "" };
    if !key.is_empty() {
        dl.add_rect_filled(point(117.0,27.0),point(135.0,38.5),color(24,25,31,240),2.0*scale);
        dl.add_rect(point(117.0,27.0),point(135.0,38.5),METAL,2.0*scale,0.8*scale,ImDrawFlags::None);
        let x=if key.len()==1 {124.0} else {121.0};
        dl.add_text_with_font(&ImFont::current(),8.0*scale,point(x,28.6),color(255,239,200,255),key);
    }
}
