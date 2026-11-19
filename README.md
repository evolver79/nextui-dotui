# Dot

The shared look of the NextUI paks Kino, Musicdisk and Tuner (TrimUI Brick, 1024x768): black cards on grey,
white for where you are, one accent dot for what is happening, dot-matrix numbers and titles.

Added to each pak as a git submodule (`ui/dot`). C + SDL2 + SDL2_ttf.

- `src/dot_tokens.h`: colours, type scale, sizes and layout bands (the same numbers as the style guide page).
- `fonts/`: Doto (round dots, static Black/Bold instances of the variable font), Geist, Geist Mono. All SIL OFL.

Buttons: A confirm/play, B back/close, X secondary, Y options, L1/R1 page or jump, SELECT switch view,
START now playing, MENU quit. The dock shows only what works on the current screen, at most four hints.
