// Renders the style guide's screens and a component sheet with the Dot library, to BMPs in the given directory,
// using SDL's software renderer (no window, so it runs headless in Docker). Compare with docs/style.html.
//   shots <out_dir> [font_dir]
#include "../src/dot.h"
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

static SDL_Surface *S;
static SDL_Renderer *R;
static Dot *D;
static const char *OUT;

static void save(const char *name) {
  SDL_RenderPresent(R);
  char p[512]; snprintf(p, sizeof p, "%s/%s.bmp", OUT, name);
  SDL_SaveBMP(S, p);
  printf("%s\n", p);
}

static void fill(int x, int y, int w, int h, Uint32 c) {
  SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(R, c >> 16, c >> 8 & 255, c & 255, 255);
  SDL_Rect r = { x, y, w, h }; SDL_RenderFillRect(R, &r);
}
static Uint32 mix(Uint32 a, Uint32 b, float t) {
  Uint32 o = 0;
  for (int s = 0; s < 24; s += 8) o |= (Uint32)((a >> s & 255) + ((int)(b >> s & 255) - (int)(a >> s & 255)) * t) << s;
  return o;
}
// Stand-in for a video frame: the guide's sunset (warm) or fjord (cool) picture.
static void video(int x, int y, int w, int h, int cool) {
  Uint32 top = cool ? 0x0e2433 : 0x2a1a3a, hor = cool ? 0x3b8fb3 : 0xff8a4c, sea = cool ? 0x0d1a22 : 0x24161f;
  int hy = (int)(h * .62f);
  for (int i = 0; i < hy; i++) fill(x, y + i, w, 1, mix(top, hor, (float)i / hy));
  fill(x, y + hy, w, h - hy, sea);
  SDL_Rect clip = { x, y, w, h }; SDL_RenderSetClipRect(R, &clip);
  int sr = (int)(w * .11f);
  dot_circle(D, x + (int)(w * .69f), y + (int)(h * .30f) + sr, sr, cool ? 0xd9f3ff : 0xffd38a);
  int h1 = (int)(h * .34f), h2 = (int)(h * .26f);
  dot_rect(D, x - (int)(w * .1f), y + hy - h1, (int)(w * .7f), h1 * 2, h1, cool ? 0x12303f : 0x3b2230, 255);
  dot_rect(D, x + (int)(w * .35f), y + hy - h2, (int)(w * .8f), h2 * 2, h2, cool ? 0x173948 : 0x3a2230, 255);
  fill(x, y + hy, w, h - hy, sea);
  for (int i = hy + 14; i < h; i += 16) dot_rect(D, x, y + i, w, 2, 0, 0xffffff, 15);
  SDL_RenderSetClipRect(R, NULL);
}

static void kino_browse(int ground) {
  dot_set_ground(D, ground);
  dot_ground(D);
  dot_header(D, "SD card / Series / Northern", "8 items");
  dot_card(D, 40, 96, 460, 528);
  video(56, 112, 428, 240, 1);
  dot_corners(D, 56, 112, 428, 240, 20, DOT_CARD);
  dot_text(D, DOT_TITLE, "Episode 3 · Ice", 70, 374, DOT_TEXT, DOT_LEFT, 400);
  dot_text(D, DOT_LABEL_T, "720p · HEVC · MKV", 70, 424, DOT_LABEL, DOT_LEFT, 400);
  dot_adot(D, 70, 488, 0x3b8fb3);
  dot_text(D, DOT_LABEL_T, "Resume", 70 + 14 + 18, 484, DOT_TEXT, DOT_LEFT, 0);
  dot_time(D, DOT_DISPLAY_L, "18:05", 68, 512, DOT_TEXT, DOT_LEFT);
  dot_progress(D, 70, 590, 400, .42f, 0);
  const char *n[5] = { "Episode 1 · Dark", "Episode 2 · Snow", "Episode 3 · Ice", "Episode 4 · Thaw", "Episode 5 · Light" };
  const char *m[5] = { "SEEN", "SEEN", "42%", "43:10", "44:02" };
  for (int i = 0; i < 5; i++) dot_row(D, 524, 96 + 94 * i, 460, DOT_I_FILM, n[i], NULL, m[i], i == 2 ? DOT_FOCUS : 0, 0);
  DotHint h[] = { { "A", "Resume" }, { "X", "From start" }, { "B", "Back" }, { "Y", "Options" } };
  dot_dock(D, h, 4);
}

