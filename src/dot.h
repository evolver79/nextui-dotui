// Dot: the shared UI of the NextUI paks (Kino, Musicdisk, Tuner). C + SDL2 renderer + SDL2_ttf.
// Positions are the style guide's: x/y is the top-left of the element's box, as in the page's CSS, in 1024x768 px.
// Text and shapes are rendered once to textures and cached (key = what and how), so drawing every frame is cheap.
#pragma once
#include <SDL2/SDL.h>
#include "dot_tokens.h"

typedef struct Dot Dot;

// Extra text styles used inside components (the public scale is DotType in dot_tokens.h).
enum { DOT_UI = DOT_NTYPES,   // Geist Medium 24: dock, pills, option rows
       DOT_CHIP_T,            // Geist Medium 23: chips
       DOT_SMALL,             // Geist Regular 22: second line of a row
       DOT_KEY_T,             // Geist Mono Medium 22: key caps
       DOT_KEY_WIDE,          // Geist Mono Medium 22, tracking .06em: wide key caps
       DOT_PILL_MONO,         // Geist Mono Medium 22, tracking .1em, uppercase: fact pills
       DOT_PANEL_T,           // Geist Mono Medium 26, tracking .14em, uppercase: card headings
       DOT_NSTYLES };

enum DotAlign { DOT_LEFT, DOT_CENTER, DOT_RIGHT };
enum DotState { DOT_NORMAL = 0, DOT_FOCUS = 1, DOT_OFF = 2, DOT_ON = 4 };   // DOT_ON combines with FOCUS on chips
enum DotIcon { DOT_I_NONE, DOT_I_PLAY, DOT_I_PAUSE, DOT_I_BACK, DOT_I_FWD, DOT_I_PREV, DOT_I_NEXT, DOT_I_FOLDER,
               DOT_I_FILM, DOT_I_NOTE, DOT_I_TV, DOT_I_SD, DOT_I_LEFT, DOT_I_RIGHT, DOT_NICONS };
enum DotGround { DOT_GROUND_TOPLEFT, DOT_GROUND_TOPCENTRE };   // OPEN.md question 1: where the light point sits

typedef struct { const char *key; const char *text; } DotHint;   // key "A", "B", "X", "Y", "L1 R1", "<>" = ◀ ▶

// --- setup ------------------------------------------------------------------------------------------------------
Dot *dot_init(SDL_Renderer *r, const char *font_dir, Uint32 accent);   // NULL on failure (SDL_GetError says why)
void dot_quit(Dot *d);
void dot_frame(Dot *d);                      // call once per frame before drawing (ages the cache)
void dot_set_accent(Dot *d, Uint32 rgb);     // Kino: follows the video's colour
Uint32 dot_accent(Dot *d);
void dot_set_night(Dot *d, int on);          // Night scheme: over video; header text and dock change
void dot_set_ground(Dot *d, int variant);    // DOT_GROUND_*

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
// List row (h 78). sub = second line or NULL; meta = right-aligned value or NULL; adot = accent dot before meta.
void dot_row(Dot *d, int x, int y, int w, int icon, const char *name, const char *sub, const char *meta,
             int state, int adot);
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
// Padded card with a heading on top; returns the y where its content starts (option rows follow at 56 pitch).
int  dot_panel(Dot *d, int x, int y, int w, int h, const char *label);
int  dot_panel_h(int content_h);   // card height for that much content under a heading
#define DOT_OPT_PITCH 56
void dot_dock(Dot *d, const DotHint *h, int n);                  // at most four hints
