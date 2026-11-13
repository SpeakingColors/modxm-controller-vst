# Roadmap: MODX M controller

Controller for the Yamaha MODX M6, close to Yamaha ESP. MODX2_* is the current design, MODX_* is the v1 backup.

## Done
- [x] Plugin skeleton: parameter map from the manual, CC and NRPN send/receive, host automation, cat-ui look, CI on three systems
- [x] Release packaging: installers (Windows, macOS, Linux Flatpak extension), uninstallers and PDF manual built by the release workflow (test builds, not tested on real machines)

## Next
- [ ] Test every map entry on the real device and flip it to verified (`hw` column in map/params.csv)
- [ ] Test the VST3 in Bitwig and Studio One on Linux
- [ ] Two-CC controls: find out which CC is the coarse one on the device

- [x] Parameter map from the MODX M data list: 308 SysEx parameters generated and size-checked (not tested on the device)
- [ ] Rest of the data list: System, Controller Box, Normal Part Element, FM-X and AN-X parts, effect parameters (need the extractor fixed for those blocks)
- [x] Home page ported natively from the mockup (generated from MODX2_Home with tools/extract_mockup.js and tools/gen_home_layout.py; render check 98% pixel match)
- [ ] Home: wire the part table (per part name, pan, volume, mute, solo), effect type menus and the Master EQ graph
- [ ] Port the other nine pages the same way, one page at a time
- [ ] Real MIDI I/O behind the Home, Part Edit (AWM2, FM-X, AN-X), Effects, Motion, Morph and Library pages
- [ ] Output analyzer page
- [ ] Curved EQ points and remaining icons in the mockup

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%

## Wired on the Home page (2026-11-02, untested on real hardware)
- Part table: mute, volume, pan of parts 1-16 (pages 1-8 / 9-16), read back from the device
- Header switches (Rev, Var, MFX, Master EQ), tempo (optional DAW tempo sync), all knobs, scenes
- Gear menu: Read all values from the device, Send all values, Send the DAW tempo
- Master EQ graph (v0.2.1): 5 bands, +-12 dB, drag a dot for gain and frequency, double click resets, wheel changes gain. Frequency steps use about 6 per octave (32 Hz to 16 kHz), taken from the Data List ranges; the curve is a drawn estimate, not the device's exact response
- Live value bubble while dragging any knob, bar or EQ dot
- Not wired yet: part names and engine badges (need bulk dump parsing), Solo and Assign 8 (no parameter in the Data List), effect type and preset lists, Audio tab
- To add to the mockup so it stays in sync: mute-on style, blank part rows
