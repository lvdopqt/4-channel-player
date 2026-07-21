# ESP32 Dual I2S Audio Player (4-Channel)

This project transforms an ESP32 into a 4-channel audio player. It reads a single 4-channel, 16-bit `.wav` file from a MicroSD card using the high-speed SDMMC 1-Bit interface, splits the audio tracks, and sends them to two separate I2S DACs (PCM5102A) simultaneously.

## Features

* **High-Speed SDMMC (1-Bit Mode):** Avoids the SPI bottleneck, ensuring smooth audio playback.
* **Dual I2S Output:** Utilizes both built-in I2S hardware peripherals of the ESP32 (I2S0 and I2S1).
* **Smart Channel Splitter:** * If a 4-channel WAV is provided, Tracks 1 & 2 go to DAC 1, and Tracks 3 & 4 go to DAC 2.
  * If a standard Stereo (2-channel) WAV is provided, it plays the same audio on both DACs.

## Hardware Requirements

* 1x ESP32 Development Board (e.g., DOIT V1 30-pin)
* 2x PCM5102A I2S DAC Modules (The "Purple Boards")
* 1x MicroSD Card Breakout Board (Native 3V, SDIO/SPI compatible like Adafruit 4682)
* MicroSD Card (Formatted as **FAT32 / MS-DOS FAT** with Master Boot Record - MBR)

## Wiring / Pinout

The wiring schematics drawing is [here](./assets/4ch-player-schematic.pdf)

### 1. SD Card Module (SDMMC 1-Bit Mode)

* **CLK:** GPIO 14
* **CMD:** GPIO 15
* **D0 (DAT0):** GPIO 2
* **3V / VDD:** 3.3V
* **GND:** GND

> *Note: You MUST add 3.3kΩ pull-up resistors to the CMD, D0, and D3 lines connecting them to 3.3V.*

### 2. DAC 1 (I2S Port 0)

* **BCK:** GPIO 27
* **LRCK (WS):** GPIO 26
* **DIN (DATA):** GPIO 25
* **SCK:** GND *(Mandatory for PCM5102A to generate its own master clock)*
* **VIN / VCC:** 5V / VIN
* **GND:** GND

### 3. DAC 2 (I2S Port 1)

* **BCK:** GPIO 32
* **LRCK (WS):** GPIO 33
* **DIN (DATA):** GPIO 22
* **SCK:** GND
* **VIN / VCC:** 5V / VIN
* **GND:** GND

### ⚠️ IMPORTANT: The "Upload Bug" (GPIO 2 / DAT0)

Because we are using **GPIO 2** for the SD Card's DAT0 line, you might encounter a `Wrong boot mode detected (0xb)` error when trying to upload new code to the ESP32.

GPIO 2 is a "strapping pin" that dictates how the ESP32 boots. The SD Card requires this pin to be pulled HIGH (3.3V), but the ESP32 requires it to be LOW (0V) to enter the flash/download mode.

**To upload new code:**

1. Disconnect the wire going to **GPIO 2** (DAT0).
2. Click Upload in the Arduino IDE.
3. Once the upload says "Done", plug the wire back into GPIO 2.
4. Press the `EN` (Reset) button on the ESP32 to run the code.

## PCM5102A Board Configuration

On the front of the purple PCM5102A boards, there are 4 solder jumpers. For this project to work and output sound (disabling the MUTE function), you must bridge them with solder as follows:

* **1 (FLT):** L
* **2 (DEMP):** L
* **3 (XSMT):** H *(Critical: Disables Mute)*
* **4 (FMT):** L *(Critical: Sets format to standard I2S)*

<img width="400" height="359" alt="jumper-i2s" src="https://github.com/user-attachments/assets/04dadfd8-33b1-4d1a-8ae8-badc137abffb" />


## Audio File Preparation

The code expects a specific audio format to process the bytes correctly without overloading the CPU.

* **Format:** WAV (`.wav`)
* **Bit Depth:** 16-bit PCM (Strictly required)
* **Sample Rate:** e.g., 44100Hz or 48000Hz (Auto-detected)
* **Filename:** `music.wav` (Placed in the root directory of the SD Card)
