# KidsOS for the Miyoo Mini Plus

<img src="docs/images/kidsos.png" width="120" align="right" alt="A child playing a Game Boy, the KidsOS icon">

A game console for small children: turn it on, pick a game, play. Nothing else is reachable.

<p align="center">
  <img src="docs/images/kid-ui.png" width="400"
       alt="The kids screen: one big game picture, the next game peeking in from the right">
</p>

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
3. The pictures on the tiles come by themselves when the console is on Wi-Fi (see
   [Pictures for the games](#pictures-for-the-games)). To use your own picture instead, put a PNG with the
   same name as the game file in `Roms/<SYSTEM>/Imgs/`. Example: `Roms/GB/Tetris.gb` and
   `Roms/GB/Imgs/Tetris.png`.
4. Put the card back and turn the console on.
5. Hold **SELECT + START** together for three seconds: the kids screen goes away and Onion's own menu
   appears. It stays until the console is turned off, or until you open **KidsOS** in the *Apps* tab and
   choose *Back to the kids screen*.
6. In *Games*, open a system, highlight a game, press **SELECT** and choose *Add to favorites*. Repeat for
   every game the child may play. **The favourites are the child's list.**
7. Turn the console off and on again. The games are there.

To remove a game later, do the same and choose *Remove from favorites*.

### Pictures for the games

Every game in the child's row gets its box art downloaded by itself, once the console knows your Wi-Fi:
hold **SELECT + START** for three seconds, open *Apps* › *Tweaks* › *Network*, turn *Wi-Fi* on and pick your
network. From then on, every time the kids screen appears, the console fetches the missing pictures in the
background and the tiles fill in within a minute. Without Wi-Fi the tiles show the game's name and the
console tries again at the next start.

The pictures come from the [libretro thumbnails](https://thumbnails.libretro.com/) collection and are found
by the game's file name: `Tetris.gb` and `Tetris (World) (Rev 1).gb` both work, `tetris_final_v2.gb` does
not. A game that gets no picture is easiest fixed by renaming its file to the game's real title.

## Using it

| Button | What it does |
|--------|--------------|
| Left / Right | Choose a game |
| A | Play |
| MENU (during a game) | Save and go back to the game list |
| SELECT (arcade games) | Insert a coin, while the day's coins last |
| Volume + / - | Volume |
| SELECT + START held for 3 s | Parent: leave the kids screen and open Onion |

<p align="center">
  <img src="docs/images/kid-ui-second.png" width="320" alt="Moving right: the next game slides into the middle">
  <img src="docs/images/volume.png" width="320" alt="The volume bar shown over the games screen">
</p>

When a game is started again, a screen with two pictures asks whether to **continue** (green badge, a
screenshot of where the game was left) or **start over** (orange arrow, the game's picture).

<p align="center">
  <img src="docs/images/resume-continue.png" width="320" alt="The continue or start over screen, continue selected">
  <img src="docs/images/resume-start-over.png" width="320" alt="The same screen with start over selected">
</p>

Holding **SELECT + START** shows a bar that fills up while the buttons are held; letting go before it is
full changes nothing, so a child pressing buttons at random stays in the kids screen.

<p align="center">
  <img src="docs/images/parent-hold.png" width="400" alt="The bar that fills while SELECT and START are held">
</p>

### Arcade coins

Arcade games (the folders ARCADE, NEOGEO, CPS and the like) come with an allowance of coins, 10 a day
unless you change it. Every SELECT press in such a game is a coin, and a coin counter in the top left corner
of the games screen shows how many are left. When they are gone the game is not interrupted: it keeps the
credits it already has, SELECT simply does nothing more, the counter goes grey and the arcade tiles fade.
The coins come back the next day. The console has no clock of its own, so unless it got the time over Wi-Fi
"the next day" means the next time it is switched on.

To change the number on the console itself: leave the kids screen (SELECT + START for three seconds), open
the *Apps* tab and start **KidsOS**. It offers the coins per day (3 to 30, or no limit), a way to hand
out today's coins again and the way back to the kids screen. The same can be done from a computer: open
`kids/settings.txt` at the top level of the card with any text editor (the file is created the first time
KidsOS starts). It looks like this:

```
# Arcade coins the kid may insert per day (0 = unlimited).
arcade_coins_per_day=10
# Systems whose games use coins, by folder tag.
arcade_systems=ARCADE,CPS1,CPS2,CPS3,NEOGEO,FBN,FBNEO,MAME
```

Set `arcade_coins_per_day=0` to switch coins off altogether: no counter, and SELECT always works. The second
line lists the game folders that count as arcade; add or remove folder names, separated by commas. To hand
out today's coins again, delete the file `kids/coins.txt` from the card.

Everything else, including the in-game menu of the emulator, is out of reach for the child. The parent can
still change Onion settings (brightness, theme music on or off, Wi-Fi) after the secret combination, under
*Apps* › *Tweaks*.

## No time limit, on purpose

KidsOS has no "maximum play time per day" and will not get one. The console does not count minutes, does
not lock itself after an hour and does not nag. This is a deliberate choice, not a missing feature.

How long a child plays is something to agree on between the child and the parents, and to keep to
together. A timer enforced by the machine takes that conversation away: the console becomes the one that
says stop, the child learns to argue with a device instead of with a person, and nobody learns to stop by
themselves. Saying "one more level and then we put it away", and then putting it away, is the limit; the
console only has to make putting it away easy, which it does by saving the game when **MENU** is pressed
and bringing it back with *Continue* next time.

The arcade coins are different in kind: they copy the arcade hall, where a handful of coins is the
budget and the game ends when they are gone. Nothing stops the child from playing another game
afterwards.

## Updating KidsOS

Download the new zip, copy the folders onto the card over the existing ones (replace when asked), put the
card back and turn the console on. Onion updates itself and keeps games, save states and favourites.

Do not use Onion's own *OTA update* app: it would install plain Onion and the kids screen would disappear.
Copying a KidsOS zip onto the card brings it back.

## Going back to plain Onion

Download [Onion](https://github.com/OnionUI/Onion/releases), copy its folders onto the card the same way,
and delete the hidden folder `.kidsos` from the card. Games and saves stay.

## Troubleshooting

- **The console shows Onion's boot picture instead of the KidsOS one:** harmless, the kids screen still
  comes up; copying the KidsOS zip onto the card again puts the picture back.
- **Black screen, or nothing reacts:** hold **POWER** until the console turns off, then turn it on again.
- **The kids screen is empty:** no favourites yet, see [Add games](#4-add-games).
- **The console starts into Onion's menu instead of the kids screen:** copy the KidsOS zip onto the card
  again as in [Copy KidsOS to the card](#2-copy-kidsos-to-the-card). Nothing else is needed.
- **A game has no picture:** check that Wi-Fi is on (see [Pictures for the games](#pictures-for-the-games))
  and that the file is named after the game. `.kidsos/covers.log` on the card says what was tried.
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
