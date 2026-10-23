// ============================================================
// 火眼载板外壳 · 参数化 OpenSCAD  v4 (2026-10-15)
// 需求源：火眼载板_外壳重做任务书.md（以此为准）
// 基线：fireeye_enclosure_v3.scad（方案 B 边仓 + 方形螺丝柱）
// v3 → v4 变更（按任务书）：
//   [A] 内腔高度 30 → 40（2026-10-15 复核：50 过高，RJ45 顶 33.4 + 余量）
//   [B] OLED 改 0.96"（27×27，显示区 21.744×10.864）
//       窗口 23.5×12.5 @ (38.1,102.4)；模块热熔胶嵌前盖内侧，4 线飞回 H1
//   [C] 电源开关 8.8×8.8 方孔 + R1 @ (18,60)（沿用 v3 [16]）
//   [D] 去掉 START 支撑柱（任务书 2.4：只靠 76 针母座摩擦力）
//   [E] 导出名 bottom_shell.stl / top_shell.stl
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
cavity_z = 40.0;    // [A] 30 → 50 → 40（2026-10-15 复核：RJ45 顶 33.4 + 止口 2.5 + 余量 4，50 过高）
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

// RJ45 窗口 [I] 按实测收口：接口 15×13，模块居中（两边各 6mm）
//   模块 x 89.4~117.4 → 接口 x 95.4~110.4（宽 15，中心 102.9）
//   模块顶 Z=20.4（腔内 18.2）→ 接口高 13 → Z 20.4~33.4
//   加 1mm 间隙 → 封闭方窗 17×15 @ x 94.4~111.4、Z 19.4~34.4
//   右侧到内腔壁还有 ~9mm 实体，可做封闭窗（不再要开放缺口）
rj45_cx  = 102.9;
rj45_w   = 15.0;
rj45_h   = 13.0;
rj45_clr = 1.0;      // 四周间隙
rj45_z0  = 19.4;     // 模块顶 20.4 - 1

// 模块支撑平台（贴 EVA 泡棉，故意做矮）
plat_x0 = 105.0;
plat_x1 = 119.0;      // [14] 123 → 119（内腔 cavity_x=120，留 1mm 余量）
plat_y0 = 3.0;
plat_y1 = 55.0;
plat_h  = 12.6;       // [F] 8.0 → 12.6：实测 h=12（模块底距板顶），顶 Z = 4.6+h-4

// [H] W5500 模块自带 2×M3 安装孔，锁在平台上（比纯泡棉可靠）
// 孔位（载板坐标）：距排针 44.5mm、距模块 28mm 长边各 1mm
//   模块 x 89.4~117.4 → 孔 x = 90.4 / 116.4；y = 53.5 - 44.5 = 9.0
//   两孔夹住 RJ45（x 95.6~111.2），可扛网线插拔力
mod_holes   = [ [90.4, 9.0], [116.4, 9.0] ];
mod_boss_d  = 7.0;    // 矮柱外径
mod_screw_d = 2.5;    // M3 自攻底孔 Φ2.5（模块 Φ3 是穿丝孔，M3 螺纹外径 3.0 咬合）
// 备注：M3 穿模块 Φ3 孔偏紧（螺纹外径也是 3.0），对准稍用力；太紧可换 M2.5 螺丝
// 矮柱高度：平台顶 12.6 → 模块底 16.6（腔内）= 4mm；不需要全高
mod_boss_h  = 4.0;

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
// 止口凸缘尺寸 [M]（插进底壳内腔）
lip_h = 2.5;    // 凸缘高（向 -Z 伸出）
lip_t = 1.5;    // 凸缘壁厚

/* ========== 外轮廓（含壁厚） ========== */
out_x = cavity_x + 2*(wall + bay);   // 120 + 16.4 = 136.4
out_y = cavity_y + 2*(wall + bay);   // 145 + 16.4 = 161.4
out_z = wall + cavity_z;          // 底壳总深（不含前盖）
// 内腔原点在模型坐标
dx = wall + bay; dy = wall + bay; dz = wall;   // 8.2 / 8.2 / 2.2
// [L] 侧壁开孔深度必须切穿 wall+bay=8.2（只切 wall=2.2 会成盲孔）
wall_cut = wall + bay + 0.2;      // 从壁外-0.1 切到内腔面+0.1

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

