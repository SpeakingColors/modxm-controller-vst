# MODX M Controller 🐈

<img src="assets/icon.svg" width="96" align="right" alt="icon">

Controller plugin for the Yamaha MODX M.

## Status
**Early build v0.1 (pre-release on the Releases page).** A MIDI controller plugin (VST3 and standalone) that sends and receives the CC messages listed in [map/params.csv](map/params.csv).

- 15 parameters, taken from: Yamaha MODX M (Data List). Only the plain channel CCs are in the map. Parameter changes and SysEx of the MODX M are not implemented.
- **None are tested on the real device yet.** The map only holds what the source above lists; ranges the manual does not print are assumed to be 0 to 127 and are marked that way in the plugin.
- No SysEx and no patch librarian yet. Nothing is sent that is not in the map.
- Not tested in a DAW yet. Builds in CI on Linux, Windows and macOS.

How it behaves: every entry is an automatable parameter. Moving it sends MIDI to the device, and incoming CC from the device moves it; incoming CC is consumed, never echoed back. At most 16 parameters are sent per audio block so the device is not flooded. Turn on CC receive and transmit on the device.

Build: `sh tests/run.sh` runs the pure C++ tests (no JUCE needed). The plugin needs JUCE 8: clone it into `third_party/JUCE`, then `cmake -B build && cmake --build build`. Changing a parameter means editing `map/params.csv` only.

<p align="center">
  <a href="https://ko-fi.com/studio87"><img src="https://img.shields.io/badge/Support_Studio_87-on_Ko--fi-FF5E5B?style=for-the-badge&logo=kofi&logoColor=white" alt="Support Studio 87 on Ko-fi"></a>
</p>

☕ Free and open source, made by one person. If it helps your music, a coffee on [Ko-fi](https://ko-fi.com/studio87) keeps it coming. 💜

## Docs
- [Interactive mockups](docs/README.md)
- [Roadmap](ROADMAP.md)
- [MIDI data format and address map](docs/MIDI-DATA-FORMAT.md)

## License
AGPL-3.0-or-later, see [LICENSE](LICENSE).

Not affiliated with or endorsed by any hardware maker named here. All trademarks belong to their owners.
