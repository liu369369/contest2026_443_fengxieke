// ============================================================
// 火眼载板外壳 · 参数化 OpenSCAD  v3 (2026-10-14)
// 基于 v2（fireeye_enclosure.scad, 10/14 18:15）—— 本版修正 5 处：
//   [12] 四角螺丝耳改为"外轮廓角外侧凸"，不再伸入内腔
//        （v2 的 ear_c=6 会让耳中心落在载板坐标 (3.8,3.8)，直径 11mm，
//          与 U7 端子本体 X2.9~13.1 / Y1.8~9.8 大面积相撞）
//   [13] 卡槽起点从外壁改为内腔面；槽深 2.0 → 1.2
//        （v2 切完还剩 0.2mm 薄壁挡着，载板滑不进去）
//   [14] 模块支撑平台 plat_x1: 123 → 119（v2 超出内腔 3mm）
//   [15] 底壳散热孔从贴墙面（Z=0）移到上侧壁（Y 最大侧）
//   [16] 新增电源拨动开关孔 Φ6.2（前盖，(18,60)）
// ------------------------------------------------------------------
// 适配：火眼载板1（105×140）+ GD32F470V-START（60×95）
//       + W5500 模块（55×28，竖插 U9，右缘探出板边 12.4mm）
// 结构：底壳 + 前盖，四角螺丝柱藏在"边仓"内（外观平整），M3 自攻；挂壁式
// 2026-10-15 更新：采用【方案 B · 边仓】——内腔不变，外轮廓四周各加 6mm 边仓，
//   螺丝柱做成 6×6 方形填料嵌在边仓四角，既不外凸、也不占内腔、也不挡载板。
//   （此前 v3 的外凸耳 ear_off=-3 会突出壳外 7.5mm，外观不可接受）
// 单位：mm
// 坐标：内腔底面左下角原点，X 右，Y 上，Z 朝前盖
// 导出：part = "back" / "front" / "both" / "assembly"
// ============================================================

// part 支持命令行覆盖，例如：
//   openscad -D part=\"back\"  -o back.stl  fireeye_enclosure_v3.scad
//   openscad -D part=\"front\" -o front.stl fireeye_enclosure_v3.scad
part = is_undef(part) ? "assembly" : part; // "back" | "front" | "both" | "assembly"

/* ========== 内腔与壁厚 ========== */
cavity_x = 120.0;   // 含右侧 W5500 探出区 13mm
cavity_y = 145.0;
cavity_z = 30.0;
wall     = 2.2;
bay      = 6.0;     // [方案B] 边仓宽度：内腔与外壁之间额外留出的空间，用于放螺丝柱
r_out    = 3.0;     // 外壁圆角
r_in     = 2.0;     // 内腔圆角
cover_t  = 2.2;     // 前盖厚

/* ========== 载板 ========== */
pcb_w = 105.0;
pcb_l = 140.0;
pcb_t = 1.6;
// 载板在内腔中：左下对齐（X 0~105, Y 0~140）

/* ========== 卡槽（四面，载板无安装孔） ========== */
slot_w   = 2.2;   // 槽宽（板 1.6 + 0.6 装配余量）
slot_d   = 1.2;   // [13] 槽深：2.0 → 1.2（壁厚仅 2.2，切 2.0 后外壁只剩 0.2）
slot_zc  = 3.8;   // 槽中心离内腔底面（Z=0）
// 载板底面 Z = slot_zc - slot_w/2 = 2.7，顶面 Z = 4.3

/* ========== W5500 模块区 ========== */
mod_over     = 13.0;  // 内腔右侧为模块多留（探出 12.4 + 0.6）
w5500_x0     = 89.4;  // 模块 x 范围（载板坐标）
w5500_x1     = 117.4;
rj45_x0      = 95.6;  // RJ45 插座 x 范围
rj45_x1      = 111.2;
rj45_y0      = 3.5;   // RJ45 y 范围（距板下边缘）
rj45_y1      = 24.5;

// 右下角开放缺口（禁止封闭方孔）
notch_x0  = 95.0;     // 从 X=95 一直通到外壳右下角
notch_h   = 25.0;     // 从底面（Y=0）往上 25mm

// 模块支撑平台（贴 EVA 泡棉，故意做矮）
plat_x0 = 105.0;
plat_x1 = 119.0;      // [14] 123 → 119（内腔 cavity_x=120，留 1mm 余量）
plat_y0 = 3.0;
plat_y1 = 55.0;
plat_h  = 8.0;        // 顶面 Z=8（做矮 3~4mm，泡棉吸收公差）