static void musicdisk_list(void) {
  dot_set_ground(D, DOT_GROUND_TOPLEFT);
  dot_set_accent(D, DOT_ACCENT_MUSICDISK);
  dot_ground(D);
  dot_header(D, "scene.org / music / artists", "212");
  const char *n[5] = { "aceman", "captain", "dreamfish", "jogeir liljedahl", "lizardking" };
  const char *m[5] = { "31", "18", "9", "44", "27" };
  for (int i = 0; i < 5; i++) dot_row(D, 40, 96 + 94 * i, 944, DOT_I_FOLDER, n[i], NULL, m[i], i == 1 ? DOT_FOCUS : 0, 0);
  dot_pill(D, 40, 572, "Playing · Space Debris", 1, 0, DOT_LEFT);
  DotHint h[] = { { "A", "Open" }, { "B", "Back" }, { "L1 R1", "Letter" }, { "Y", "Random" } };
  dot_dock(D, h, 4);
}

static void tuner_list(void) {
  dot_set_accent(D, DOT_ACCENT_TUNER);
  dot_ground(D);
  dot_header(D, "TV / All channels", "48");
  const char *n[5] = { "NRK1", "NRK2", "TV 2 Direkte", "TVNorge", "NRK Super" };
  const char *s[5] = { "Dagsrevyen · next 19:45 Norge i dag", "Urix · next 20:05 Dokumentar: Havet",
                       "Nyhetene · next 19:30 Været", "Kompani Lauritzen · next 20:30 71° nord",
                       "Supermorgen · next 07:30 Fantorangen" };
  const char *m[5] = { "1", "2", "3", "4", "5" };
  for (int i = 0; i < 5; i++) dot_row(D, 40, 96 + 94 * i, 944, DOT_I_TV, n[i], s[i], m[i], i == 0 ? DOT_FOCUS : 0, i == 2);
  dot_pill(D, 40, 572, "Next on Two · 20:50 Studio", 1, 1, DOT_LEFT);
  DotHint h[] = { { "A", "Watch" }, { "X", "Favourite" }, { "B", "Back" }, { "Y", "Guide" } };
  dot_dock(D, h, 4);
}

static void kino_playing(void) {
  dot_set_accent(D, DOT_ACCENT_KINO);
  video(0, 0, 1024, 768, 0);
  dot_pill(D, 40, 36, "The Long Harbour", 1, 0, DOT_LEFT);
  dot_pill(D, 984, 36, "1080p · 5.1", 0, 1, DOT_RIGHT);
  dot_card(D, 40, 500, 944, 232);
  dot_time(D, DOT_DISPLAY_XL, "41:12", 76, 526, DOT_TEXT, DOT_LEFT);
  dot_text(D, DOT_LABEL_T, "/ 1:52:08", 370, 546, DOT_LABEL, DOT_LEFT, 0);
  dot_adot(D, 370, 588, dot_accent(D));
  dot_text(D, DOT_LABEL_T, "Paused", 402, 584, DOT_TEXT, DOT_LEFT, 0);
  dot_round(D, 702, 570, DOT_I_BACK, 0, 0);
  dot_round(D, 806, 570, DOT_I_PLAY, 1, DOT_FOCUS);
  dot_round(D, 900, 570, DOT_I_FWD, 0, 0);
  dot_progress(D, 76, 644, 872, .37f, 1);
  dot_text(D, DOT_LABEL_T, "◀ ▶ 10 s · L1 R1 1 min · Y options", 76, 690, DOT_LABEL, DOT_LEFT, 0);
}

