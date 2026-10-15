// ============================================================
// 火眼载板外壳 · 参数化 OpenSCAD  v3 (2026-10-14)
// 需求源：火眼载板_外壳设计规格.md v1.0 + 脚本修改清单（含第二轮）
//        + reviewer 修正（螺丝耳/支撑柱/止口/缺口方向）
// 适配：火眼载板1（105 x 140）+ GD32F470V-START（60 x 95）常驻插接
//       + W5500 模块（55 x 28，竖插 U9，右缘探出板边 12.4mm）
// 结构：底壳 + 前盖，四角外凸螺丝耳，M3 自攻；挂壁式
// 单位：mm
// 坐标：内腔底面左下角原点，X 右，Y 上，Z 朝前盖
// 导出：part="back" / "front" / "both" / "assembly"
// ============================================================

part = "assembly"; // "back" | "front" | "both" | "assembly"

/* ========== 内腔与壁厚 ========== */
cavity_x = 120.0;   // 含右侧 W5500 探出区 13mm
cavity_y = 145.0;
cavity_z = 30.0;
wall     = 2.2;
r_out    = 3.0;     // 外壁圆角
r_in     = 2.0;     // 内腔圆角
cover_t  = 2.2;     // 前盖厚

/* ========== 载板 ========== */
pcb_w = 105.0;
pcb_l = 140.0;
pcb_t = 1.6;
// 载板在内腔中：左下对齐（X 0~105, Y 0~140）

/* ========== 卡槽 ========== */
slot_w   = 2.2;   // 槽宽（板 1.6 + 0.6 公差）
slot_d   = 1.2;   // 槽深（壁厚 2.2，切 1.2 后外壁剩 1.0）
slot_zc  = 3.8;   // 槽中心离内腔底面
// 板底 Z=slot_zc-slot_w/2=2.7，板顶 Z=4.3

/* ========== W5500 模块区 ========== */
w5500_over = 12.4;
w5500_x0   = 89.4;
w5500_x1   = 117.4;
rj45_x0    = 95.6;
rj45_x1    = 111.2;
rj45_y0    = 3.5;
rj45_y1    = 24.5;

// 右下角开放缺口：Y 0~25 上切，X 95→右缘，Z 全深（开放，非封闭方孔）
notch_x0 = 95.0;
notch_y  = 25.0;

// 模块支撑平台（贴 EVA，故意做矮）
plat_x0 = 105.0;
plat_x1 = 119.0;
plat_y0 = 3.0;
plat_y1 = 55.0;
plat_h  = 8.0;

/* ========== START 板支撑 ========== */
start_w = 60.0;
start_l = 95.0;
start_x0 = 8.7;
start_y0 = 38.3;
post_top_z = 13.1;   // 距内腔底面（含板厚后 = 板顶 4.6 + 插合 8.5）
post_d     = 8.0;
posts = [ [4,45], [4,125], [72,45], [72,125] ];

/* ========== 螺丝耳（外凸，不占内腔） ========== */
ear_d    = 11.0;
ear_c    = 4.0;      // 耳中心外偏：内缘 1.5 < 壁内面 2.2，不进内腔；仍与外圆角 R3 相交可熔合
screw_d  = 3.2;
step_clr = 0.3;      // 止口间隙
lip_h    = 2.5;      // 前盖止口凸缘高度
lip_t    = 1.5;      // 凸缘壁厚

/* ========== 外轮廓 ========== */
out_x = cavity_x + 2*wall;
out_y = cavity_y + 2*wall;
out_z = wall + cavity_z;
dx = wall; dy = wall; dz = wall;

// 板顶面 Z（内腔底起算 slot_zc+pcb_t/2 = 4.6）
pcb_top_z = slot_zc + pcb_t/2;

/* ========== 开口坐标（载板坐标 = 内腔坐标） ========== */
u7  = [8.0, 5.8];
u8  = [22.2, 5.6];
dc1 = [61.8, 5.5];
led1 = [68.7, 5.0];
slot_a = [1, 30, 10];   // 缺口 A：U7+U8 合并
slot_b = [56, 72, 10];  // 缺口 B：DC1+LED1 合并

u10_y = 82.9;
serial_w = 12.0;
serial_h = 6.0;

// 前盖
h1   = [38.1, 102.4]; // OLED 0.69"（可视区约 17.3×5.3）
oled_w = 18.0;
oled_h = 7.0;
oled_ledge = 1.0;
buz  = [91.7, 95.8];
buz_d = 9.0;
u11  = [89.8, 115.9];
alarm_d = 7.0;
h2   = [74.7, 59.0];
ntc_d = 4.0;
sw_x = 18.0;  sw_y = 60.0;  sw_d = 6.2;  // 电源拨动开关 MTS-102

vent_d = 4.0;
vents = [ [20, 18], [45, 18], [70, 18], [95, 18] ];

/* ========== 挂壁键孔（背面 Z=0） ========== */
kh_head_d = 5.5;
kh_neck_d = 3.2;
kh_span   = 80.0;
kh_top    = 18.0;

