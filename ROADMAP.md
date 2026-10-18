# Roadmap: MODX M controller

Controller for the Yamaha MODX M6, close to Yamaha ESP. MODX2_* is the current design, MODX_* is the v1 backup.

## Next
- [ ] Parameter map from the MODX M data list (SysEx bulk dump and parameter change tables)
- [ ] Real MIDI I/O behind the Home, Part Edit (AWM2, FM-X, AN-X), Effects, Motion, Morph and Library pages
- [ ] Output analyzer page
- [ ] Curved EQ points and remaining icons in the mockup

## Every cat plugin
- Draw through `catui::RenderGate` so meters and animations stop when the window is not in front (audio, automation and MIDI keep running)
- Settings page with MIDI filters on by default, selectable preset/backup folders, UI size reset to 100%
