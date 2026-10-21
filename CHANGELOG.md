# Changelog

All notable changes to KidsOS. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versions follow [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Mini Flip: closing the lid saves the game and switches the console off, like POWER does.

## [0.1.2] - 2026-10-13

### Added

- `install.sh` puts KidsOS on a card with one command on Mac and Linux: it finds the card, asks the
  language and the Wi-Fi network, downloads the chosen version, formats the card and copies it over. A card
  that already has KidsOS or Onion is updated in place, games and saves included.

### Changed

- The first start needs no button press: Onion's guide and Package Manager are skipped and the console
  installs and reboots into the kids screen by itself. The Package Manager is still in Onion's Apps.
- The install screen shows the KidsOS picture instead of Onion's background and tips, and says
  "Installing KidsOS".
- The zip is called `KidsOS-<version>.zip`, without the Onion version.
- The kids screen shows the KidsOS logo while no games are chosen yet.

## [0.1.1] - 2026-10-13

### Added

- The texts the kid sees are in capitals and follow Onion's language setting: English, German, Italian,
  French, Spanish and Dutch. Any other language shows English.
- SSH is on whenever Wi-Fi is on, so the card can be reached from a computer; `ssh=0` in `kids/settings.txt`
  turns it off.

### Fixed

- The release workflow could not write the zip after the cross-compile; releases published on GitHub now get
  their zip built and attached automatically.

## [0.1.0] - 2026-10-12

First release: the kids launcher for the Miyoo Mini Plus, shipped as the official Onion release with the
launcher inside `onion.pak`.

- A row of big game pictures, A to play, MENU to stop; the favourites marked in Onion are the kid's games.
- Continue or start over when a game has a save state; POWER saves and switches off.
- Daily arcade coins, set from the KidsOS app in Onion or in `kids/settings.txt`.
- Box art fetched automatically; KidsOS theme, boot picture and sounds.
- Trimmed Onion packages for a faster first boot; Game Boy, Game Boy Color, GBA, NES, SNES, DS and Neo Geo
  preinstalled.

[Unreleased]: https://github.com/coorasse/onionKidsOS/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/coorasse/onionKidsOS/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/coorasse/onionKidsOS/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/coorasse/onionKidsOS/releases/tag/v0.1.0