// 前盖（Z=out_z 朝外）
h1   = [38.1, 102.4]; // OLED 中心（载板坐标）
oled_w = 23.7;        // [N] 窗 = AA 21.74 + 两侧各 1mm
oled_h = 12.9;        //     AA 10.86 + 两侧各 1mm
// [N] OLED 内侧定位台（按图纸实测 27.30×27.80，热熔胶/螺丝嵌装）
// 图纸：PCBA 27.30×27.80，AA 21.74×10.86，PANEL 26.70
//       4×φ2.00 安装孔（φ3.50 沉头），孔距 23.30×23.80
oled_mod_x = 27.30;   // PCBA 宽
oled_mod_y = 27.80;   // PCBA 高
oled_aa_w  = 21.74;   // 可视区宽
oled_aa_h  = 10.86;   // 可视区高
oled_clr   = 0.4;     // 装配间隙（每边）
oled_wall  = 1.5;     // 定位台壁厚
oled_seat_h = 1.5;    // 定位台高
// 安装孔（相对 PCBA 左下角）：X 6.65/20.65，Y 2.00/25.80
oled_mholes = [ [6.65, 2.00], [20.65, 2.00], [6.65, 25.80], [20.65, 25.80] ];
oled_mhole_d = 1.6;   // M2 自攻底孔（模块孔 φ2.00 穿丝）
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

// 散热：发热区 Y 10~35（MP1584/继电器/分流电阻）
// [J] 4×φ4 → 2×5 网格（前盖 + 上侧壁对流），左壁再补 2 个
vent_d = 4.0;
vents = [ for (v = [18, 30], u = [15, 35, 55, 75, 95]) [u, v] ]; // 10×φ4
vents_left = [ [18, 20], [30, 20] ]; // 左壁（X=0），Y 热区、Z 高于板

// [K] 前盖 logo 文字「火眼」（阴刻，避开所有开孔的空位）
logo_txt  = "火眼";
logo_x    = 55.0;     // 载板坐标
logo_y    = 80.0;
logo_size = 12.0;
logo_depth = 0.6;     // 阴刻深度
logo_font = "Microsoft YaHei";

// [O] 底壁端子标识（缺口 A 上方外壁，24V 输入 / 负载输出）
// U7/U8 中心距仅 14.2mm，字要小才分得开
lbl_depth = 0.5;
lbl_size  = 2.8;
lbl_z     = 18.0;     // 缺口 A 顶 Z=15 之上，靠前盖侧（挂壁后朝外可见）
lbl_u7    = "24V";
lbl_u8    = "OUT";

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
/* W5500 模块 M3 锁固矮柱 [H]                                     */
/* 只立在平台超出板边那块（X 105~119），不做桥臂到板上方；         */
/* 右孔 (116.4, 9.0) 正好落在平台内，4mm 矮柱 + M3 底孔           */
/* ============================================================ */
module w5500_mount_bosses() {
    for (h = mod_holes) {
        if (h[0] >= plat_x0) {
            translate([dx + h[0], dy + h[1], dz + plat_h])
                difference() {
                    cylinder(d=mod_boss_d, h=mod_boss_h, $fn=24);
                    translate([0, 0, -0.1])
                        cylinder(d=mod_screw_d, h=mod_boss_h + 0.2, $fn=16);
                }
        }
        // 左孔 x=90.4 在载板正上方，不能立柱（挡板滑入）；只用右孔 + 平台支撑
    }
}

/* ============================================================ */
/* 螺丝柱（边仓四角的方形填料）                                    */
/* 注意：孔必须在 back_shell 的 difference() 里挖；                  */
/*       若在本模块内部 difference()，外体 rbox 会把孔填回           */
/* ============================================================ */
module screw_posts(h) {
    for (p = ear_pos)
        translate([p[0] - post_fill/2, p[1] - post_fill/2, 0])
            cube([post_fill, post_fill, h]);
}

// 螺丝通孔（在 back_shell 的 difference 里调用）
module screw_holes(h) {
    for (p = ear_pos)
        translate([p[0], p[1], -0.1])
            cylinder(d=screw_d, h=h + 0.2, $fn=24);
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

        // ---- 底面缺口 A：U7+U8（X 1~30，高 10，通到底边）[L] 切穿 wall+bay ----
        translate([dx + slot_a[0], -0.1, dz + slot_zc - 1])
            cube([slot_a[1] - slot_a[0], wall_cut, slot_a[2]]);

        // ---- 底面缺口 B：DC1+LED1（X 56~72，高 10，通到底边）----
        translate([dx + slot_b[0], -0.1, dz + slot_zc - 1])
            cube([slot_b[1] - slot_b[0], wall_cut, slot_b[2]]);

        // ---- RJ45 封闭方窗（17×15，对准接口 15×13）[I] ----
        translate([dx + rj45_cx - (rj45_w + 2*rj45_clr)/2, -0.1, rj45_z0])
            cube([rj45_w + 2*rj45_clr, wall_cut, rj45_h + 2*rj45_clr]);

        // ---- 左面串口槽 ----
        translate([-0.1, dy + u10_y - serial_w/2, dz + slot_zc])
            cube([wall_cut, serial_w, serial_h]);

        // ---- [15] 上侧壁散热孔 ----
        for (v = vents)
            translate([dx + v[0], out_y + 0.1, dz + v[1]])
                rotate([-90, 0, 0])
                    cylinder(d=vent_d, h=wall_cut, $fn=20);

