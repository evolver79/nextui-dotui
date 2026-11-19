// Dot: the shared look of the NextUI paks (Kino, Musicdisk, Tuner). Every number here matches the style guide page.
// Screen: 1024x768 logical, the Brick's 3.2" panel. Nothing is drawn smaller than 22 px.
#pragma once

// --- colour -----------------------------------------------------------------------------------------------------
#define DOT_GROUND_HI   0xa3a3a1   // grey ground, radial from top-left
#define DOT_GROUND_MID  0x6d6d6b
#define DOT_GROUND_LO   0x5c5c5a
#define DOT_CARD        0x0f0f10   // black cards
#define DOT_RAISED      0x1f1f20   // round buttons, icon wells
#define DOT_TRACK       0x444444   // unlit dots
#define DOT_TOGGLE_OFF  0x333333
#define DOT_LINE        0x3a3a3a
#define DOT_PAPER       0xededeb   // selection and the primary button: white = where you are
#define DOT_INK         0x111111   // text on paper
#define DOT_TEXT        0xf2f2f0
#define DOT_LABEL       0x8d8d8a
#define DOT_DIM         0x5a5a58
#define DOT_SCRIM_ALPHA 179        // Night: black at 70% over video
// The accent is a state dot only (playing, paused, resume, live), never focus. One per app:
#define DOT_ACCENT_KINO      0xff6a2b   // follows the video's colour when playing
#define DOT_ACCENT_MUSICDISK 0xff3b30
#define DOT_ACCENT_TUNER     0x34c759
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
#define DOT_ROW_H 78
#define DOT_WELL 46
#define DOT_ROUND 76
#define DOT_ROUND_BIG 96
#define DOT_KEY 58
#define DOT_PILL_H 56
#define DOT_CHIP_H 48
#define DOT_OPT_H 48
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
