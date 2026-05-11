
<p align="left">
  <img src="https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/gameboy.webp" width="180" />

  <span>
     <h1>GBEMU</h1>
    <h1> Gameboy Emulator writen in C++</h1>

  </span>
</p>
# Features
- Load .gb roms
- Input
- cycle-accurate APU (see src/gb_apu.cpp)

## supported mappers
- MBC_NONE (0x00) — no banking
- MBC1 (0x01, 0x02, 0x03)
- MBC2 (0x05, 0x06)
- MBC3 (0x0F, 0x10, 0x11, 0x12, 0x13)
- MBC5 (0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E)

## Screenshots 

### Super Mario Land
![Super Mario Land](https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot1.jpg)

### Metroid II
![Metroid II](https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot2.jpg)

### Pokémon Yellow Version
![Pokémon Yellow Version](https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot3.jpg)

### Pokémon Red Version (running in the web version)
![Pokémon Red Version](https://raw.githubusercontent.com/TS-DEV-DEBUG-V2/gb-emu/main/assets/screenshot4.jpg)
## This isnt supposed to be mainstream its just a hobby project but i will maintain it 
### Demo
- you can play a demo of it (working with pokemon fire red) [here](https://ts-dev-debug-v2.github.io/gb-emu/wasm/) or if you are on a VERY old browser, click [here](https://ts-dev-debug-v2.github.io/gb-emu/js/)
## Build
- gonna add build instructions soon 
# TODO
- ~~add saving~~ (fully implemented now)
- ~~fix some ppu stuff on some games (still VERY functionable but in for example super mario land the timer text jiggles abit)~~ (fully implemented now)
- add more mappers
- add save states
