# Wi-Fi + IMU963RA + VOFA+

This is an independent Keil 5 project for the MSPM0G3519 motherboard.
The original `WifiVofaGongCheng` project is not modified.

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
angle:z,x,y,ready
```

The angle starts at `0 degrees` after power-up and is calculated by
integrating the gyroscope. The magnetometer is not used, so this is a
relative angle and will slowly drift over long periods.

Units and channel meaning:

| Channel | Meaning | Unit |
| --- | --- | --- |
| 1 | Relative angle around Z | degree |
| 2 | Relative angle around X | degree |
| 3 | Relative angle around Y | degree |
| 4 | IMU initialization state | 0/1 |

The default IMU963RA ranges are +/-8 g, +/-2000 deg/s and +/-8 gauss.

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