static void options(int night) {
  if (night) { video(0, 0, 1024, 768, 0); dot_scrim(D); } else dot_ground(D);
  dot_set_night(D, night);
  dot_title(D, "OPTIONS");
  dot_header(D, NULL, "B close");
  int y = dot_panel(D, 40, 96, 464, 170, "Aspect");
  const char *a[5] = { "Original", "4:3", "16:9", "Stretch", "Zoom" };
  dot_chips(D, 70, y, 404, a, 5, 0, -1);
  y = dot_panel(D, 40, 282, 464, 254, "Picture");
  dot_opt_slider(D, 70, y, 404, "Brightness", .5f, DOT_FOCUS);
  dot_opt_slider(D, 70, y + 56, 404, "Contrast", .6f, 0);
  dot_opt(D, 70, y + 112, 404, "Deinterlace", "AUTO", 0);
  y = dot_panel(D, 520, 96, 464, 254, "Subtitles");
  dot_opt_toggle(D, 550, y, 404, "Show", 1, 0);
  dot_opt(D, 550, y + 56, 404, "Track", "ENGLISH", 0);
  dot_opt(D, 550, y + 112, 404, "Prefer", "ENG, NOR", 0);
  y = dot_panel(D, 520, 366, 464, 170, "Brick");
  dot_opt_toggle(D, 550, y, 404, "Lights follow video", 0, 0);
  DotHint h[] = { { "A", "Change" }, { "<>", "Adjust" }, { "B", "Close" } };
  dot_dock(D, h, 3);
  dot_set_night(D, 0);
}

// Stand-in poster: a two-colour gradient with the title in dots, 176 x 264.
static SDL_Texture *poster(const char *title, Uint32 top, Uint32 bottom) {
  SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, DOT_POSTER_W, DOT_POSTER_H);
  SDL_SetRenderTarget(R, t);
  for (int i = 0; i < DOT_POSTER_H; i++) fill(0, i, DOT_POSTER_W, 1, mix(top, bottom, (float)i / DOT_POSTER_H));
  dot_circle(D, 120, 90, 46, mix(bottom, 0xffffff, .55f));
  dot_text(D, DOT_DISPLAY_M, title, 14, 190, 0xffffff, DOT_LEFT, 150);
  SDL_SetRenderTarget(R, NULL);
  return t;
}

static void jellyfin_grid(void) {
  dot_set_accent(D, DOT_ACCENT_KINO);
  dot_ground(D);
  static const char *t[10] = { "DUNE", "HARBOUR", "NORTH", "ORBIT", "TIDE", "ECHO", "FJORD", "AURORA", "RALLY", "CABIN" };
  static const Uint32 c[10][2] = { { 0xd9a35b, 0x5a2f14 }, { 0x2f5d7c, 0x0d1a22 }, { 0x8fb8c9, 0x22323d }, { 0x3a2d6b, 0x0c0a1f },
    { 0x1f8a8a, 0x08302f }, { 0x9b3b3b, 0x2a0d10 }, { 0x4f7d4a, 0x14240f }, { 0x3bb38f, 0x1b1340 }, { 0xe0662b, 0x301208 },
    { 0xa58d6a, 0x2b2218 } };
  for (int i = 0; i < 10; i++) {
    SDL_Texture *p = poster(t[i], c[i][0], c[i][1]);
    int x = 40 + (i % 5) * 192, y = 96 + (i / 5) * 280;
    dot_poster(D, p, x, y, DOT_POSTER_W, DOT_POSTER_H, i == 1, i == 1 ? .37f : i == 6 ? .8f : 0, i == 4 || i == 9, i == 2);
    SDL_DestroyTexture(p);
  }
  dot_text(D, DOT_TITLE, "The Long Harbour", 40, 22, 0x161616, DOT_LEFT, 600);
  dot_text(D, DOT_LABEL_T, "2021 · 1h 52m · 2 / 48", 984, 30, 0x161616, DOT_RIGHT, 0);
  DotHint h[] = { { "A", "Resume" }, { "L1 R1", "Page" }, { "Y", "Sort" }, { "B", "Back" } };
  dot_dock(D, h, 4);
}

