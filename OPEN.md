# Open questions

Decide after testing. The header and button questions get tested in the `--shots` demo and on the Brick, not on the style guide page.

1. **Header contrast.** Dark header text at top right lands on the darkest part of the ground gradient (~3.5:1).
   Option: move the gradient's light point to top centre (`dot_set_ground(d, DOT_GROUND_TOPCENTRE)`).
   Measured in test/shots (text #161616): top-left light = crumb 6.9:1, count 3.2:1; top centre = 4.9:1 and 4.8:1
   (both pass 4.5:1). Compare kino-browse.png with kino-browse-ground-topcentre.png, then look on the Brick panel.
2. **Button placeholders.** Kino X = play from start, Tuner X = favourite, Tuner Y = TV guide, Musicdisk Y = shuffle.
   Test: try them in the real apps on the Brick before locking them in the README.
3. **GitHub visibility** of nextui-kino-pak, brick-tool and nextui-dotui. Tuner and Musicdisk include Dot as a submodule,
   so their release workflows need nextui-dotui on GitHub, public (or a deploy key).
