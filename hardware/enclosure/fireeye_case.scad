// ============================================================
// 火眼载板外壳 — 上下盖 + 四角螺丝 + 挂壁
// 载板 105 x 140 mm（嘉立创 EDA 实测）
// 用法：OpenSCAD 打开 → F6 渲染 → 导出 STL
//   bottom_shell();  // 下盖
//   top_cover();     // 上盖
//   assembly();      // 预览（默认）
// ============================================================

part = "assembly"; // "bottom_shell" | "top_cover" | "assembly"

// ---------- 载板 ----------
pcb_l   = 105;      // X
pcb_w   = 140;      // Y
pcb_t   = 1.6;

// ---------- 外壳 ----------
wall    = 2.5;
clearance = 0.4;    // 装配间隙
standoff_h = 8;     // 板下立柱（通孔焊脚净空）
inner_above_pcb = 42; // 载板上方净高（含 START 叠层 ≈27、继电器 15.8）

// 外壳内腔（含间隙）
inner_l = pcb_l + 2*clearance + 0.8; // 边框限位
inner_w = pcb_w + 2*clearance + 0.8;
inner_h = standoff_h + pcb_t + inner_above_pcb;

// 外轮廓
out_l = inner_l + 2*wall;
out_w = inner_w + 2*wall;
bottom_shell_h = wall + standoff_h + pcb_t + 12; // 侧壁罩住板下与低矮件
top_cover_h    = inner_h - (standoff_h + pcb_t - 8) + wall; // 余量给上盖侧壁搭接

// 简化：两段高度
bot_h = wall + standoff_h + 18;          // ≈28.5
top_h = wall + inner_above_pcb + 10;     // ≈54.5 → 合盖总高 ≈ 83（含搭接重叠）

// ---------- 螺丝柱 ----------
screw_d      = 3.2;   // M3 自攻底孔
screw_head_d = 6.2;
post_d       = 8;
post_inset   = 6;     // 距内壁

// ---------- 挂壁键孔 ----------
keyhole_head_d = 5.5;
keyhole_neck_d = 3.2;
keyhole_span   = 80;
keyhole_top    = 18;

// ---------- 接口坐标（载板左下角原点，X 右 Y 上）----------
// 下盖 · 底壁
u7  = [8.0, 5.8];    u7_slot  = [14, 14];
u8  = [22.2, 5.6];   u8_slot  = [14, 14];
dc1 = [61.9, 5.5];   dc1_d    = 10;
led1 = [68.7, 5.2];  led1_d   = 6;
// 左壁 / 右壁
u10_y = 82.9;        u10_slot = [16, 10];   // 深度方向 X
u9_y  = 53.5;        u9_slot  = [30, 16];
// 上盖 · 顶面
buzz = [91.7, 95.8]; buzz_d = 2; buzz_n = 9;
h1   = [38.1, 102.4]; h1_slot = [30, 12];
h2   = [74.7, 59.0];  h2_d    = 6;
u11  = [89.8, 115.9]; u11_d   = 6;
// START 板覆盖区（上盖开窗）
start_x = [7.5, 67.5];
start_y = [38, 133];

// 模型坐标原点：外壳外轮廓左下角，X 右，Y 上，Z 下盖底面
// 载板在壳内的偏移
pcb_ox = wall + clearance + 0.4;
pcb_oy = wall + clearance + 0.4;
pcb_oz = wall + standoff_h;

// ---------- 工具 ----------
module rounded_rect(l, w, h, r=2, center=false) {
    translate(center ? [-l/2, -w/2, 0] : [0, 0, 0])
    linear_extrude(h)
        offset(r) offset(-r)
            square([l, w]);
}

// 四角柱坐标
function post_pos() = [
    [wall + post_inset,           wall + post_inset],
    [out_l - wall - post_inset,   wall + post_inset],
    [wall + post_inset,           out_w - wall - post_inset],
    [out_l - wall - post_inset,   out_w - wall - post_inset]
];

module screw_posts(h, blind=true) {
    for (p = post_pos()) {
        translate([p[0], p[1], 0]) {
            difference() {
                cylinder(h=h, d=post_d, $fn=32);
                translate([0, 0, blind ? h-14 : -1])
                    cylinder(h=blind ? 15 : h+2, d=screw_d, $fn=24);
            }
        }
    }
}

module screw_head_relief(h=6) {
    for (p = post_pos()) {
        translate([p[0], p[1], h-3])
            cylinder(h=4, d=screw_head_d, $fn=24);
    }
}