/* ========== START 板支撑 ========== */
start_w = 60.0;
start_l = 95.0;
// START 在载板上：X 8.7~68.7, Y 38.3~133.3
start_x0 = 8.7;
start_y0 = 38.3;
post_top_z = 13.1;    // 柱顶高（载板顶面 4.3 + 插合净空 8.5）
post_d     = 8.0;
posts = [ [4,45], [4,125], [72,45], [72,125] ];

/* ========== 螺丝柱（嵌在边仓四角，6×6 方形填料） ========== */
// 关键约束：载板是整块 105×140、占满内腔底面，且从前方滑入卡槽，
//   所以螺丝柱不能伸进内腔（否则挡住载板），也不能悬空或外凸。
// 解法：在"边仓"（内腔与外壁之间的 6mm 空间）的四角做 6×6 实心柱，
//   柱高从底壳底面一直到顶（out_z），前盖螺丝拧进柱顶咬住。
post_fill = bay;     // 方形填料的边长（= 边仓宽）
screw_d = 3.2;       // M3 自攻底孔
step_clr = 0.3;      // 底壳/前盖台阶定位间隙
cout_d  = 6.2;       // 前盖沉头孔直径
cout_dz = 1.0;       // 沉头深度

/* ========== 外轮廓（含壁厚） ========== */
out_x = cavity_x + 2*(wall + bay);   // 120 + 16.4 = 136.4
out_y = cavity_y + 2*(wall + bay);   // 145 + 16.4 = 161.4
out_z = wall + cavity_z;          // 底壳总深（不含前盖）
// 内腔原点在模型坐标
dx = wall + bay; dy = wall + bay; dz = wall;   // 8.2 / 8.2 / 2.2

// 四角螺丝柱中心（模型坐标）= 边仓四角的正中
ear_pos = [
  [wall + bay/2,              wall + bay/2],
  [out_x - wall - bay/2,      wall + bay/2],
  [wall + bay/2,              out_y - wall - bay/2],
  [out_x - wall - bay/2,      out_y - wall - bay/2]
];

/* ========== 开口坐标（载板坐标 = 内腔坐标） ========== */
// 底面（Y=0，接线侧，挂壁朝下）
u7  = [8.0, 5.8];    // 24V 输入端子
u8  = [22.2, 5.6];   // 负载输出端子
dc1 = [61.8, 5.5];   // 12V 电源座
led1 = [68.7, 5.0];  // 电源指示灯
// 缺口 A：U7+U8 合并，X 1~30，高 10，通到底边
// 缺口 B：DC1+LED1 合并，X 56~72，高 10，通到底边
slot_a = [1, 30, 10];   // x0, x1, height
slot_b = [56, 72, 10];

// 左面（X=0）
u10_y = 82.9;        // 调试串口
serial_w = 12.0;     // 沿 Y
serial_h = 6.0;      // 沿 Z

// 前盖（Z=30）
h1   = [38.1, 102.4]; // OLED
oled_w = 22.0;
oled_h = 12.0;
buz  = [91.7, 95.8];  // 蜂鸣器
buz_d = 9.0;
u11  = [89.8, 115.9]; // 报警灯
alarm_d = 7.0;
h2   = [74.7, 59.0];  // NTC 引线
ntc_d = 4.0;

// [16] 电源开关：8.5×8.5 方形按钮开关（自锁型），面板开方孔
sw_x  = 18.0;        // 载板坐标（孔中心）
sw_y  = 60.0;
sw_sq = 8.8;         // 方孔边长（开关头 8.5×8.5，留 0.3 余量便于卡扣入位）
sw_r  = 1.0;         // 孔四角圆角（减应力集中，避免打印开裂）

// 散热：发热区 Y 10~35
vent_d = 4.0;
vents = [ [20, 18], [45, 18], [70, 18], [95, 18] ]; // 4×φ4

/* ========== 挂壁键孔（底壳背面 Z=0） ========== */
kh_head_d = 5.5;
kh_neck_d = 3.2;
kh_span   = 80.0;
kh_top    = 18.0;    // 距内腔顶（Y 最大侧）

/* ============================================================ */
/* 工具模块                                                      */
/* ============================================================ */

// 圆角矩形体（XY 圆角，Z 拉伸）
module rbox(w, l, h, r=2, center_z=false) {
    translate([0, 0, center_z ? -h/2 : 0])
        linear_extrude(h)
            offset(r) offset(-r)
                square([w, l]);
}

