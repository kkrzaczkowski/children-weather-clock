# Technical specification

Children's weather clock firmware for the Waveshare **ESP32-S3-Touch-LCD-7** (touch version). Hardware facts below come from the [Waveshare product page](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7) and the linked [user guide](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use). Pin use in this repository is taken from `components/waveshare_rgb_lcd_port.h` and `main/main.c`.

## Project

| Item | Value |
| --- | --- |
| ESP-IDF project name | `children_weather_clock` |
| Location | Workspace root (`CMakeLists.txt`, `main/`, `components/`) |
| Target | ESP32-S3 |
| Framework | ESP-IDF 5.5.6 (`C:\esp\v5.5.6\esp-idf`) |
| Flash / monitor port in VS Code | COM6, UART |
| Reference sample | `sample-project-09_lvgl_v9_demo/` (not part of this build) |
| Current screen | Centered label `Hello world`, Montserrat 24, rotation 0 |

`app_main` initializes the RGB panel and GT911 touch, turns the backlight on through the CH422G, starts `esp_lv_adapter` with the task stack in PSRAM, registers the 800×480 display and the touch input, then creates the label on the active screen inside the LVGL lock.

## Software libraries

Locked in `dependencies.lock`. Declared ranges are in `main/idf_component.yml`.

| Component | Declared | Locked |
| --- | --- | --- |
| ESP-IDF | `>=5.1.0` | 5.5.6 |
| `lvgl/lvgl` | `^9` | 9.5.0 |
| `espressif/esp_lvgl_adapter` | `^0.5.2` | 0.5.3 |
| `espressif/esp_lcd_touch_gt911` | `^1` | 1.2.1 |
| `espressif/esp_lcd_touch` | transitive | 1.2.1 |

LVGL is built with 16-bit color, FreeRTOS OS layer, refresh period 15 ms, two software draw units, and Montserrat 12, 16, 20, and 24. The widgets demo is turned off.

Build defaults in `sdkconfig.defaults`:

- CPU 240 MHz
- Flash QIO, 80 MHz, **image size set to 8 MB**
- Octal PSRAM, 80 MHz, XIP from PSRAM, rodata in PSRAM, memory test off
- FreeRTOS tick 1000 Hz
- Instruction cache 32 KB, data cache 64 KB, data cache line 64 bytes
- RGB tear-avoid enabled, LVGL port task on core 1

The module itself is an ESP32-S3N16R8 with **16 MB** flash. The 8 MB setting is the current project image size, not the capacity of the chip.

Display timings used by this firmware (`waveshare_rgb_lcd_port.h`):

- 800×480, RGB565, 16 data bits
- Pixel clock 16 MHz
- Horizontal: pulse 4, back porch 8, front porch 8, pixel clock active on the negative edge
- Vertical: pulse 4, back porch 8, front porch 8
- Frame buffers in PSRAM, bounce buffer of 10 lines
- Rotation 0, tear-avoid mode `ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_RGB`

## Board

This unit is the capacitive-touch variant.

| Item | Value |
| --- | --- |
| Product | ESP32-S3-Touch-LCD-7 |
| SKU | 27078 (touch). SKU 30406 is the same board family without touch. |
| Module | ESP32-S3N16R8 |
| Processor | Xtensa 32-bit LX7, dual core, up to 240 MHz |
| Wireless | 2.4 GHz Wi-Fi 802.11 b/g/n and Bluetooth 5 (LE), onboard antenna |
| On-chip memory | 512 KB SRAM, 384 KB ROM |
| Flash | 16 MB, stacked in the module |
| PSRAM | 8 MB, stacked in the module |
| Power input | USB Type-C, 5 V |
| Typical consumption | 5 V, 450 mA |
| Operating temperature | 0 °C to 65 °C |
| Outline, touch version | 192.96 × 110.76 mm |
| Outline, non-touch version | 164 × 97 mm |

### Display and touch

| Item | Value |
| --- | --- |
| Size | 7 inch |
| Panel | IPS |
| Resolution | 800 × 480 |
| Colors | 65K (RGB565) |
| Interface | RGB parallel |
| Viewing angle | 170° |
| Brightness | 345 cd/m² |
| Touch | Capacitive, 5-point, interrupt capable, tempered glass |
| Touch controller | GT911 (driver `esp_lcd_touch_gt911`; datasheet listed in the product resources) |
| Panel driver datasheet listed by Waveshare | ST7262 |
| IO expander | CH422G on I²C, used for backlight, touch reset, TF-card chip select, and USB/CAN select |

## Onboard resources

Numbering matches the product page.

1. ESP32-S3N16R8 module, 240 MHz, 16 MB flash, 8 MB PSRAM
2. 7 inch display panel connector
3. Touch panel connector
4. TF card slot
5. USB Type-C port (power and native USB)
6. USB-to-UART Type-C port (UART1)
7. UART2 header. UART1 and UART2 share one UART; switch 15 selects which one is connected
8. Sensor header, HY2.0 3-pin
9. CAN header, HY2.0 2-pin
10. I²C header, HY2.0 4-pin
11. RS485 header, HY2.0 2-pin
12. Single-cell 3.7 V lithium battery header, PH2.0
13. CAN 120 Ω termination jumper
14. RS485 120 Ω termination jumper
15. UART1 / UART2 select switch
16. BOOT button. Hold it to enter download mode
17. RESET button
18. I²C level select, 3.3 V or 5 V
19. DONE indicator, lithium charge complete
20. CHG indicator, lithium charging
21. PWR indicator

The [user guide](https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use) calls the ADC, CAN, I²C, and RS485 headers PH2.0 and says to use PH2.0 to 2.54 mm Dupont cables. The product page and FAQ call those same cables HY2.0. The battery connector is PH2.0 on both pages.

