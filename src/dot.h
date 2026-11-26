// Dot: the shared UI of the NextUI paks (Kino, Musicdisk, Tuner, Speed). C + SDL2 renderer + SDL2_ttf.
// Positions are the style guide's: x/y is the top-left of the element's box, as in the page's CSS, in 1024x768 px.
// Text and shapes are rendered once to textures and cached (key = what and how), so drawing every frame is cheap.
#pragma once
#include <SDL2/SDL.h>
#include "dot_tokens.h"

typedef struct Dot Dot;

// Extra text styles used inside components (the public scale is DotType in dot_tokens.h).
enum { DOT_UI = DOT_NTYPES,   // Geist Medium 30: dock, pills, option rows
       DOT_CHIP_T,            // Geist Medium 28: chips
       DOT_SMALL,             // Geist Medium 26: second line of a row
       DOT_KEY_T,             // Geist Mono Medium 26: key caps
       DOT_KEY_WIDE,          // Geist Mono Medium 24, tracking .04em: wide key caps
       DOT_PILL_MONO,         // Geist Mono Medium 24, tracking .08em, uppercase: fact pills
       DOT_PANEL_T,           // Geist Mono Medium 26, tracking .1em, uppercase: card headings
       DOT_VALUE_L,           // Geist Mono Medium 32: columns of numbers and notes (Musicdisk's pattern)
       DOT_NSTYLES };

enum DotAlign { DOT_LEFT, DOT_CENTER, DOT_RIGHT };
enum DotState { DOT_NORMAL = 0, DOT_FOCUS = 1, DOT_OFF = 2, DOT_ON = 4, DOT_STAR = 8 };   // DOT_STAR: a row's favourite star   // DOT_ON combines with FOCUS on chips
enum DotIcon { DOT_I_NONE, DOT_I_PLAY, DOT_I_PAUSE, DOT_I_BACK, DOT_I_FWD, DOT_I_PREV, DOT_I_NEXT, DOT_I_FOLDER,
               DOT_I_FILM, DOT_I_NOTE, DOT_I_TV, DOT_I_SD, DOT_I_LEFT, DOT_I_RIGHT, DOT_I_STAR, DOT_NICONS };
enum DotGround { DOT_GROUND_TOPLEFT, DOT_GROUND_TOPCENTRE };   // OPEN.md question 1: where the light point sits

typedef struct { const char *key; const char *text; } DotHint;   // key "A", "B", "X", "Y", "L1 R1", "<>" = ◀ ▶

// --- setup ------------------------------------------------------------------------------------------------------
// The screen size, before dot_init and before making the window: dot_screen_detect() finds it (DOT_SCREEN=WxH in the
// environment, else the display's mode when it's the Brick's 1024x768 or the Smart Pro's 1280x720, else 1024x768) and
// sets it; dot_screen() sets it outright. Then DOT_W and DOT_H are the window's and the logical size.
void dot_screen(int w, int h);
void dot_screen_detect(void);
Dot *dot_init(SDL_Renderer *r, const char *font_dir, Uint32 accent);   // NULL on failure (SDL_GetError says why)
void dot_quit(Dot *d);
void dot_frame(Dot *d);                      // call once per frame before drawing (ages the cache)
void dot_set_accent(Dot *d, Uint32 rgb);     // Kino: follows the video's colour
Uint32 dot_accent(Dot *d);
void dot_set_night(Dot *d, int on);          // Night scheme: over video; header text and dock change
void dot_set_ground(Dot *d, int variant);    // DOT_GROUND_*
void dot_set_theme(Dot *d, const DotTheme *t);   // the app's ground and accent (DOT_THEME_*); the default is neutral
// Something else drawn behind instead of the ground (a picture): its colour at a point, for rounded corners. NULL: the ground.
void dot_set_backdrop(Dot *d, Uint32 (*fn)(float x, float y));