/* ============================================================ */
/* 工具                                                          */
/* ============================================================ */
module rbox(w, l, h, r=2, center_z=false) {
    translate([0, 0, center_z ? -h/2 : 0])
        linear_extrude(h)
            offset(r) offset(-r)
                square([w, l]);
}

// 四耳中心（外凸，相对外轮廓角 ear_c 外偏）
function ear_positions() = [
    [ -ear_c,        -ear_c        ],
    [ out_x + ear_c, -ear_c        ],
    [ -ear_c,        out_y + ear_c ],
    [ out_x + ear_c, out_y + ear_c ]
];

/* ============================================================ */
/* 螺丝耳 —— 必须在内腔 subtraction 之后 union，否则被掏空        */
/* ============================================================ */
module screw_ears(h) {
    for (p = ear_positions())
        translate([p[0], p[1], 0])
            difference() {
                cylinder(d=ear_d, h=h, $fn=40);
                translate([0, 0, -0.1])
                    cylinder(d=screw_d, h=h + 0.2, $fn=24);
            }
}

/* ============================================================ */
/* 卡槽 —— 从内腔面往壁内切；板边无壁处加导轨                      */
/* ============================================================ */
module pcb_card_slots() {
    // 左壁（X=dx 面）
    translate([dx - slot_d, dy - 0.5, dz + slot_zc - slot_w/2])
        cube([slot_d + 0.1, pcb_l + 1.0, slot_w]);
    // 下壁（Y=dy 面）
    translate([dx - 0.5, dy - slot_d, dz + slot_zc - slot_w/2])
        cube([pcb_w + 1.0, slot_d + 0.1, slot_w]);
    // 上边：板边 Y=dy+pcb_l 无墙（腔到 147），加导轨再开槽
    translate([dx + 5, dy + pcb_l - 1.5, dz])
        difference() {
            cube([pcb_w - 10, 3.0, slot_zc + slot_w]);
            translate([-0.1, 1.5 - slot_w/2 + 0.1, slot_zc - slot_w/2])
                cube([pcb_w - 10 + 0.2, slot_w, slot_w]);
        }
    // 右边：X=dx+pcb_w 无墙，模块平台兼作限位（平台在 Z8，板在 Z2.7~4.3）
    // 再加一条低位导轨贴板右缘
    translate([dx + pcb_w, dy + 5, dz])
        difference() {
            cube([2.0, pcb_l - 10, slot_zc + slot_w]);
            translate([2.0 - slot_d, -0.1, slot_zc - slot_w/2])
                cube([slot_d + 0.1, pcb_l - 10 + 0.2, slot_w]);
        }
}

/* ============================================================ */
/* START 支撑柱 —— 从板顶面起，不穿板                            */
/* ============================================================ */
module start_posts() {
    for (p = posts)
        translate([dx + p[0], dy + p[1], dz + pcb_top_z])
            cylinder(d=post_d, h=post_top_z - pcb_top_z, $fn=32);
}

/* ============================================================ */
/* W5500 支撑平台                                                 */
/* ============================================================ */
module w5500_platform() {
    translate([dx + plat_x0, dy + plat_y0, dz])
        cube([plat_x1 - plat_x0, plat_y1 - plat_y0, plat_h]);
}

/* ============================================================ */
/* 挂壁键孔（背面 Z=0 外壁）                                      */
/* ============================================================ */
module wall_keyholes() {
    for (x = [out_x/2 - kh_span/2, out_x/2 + kh_span/2])
        translate([x, dy + cavity_y - kh_top, -0.1]) {
            cylinder(d=kh_head_d, h=wall + 0.2, $fn=24);
            translate([0, -8, 0]) cylinder(d=kh_neck_d, h=wall + 0.2, $fn=24);
            translate([-kh_neck_d/2, -8, 0])
                cube([kh_neck_d, 8, wall + 0.2]);
        }
}

/* ============================================================ */
/* 底壳                                                          */
/* ============================================================ */
module back_shell() {
    difference() {
        union() {
            rbox(out_x, out_y, out_z, r=r_out);
        }

        // 掏内腔（前面敞开）
        translate([dx, dy, dz])
            rbox(cavity_x, cavity_y, cavity_z + 0.1, r=r_in);

        // 卡槽
        pcb_card_slots();

        // 缺口 A：U7+U8（Y=0 壁，通到底边）
        translate([dx + slot_a[0], -0.1, dz + slot_zc - 1])
            cube([slot_a[1] - slot_a[0], wall + 0.2, slot_a[2]]);

        // 缺口 B：DC1+LED1
        translate([dx + slot_b[0], -0.1, dz + slot_zc - 1])
            cube([slot_b[1] - slot_b[0], wall + 0.2, slot_b[2]]);

        // 右下角开放缺口：Y 0~25、X 95→右缘、Z 全深
        translate([dx + notch_x0, -0.1, -0.1])
            cube([out_x - (dx + notch_x0) + wall + 1, notch_y, out_z + 0.2]);

        // 左面串口槽
        translate([-0.1, dy + u10_y - serial_w/2, dz + slot_zc])
            cube([wall + 0.2, serial_w, serial_h]);

        // 上侧壁散热孔（挂壁后朝上，与前盖对流）
        for (v = vents)
            translate([dx + v[0], dy + cavity_y - 0.1, dz + cavity_z/2])
                rotate([-90, 0, 0])
                    cylinder(d=vent_d, h=wall + 0.2, $fn=20);

        wall_keyholes();
    }