        // ---- [J] 左壁散热孔（发热区 Y 10~35，高于板面）----
        for (v = vents_left)
            translate([-0.1, dy + v[0], dz + v[1]])
                rotate([0, 90, 0])
                    cylinder(d=vent_d, h=wall_cut, $fn=20);

        // ---- [O] 底壁端子标识（缺口 A 上方外壁，阴刻）----
        // 墙外侧面 Y=0，文字沿 X 可读、Z 向上；rotate([90,0,0]) 使字面朝 -Y
        translate([dx + u7[0], 0.6, dz + lbl_z])
            rotate([90, 0, 0])
                linear_extrude(lbl_depth + 0.1)
                    text(lbl_u7, size=lbl_size, font=logo_font,
                         halign="center", valign="center");
        translate([dx + u8[0], 0.6, dz + lbl_z])
            rotate([90, 0, 0])
                linear_extrude(lbl_depth + 0.1)
                    text(lbl_u8, size=lbl_size, font=logo_font,
                         halign="center", valign="center");

        // ---- 螺丝通孔（四角，从前盖穿到底）----
        screw_holes(out_z);

        // ---- 挂壁键孔 ----
        wall_keyholes();
    }

    // 内部结构（在掏腔之后叠加）
    if (use_start_posts) start_posts();
    w5500_platform();
    w5500_mount_bosses();   // [H] 模块 M3 锁固矮柱

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
        union() {
            // 盖板（local Z=0 朝内腔，Z=cover_t 朝外）
            rbox(out_x, out_y, cover_t, r=r_out);

            // [M] 止口凸缘：向 -Z 伸出，插进底壳内腔
            //   外廓比内腔小 2×(step_clr+0.2)，与底壳止口环内缘留 0.2 间隙
            translate([dx + step_clr + 0.2, dy + step_clr + 0.2, -lip_h])
                difference() {
                    rbox(cavity_x - 2*(step_clr + 0.2),
                         cavity_y - 2*(step_clr + 0.2), lip_h + 0.1, r=r_in);
                    translate([lip_t, lip_t, -0.1])
                        rbox(cavity_x - 2*(step_clr + 0.2) - 2*lip_t,
                             cavity_y - 2*(step_clr + 0.2) - 2*lip_t,
                             lip_h + 0.3, r=r_in);
                }

            // [N] OLED 定位台（模块 27×27 嵌在台圈内）
            oled_seat();
        }

        // 螺丝过孔 + 浅沉头（与边仓四角柱对称）
        for (p = ear_pos) {
            translate([p[0], p[1], -0.1])
                cylinder(d=screw_d, h=cover_t + 0.2, $fn=24);
            translate([p[0], p[1], cover_t - cout_dz])
                cylinder(d=cout_d, h=cout_dz + 0.2, $fn=24);
        }

        // ---- OLED 窗 23.5×12.5（0.96" 显示区 + 余量）[B] ----
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

        // ---- [K] logo 文字「火眼」阴刻在外侧面 ----
        translate([dx + logo_x, dy + logo_y, cover_t - logo_depth])
            linear_extrude(logo_depth + 0.1)
                text(logo_txt, size=logo_size, font=logo_font,
                     halign="center", valign="center");
    }
}

/* ============================================================ */
/* OLED 内侧定位台 + M2 螺丝柱 [N]                                */
/* 图纸：PCBA 27.30×27.80，4×φ2.00 安装孔，孔距 23.30×23.80       */
/* 台圈限位 + 4 根 M2 矮柱（φ1.6 底孔），热熔胶辅助               */
/* ============================================================ */
module oled_seat() {
    oled_outer_x = oled_mod_x + 2*oled_clr + 2*oled_wall;
    oled_outer_y = oled_mod_y + 2*oled_clr + 2*oled_wall;
    oled_inner_x = oled_mod_x + 2*oled_clr;
    oled_inner_y = oled_mod_y + 2*oled_clr;
    ox = dx + h1[0] - oled_outer_x/2;
    oy = dy + h1[1] - oled_outer_y/2;
    // 台圈（PCBA 嵌在里面，贴盖面）
    translate([ox, oy, -oled_seat_h])
        difference() {
            rbox(oled_outer_x, oled_outer_y, oled_seat_h + 0.1, r=1);
            translate([oled_wall, oled_wall, -0.1])
                rbox(oled_inner_x, oled_inner_y, oled_seat_h + 0.3, r=1);
        }
    // 4 根 M2 螺丝柱（对准模块安装孔）
    // 模块左下角 = 台圈内孔左下角 + clr
    mx = ox + oled_wall + oled_clr;
    my = oy + oled_wall + oled_clr;
    for (p = oled_mholes)
        translate([mx + p[0], my + p[1], -oled_seat_h])
            difference() {
                cylinder(d=4.0, h=oled_seat_h + 0.1, $fn=20);
                translate([0, 0, -0.1])
                    cylinder(d=oled_mhole_d, h=oled_seat_h + 0.3, $fn=12);
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
