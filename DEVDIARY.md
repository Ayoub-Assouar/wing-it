# Wing It — Dev Diary
### by Ayoub Assouar | March 2026

---

So I wanted to make a game completely from scratch. No Unity, no Godot, no engine at all.
Just C++ and raw Windows API. Figured it would teach me more than using an engine.

---

## Why Win32 / no engine

I wanted one single .exe file that just runs. No install, no DLLs, no nothing.
Win32 API lets you do that. Also I wanted to actually understand what's happening
under the hood instead of clicking buttons in an editor.

---

## The game loop

First version used `GetTickCount` for timing. Turns out that only gives you like 15ms
resolution so the framerate was inconsistent. Fixed it by switching to
`QueryPerformanceCounter` which gives microsecond precision, and added
`timeBeginPeriod(1)` to make Windows timer resolution 1ms.

The loop sleeps until 1ms before the next frame then spins the last millisecond.
Also added a skip-ahead guard so if you alt-tab and come back it doesn't try to
simulate 500 frames at once.

---

## Physics tuning

Spent a while tweaking these until it felt right:
- Gravity: 0.45 per frame
- Jump velocity: -8.0
- Max fall speed: 10.0
- Pipe speed: 2.8px per frame
- Pipe gap: 150px
- Space between pipes: 180px

The hitbox is also shrunk 4px on each side so it's a bit forgiving. Feels better to play.

---

## Rendering

Everything is GDI. Double buffered — draw to an offscreen bitmap then BitBlt to the window.
No flicker at all.

The bird rotation was the tricky part. Used `SetWorldTransform` with an XFORM matrix
to rotate around the bird's center. Angle is calculated from the vertical velocity so
it tilts up on jump and nosedives when falling.

Drew the bird with ellipses and a polygon for the beak. Colors I picked myself.
Same for the pipes (3 layers: main body, cap, highlight stripe) and the background.

---

## Sounds

Didn't want to use any samples that I didn't own. So I generated all 5 sounds from
scratch using Python — pure sine waves, noise, exponential envelopes.

- Flap: rising chirp 400→900Hz
- Score: ascending ding 880→1320Hz  
- Hit: low thump at 120Hz + noise burst
- Die: descending wail 600→150Hz
- Swoosh: 1200→300Hz downward sweep

Converted to WAV, embedded as byte arrays in a header file.
These are 100% mine.

---

## Icon

Drew it programmatically with Python/Pillow. Teal circle, yellow bird, white eye,
orange beak. Took a few tries to get the ICO format right — Windows Explorer needs
the pixel data in bottom-up order with a specific BMP DIB header format
(`biHeight = h*2` to account for the AND mask). PNG-in-ICO doesn't work reliably.

Embedded into the exe via windres resource file.

---

## Anti-cheat

Added a few layers:
- Debugger detection at startup and during gameplay
- Speed hack detection by comparing QPC time vs GetTickCount. Cheat Engine shifts
  GetTickCount but not QPC so the ratio breaks. Three strikes and you're flagged.
- Memory integrity check on the best score — XOR checksum, if it changes externally
  the game wipes it and flags the session
- Registry score is XOR encrypted so you can't just edit it to a high number

If any of these fire, the game doesn't crash. It just silently breaks:
random drift on the bird, pipe gap shrinks to 40px, score never goes up.
Cheater thinks they're just bad.

---

## Registry / score saving

Saves best score to `HKCU\Software\WingIt47`. Chose registry over a file so
there's nothing extra sitting next to the exe. Score is XOR'd with a key before
saving so it's not a raw number in regedit.

---

## Build

Single compile command, fully static linked so it runs anywhere:

```
g++ flappy2.cpp resource.res -o "Wing It.exe" -I. -O2 -std=c++17 -mwindows -lgdi32 -lwinmm -ladvapi32 -static
```

Had to use `windres --use-temp-file` because MinGW has a pipe bug that breaks
windres otherwise. Also MinGW 15+ requires explicit `std::` prefix for min/max/abs
so had to add `#include <algorithm>` and fix those calls.

---

## What I learned

- Win32 API is verbose but powerful — good to understand it
- Precise frame timing matters a lot for game feel
- ICO format is annoying but documented
- Anti-cheat is more about making cheating pointless than actually blocking it
- Generating your own assets means you own everything

---

*Ayoub Assouar — March 2026*
