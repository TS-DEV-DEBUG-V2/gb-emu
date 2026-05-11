<p align="center">
  <img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/gameboy.webp" width="180" alt="GBEMU Logo"/>
</p>

<h1 align="center">GBEMU</h1>
<h3 align="center">A Nintendo Game Boy Emulator written in C++</h3>

<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows-blue?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/platform-WebAssembly-orange?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B-00599C?style=for-the-badge&logo=cplusplus"/>
  <img src="https://img.shields.io/badge/emulator-GameBoy-8bac0f?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/status-active-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/project-hobby-purple?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/audio-APU-yellow?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/build-CMake-blueviolet?style=for-the-badge"/>
  <img src="https://img.shields.io/github/stars/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/forks/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/issues/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/license/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/last-commit/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/repo-size/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/commit-activity/m/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/languages/top/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/languages/code-size/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/github/watchers/TS-DEV-DEBUG-V2/gb-emu?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/accuracy-cycle--accurate-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/audio-working-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/input-working-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/saving-implemented-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/save_states-planned-lightgrey?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/mappers-supported-blue?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/web_build-working-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/ppu-improving-orange?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/apu-cycle_accurate-success?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/license-MIT-brightgreen?style=for-the-badge"/>
</p>

---

# Features

- Load `.gb` ROMs
- Functional input handling
- Cycle-accurate APU implementation
- WebAssembly build
- Save support
- Multiple mapper implementations
- Hobby project focused on accuracy and learning

---

# Supported Mappers

| Mapper | IDs |
|---|---|
| **MBC_NONE** | `0x00` |
| **MBC1** | `0x01`, `0x02`, `0x03` |
| **MBC2** | `0x05`, `0x06` |
| **MBC3** | `0x0F`, `0x10`, `0x11`, `0x12`, `0x13` |
| **MBC5** | `0x19`, `0x1A`, `0x1B`, `0x1C`, `0x1D`, `0x1E` |

---
# Development
![dev](https://repobeats.axiom.co/api/embed/dbddf9f765e9b0078408c44f59808bbab1282d88.svg "Repobeats analytics image")
# Screenshots

## Super Mario Land

<img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot1.jpg" alt="Super Mario Land" style="display:block; margin:0 auto; width:300px;">

## Metroid II

<img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot2.jpg" alt="Metroid II" style="display:block; margin:0 auto; width:300px;">

## Pokémon Yellow Version

<img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot3.jpg" alt="Pokémon Yellow Version" style="display:block; margin:0 auto; width:300px;">

## Pokémon Red Version (Web Build)

<img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot4.jpg" alt="Pokémon Red Version" style="display:block; margin:0 auto; width:300px;">
---

# Demo

You can try the WebAssembly version here:

### [Play Demo](https://ts-dev-debug-v2.github.io/gb-emu/wasm/)

---

# Build

Build instructions will be added soon.

---

# TODO

- ~~add saving~~ (Implemented !)
- ~~fix some PPU issues on certain games~~ (Implemented !)
- add more mappers
- add save states

---

# Notes

This project is mainly a hobby project focused on learning emulator development and low-level systems programming.  
It is not intended to compete with mainstream emulators, but it will continue to be maintained and improved over time.
