# Wing It 🐦

A fast-paced arcade game built from scratch in C++ using the Win32 API.

**By Ayoub Assouar** — [@EvenKiller0](https://youtube.com/@EvenKiller01YT)

---

## Download

👉 **[Download Wing It.exe](../../releases/latest)**

No installer. No dependencies. Just run it.

- Windows 7 / 8 / 10 / 11 — 64-bit
- Single `.exe` file, ~491 KB

---

## Gameplay

Flap through pipes. Don't die. Beat your best score.

| Key | Action |
|-----|--------|
| `SPACE` or `Left Click` | Flap |
| `ESC` | Quit |

---

## Features

- **60 FPS** locked — precision game loop via `QueryPerformanceCounter`
- **Original sounds** — all 5 sound effects synthesized from scratch (no samples)
- **Original icon** — custom bird character, drawn programmatically
- **Best score** saved locally between sessions
- **Anti-cheat protection** — speed-hack detection, memory integrity checks
- **Single portable exe** — no installer, no DLLs, no registry junk on uninstall

---

## Technical Details

Built entirely from scratch — no game engine, no framework.

| Component | Details |
|-----------|---------|
| Language | C++17 |
| Graphics | Win32 GDI (double-buffered) |
| Audio | Windows WinMM (`PlaySound`) |
| Frame timing | `QueryPerformanceCounter` + `timeBeginPeriod(1)` |
| Persistence | Windows Registry (`HKCU\Software\WingIt47`) |
| Build | MinGW g++ — single compile command, static linked |

### Architecture decisions I made:
- **No engine** — raw Win32 API for full control over every pixel
- **Double buffering** — `hMemDC` + `BitBlt` to eliminate flicker
- **Dual sound system** — async fire-and-forget + threaded sync playback
- **State machine** — `IDLE / PLAYING / DEAD` with clean transitions
- **Inner hitbox** — collision box shrunk 4px per side for fair gameplay

---

## Build From Source

> Source code is not publicly available. This repo distributes the compiled binary only.

If you want to discuss the technical implementation, open an issue.

---

## Copyright

Copyright (c) 2026 Ayoub Assouar. All rights reserved.

Unauthorized copying, reverse engineering, decompilation, or redistribution
of this software is strictly prohibited.

The Flappy Bird game concept was originally created by Dong Nguyen / .GEARS Studios.
Wing It is an independent reimplementation with 100% original code and assets.

---

## Author

**Ayoub Assouar**
- GitHub: [@Ayoub-Assouar](https://github.com/Ayoub-Assouar)
- YouTube: [@EvenKiller01YT](https://youtube.com/@EvenKiller01YT)
- Age: 15 | Essaouira, Morocco