// --- base -------------------------------------------------------------------------------------------------------
void dot_ground(Dot *d);                                     // full-screen grey ground (baked once)
void dot_scrim(Dot *d);                                      // Night: black at 70% over whatever is drawn
void dot_rect(Dot *d, int x, int y, int w, int h, int r, Uint32 rgb, int alpha);    // rounded, anti-aliased
void dot_card(Dot *d, int x, int y, int w, int h);           // black card, radius 30, soft shadow
void dot_circle(Dot *d, int cx, int cy, int r, Uint32 rgb);
void dot_adot(Dot *d, int x, int y, Uint32 rgb);             // 14 px state dot, top-left at x,y
void dot_icon(Dot *d, int icon, int cx, int cy, int size, Uint32 rgb);
// Rounds an image's corners by painting the outside of a radius-r corner in bg (the colour behind the image).
void dot_corners(Dot *d, int x, int y, int w, int h, int r, Uint32 bg);   // bg DOT_BG_GROUND: the ground there
#define DOT_BG_GROUND 0xff000000
#define DOT_BG_GLASS 0xfe000000   // on a glass card: the card over the ground
// Poster/cover on the ground, radius 16: focus = the round-button ring; progress > 0 = dot progress on a strip
// along the bottom; seen = dimmed; fresh = accent dot top-right. img NULL = a placeholder.
void dot_poster(Dot *d, SDL_Texture *img, int x, int y, int w, int h, int focus, float progress, int seen, int fresh);

// Text. y is the top of the CSS line box of that style. Returns the drawn width. maxw > 0 cuts with an ellipsis.
int dot_text(Dot *d, int style, const char *s, int x, int y, Uint32 rgb, int align, int maxw);
int dot_text_w(Dot *d, int style, const char *s);
int dot_line_h(Dot *d, int style);
// Dot-matrix time ("41:12"): Doto with its colon drawn as two round dots (Doto's own colon looks like a cross).
int dot_time(Dot *d, int style, const char *s, int x, int y, Uint32 rgb, int align);

// --- components -------------------------------------------------------------------------------------------------
void dot_header(Dot *d, const char *crumb, const char *count);   // crumb left, count right, y 30
void dot_title(Dot *d, const char *s);                           // Display M screen title at 40,28 (options)
// List row (h 96). sub = second line or NULL; meta = right-aligned value or NULL; adot = accent dot before meta.
void dot_row(Dot *d, int x, int y, int w, int icon, const char *name, const char *sub, const char *meta,
             int state, int adot);
// More below: a list that goes on past the screen draws the next row too, at its usual place, between dot_peek and
// dot_peek_end. Only its top DOT_PEEK px show, cut off, so it's clear there is more to scroll to.
#define DOT_PEEK 40
void dot_peek(Dot *d, int y);   // y: the peeking row's top
void dot_peek_end(Dot *d);
int  dot_key(Dot *d, int x, int y, const char *key, int primary, int off);   // off 1 disabled, 2 on a card; returns width
void dot_round(Dot *d, int cx, int cy, int icon, int big, int state);   // state: DOT_FOCUS ring, DOT_ON lit, DOT_OFF
int  dot_pill(Dot *d, int x, int y, const char *s, int adot, int mono, int align);   // align: x is left/right edge
void dot_toast(Dot *d, const char *s, float alpha);              // bottom centre
int  dot_chip(Dot *d, int x, int y, const char *s, int state);   // returns width
int  dot_chips(Dot *d, int x, int y, int w, const char **s, int n, int on, int focus);   // wraps; returns height
void dot_toggle(Dot *d, int x, int y, int on, int focused);
void dot_slider(Dot *d, int x, int y, float frac, int focused);  // 170 x 12
void dot_progress(Dot *d, int x, int y, int w, float frac, int knob);   // h 24; knob 1 shown, 2 focused
// Option row inside a padded card: x,w are the card's content box; the focus highlight reaches 14 px past it.
void dot_opt(Dot *d, int x, int y, int w, const char *label, const char *value, int state);
void dot_opt_toggle(Dot *d, int x, int y, int w, const char *label, int on, int state);
void dot_opt_slider(Dot *d, int x, int y, int w, const char *label, float frac, int state);
// Padded card with a heading on top; returns the y where its content starts (option rows follow at 72 pitch).
int  dot_panel(Dot *d, int x, int y, int w, int h, const char *label);
int  dot_panel_h(int content_h);   // card height for that much content under a heading
#define DOT_OPT_PITCH 72
void dot_dock(Dot *d, const DotHint *h, int n);                  // at most four hints
// A full-bleed band along the bottom edge (over video: the watch info), square, solid card colour, hairline on top.
void dot_band(Dot *d, int y, int h);
// A card in the middle of the screen for a moment (picking an audio track): icon in an accent well, a label (mono,
// uppercase) over a value, and n dots with the at-th lit (n < 2: none). Comes in, stays, goes (DOT_NOTICE_MS in
// all, from since). Returns 0 once it has gone.
int dot_notice(Dot *d, int icon, const char *label, const char *value, int at, int n, Uint32 since);
// Waiting, in the middle of the screen (tuning in, buffering): icon in an accent well with a ring of dots running
// round it (spin), a label over a value. show: drawn every frame, it fades in and out with s (DOT_SLIDE_MS).
typedef struct DotSlide { float v; Uint32 at; } DotSlide;   // (dot_slide, under motion)
void dot_busy(Dot *d, DotSlide *s, int show, int icon, const char *label, const char *value, int spin);
// A switch between two (or a few) choices: a pill split into halves, the chosen one paper; the paper slides over
// when the choice changes (s keeps where it is). focused: a light ring round it.
typedef struct { float pos; Uint32 at; } DotSwitch;
void dot_switch(Dot *d, DotSwitch *s, int x, int y, int w, const char *const *labels, int n, int on, int focused);
// A picker in the middle of the screen (audio and subtitles while watching): a card with a switch on top when there
// are tabs (ntabs > 1), and option rows below, the focused one paper, the one in use with an accent dot. Fades in
// and out with s (show). At most 5 rows in view; the list scrolls with the focus.
typedef struct { DotSlide slide; DotSwitch sw; int top; } DotPicker;
void dot_picker(Dot *d, DotPicker *p, int show, const char *const *tabs, int ntabs, int tab,
                const char *const *items, int n, int cursor, int in_use);