/* ============================================================ */
/* 卡槽：四周凹槽，载板滑入                                        */
/* [13] 起点改为内腔面（dx/dy），槽深 slot_d=1.2                    */
/* ============================================================ */
module pcb_card_slots() {
    // 左壁（内腔面 X=dx，往壁内切 slot_d）
    translate([dx - slot_d, dy - 0.5, dz + slot_zc - slot_w/2])
        cube([slot_d + 0.1, pcb_l + 1.0, slot_w]);
    // 下壁（内腔面 Y=dy）
    translate([dx - 0.5, dy - slot_d, dz + slot_zc - slot_w/2])
        cube([pcb_w + 1.0, slot_d + 0.1, slot_w]);
    // 上壁（内腔面 Y=dy+pcb_l）
    translate([dx - 0.5, dy + pcb_l, dz + slot_zc - slot_w/2])
        cube([pcb_w + 1.0, slot_d + 0.1, slot_w]);
    // 右侧（X=dx+pcb_w）：此处内腔无实体壁，不做槽；
    // 载板右侧限位由 w5500_platform() 承担
}

/* ============================================================ */
/* START 支撑柱                                                    */
/* v3 暂不启用（见下方说明）。保留定义以便将来恢复。                 */
/*                                                                */
/* 为什么不用：载板是整块 105×140、占满内腔底面，且从前方滑入卡槽。   */
/*   ① 柱子从内腔底面起(Z=2.2) → 挡住载板滑入路径，装不进去；       */
/*   ② 柱子从载板顶面起(Z=4.3) → 悬空，与底壳不连通，            */
/*      打印出来是"浮在空中的圆柱"，会掉。                        */
/* 结论：START 板暂时只靠 76 针母座的摩擦力固定（插拔力 4~8kg，     */
/*       日常够用；插拔时用手扶住板子即可）。                      */
/* 建议替代：实测 START 板四角的空白区后，改在**前盖内侧**做 4 个    */
/*       小压块压住板边（不占内腔、不悬空）。                       */
/* ============================================================ */
module start_posts() {
    for (p = posts)
        translate([dx + p[0], dy + p[1], dz])
            cylinder(d=post_d, h=post_top_z, $fn=32);
}
use_start_posts = false;   // ← 置 true 可启用（当前会挡住载板，勿开）

/* ============================================================ */
/* W5500 模块支撑平台（右侧，顶面 Z=plat_h）                        */
/* ============================================================ */
module w5500_platform() {
    translate([dx + plat_x0, dy + plat_y0, dz])
        cube([plat_x1 - plat_x0, plat_y1 - plat_y0, plat_h]);
}

/* ============================================================ */
/* 螺丝柱（边仓四角的方形填料，实心到顶）                          */
/* ============================================================ */
module screw_posts(h) {
    for (p = ear_pos)
        translate([p[0] - post_fill/2, p[1] - post_fill/2, 0])
            difference() {
                cube([post_fill, post_fill, h]);
                translate([post_fill/2, post_fill/2, -0.1])
                    cylinder(d=screw_d, h=h + 0.2, $fn=24);
            }
}

/* ============================================================ */
/* 挂壁键孔（底壳背面 = Z=0 外壁）                                  */
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
/* 底壳（back shell）                                              */
/* ============================================================ */
module back_shell() {
    difference() {
        union() {
            // 外体
            rbox(out_x, out_y, out_z, r=r_out);
            // 螺丝柱（嵌在边仓四角，一直做到顶）
            screw_posts(out_z);
        }

        // 掏内腔（前面敞开）
        translate([dx, dy, dz])
            rbox(cavity_x, cavity_y, cavity_z + 0.1, r=r_in);

        // ---- 卡槽 ----
        pcb_card_slots();

        // ---- 底面缺口 A：U7+U8（X 1~30，高 10，通到底边）----
        translate([dx + slot_a[0], -0.1, dz + slot_zc - 1])
            cube([slot_a[1] - slot_a[0], wall + 0.2, slot_a[2]]);

        // ---- 底面缺口 B：DC1+LED1（X 56~72，高 10，通到底边）----
        translate([dx + slot_b[0], -0.1, dz + slot_zc - 1])
            cube([slot_b[1] - slot_b[0], wall + 0.2, slot_b[2]]);

        // ---- 右下角开放缺口（RJ45 + 模块悬空段 + 网线插拔）----
        translate([dx + notch_x0, -0.1, -0.1])
            cube([out_x - (dx + notch_x0) + wall + 1, wall + 0.2, notch_h]);

        // ---- 左面串口槽 ----
        translate([-0.1, dy + u10_y - serial_w/2, dz + slot_zc])
            cube([wall + 0.2, serial_w, serial_h]);

        // ---- [15] 上侧壁散热孔（原开在 Z=0 贴墙面，吸不到风）----
        // v[0]=距左 X，v[1]=距下 Y（此处作为高度 Z 使用）
        // 从外壁之外往内切，确保切穿壁厚（v3 初版起点写在内腔面，没切到壁）
        for (v = vents)
            translate([dx + v[0], out_y + 0.1, dz + v[1]])
                rotate([-90, 0, 0])
                    cylinder(d=vent_d, h=wall + 0.2, $fn=20);

        // ---- 挂壁键孔 ----
        wall_keyholes();
    }