## Interfaces

GPIO numbers are the ESP32-S3 pins. EXIO pins are outputs of the CH422G, not ESP32 GPIOs.

### LCD (RGB)

| ESP32-S3 or expander | LCD signal | Meaning |
| --- | --- | --- |
| GPIO0 | G3 | Green bit 3 |
| GPIO1 | R3 | Red bit 3 |
| GPIO2 | R4 | Red bit 4 |
| GPIO3 | VSYNC | Vertical sync |
| GPIO5 | DE | Data enable |
| GPIO7 | PCLK | Pixel clock |
| GPIO10 | B7 | Blue bit 7 |
| GPIO14 | B3 | Blue bit 3 |
| GPIO17 | B6 | Blue bit 6 |
| GPIO18 | B5 | Blue bit 5 |
| GPIO21 | G7 | Green bit 7 |
| GPIO38 | B4 | Blue bit 4 |
| GPIO39 | G2 | Green bit 2 |
| GPIO40 | R7 | Red bit 7 |
| GPIO41 | R6 | Red bit 6 |
| GPIO42 | R5 | Red bit 5 |
| GPIO45 | G4 | Green bit 4 |
| GPIO46 | HSYNC | Horizontal sync |
| GPIO47 | G6 | Green bit 6 |
| GPIO48 | G5 | Green bit 5 |
| CH422G EXIO2 | DISP | Backlight enable |

This firmware turns the backlight on by putting the CH422G into output mode (write `0x01` to I²C address `0x24`) and then writing `0x1E` to address `0x38`.

### Touch (this unit)

| ESP32-S3 or expander | Touch signal | Meaning |
| --- | --- | --- |
| GPIO4 | TP_IRQ | Touch interrupt |
| GPIO8 | TP_SDA | I²C data, shared bus |
| GPIO9 | TP_SCL | I²C clock, shared bus |
| CH422G EXIO1 | TP_RST | Touch reset |

The firmware installs the legacy I²C master on port 0 at 400 kHz, pulses the touch controller through EXIO1 and GPIO4, then creates a GT911 device over that bus. Touch is registered with LVGL only when that handle is non-NULL. The GT911 interrupt and reset GPIOs passed to the touch driver are `-1`, because reset is done through the expander before the driver starts.

### I²C

GPIO8 is SDA and GPIO9 is SCL. The same bus serves the CH422G, the GT911, and the external I²C header. A switch selects 3.3 V or 5 V for the header.

Occupied 7-bit addresses, from the user guide. Do not attach another device on these addresses:

- `0x20`–`0x27` and `0x30`–`0x3F`: CH422G
- `0x5D`: GT911

This firmware talks to the expander at `0x24` and `0x38`.

### USB and CAN (shared pins)

GPIO19 and GPIO20 are shared. CH422G EXIO5 selects the function: low is USB, high is CAN. The user guide and FAQ say the pin defaults high, so the native USB port stays in CAN mode until software pulls EXIO5 low. If the computer does not see a USB COM port, hold BOOT, connect the USB cable, then release BOOT.

| Function | ESP32-S3 | Signal |
| --- | --- | --- |
| USB | GPIO19 | D− |
| USB | GPIO20 | D+ |
| CAN | GPIO19 | CAN RX |
| CAN | GPIO20 | CAN TX |

CAN uses a TJA1051 transceiver. A jumper connects a 120 Ω termination resistor by default; set it to NC to disconnect the resistor. Programming and logs for this project use the UART Type-C port, not this native USB port.

### UART

Both connectors use ESP32-S3 UART0: GPIO43 TX and GPIO44 RX. The DIP switch selects one of them.

- UART1: Type-C USB-to-UART through a CH343, with the automatic download circuit. This is the port used to flash and print logs. Press RESET after flashing to run the program.
- UART2: logic-level header on the same UART0 pins.

The FAQ specifies an HY2.0 4-pin cable for the UART header.

### RS485

Automatic transmit/receive control. A jumper connects a 120 Ω termination resistor by default.

| ESP32-S3 | Signal |
| --- | --- |
| GPIO15 | RS485 TX |
| GPIO16 | RS485 RX |

### TF card

SPI, and the user guide also notes MMC is supported. Chip select is not an ESP32 GPIO.

| ESP32-S3 or expander | Signal |
| --- | --- |
| GPIO11 | MOSI |
| GPIO12 | SCK |
| GPIO13 | MISO |
| CH422G EXIO4 | SD_CS, active low |

### Sensor (ADC)

HY2.0 3-pin header: 3V3, AD, GND. AD is GPIO6 (ADC).

### Battery

PH2.0 header for one 3.7 V lithium cell. Do not connect more than one cell or a multi-cell pack. Waveshare recommends a capacity of 2000 mAh or less. CHG lights while charging. DONE lights when charging is finished.

### Buttons

- BOOT: hold while connecting USB power to enter the download bootloader when the port is not detected.
- RESET: restart the chip. Press it after a UART download so the new program runs.

## Sources

- Product page, specifications, onboard resources, and interface tables: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7
- User guide (download port, battery limit, I²C address map, termination jumpers): https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use
- FAQ (UART1 versus UART2, USB/CAN select, header cables): https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/FAQ
- Sensor header is GPIO6: https://docs.waveshare.com/docs/ESP32/ESP32-S3/ESP32-S3-Touch-LCD-7/Arduino/Arduino-Sensor-AD-Demo
- Chip datasheets listed by Waveshare (ESP32-S3, ST7262, GT911, CH343, TJA1051, CH422G): https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Resources-And-Documents
