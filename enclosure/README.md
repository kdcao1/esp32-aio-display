# AIO tape-on mount

`aio_mount.scad` is a two-part mount for the Waveshare 2.8C round board:

- **cup**: the board drops in screen-up; the flat back flange takes double-sided tape (3M VHB or similar). Side slots for the USB-C cable, the second USB-C port, the microSD card and the display ribbon.
- **ring**: presses over the cup and holds the screen edge. A drop of glue makes it permanent.

The numbers at the top of the file are estimates from a photo. To check the fit:

1. Print `aio_mount_test.stl` (a 3 mm ring, a few minutes). The board should drop in with a little play.
2. Too tight or too loose: change `board_d` in the .scad, re-export, repeat.
3. Check `board_t` (screen front to the tallest part on the back) and the cutout angles, then export `cup` and `ring` (OpenSCAD: set `part`, F6, File > Export > STL).

Print in PETG or ASA if the pump gets warm; no supports needed (cup and ring both print open side up / face down).
