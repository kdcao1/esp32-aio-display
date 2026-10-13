// Tweaks lcd_original.stl (the SolidWorks part). Front view: screen side up (+Y),
// ribbon flat toward +Z. Angles: 0 = +X, 90 = -Z (away from the flat).

clearance  = 0.6;       // extra inner diameter over the original 75 mm
tab_extra  = 1.0;       // extra length toward the flat
tab_cut_z  = 78.5;      // where the tab is stretched (straight section)

sd_angle   = 138;
sd_width   = 16;

port_gaps   = [[-147, -124], [-56, -33]];  // original openings next to the flat, both closed
port_angle  = -45.5;    // USB opening (right of the flat, front view)
port_width  = 13;
port_top    = 7.5;      // from the back
port_from_r = 36;       // notch through the back from this radius outward
gap_fill_r  = 38.4;     // closed gaps are filled from this radius outward

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

module gap_fill(a0, a1) {
    translate(center) rotate([-90, 0, 0]) linear_extrude(10.6)
        intersection() {
            difference() { circle(r = 39.8, $fn = 180); circle(r = gap_fill_r, $fn = 180); }
            polygon([[0, 0], for (a = [a0 : 2 : a1]) 50 * [cos(a), sin(a)], 50 * [cos(a1), sin(a1)]]);
        }
}

difference() {
    union() {
        stretched();
        for (g = port_gaps) gap_fill(g[0], g[1]);
        translate(center) intersection() {
            translate([0, 12.5 - flat_skin, 0]) rotate([-90, 0, 0]) cylinder(r = 39, h = flat_skin, $fn = 180);
            translate([-50, 0, flat_d]) cube([100, 20, 10]);
        }
    }
    radial_cut(sd_angle, sd_width, 30, floor_top, ring_bottom);
    radial_cut(port_angle, port_width, port_from_r, -1, port_top);
}
