// Tape-on mount for the Waveshare ESP32-S3(-Touch)-LCD-2.8C round display.
// Two parts: a cup the board drops into (flat back for double-sided tape) and a
// front ring that presses over the cup to hold the screen in.
//
// Measure, then edit the numbers below. Print `part = "test"` first: a thin ring
// that checks the board diameter in a few minutes.
//
// Angles are measured looking at the BACK of the board (component side), with
// 0 = right, 90 = top, counter-clockwise positive.

part = "cup";            // "cup", "ring", "test", or "both" (preview)

board_d      = 74.5;     // board/screen outer diameter (widest point, excluding the ribbon tab)
board_t      = 15.0;     // screen glass to the tallest part on the back (USB plug excluded)
fit          = 0.6;      // extra diameter so the board drops in

wall         = 2.4;      // side wall thickness
back_t       = 2.0;      // back flange thickness
flange_w     = 11;       // width of the back flange (tape area); the center stays open
lip_w        = 2.2;      // how far the front ring overlaps the screen edge
lip_t        = 1.6;      // front ring face thickness
ring_skirt   = 5;        // how far the front ring slides over the cup
ring_fit     = 0.3;      // gap between ring and cup (raise if too tight)

// Cutouts in the side wall: [angle, width, height from back plate]
usb_cut      = [-45, 13, 9];   // USB-C cable (port labelled USB)
uart_cut     = [45, 13, 9];    // second USB-C (UART); set width to 0 to leave closed
sd_cut       = [140, 16, 5];   // microSD slot
ribbon_cut   = [0, 22, 99];    // display ribbon that wraps round the edge (full height)

$fn = 160;

inner_d = board_d + fit;
outer_d = inner_d + 2 * wall;
cup_h = back_t + board_t;

module wall_cut(c) {
    angle = c[0]; width = c[1]; height = c[2];
    if (width > 0)
        rotate([0, 0, angle])
            translate([inner_d / 2 - 1, -width / 2, back_t])
                cube([wall + 2, width, min(height, cup_h)]);
}

module cup() {
    mirror([1, 0, 0])
    difference() {
        cylinder(d = outer_d, h = cup_h);
        translate([0, 0, back_t]) cylinder(d = inner_d, h = cup_h);
        translate([0, 0, -1]) cylinder(d = inner_d - 2 * flange_w, h = back_t + 2);
        for (c = [usb_cut, uart_cut, sd_cut, ribbon_cut]) wall_cut(c);
    }
}

module ring() {
    difference() {
        cylinder(d = outer_d + 2 * ring_fit + 2 * wall, h = lip_t + ring_skirt);
        translate([0, 0, lip_t]) cylinder(d = outer_d + 2 * ring_fit, h = ring_skirt + 1);
        translate([0, 0, -1]) cylinder(d = board_d - 2 * lip_w, h = lip_t + 2);
    }
}

module test_ring() {
    difference() {
        cylinder(d = outer_d, h = 3);
        translate([0, 0, -1]) cylinder(d = inner_d, h = 5);
    }
}

if (part == "cup") cup();
else if (part == "ring") ring();
else if (part == "test") test_ring();
else {
    cup();
    translate([outer_d + 15, 0, 0]) ring();
}