// Subtitles: each line white on its own dark box, wrapped to the screen, centred, the last line's box ending at
// bottom. Lines in text are split by \n.
void dot_subtitle(Dot *d, const char *text, int bottom);
// Stats over the whole screen, the picture dimmed behind: "LABEL|value" lines, the block in the middle.
void dot_stats(Dot *d, char lines[][96], int n);

// --- motion -------------------------------------------------------------------------------------------------------
// One curve and a few timings for every app (style guide, Motion). It all runs on SDL_GetTicks(); with
// dot_set_still on (screenshots) everything is drawn where it ends up.
#define DOT_RISE 50          // px a card rises coming in (from below its place), and again going (above it)
#define DOT_IN_MS 320        // a card coming in
#define DOT_OUT_MS 250       // a card going
#define DOT_SLIDE_MS 250     // the band sliding up from the bottom edge, or down
#define DOT_SWAP_MS 300      // new content in something that stays up: the old fades out, then the new in
#define DOT_LIST_MS 55       // list rows and the highlight: their ease's time constant (settled in about 0.2 s)
#define DOT_NOTICE_MS 1900   // a notice: 200 in, then up, 300 out
void dot_set_still(Dot *d, int on);
void dot_set_alpha(Dot *d, float a);        // what's drawn next at this opacity (0-1; set 1 again after)
float dot_ease_out(float t);                // cubic ease-out, t clamped to 0-1
// A card's way in and out: in and out (0-1, how far each has gone, linear). Sets the opacity; returns the card's
// y offset (DOT_RISE below its place at the start, DOT_RISE above at the end).
int dot_rise(Dot *d, float in, float out);
// Something that slides in and out (the band): up says where it should be. Returns how far up it is, eased (0
// hidden). Not drawn for a moment (another screen): it comes in from hidden.
float dot_slide(Dot *d, DotSlide *s, int up);
// New content while it's showing: give the content now on (a pointer standing for it) and whether it's showing.
// Returns which to draw, the old one or the new, and sets *alpha for it.
typedef struct { const void *from, *to; Uint32 at; } DotSwap;
const void *dot_swap(Dot *d, DotSwap *s, const void *now, int showing, float *alpha);
// A list that moves: rows at the 108 pitch from the content's top, `rows` of them in view and, with peek, the next
// row's top under them (dot_peek). The rows slide to `top`, the paper highlight to `cursor`; row(ud, i, y, sel)
// draws row i at y, plain or (sel) focused, clipped. key: which list (another key, or a long jump: no slide).
typedef struct { int key; float top, cur; Uint32 at; } DotList;
typedef void (*DotRowFn)(void *ud, int i, int y, int sel);
void dot_list(Dot *d, DotList *l, int key, int n, int rows, int top, int cursor, int x, int w, int peek, DotRowFn row, void *ud);
