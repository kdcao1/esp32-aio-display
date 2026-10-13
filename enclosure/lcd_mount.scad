// Tweaks lcd_original.stl (the SolidWorks part). Front view: screen side up (+Y),
// ribbon flat toward +Z. Angles: 0 = +X, 90 = -Z (away from the flat).

clearance  = 0.6;       // extra inner diameter over the original 75 mm
tab_extra  = 1.0;       // extra length toward the flat
tab_cut_z  = 78.5;      // where the tab is stretched (straight section)

sd_angle   = 138;
sd_width   = 16;

port_angles = [-134.5, -45.5];  // USB and UART openings next to the flat
port_width  = 11;
port_from_r = 36;       // notch through the back from this radius outward
port_top    = 6.5;      // openings are closed above this (original: 9.5)

flat_d     = 36;      // screen opening is cut flat this far from center toward the flat
flat_skin  = 1;       // thickness of the cover over the PCB at the flat

floor_top = 2;
ring_bottom = 10.5;     // underside of the top ring

center = [39.5, 0, 39.49];
s = (75 + clearance) / 75;

module scaled() {
    translate(center) scale([s, 1, s]) translate(-center) import("lcd_original.stl");
}

module stretched() {
    e = 0.01;
    intersection() { scaled(); translate([-50, -1, -50]) cube([200, 20, tab_cut_z + 50 + e]); }
    translate([0, 0, tab_extra])
        intersection() { scaled(); translate([-50, -1, tab_cut_z - e]) cube([200, 20, 100]); }
    translate([0, 0, tab_cut_z - e]) linear_extrude(tab_extra + 2 * e)
        projection(cut = true) translate([0, 0, -tab_cut_z]) scaled();
}

module radial_cut(angle, width, r0, y0, y1) {
    translate(center) rotate([0, angle, 0])
        translate([r0, y0, -width / 2]) cube([60 - r0, y1 - y0, width]);
}

module port_fill(angle) {
    p = [center[0] + 38.8 * cos(angle), -(center[2] - 38.8 * sin(angle))];
    rotate([-90, 0, 0]) translate([0, 0, port_top]) linear_extrude(9.6 - port_top)
        intersection() {
            hull() intersection() {
                projection(cut = true) translate([0, 0, -5]) rotate([90, 0, 0]) stretched();
                translate(p) square(14, center = true);
            }
            translate([center[0], -center[2]]) difference() { circle(r = 41, $fn = 180); circle(r = 37.9, $fn = 180); }
        }
}

difference() {
    union() {
        stretched();
        for (a = port_angles) port_fill(a);
        translate(center) intersection() {
            translate([0, 12.5 - flat_skin, 0]) rotate([-90, 0, 0]) cylinder(r = 39, h = flat_skin, $fn = 180);
            translate([-50, 0, flat_d]) cube([100, 20, 10]);
        }
    }
    radial_cut(sd_angle, sd_width, 30, floor_top, ring_bottom);
    for (a = port_angles) radial_cut(a, port_width, port_from_r, -1, 3.01);
}