    // 内部结构（在掏腔之后叠加）
    if (use_start_posts) start_posts();
    w5500_platform();

    // 前缘台阶定位（0.4mm 高止口）
    // 外沿外扩 0.2mm，避免与内腔壁面完全重合导致非流形
    translate([dx - 0.2, dy - 0.2, dz + cavity_z])
        difference() {
            rbox(cavity_x + 0.4, cavity_y + 0.4, 0.4, r=r_in);
            translate([step_clr + 0.2, step_clr + 0.2, -0.1])
                rbox(cavity_x - 2*step_clr, cavity_y - 2*step_clr, 0.6, r=r_in);
        }
}

/* ============================================================ */
/* 前盖（front cover）                                             */
/* ============================================================ */
module front_cover() {
    difference() {
        // 盖板
        rbox(out_x, out_y, cover_t, r=r_out);

        // 内侧凸缘（插进底壳内腔，留 step_clr 间隙）
        translate([dx + step_clr, dy + step_clr, 0])
            rbox(cavity_x - 2*step_clr, cavity_y - 2*step_clr, cover_t - 0.4, r=r_in);

        // 螺丝过孔 + 浅沉头（与边仓四角柱对称）
        for (p = ear_pos) {
            translate([p[0], p[1], -0.1])
                cylinder(d=screw_d, h=cover_t + 0.2, $fn=24);
            translate([p[0], p[1], cover_t - cout_dz])
                cylinder(d=cout_d, h=cout_dz + 0.2, $fn=24);
        }

        // ---- OLED 窗 22×12 ----
        translate([dx + h1[0] - oled_w/2, dy + h1[1] - oled_h/2, -0.1])
            cube([oled_w, oled_h, cover_t + 0.2]);

        // ---- 蜂鸣器 φ9 ----
        translate([dx + buz[0], dy + buz[1], -0.1])
            cylinder(d=buz_d, h=cover_t + 0.2, $fn=32);

        // ---- 报警灯 φ7 ----
        translate([dx + u11[0], dy + u11[1], -0.1])
            cylinder(d=alarm_d, h=cover_t + 0.2, $fn=32);

        // ---- NTC 引线 φ4 ----
        translate([dx + h2[0], dy + h2[1], -0.1])
            cylinder(d=ntc_d, h=cover_t + 0.2, $fn=20);

        // ---- [16] 电源开关孔：8.5×8.5 方形按钮开关（带 R1 圆角）----
        translate([dx + sw_x, dy + sw_y, -0.1])
            linear_extrude(cover_t + 0.2)
                offset(r = sw_r) offset(r = -sw_r)
                    square([sw_sq - 2*sw_r, sw_sq - 2*sw_r], center = true);

        // ---- 散热孔（与底壳上侧壁同区域，形成对流）----
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

    // 载板预览（卡槽内）
    color("DarkGreen", 0.5)
        translate([dx, dy, dz + slot_zc - pcb_t/2])
            cube([pcb_w, pcb_l, pcb_t]);

    // START 板预览
    color("Navy", 0.4)
        translate([dx + start_x0, dy + start_y0, dz + post_top_z])
            cube([start_w, start_l, 1.6]);

    // W5500 模块预览（右缘探出板外）
    color("Purple", 0.45)
        translate([dx + w5500_x0, dy + 20, dz + post_top_z - 2])
            cube([w5500_x1 - w5500_x0, 55, 1.6]);
}

/* ========== 渲染时打印关键尺寸（便于核对） ========== */
echo(str("ENCLOSURE 外轮廓=", out_x, " x ", out_y, " x ", out_z,
         "  |  内腔=", cavity_x, " x ", cavity_y, " x ", cavity_z,
         "  |  边仓=", bay, "  壁厚=", wall,
         "  |  载板原点 dx,dy=", dx, ",", dy));

/* ========== 出口 ========== */
if (part == "back") back_shell();
else if (part == "front") front_cover();
else if (part == "assembly") assembly();
else {
    back_shell();
    translate([out_x + 20, 0, 0]) front_cover();
}
