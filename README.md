# AULA F108 Pro — Reverse Engineering

Reverse engineering of the AULA F108 Pro keyboard and its rebranded clones (AJAZZ, Epomaker, and possibly others).

![Alt text](AULA%20F108%20Pro.png)

---

## Purpose

The goal of this project is to reverse engineer the keyboard firmware and storage in order to:

- Replace UI images with custom ones (custom boot logo, icons, screens).
- Add support for additional languages.
- Understand the HID protocol for potential custom tooling.
- Extend existing features.

---

## Hardware Overview

The keyboard is built around two separate chips:

### SN32F290 — ARM Cortex-M0 MCU (256 KB flash)
Contains:
- All keyboard logic (key scanning, debounce, layers, macros).
- USB HID protocol implementation.
- Wireless stack (Bluetooth, WI-Fi 2.4 GHz).
- Bitmap fonts for UI strings (very few texts are actually Strings, as most are included in the images).

### SPI — 16 MB flash
It contains only graphical data:
- Boot animation.
- Custom Animation (the animation that you can already replace using the official AULA tool).
- Status icons: battery levels, caps lock, scroll lock, connection state etc.
- All accessible screens.

---

## Usage

### 1. Extract images from SPI flash dump

```
unpack.exe <spi_flash.bin>
```

Reads the binary SPI flash image and exports all image blocks as PNG files into an `export/` folder in the current directory.

Each file is named `NNNN_XXXXXXX.png` where:
- `NNNN` is the frame index within its block.
- `XXXXXXX` is the hex offset of the image inside the `.bin` file.

### 2. Edit images

Copy the PNG you want to modify from `export/` folder to a new folder named `patch/` and edit them with any image editor. Keep the original filenames because the offset encoded in the name is used to locate the image in the binary.

> **Important:** images must be saved at the exact same resolution as the original. The format is raw RGB565 Little Endian, so avoid adding transparency or changing color depth.

### 3. Repack the patched images into the binary

```
repack.exe <spi_flash.bin>
```

Reads all PNG files from the `patch/` folder, converts them back to RGB565 LE, and writes a new file named `<spi_flash>_patched.bin` alongside the original. Only images that differ from the original are written.

Files in `patch/` that don't match a known offset in the block table are ignored.
