# Wi-Fi + IMU963RA + VOFA+

This is an independent Keil 5 project for the MSPM0G3519 motherboard.
The original `WifiVofaGongCheng` project is not modified.

## Source layout

| Module | Responsibility |
| --- | --- |
| `main.c` | System tick and 20 ms scheduling |
| `pid.c/.h` | PID calculation and integral limiting |
| `encoder.c/.h` | PB10 encoder interrupt and pulse counter |
| `motor.c/.h` | PB12 PWM, PB13 direction, speed control and stall protection |
| `imu963.c/.h` | IMU963RA setup, zero-bias calibration and relative angles |
| `wifi.c/.h` | SPI Wi-Fi connection, VOFA telemetry and reconnect handling |

## Hardware

| Device | Signal | MSPM0G3519 pin |
| --- | --- | --- |
| SPI Wi-Fi | SCK | PA12 |
| SPI Wi-Fi | MOSI | PA9 |
| SPI Wi-Fi | MISO | PA13 |
| SPI Wi-Fi | CS | PA7 |
| SPI Wi-Fi | RST | PA8 |
| SPI Wi-Fi | INT | PA15 |
| IMU963RA | SCK | PB23 |
| IMU963RA | MOSI | PB22 |
| IMU963RA | MISO | PB21 |
| IMU963RA | CS | PB19 |
| Motor PWM | PWM | PB12 |
| Motor driver | DIR / enable | PB13 |
| Motor encoder | A | PB10 |

Connect 3.3 V and GND according to the motherboard attitude-sensor
connector. The IMU963RA module supports 3.3 V to 5 V, but the motherboard
connector supplies 3.3 V.

## Wi-Fi and VOFA+

Edit these definitions in `user/src/main.c` before building:

```c
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define HOST_IP         "192.168.1.100"
#define HOST_PORT       "8086"
```

In VOFA+, create a TCP client connection to `HOST_IP:8086`. Select the
FireWater protocol. Each frame is sent every 20 ms:

```text
angle:z,x,y,target_rps,actual_rps,duty,ready,stall
```

The angle starts at `0 degrees` after power-up and is calculated by
integrating the gyroscope. The magnetometer is not used, so this is a
relative angle.

Keep the board completely still for about 3 seconds after power-up.
The firmware waits for the sensor to settle, then averages 512 gyroscope
samples to calibrate the X/Y/Z zero-rate offsets. While the board is later
detected as stationary, angle integration is locked and the gyro offsets
are updated quickly to suppress temperature-related drift.

Units and channel meaning:

| Channel | Meaning | Unit |
| --- | --- | --- |
| 1 | Relative angle around Z | degree |
| 2 | Relative angle around X | degree |
| 3 | Relative angle around Y | degree |
| 4 | Target motor speed | revolutions/second |
| 5 | Measured motor speed | revolutions/second |
| 6 | Motor PWM duty | percent |
| 7 | IMU initialization state | 0/1 |
| 8 | Stall protection latched | 0/1 |

The default IMU963RA ranges are +/-8 g, +/-2000 deg/s and +/-8 gauss.

The motor target speed is selected from the relative X angle:

| X angle | Target speed |
| --- | --- |
| below -7 degrees | 30 RPS |
| above 6 degrees | 10 RPS |
| -7 to 6 degrees inclusive | 20 RPS |

PB13 is driven low for reverse operation and returned high when the motor
output stops. The encoder uses PB10 rising edges and the code converts
encoder speed to motor-shaft speed using the 3:1 encoder-to-motor ratio.

The motor has stall protection derived from the earlier controller:

- PWM duty must be at least 30 percent
- Measured motor speed must remain below 0.30 RPS
- Both conditions must persist for 600 ms

When a stall is detected, PWM is disabled, PB13 returns high and the fault
is latched in RAM. The motor remains stopped until the MCU is reset.

Changing the angle-selected target speed preserves the PID integral term
for a bumpless transfer. The integral is cleared only during motor stop or
controller initialization.

## Build

Open `mdk/Wifi_Imu963ra_Vofa.uvprojx` in Keil 5 and run **Rebuild**.
The HEX output is:

```text
mdk/Objects/Wifi_Imu963ra_Vofa.hex
```

The command-line build script uses the TI Arm Clang installation currently
configured for the original project:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_wifi_imu.ps1
```
