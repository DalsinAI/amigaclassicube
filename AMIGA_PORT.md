ClassiCube for AmigaOS 3.x
==========================

ClassiCube 1.3.9 (https://github.com/ClassiCube/ClassiCube, BSD-3-Clause),
the block-building game, for AmigaOS 3.x on a 68040 or 68060 with an FPU
and an RTG graphics card. Singleplayer only.

What you need
-------------

- AmigaOS 3.x, a 68040 or 68060 with an FPU (not an LC040/EC040).
- OpenGPU's runtime (OpenUp installs it): opengpu.library and
  LIBS:OpenGPU/SDL2.module.
- An RTG screen mode of 640x480 in 16 or 32-bit colour that SDL 2 can
  open, or a 16/32-bit Workbench for the windowed mode.
- About 16 MB of free Fast RAM.

How to play
-----------

Double-click ClassiCube in its drawer (or run it from a Shell; the game
always works in its own drawer, PROGDIR:). It goes straight into
singleplayer: a new 64x64x64 world is generated, which takes a while on a
68060.

- Mouse: look; left button breaks a block, right button places one, middle
  button picks the block you point at.
- W A S D: walk. Space: jump. B: choose a block. 1-9: hotbar. Z: fly
  (Q/E up and down), Shift: faster.
- Esc: the pause menu (options, "Generate new level...", "Save level...",
  "Load level...", quit). T: chat and commands (/client help).
- F: view distance. F1 hides the interface, F3 the frame rate and
  position, F5 is the third-person view, F12 takes a screenshot
  (screenshots/).

Worlds are saved as .cw files in maps/ (Esc, "Save level..."). A .cw, .dat,
.lvl, .fcm or .schematic world given on the command line is loaded at
start: `ClassiCube maps/myworld.cw`. The view distance starts at 64
blocks (upstream: 512).

Speed. Everything is drawn by the processor (ClassiCube's software
renderer). To make it faster, set a smaller drawing size in options.txt
(it is written in the drawer after the first run), for example:

    window-width=320
    window-height=240

The picture is then scaled up to the 640x480 screen. A shorter view
distance (Esc, Options, Graphics) and the "Fast" or "Fastest" fps limit
also help. `amiga-windowed=true` in options.txt opens a window on the
Workbench (of the drawing size) instead of a screen.

What was done
-------------

- src/Game.h: a default view distance of 64 blocks.
- src/Core.h: a PLAT_AMIGA_RTG target. Big-endian (CC_BIG_ENDIAN; GCC for
  the 68k does not define __BIG_ENDIAN__, which upstream's test looks for),
  SDL 2 window, software renderer (CC_GFX_BACKEND_SOFTGPU), no network,
  no sound, no plugins, no FreeType (the texture pack's bitmap font), the
  map generator in steps on the one task (CC_BUILD_COOPTHREADED), and the
  low-memory settings (8-bit block IDs, 64x64x64 default world).
  Upstream's own PLAT_AMIGA target (src/amiga/, misc/amiga/) is an
  unfinished stub (no files, no picture) and is not used.
- amiga/Platform_AmigaRTG.c: the platform layer: files and drawers through
  dos.library (Open/Read/Write/Seek, Lock/Examine/ExNext, CreateDir),
  AmigaDOS error codes and Fault() texts, PROGDIR: as the current drawer
  while the game runs (the Shell's is given back at the end), time through
  libnix, no threads, sockets, plugins, updater or crash handler. Without
  arguments it starts singleplayer, so the launcher (server list, sign-in,
  update check, texture download) is never shown.
- src/Window_SDL2.c (#ifdef CC_BUILD_AMIGA_RTG): a 640x480 full-screen
  window (or a Workbench window), a renderer, and a streaming XRGB8888
  texture of the drawing size that the renderer scales to the screen.
  OpenGPU's SDL 2 reports a 24-bit window surface, so the surface is not
  used. The mouse position and warp are scaled between the two sizes. No
  fullscreen toggling or resizing. The joystick subsystem is started on its
  own, so that its failure cannot take the screen with it.
- amiga/amiga_dialog.c: message boxes are Intuition requesters on the
  Workbench (brought to the front over the game's screen, then sent back);
  the text also goes to the Shell.
- amiga/amiga_screens.c (from the Team's Seven Kingdoms port): SDL2.module
  0.6 leaves empty screens behind after a full-screen window; they are
  closed (or sent behind the Workbench) at exit.
- amiga/stack.c: a 1 MB stack (ClassiCube expects 256 KB).

Byte order: ClassiCube reads every file format (the .cw NBT, .lvl, .dat,
.fcm and .schematic worlds, zip, gzip/deflate, PNG) a byte at a time
through its Mem_Read/Stream_Read *_LE/*_BE helpers, and upstream already
runs on big-endian machines (GameCube, Wii, N64, IRIX, HP-UX). Pixels are
32-bit words (0xAARRGGBB) throughout, given to SDL as XRGB8888.

Data
----

texpacks/classicube.zip is ClassiCube's own default texture pack
(misc/cc_textures.zip in the upstream repository, the one upstream's own
packages ship as texpacks/classicube.zip; textures by Goodlyay, see
credits.txt), under the repository's BSD-3-Clause licence (LICENSE.txt).
It holds the terrain, the interface, the font and the mob skins. Nothing
is downloaded. Without it the game starts with a "Missing file" warning
and no textures or text.

How to build
------------

    make -f amiga/Makefile.amigaos -j2 \
         STOVE=/path/to/openamigagcc-16.2.0-open4 \
         NDK=/path/to/OpenNDK/Include_H SDL2KIT=/path/to/opengpu-sdk
    make -f amiga/Makefile.amigaos dist

The program is build/amigaos/ClassiCube (and ClassiCube.debug with
symbols); the drawer is dist/ClassiCube (+ dist/ClassiCube.info). No
library is needed beyond the OpenGPU kit's libSDL2.a: ClassiCube has its
own inflate and PNG decoder. The icons: amiga/icons/icons.json.

Known limits
------------

- Singleplayer only: no multiplayer, launcher, server list, skins from the
  internet, texture pack downloads or update checks.
- No sound or music (ClassiCube has no SDL audio backend, and the sounds
  are a separate download).
- Software rendering: expect a few frames a second at 640x480 on a 68060;
  less at higher view distances.
- No clipboard paste beyond what SDL2.module offers, no file requesters
  (worlds are saved and loaded by name in maps/).