    // ---- 以下在 difference 之后 union，不会被内腔掏掉 ----
    start_posts();
    w5500_platform();
    screw_ears(out_z);   // 全高耳，M3×12 从前面拧入

    // 前缘止口：内腔口一圈薄止口，与前盖凸缘配合（止口高 0.4，前盖凸缘插进来）
    translate([dx, dy, dz + cavity_z - 0.4])
        difference() {
            rbox(cavity_x, cavity_y, 0.4, r=r_in);
            translate([step_clr + lip_t, step_clr + lip_t, -0.1])
                rbox(cavity_x - 2*(step_clr + lip_t),
                     cavity_y - 2*(step_clr + lip_t), 0.6, r=r_in);
        }
}

/* ============================================================ */
/* 前盖                                                          */
/* ============================================================ */
module front_cover() {
    difference() {
        union() {
            // 盖板（local Z=0 朝内腔，Z=cover_t 朝外）
            rbox(out_x, out_y, cover_t, r=r_out);
            // 止口凸缘：插进底壳内腔，留 step_clr 间隙
            translate([dx + step_clr, dy + step_clr, -lip_h])
                difference() {
                    rbox(cavity_x - 2*step_clr, cavity_y - 2*step_clr,
                         lip_h + 0.1, r=r_in);
                    translate([lip_t, lip_t, -0.1])
                        rbox(cavity_x - 2*step_clr - 2*lip_t,
                             cavity_y - 2*step_clr - 2*lip_t,
                             lip_h + 0.3, r=r_in);
                }
            // 四耳（与底壳对齐）
            for (p = ear_positions())
                translate([p[0], p[1], -lip_h])
                    cylinder(d=ear_d, h=lip_h + cover_t, $fn=40);
        }

        // 螺丝过孔
        for (p = ear_positions())
            translate([p[0], p[1], -lip_h - 0.1])
                cylinder(d=screw_d, h=lip_h + cover_t + 0.2, $fn=24);

        // OLED 窗 18×7 + 内侧安装沉台（0.69 寸，热熔胶嵌屏）
        translate([dx + h1[0] - oled_w/2, dy + h1[1] - oled_h/2, -0.1])
            cube([oled_w, oled_h, cover_t + 0.2]);
        translate([dx + h1[0] - oled_w/2 - oled_ledge,
                   dy + h1[1] - oled_h/2 - oled_ledge, -0.1])
            cube([oled_w + 2*oled_ledge, oled_h + 2*oled_ledge, 0.8]);

        // 蜂鸣器 φ9
        translate([dx + buz[0], dy + buz[1], -0.1])
            cylinder(d=buz_d, h=cover_t + 0.2, $fn=32);

        // 报警灯 φ7
        translate([dx + u11[0], dy + u11[1], -0.1])
            cylinder(d=alarm_d, h=cover_t + 0.2, $fn=32);

        // NTC 引线 φ4
        translate([dx + h2[0], dy + h2[1], -0.1])
            cylinder(d=ntc_d, h=cover_t + 0.2, $fn=20);

        // 电源拨动开关 φ6.2
        translate([dx + sw_x, dy + sw_y, -0.1])
            cylinder(d=sw_d, h=cover_t + 0.2, $fn=32);

        // 散热孔
        for (v = vents)
            translate([dx + v[0], dy + v[1], -0.1])
                cylinder(d=vent_d, h=cover_t + 0.2, $fn=20);
    }
}

/* ============================================================ */
/* 装配预览                                                        */
/* ============================================================ */
module assembly() {
    color("SteelBlue") back_shell();
    color("Orange", 0.75) translate([0, 0, out_z]) front_cover();

    color("DarkGreen", 0.5)
        translate([dx, dy, dz + slot_zc - pcb_t/2])
            cube([pcb_w, pcb_l, pcb_t]);

    color("Navy", 0.4)
        translate([dx + start_x0, dy + start_y0, dz + pcb_top_z + 0.1])
            cube([start_w, start_l, 1.6]);

    color("Purple", 0.45)
        translate([dx + w5500_x0, dy + 20, dz + plat_h])
            cube([w5500_x1 - w5500_x0, 55, 1.6]);
}

/* ========== 出口 ========== */
if (part == "back") back_shell();
else if (part == "front") front_cover();
else if (part == "assembly") assembly();
else {
    back_shell();
    translate([out_x + 20, 0, 0]) front_cover();
}
