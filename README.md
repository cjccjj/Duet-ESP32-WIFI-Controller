# Duet-ESP32-WIFI-Controller
A simple WIFI Controller to remotely control 3D printer running a Duet control board

This is a draft code that tested working on my spefic ESP32 board and 3D printer set up. 

The controller has a small display, 2 buttons and 6 touchpads.
There are some pictures of it in the Pictures folder.

```
strut_MODE MODE[6] = {
    {0, "Job", "/rr_model?key=job", 2, 0, 0},                           // Job, update on timer
    {1, "Status", "/rr_model?flags=d9fn", 2, 0, 0},                     // Status update on timer
    {2, "Main Control", "/rr_files?dir=0:/macros/Main_Control/", 1, 1, 0}, // Main job control
    {3, "Heater Control", "/rr_files?dir=0:/macros/Heater_Control/", 1, 1, 0},   // Heaters
    {4, "Fan Control", "/rr_files?dir=0:/macros/Fan_Control/", 1, 1, 0},   // Fans
    {5, "Macros", "/rr_files?dir=0:/macros/", 1, 1, 0}};                // macros
```
It has 6 modes/pages to show status from the printer. Press button A and B to toggle through the 6 pages.
Modes 0 and 1 are updated every second by a timer.
Modes 2 to 5 are actually the same, that it loads filelist prepared in Duet SD card, just from different folders.

```
"/rr_gcode?gcode=M98%20P%22" + url + "%22"; // M98 "path and filename"
```
To run a command or gcode, press on one of the six touchpads.
Thanks to Duet firmware which runs everything as gcode or a g file, it is very easy to just call the remote file to run, using the M98 command as above.
And so, it is very easy to add or change a menu item in realtime, just do it online from a PC with DWC.

Arduino libraries used: ArduinoJson, ADAfruit GFX, Adafruit_SH1106_ESP32, ezButton





