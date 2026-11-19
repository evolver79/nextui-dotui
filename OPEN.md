# Open questions

Decide after testing. The header and button questions get tested in the `--shots` demo and on the Brick, not on the style guide page.

1. **Header contrast.** Dark header text at top right lands on the darkest part of the ground gradient (~3.5:1).
   Option: move the gradient's light point to top centre. Test: render both gradients in `--shots`, then look on the Brick panel.
2. **Button placeholders.** Kino X = play from start, Tuner X = favourite, Tuner Y = TV guide, Musicdisk Y = shuffle.
   Test: try them in the real apps on the Brick before locking them in the README.
3. **GitHub visibility** of nextui-kino-pak, brick-tool and nextui-dotui. Tuner and Musicdisk include Dot as a submodule,
   so their release workflows need nextui-dotui on GitHub, public (or a deploy key).