static void components(void) {
  dot_set_accent(D, DOT_ACCENT_KINO);
  dot_ground(D);
  dot_header(D, "Components", "sheet");
  int x = 40;
  x += dot_key(D, x, 80, "A", 1, 0) + 12; x += dot_key(D, x, 80, "B", 0, 0) + 12; x += dot_key(D, x, 80, "X", 0, 1) + 12;
  x += dot_key(D, x, 80, "L1 R1", 0, 0) + 12; x += dot_key(D, x, 80, "<>", 0, 0) + 12; x += dot_key(D, x, 80, "MENU", 0, 0) + 30;
  dot_round(D, x + 38, 109, DOT_I_PREV, 0, 0); dot_round(D, x + 142, 109, DOT_I_PAUSE, 1, DOT_FOCUS);
  dot_round(D, x + 246, 109, DOT_I_NEXT, 0, DOT_OFF);
  x = 40;
  x += dot_chip(D, x, 168, "Original", DOT_ON) + 14; x += dot_chip(D, x, 168, "16:9", DOT_FOCUS) + 14;
  x += dot_chip(D, x, 168, "Zoom", DOT_ON | DOT_FOCUS) + 14; x += dot_chip(D, x, 168, "Off", DOT_OFF) + 24;
  dot_toggle(D, x, 174, 1, 0); dot_toggle(D, x + 80, 174, 0, 0);
  dot_slider(D, x + 170, 186, .5f, 0);
  dot_pill(D, 40, 236, "The Long Harbour", 1, 0, DOT_LEFT);
  dot_pill(D, 400, 236, "1080p · 5.1", 0, 1, DOT_LEFT);
  dot_progress(D, 620, 252, 364, .6f, 1);
  dot_row(D, 40, 310, 460, DOT_I_FOLDER, "A very long folder name that will not fit in the row", NULL, "128", 0, 0);
  dot_row(D, 524, 310, 460, DOT_I_NOTE, "Space Debris", "Captain · 1990 · MOD", "4:12", DOT_FOCUS, 1);
  dot_row(D, 40, 404, 460, DOT_I_SD, "Card unavailable", NULL, NULL, DOT_OFF, 0);
  dot_row(D, 524, 404, 460, 0, "No icon, accent dot", NULL, "LIVE", 0, 1);
  dot_card(D, 40, 498, 944, 130);
  dot_time(D, DOT_DISPLAY_XL, "09:35", 70, 516, DOT_TEXT, DOT_LEFT);
  dot_time(D, DOT_DISPLAY_L, "1:52:08", 380, 527, DOT_TEXT, DOT_LEFT);
  dot_time(D, DOT_DISPLAY_M, "12:00", 760, 538, DOT_LABEL, DOT_LEFT);
  dot_text(D, DOT_DISPLAY_M, "41:12", 760, 580, DOT_DIM, DOT_LEFT, 0);   // Doto's own colon, for comparison
  DotHint h[] = { { "A", "Play" }, { "B", "Back" } };
  dot_dock(D, h, 2);
  dot_toast(D, "Saved where you stopped", 1);
}

int main(int argc, char **argv) {
  OUT = argc > 1 ? argv[1] : ".";
  const char *fonts = argc > 2 ? argv[2] : "fonts";
  S = SDL_CreateRGBSurfaceWithFormat(0, DOT_W, DOT_H, 32, SDL_PIXELFORMAT_ARGB8888);
  R = SDL_CreateSoftwareRenderer(S);
  if (!R || !(D = dot_init(R, fonts, DOT_ACCENT_KINO))) { printf("init: %s\n", SDL_GetError()); return 1; }
  TTF_Font *mono = TTF_OpenFont("fonts/GeistMono-Medium.ttf", 22);
  printf("Geist Mono has ◀ %d, ▶ %d, · %d, … %d\n", TTF_GlyphIsProvided(mono, 0x25c0), TTF_GlyphIsProvided(mono, 0x25b6),
         TTF_GlyphIsProvided(mono, 0xb7), TTF_GlyphIsProvided(mono, 0x2026));
  struct { const char *name; void (*f)(void); } shots[] = { { "components", components }, { "jellyfin-grid", jellyfin_grid } };
  Uint32 t0 = SDL_GetTicks();
  kino_browse(DOT_GROUND_TOPLEFT); save("kino-browse");
  kino_browse(DOT_GROUND_TOPCENTRE); save("kino-browse-ground-topcentre");
  musicdisk_list(); save("musicdisk-list");
  tuner_list(); save("tuner-list");
  kino_playing(); save("kino-playing");
  options(0); save("options");
  options(1); save("options-night");
  for (unsigned i = 0; i < sizeof shots / sizeof *shots; i++) { dot_frame(D); shots[i].f(); save(shots[i].name); }
  printf("all shots in %u ms (first draws bake the textures)\n", SDL_GetTicks() - t0);
  dot_quit(D);
  return 0;
}
