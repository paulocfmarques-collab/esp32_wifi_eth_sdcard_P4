# Sonar on the OLED

Firmware: **v1.25.3**. The SSD1306 sonar view is page **8 of 8**, immediately after the camera. Press the GPIO27 button to reach it, send `tela:7`, or use the Sonar on OLED action in the web terminal or Home Assistant. The next button press returns to the clock.

The sonar and camera pages do not switch automatically or enter the idle screensaver. OTA can show progress instead. Opening the sonar does not start the servo; send `servo:on` to sweep.

| Display element | Meaning |
| --- | --- |
| Four semicircular rings | 1 m, 2 m, 3 m, and 4 m |
| Sweep line | Current commanded servo direction while the motor is enabled |
| Echo dots | Valid distance and acquisition angle, grouped in 35 sectors of 5° |
| Circled echo | Confirmed target under tracking |
| Footer | Current commanded angle and latest valid distance |
| `S/ECO` / `OFF` | No recent valid echo / sonar disabled |

New echoes appear as larger dots for about 1.5 seconds. Each sector keeps its latest echo for up to six seconds. The OLED redraw is limited to one update per 150 ms. The distance in the footer and the sweep line can describe different instants; the dots use their own acquisition angles.

To check the view, confirm the firmware with `version`, open `tela:7`, and start a sweep. Check that the line follows the commanded position and that real echoes appear. Wait over a minute to verify the page stays visible, then press the button to return to the clock. See [radar tracking](RADAR_TRACKING.md) for CSV diagnostics.
