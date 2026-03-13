# Wing It 🐦

Arcade game I made from scratch in C++ with Win32 API. No engine, no framework, just raw Windows code.

**By Ayoub Assouar** — [@EvenKiller01YT](https://youtube.com/@EvenKiller01YT)

---

## Download

👉 **[Download Wing It v5.0](../../releases/latest)**

Run the installer, launch from the Start Menu or desktop. Clean uninstall included.

- Windows 7 / 8 / 10 / 11
- Installer is about 2 MB

---

## How to play

Flap through the pipes. Don't hit anything. Beat your best score.

| Key | Action |
|-----|--------|
| `SPACE` or `Left Click` | Flap |
| `1` | Use Ghost ability |
| `2` | Use Slow Mo ability |
| `3` | Use Shield ability |
| `S` | Open Shop (main screen) |
| `ESC` | Quit |

---

## What's in it

- Locked 60 FPS using `QueryPerformanceCounter` — no jitter
- All 5 sounds made by me from scratch using math, no samples taken from anywhere
- Custom bird icon I drew programmatically
- **Coins system** — earn coins every run based on your score
- **Shop** — buy ability uses with coins (stack up to 5)
- **3 Abilities** — Ghost, Slow Mo, Shield
- Best score saves between sessions
- Anti-cheat built in — speed hacks and memory patching don't work

---

## Abilities

| Ability | Key | Effect | Duration | Cost |
|---------|-----|--------|----------|------|
| Ghost | `1` | Pass through pipes | 5s | 120 coins |
| Slow Mo | `2` | Halve pipe speed | 6s | 150 coins |
| Shield | `3` | Survive one hit | Until hit | 100 coins |

Buy uses in the Shop. Each use is consumed on activation — buy again to restock.

---

## Technical stuff

No game engine. Everything written manually.

| Thing | How |
|-------|-----|
| Language | C++17 |
| Graphics | Win32 GDI, double buffered |
| Audio | WinMM `PlaySound` with a thread per sound |
| Frame timing | `QueryPerformanceCounter` + `timeBeginPeriod(1)` |
| Score saving | Windows Registry, XOR encrypted |
| Build | MinGW g++, fully static |
| Installer | Inno Setup 6 |

Stuff I figured out along the way:
- Used `SetWorldTransform` for rotating the bird smoothly
- Made the hitbox 4px smaller on each side so it feels fair
- Speed hack detection by comparing QPC time vs GetTickCount — if someone uses Cheat Engine the ratio breaks and the game silently punishes them
- Had to fix a MinGW windres pipe bug with `--use-temp-file` to get the icon showing in Explorer
- ICO format has to be bottom-up 32bpp BMP DIB or Windows won't show it

---

## Source code

Not public. Binary only.

Open an issue if you want to talk about how something works.

---

## Copyright

Copyright (c) 2026 Ayoub Assouar. All rights reserved.

Don't copy, reverse engineer, or redistribute this without permission.

---

**Ayoub Assouar**
[GitHub](https://github.com/Ayoub-Assouar) · [YouTube](https://youtube.com/@EvenKiller01YT) · 15 y/o · Morocco
