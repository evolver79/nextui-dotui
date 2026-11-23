// Dot: the shared look of the NextUI paks (Kino, Musicdisk, Tuner). Every number here matches the style guide page.
// Screen: 1024x768 logical, the Brick's 3.2" panel. Nothing is drawn smaller than 22 px.
#pragma once

// --- colour -----------------------------------------------------------------------------------------------------
// The ground: an evening colour per app (DotTheme below), radial from the light point; light text sits on it at 6:1 or
// more. These are the neutral default, for an app that sets no theme.
#define DOT_GROUND_HI   0x5b5b5b
#define DOT_GROUND_MID  0x424242
#define DOT_GROUND_LO   0x333333
#define DOT_CARD        0x0f0f10   // black cards: glass, at DOT_GLASS_A over the ground
#define DOT_GLASS       0x0e1013   // glass cards, rows and the dock: a cool charcoal, so it reads as glass, not brown
#define DOT_GLASS_A     214        // card opacity (84%)
#define DOT_ON_GROUND   0xf2f2f0   // header text and the like straight on the ground (6:1 or more)
#define DOT_RAISED      0x1f1f20   // round buttons, icon wells
#define DOT_TRACK       0x444444   // unlit dots
#define DOT_TOGGLE_OFF  0x333333
#define DOT_LINE        0x3a3a3a
#define DOT_PAPER       0xededeb   // selection and the primary button: white = where you are
#define DOT_INK         0x111111   // text on paper
#define DOT_TEXT        0xf2f2f0
#define DOT_LABEL       0x9d9d9a
#define DOT_DIM         0x7a7a77   // 4.5:1 on a card: disabled, but still readable
#define DOT_SCRIM_ALPHA 179        // Night: black at 70% over video
// The accent is a state dot only (playing, paused, resume, live), never focus. One per app:
#define DOT_ACCENT_KINO      0xff8a3d   // copper-orange; on the cards it follows the video's colour when playing
#define DOT_ACCENT_MUSICDISK 0xff4f9a   // magenta: off the status red, so "playing" never reads as an error
#define DOT_ACCENT_TUNER     0x2fc8d8   // cyan: off the status green, so "live" never reads as "connected"
#define DOT_ACCENT_SPEED     0xbf7bff   // violet

// Each app's look: its ground (three stops at the same lightness in every app) and its accent. dot_set_theme().
typedef struct { unsigned int ground_hi, ground_mid, ground_lo, accent; } DotTheme;
#define DOT_THEME_KINO      { 0x7d4f23, 0x61370d, 0x4d2807, DOT_ACCENT_KINO }        // evening copper
#define DOT_THEME_TUNER     { 0x1e6573, 0x0f4b58, 0x063b46, DOT_ACCENT_TUNER }       // teal
#define DOT_THEME_MUSICDISK { 0x80455d, 0x632f44, 0x4e2235, DOT_ACCENT_MUSICDISK }   // rose
#define DOT_THEME_SPEED     { 0x5a5287, 0x423a6a, 0x332c54, DOT_ACCENT_SPEED }       // violet
// Status, shared by every app: dots, short text, thin outlines and chart marks, never fills; always with a word or icon.
// Each has a darker shade for the paper (selected) surface. Contrast: 6.9-10.5 on card, 4.6-5.2 on paper.
#define DOT_GOOD             0x4fd18b   // won, done, saved, connected
#define DOT_GOOD_ON_PAPER    0x177a43
#define DOT_WARN             0xf2b544   // stale, close to a limit, needs a look
#define DOT_WARN_ON_PAPER    0x8a5800
#define DOT_CRITICAL         0xff6b61   // lost, failed, no signal
#define DOT_CRITICAL_ON_PAPER 0xc0261c
#define DOT_INFO             0x6fa8ff   // a note, a sample, something new
#define DOT_INFO_ON_PAPER    0x2f63c8
// Series: categories, not states (chart lines, home/draw/away). In this order. All 8+ : 1 on card.
#define DOT_SERIES_1         0x6fa8ff   // blue
#define DOT_SERIES_2         0xe88cff   // violet
#define DOT_SERIES_3         0x4fd1c5   // teal
#define DOT_SERIES_4         0xe6c27a   // sand
#define DOT_SERIES_5         0xc9c9c4   // grey

// --- type (px) --------------------------------------------------------------------------------------------------
enum DotType {
  DOT_DISPLAY_XL,   // Doto Black 92: clock, time
  DOT_DISPLAY_L,    // Doto Black 70
  DOT_DISPLAY_M,    // Doto Black 48: screen titles
  DOT_TITLE,        // Geist SemiBold 32
  DOT_BODY,         // Geist Medium 25: list rows
  DOT_BODY_S,       // Geist Regular 24
  DOT_LABEL_T,      // Geist Mono Medium 22, uppercase, tracking .14em
  DOT_VALUE,        // Geist Mono Medium 22: numbers at the right of rows
  DOT_NTYPES
};

// --- space and shape (px) ---------------------------------------------------------------------------------------
#define DOT_W 1024
#define DOT_H 768
#define DOT_MARGIN 40
#define DOT_GAP 16
#define DOT_PAD 24
#define DOT_R_CARD 30
#define DOT_R_ROW 26
#define DOT_R_CHIP 24
#define DOT_R_POSTER 16
#define DOT_POSTER_W 176   // grid: 5 x 2 posters at a 192 x 280 pitch fill content 96-640
#define DOT_POSTER_H 264
#define DOT_ROW_H 96
#define DOT_WELL 56
#define DOT_ROUND 76
#define DOT_ROUND_BIG 96
#define DOT_KEY 62
#define DOT_PILL_H 60
#define DOT_CHIP_H 56
#define DOT_OPT_H 64
#define DOT_TOGGLE_W 64
#define DOT_TOGGLE_H 36
#define DOT_DOT_R 4        // progress dots
#define DOT_DOT_PITCH 18
#define DOT_KNOB 24
#define DOT_SLIDER_PITCH 17
#define DOT_SLIDER_W 170
// layout bands
#define DOT_HEADER_Y 34    // crumb / count baseline band 0-80
#define DOT_CONTENT_TOP 96
#define DOT_CONTENT_BOTTOM 640
#define DOT_DOCK_Y 652
#define DOT_DOCK_H 88
// motion (ms)
#define DOT_FADE_MS 200
#define DOT_AUTOHIDE_MS 3000
