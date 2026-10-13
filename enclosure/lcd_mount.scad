// Tweaks lcd_original.stl (the SolidWorks part): a little more room for the board
// and a slot for the microSD card. Front view: screen side up, ribbon flat toward +Z.

clearance = 0.6;        // extra inner diameter over the original 75 mm
sd_angle  = 138;        // front view, 0 = +X, 90 = -Z (away from the flat)
sd_width  = 16;
sd_bottom = 2;          // floor top
sd_top    = 10.5;       // underside of the top ring

center = [39.5, 0, 39.49];
s = (75 + clearance) / 75;

difference() {
    translate(center) scale([s, 1, s]) translate(-center) import("lcd_original.stl");
    translate(center) rotate([0, sd_angle, 0])
        translate([30, sd_bottom, -sd_width / 2]) cube([20, sd_top - sd_bottom, sd_width]);
}