// ============================================================
// 下盖
// ============================================================
module bottom_shell() {
    difference() {
        union() {
            // 外壳体
            rounded_rect(out_l, out_w, bot_h, r=3);
            // 立柱（托载板）
            translate([0, 0, wall])
                screw_posts(standoff_h + 20, blind=true);
            // 板周限位边框（高 3 mm）
            translate([pcb_ox-1, pcb_oy-1, wall+standoff_h])
                difference() {
                    rounded_rect(pcb_l+2, pcb_w+2, 3, r=1);
                    translate([1, 1, -0.1])
                        rounded_rect(pcb_l, pcb_w, 3.2, r=1);
                }
        }

        // 掏内腔
        translate([wall, wall, wall])
            rounded_rect(inner_l, inner_w, bot_h, r=2);

        // ---- 底壁开孔（映射：壳内 pcb_ox + 载板坐标）----
        // U7
        translate([pcb_ox + u7[0] - u7_slot[0]/2,
                   pcb_oy + u7[1] - u7_slot[1]/2, -1])
            cube([u7_slot[0], u7_slot[1], wall+2]);
        // U8
        translate([pcb_ox + u8[0] - u8_slot[0]/2,
                   pcb_oy + u8[1] - u8_slot[1]/2, -1])
            cube([u8_slot[0], u8_slot[1], wall+2]);
        // DC1
        translate([pcb_ox + dc1[0], pcb_oy + dc1[1], -1])
            cylinder(h=wall+2, d=dc1_d, $fn=32);
        // LED1 导光孔
        translate([pcb_ox + led1[0], pcb_oy + led1[1], -1])
            cylinder(h=wall+2, d=led1_d, $fn=24);

        // ---- 左壁 U10 串口槽 ----
        translate([-1, pcb_oy + u10_y - u10_slot[1]/2, wall + standoff_h + 2])
            cube([wall+2, u10_slot[1], u10_slot[0]]);

        // ---- 右壁 U9 / W5500 槽 ----
        translate([out_l - wall - 1, pcb_oy + u9_y - u9_slot[1]/2, wall + standoff_h + 2])
            cube([wall+2, u9_slot[1], u9_slot[0]]);

        // ---- 挂壁键孔（后壁 = +Y 外侧）----
        for (dx = [-keyhole_span/2, keyhole_span/2]) {
            translate([out_l/2 + dx, out_w - wall/2, bot_h - keyhole_top]) {
                rotate([-90, 0, 0]) {
                    cylinder(h=wall+2, d=keyhole_head_d, $fn=24);
                    translate([0, keyhole_head_d/2 - 0.5, 0])
                        cylinder(h=wall+2, d=keyhole_neck_d, $fn=20);
                }
            }
        }

        // 螺丝头让位
        translate([0, 0, bot_h-8]) screw_head_relief(8);
    }
}

// ============================================================
// 上盖
// ============================================================
module top_cover() {
    translate([0, 0, 0])
    difference() {
        union() {
            rounded_rect(out_l, out_w, top_h, r=3);
            // 内侧搭接唇（插进下盖内腔）
            translate([wall - clearance, wall - clearance, -8])
                difference() {
                    rounded_rect(inner_l + 2*clearance - 0.2,
                                 inner_w + 2*clearance - 0.2, 8, r=2);
                    translate([-1, -1, -0.1])
                        rounded_rect(inner_l - 2*wall + 2,
                                     inner_w - 2*wall + 2, 8.2, r=2);
                }
        }

        // 掏内腔（顶面留 2.5 壁）
        translate([wall, wall, -1])
            rounded_rect(inner_l, inner_w, top_h - wall + 1, r=2);

        // ---- 顶面 · 蜂鸣器声孔阵列 ----
        for (i = [0:2], j = [0:2]) {
            translate([pcb_ox + buzz[0] + (i-1)*4,
                       pcb_oy + buzz[1] + (j-1)*4, top_h - wall - 1])
                cylinder(h=wall+2, d=buzz_d, $fn=16);
        }

        // ---- 顶面 · OLED 窗 ----
        translate([pcb_ox + h1[0] - h1_slot[0]/2,
                   pcb_oy + h1[1] - h1_slot[1]/2, top_h - wall - 1])
            cube([h1_slot[0], h1_slot[1], wall+2]);

        // ---- 顶面 · H2 / U11 出线孔 ----
        translate([pcb_ox + h2[0], pcb_oy + h2[1], top_h - wall - 1])
            cylinder(h=wall+2, d=h2_d, $fn=24);
        translate([pcb_ox + u11[0], pcb_oy + u11[1], top_h - wall - 1])
            cylinder(h=wall+2, d=u11_d, $fn=24);

        // ---- 顶面 · START 板开窗（USB / 按键 / GD-Link）----
        translate([pcb_ox + start_x[0], pcb_oy + start_y[0], top_h - wall - 1])
            cube([start_x[1]-start_x[0], start_y[1]-start_y[0], wall+2]);

        // ---- 顶面 · 散热缝（START 区两侧）----
        for (sx = [start_x[0]-4, start_x[1]+1]) {
            translate([pcb_ox + sx, pcb_oy + start_y[0], top_h - wall - 1])
                cube([3, start_y[1]-start_y[0], wall+2]);
        }

        // ---- 顶面 · 螺丝通孔 ----
        for (p = post_pos()) {
            translate([p[0], p[1], top_h - wall - 1])
                cylinder(h=wall+2, d=screw_head_d, $fn=24);
            translate([p[0], p[1], -1])
                cylinder(h=top_h + 2, d=screw_d, $fn=24);
        }
    }
}

// ============================================================
// 预览装配
// ============================================================
module assembly() {
    color("SteelBlue") bottom_shell();
    color("Silver")
        translate([pcb_ox, pcb_oy, pcb_oz])
            cube([pcb_l, pcb_w, pcb_t]);
    color("Orange")
        translate([0, 0, bot_h - 8])
            top_cover();
}

if (part == "bottom_shell") bottom_shell();
else if (part == "top_cover") top_cover();
else assembly();
