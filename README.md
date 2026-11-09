# VIXULY-Source-code

This is the Firmware for the final term project of ME4169 - Microprocessor's class of HUST

## Overview

**VI XU LY** is a closed-loop DC motor speed controller built around an **ATmega88PA** running at 16 MHz. The motor speed is measured by a quadrature encoder and regulated by a fixed-point PID controller. The target speed can be set either by a **potentiometer** or by commands sent over **UART**. A small **WeAct ST7735 TFT module** shows the current mode, the system state, the target speed and the measured speed (RPM).

The firmware is written as bare-metal AVR code (direct register access, hand-written ISRs). The Arduino framework is only used for `millis()` and the `setup()`/`loop()` entry points, under PlatformIO.

## Main Components

### Hardware

| Part | Function |
|------|----------|
| ATmega88PA-A (TQFP-32) + 16 MHz crystal | Main MCU |
| DRV8220 | Brushed DC motor driver (H-bridge) |
| Quadrature encoder | Speed feedback (x4 decoding) |
| [WeAct ST7735 TFT module](https://github.com/WeActStudio/WeAct-ST7735) (128x160) | Status display, SPI |
| TXB0106 | Level shifter between the 5 V MCU and the 3.3 V TFT |
| Potentiometer (RV1) | Manual speed setpoint |
| 2 push buttons + 2 LEDs | RUN / STOP user interface |
| LMR51420 buck + AP2112K-3.3 LDO | 12 V input to 5 V and 3.3 V rails |
| PFET + Zener input stage | Reverse polarity protection |
### Pin Map

| Signal | Pin | Notes |
|--------|-----|-------|
| TFT RST / BL / CS | PB0 / PB1 / PB2 | |
| TFT MOSI / SCK | PB3 / PB5 | Hardware SPI |
| TFT DC | PD7 | |
| Encoder A / B | PD2 / PD3 | INT0 / INT1 |
| Motor nSLEEP | PD4 | |
| Motor IN1/PH, IN2/EN | PD5 / PD6 | Software PWM on Timer2 |
| UART RX | PD0 | RX only |
| Potentiometer | PC0 | ADC0 |
| RUN button / RUN LED | PC1 / PC2 | |
| STOP button / STOP LED | PC3 / PC4 | |

> **JP1** on the board selects the DRV8220 control mode. Keep it **open** for PWM (IN1/IN2) mode (default). If it is soldered, enable `MOTOR_MODE_PH_EN` in `config.h`.

### Software Modules

| File | Description |
|------|-------------|
| `main.cpp` | Entry point: only calls `app_init()` and `app_update()` |
| `app.cpp / app.h` | Application layer: state machine, mode selection, LEDs, display updates (implements `VIXULY_Logic.txt`) |
| `config.h` | **Single place for every tunable value**: pins, TFT, ADC, encoder, motor, PID gains, button timing, UI layout |
| `motion.c / motion.h` | Fixed-point (Q8) PID velocity controller with feed-forward, setpoint ramp, anti-windup and filtered D-term |
| `motor.c / motor.h` | DRV8220 driver, software PWM on Timer2 ISRs (Timer0 / `millis()` untouched) |
| `encoder_mini.cpp / .h` | x4 quadrature decoder, optional STM32-style digital input filter (Timer1), velocity and RPM calculation |
| `button.c / button.h` | Debounced button with short-press / long-press detection |
| `uart.c / uart.h` | Interrupt-driven UART receiver with ring buffer and ASCII integer parser |
| `adc.c / adc.h` | Minimal 10-bit ADC driver |
| `st7735_mini.c / .h` | Lightweight text-only driver for the WeAct ST7735 module (small flash footprint) |

## Operation Logic

The system has three user-visible states: **STOP**, **RUN** and **SELECT** (mode selection). There are two operation modes:

- **POTENTIOMETER**: the speed setpoint comes from the pot on PC0
- **UART**: the speed setpoint comes from serial commands

| Button | Current state | Action | Result |
|--------|---------------|--------|--------|
| STOP | any | Press | Stop the system. The motor driver is put to sleep (PD4 low), the PID is reset, and any unfinished mode selection is cancelled |
| RUN | STOP | Short press | Start running in the current mode |
| RUN | STOP | Hold ~2 s | Enter mode selection |
| RUN | SELECT | Short press | Swap between POTENTIOMETER and UART |
| RUN | SELECT | Hold ~2 s | Confirm the shown mode and return to STOP |
| RUN | RUN | any | Ignored |

**LEDs**

- RUN LED (PC2) is on only while the system is running
- STOP LED (PC4) is on while stopped or selecting

The global variable `running` is `1` while the system is running and `0` otherwise.

## Quick User Guide

### 1. Build and flash

The project uses [PlatformIO](https://platformio.org/).

1. Clone the repository:
   ```bash
   git clone https://github.com/<your-username>/VIXULY-Source-code.git
   ```
2. Open the `Department of Software Engineering` folder in VS Code with the PlatformIO extension.
3. Review `src/config.h`, especially the items in the **Configuration** section below.
4. Build and upload with your ISP programmer, using the debug/programming header on the board.

### 2. Power up

1. Connect a **12 V DC** supply to the input terminal (observe polarity).
2. Connect the motor and the encoder.
3. After power-up the system starts in **STOP** (red "STOP" on the screen, STOP LED on).

### 3. Select a mode

1. In STOP, hold the **RUN** button for about 2 s. The screen shows **SELECT** in orange.
2. Short-press **RUN** to toggle between `Mode: Potentiometer` and `Mode: UART`.
3. Hold **RUN** for about 2 s again to confirm. The system returns to STOP with the chosen mode.

### 4. Run

Short-press **RUN**. The screen shows green **RUNNING**, and the RUN LED turns on.

- **Potentiometer mode**: turn the pot. The center position (with a small dead band) is 0 RPM. Turning it to one side gives clockwise rotation, and turning it to the other side gives counter-clockwise rotation. Full travel corresponds to `APP_MAX_RPM`.
- **UART mode**: send speed commands as described below.

Press **STOP** at any time to stop the motor.

### 5. UART protocol

| Setting | Value |
|---------|-------|
| Baud rate | 115200 |
| Format | 8N1 |
| Direction | RX only (PC to board) |

Send one **signed integer per line**, ending with `\n` or `\r`. The value is clamped to **-255 ... +255** and scaled linearly to **-APP_MAX_RPM ... +APP_MAX_RPM**. Other characters are ignored.

```text
255\n     -> full speed, forward
-128\n    -> about half speed, reverse
0\n       -> stop (motor held at 0 RPM)
```

Example with Python:

```python
import serial
ser = serial.Serial("COM3", 115200)
ser.write(b"200\n")
```

### 6. Display

```text
Mode: Potentiometer        <- active mode
Velocity =                 <- measured speed
 1234 (RPM)
Target = 1500              <- setpoint
RUNNING                    <- state
```

## Configuration

All the important parameters are in `config.h`:

| Parameter | Meaning |
|-----------|---------|
| `ENC_PPR` | **Set this to your encoder's pulses per revolution.** The RPM calculation depends on it |
| `ENC_INVERT` | Swap the encoder counting direction |
| `ENC_FILTER_N`, `ENC_FILTER_SAMPLE_HZ` | Encoder input noise filter (set `ENC_FILTER_N` to 1 to disable) |
| `MOTOR_INVERT` | Flip the motor rotation direction |
| `MOTOR_T2_PRESCALER` | Software PWM frequency (default 32 gives about 1.95 kHz) |
| `MOTION_KP`, `MOTION_KI`, `MOTION_KD`, `MOTION_KFF` | PID and feed-forward gains, **tune these on your motor** |
| `MOTION_ACCEL` | Setpoint ramp rate in units/s (0 = off) |
| `APP_MAX_RPM` | Speed at full pot travel or UART ±255 |
| `APP_OPEN_LOOP` | `1` = send the target directly as PWM duty (no PID), useful for testing the hardware |
| `APP_POT_INVERT`, `APP_POT_DEADBAND` | Potentiometer direction and dead band |
| `BTN_LONG_MS`, `BTN_DEBOUNCE_MS` | Button timing |
| `TFT_LANDSCAPE`, `TFT_COL_OFFSET`, `TFT_ROW_OFFSET` | Display orientation and offsets |

> **Tip:** if something does not behave as expected during bring-up, set `APP_OPEN_LOOP` to `1` first to check the motor and the pot without the PID in the way, then switch back to closed-loop and tune the gains.

## Repository Structure

The repository is organized like a university, with one "department" per area of the project:

```text
VIXULY-Source-code/
├── Department of Administration Services/     # Administration: final term report
│   └── (final term report)
│
├── Department of Electrical Engineering/      # Electrical: PCB design files
│   ├── Docs/                                  #   Datasheets, Reference designs,... used in this project
│   ├── Util/                                  #   External libraries, 3D CAD files used in the PCB design
│   └── 中国制造的PCB/                          #   PCB project, Schematic, Layout, Gerber files,..
│
├── Department of Mechanical Engineering/      # Mechanical: 3D printed parts
│   └── Encoder flange/                        #   Flange with a radial magnet at the center,
│                                              #   for a second encoder used to cross-check
│                                              #   the velocity measurement
│
├── Department of Software Engineering/        # Software: the firmware
│   ├── src/
│   │   ├── main.cpp
│   │   ├── app.cpp / app.h
│   │   ├── config.h
│   │   ├── motion.c / motion.h
│   │   ├── motor.c / motor.h
│   │   ├── encoder_mini.cpp / encoder_mini.h
│   │   ├── button.c / button.h
│   │   ├── uart.c / uart.h
│   │   ├── adc.c / adc.h
│   │   └── st7735_mini.c / st7735_mini.h
│   ├── VIXULY_Logic.txt                       #   Button / mode logic specification
│   └── platformio.ini
│
└── README.md
```

| Folder | Contents |
|--------|----------|
| **Department of Administration Services** | Final term report |
| **Department of Electrical Engineering** | Schematic, PCB files and Gerber files of the VI XU LY board |
| **Department of Mechanical Engineering** | 3D printed parts. Currently a flange with a radial magnet at its center, used for a second encoder that confirms the velocity measurement |
| **Department of Software Engineering** | Firmware for the ATmega88PA (this is what the sections above describe) |

> **Build note:** open the `Department of Software Engineering` folder (not the repo root) in PlatformIO.
## Acknowledgements

- Display module: [WeAct Studio ST7735](https://github.com/WeActStudio/WeAct-ST7735). `st7735_mini` is a trimmed, text-only driver written for this project so that it fits in the ATmega88PA's flash.

## Course Information

- **Course:** ME4169 - Microprocessor
- **University:** Hanoi University of Science and Technology (HUST)
- **Type:** Final term project

## Authors

| Member | GitHub | Role |
|--------|--------|------|
| **1m4n00b6292** | [@<your-github-username>](https://github.com/<your-github-username>) | Hardware and system design: PCB design, schematic, component selection, planning the system function and logic |
| **depzaineversai** | [@anhkiethp58-hue](https://github.com/anhkiethp58-hue) | Firmware: firmware development and integration |

## License

This project is licensed under the [MIT License](LICENSE).
