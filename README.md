# AULA F108 Pro — Reverse Engineering

Reverse engineering of the AULA F108 Pro keyboard and its rebranded clones (AJAZZ, Epomaker, and possibly others).

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
Apparently it contains only graphical data:
- Boot animation.
- Custom Animation (the animation that you can already replace using the official AULA tool).
- Status icons: battery levels, caps lock, scroll lock, connection state etc.
- All accessible screens.

---

## Image Format

All images are stored as **raw RGB565 Little Endian**
