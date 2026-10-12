# KidsOS for the Miyoo Mini Plus

A game console for small children: turn it on, pick a game, play. Nothing else is reachable.

![The kids screen: one big game tile at a time](docs/images/kid-ui.png)

The child sees one thing only: a row of big game pictures. Left and right to choose, **A** to play,
**MENU** to stop. A game played before asks one picture question first: continue where you left off, or
start over. No reading needed, no menus, no settings.

The parent decides which games are in the row, and can reach the full system with a secret button
combination.

KidsOS is [Onion](https://onionui.github.io/) (the best-known custom firmware for the Miyoo Mini) with a
different, child-sized front end. Everything else is Onion: emulators, save states, Wi-Fi, box art.

> **Early version.** The launcher has been used on a real Miyoo Mini Plus, but the installation zip below
> has been tested in an emulated boot only. If something goes wrong, the
> [troubleshooting](#troubleshooting) section tells you how to get back to a working device: nothing is
> written to the console itself, only to the SD card.

## What you need

- A **Miyoo Mini Plus**. The original Miyoo Mini should work too but has not been tried.
- A **microSD card** of 32 GB or more from a reputable brand (SanDisk, Samsung). The card that comes with
  the console is slow and unreliable.
- A computer with a card reader.
- Game files (ROMs) for the systems you want: Game Boy, NES, SNES, PlayStation and
  [many more](https://onionui.github.io/docs/emulators/folders).

## Installation

Installation takes about fifteen minutes, most of it waiting.

### 1. Format the card as FAT32

- **Mac:** open *Disk Utility*, select the card (the top-level entry, not the volume below it), click
  *Erase*, choose Format *MS-DOS (FAT)* and Scheme *Master Boot Record*, click *Erase*.
- **Windows:** for cards bigger than 32 GB use [Rufus](https://rufus.ie/): Boot selection *Non bootable*,
  File system *Large FAT32*, then *Start*.

### 2. Copy KidsOS to the card

1. Download the latest `KidsOS-x.y.z.zip` from the
   [releases page](https://github.com/coorasse/onionKidsOS/releases).
2. Unzip it. You get seven folders and two small text files.
3. Copy **everything** to the root of the card. One of the folders, `.tmp_update`, is hidden: on a Mac press
   `Cmd + Shift + .` in the Finder to see hidden files, on Windows enable *Hidden items* in the *View* menu.
   Without that folder nothing happens.

### 3. First start

1. Put the card in the console and turn it on. Onion installs itself; this takes several minutes.
2. A short guide to Onion appears. Go through it with **A**.
3. The *Package Manager* appears: a list of systems. Move with the d-pad, press **A** to tick the ones you
   have games for (for example *Game Boy*, *SNES*, *PlayStation*), then press **START** twice.
4. The console reboots into the kids screen. It is empty: no games have been chosen yet.

### 4. Add games

1. Turn the console off and put the card back in the computer.
2. Copy your game files into `Roms/<SYSTEM>/`. The folder names are short codes:
   `GB` (Game Boy), `GBC` (Game Boy Color), `GBA` (Game Boy Advance), `FC` (NES), `SFC` (SNES),
   `PS` (PlayStation). The full table is in
   [Onion's documentation](https://onionui.github.io/docs/emulators/folders).
3. Optional, but what makes the kids screen nice: a picture for each game, as a PNG with the same name as the
   game file, in `Roms/<SYSTEM>/Imgs/`. Example: `Roms/GB/Tetris.gb` and `Roms/GB/Imgs/Tetris.png`.
   Without a picture the tile shows the game's name.
4. Put the card back and turn the console on.
5. Hold **SELECT + START** together for three seconds: the kids screen goes away and Onion's own menu
   appears.
6. In *Games*, open a system, highlight a game, press **SELECT** and choose *Add to favorites*. Repeat for
   every game the child may play. **The favourites are the child's list.**
7. Turn the console off and on again. The games are there.

To remove a game later, do the same and choose *Remove from favorites*.

## Using it

| Button | What it does |
|--------|--------------|
| Left / Right | Choose a game |
| A | Play |
| MENU (during a game) | Save and go back to the game list |
| Volume + / - | Volume |
| SELECT + START held for 3 s | Parent: leave the kids screen and open Onion until the next power off |

When a game is started again, a screen with two pictures asks whether to **continue** (green badge, a
screenshot of where the game was left) or **start over** (orange arrow, the game's picture).

Everything else, including the in-game menu of the emulator, is out of reach for the child. The parent can
still change Onion settings (brightness, theme music on or off, Wi-Fi) after the secret combination, under
*Apps* › *Tweaks*.

## Updating KidsOS

Download the new zip, copy the folders onto the card over the existing ones (replace when asked), put the
card back and turn the console on. Onion updates itself and keeps games, save states and favourites.

Do not use Onion's own *OTA update* app: it would install plain Onion and the kids screen would disappear.
Copying a KidsOS zip onto the card brings it back.

## Going back to plain Onion

Download [Onion](https://github.com/OnionUI/Onion/releases), copy its folders onto the card the same way,
and delete the hidden folder `.kidsos` from the card. Games and saves stay.

## Troubleshooting

- **Black screen, or nothing reacts:** hold **POWER** until the console turns off, then turn it on again.
- **The kids screen is empty:** no favourites yet, see [Add games](#4-add-games).
- **The console starts into Onion's menu instead of the kids screen:** copy the KidsOS zip onto the card
  again as in [Copy KidsOS to the card](#2-copy-kidsos-to-the-card). Nothing else is needed.
- **A game does not start:** try it from Onion's menu (SELECT + START). If it does not start there either,
  the game file or the emulator setup is the problem, not KidsOS.

## How it works, for the curious

Onion lives entirely on the SD card; the console's own firmware only starts it. KidsOS adds a small
launcher to Onion's installation package and a boot hook that puts the launcher in front of Onion's menu.
Onion's scripts are not modified, so Onion keeps working as documented, and copying a new KidsOS zip onto
the card always restores the kids screen. Details, build instructions and the development setup are in
[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

KidsOS is free software under the [GPL-3.0](LICENSE), like Onion. It is not affiliated with Miyoo or with
the Onion team.
