# Roadmap: MODX M controller

Controller for the Yamaha MODX M6, close to Yamaha ESP. MODX2_* is the current design, MODX_* is the v1 backup.

## Done
- [x] Plugin skeleton: parameter map from the manual, CC and NRPN send/receive, host automation, cat-ui look, CI on three systems
- [x] Release packaging: installers (Windows, macOS, Linux Flatpak extension), uninstallers and PDF manual built by the release workflow (test builds, not tested on real machines)

## Next
- [ ] Test every map entry on the real device and flip it to verified (`hw` column in map/params.csv)
- [ ] Test the VST3 in Bitwig and Studio One on Linux
- [ ] Two-CC controls: find out which CC is the coarse one on the device

- [ ] Parameter map from the MODX M data list (SysEx bulk dump and parameter change tables)
- [ ] Real MIDI I/O behind the Home, Part Edit (AWM2, FM-X, AN-X), Effects, Motion, Morph and Library pages
- [ ] Output analyzer page
- [ ] Curved EQ points and remaining icons in the mockup

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
