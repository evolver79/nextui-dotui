// Dot: see dot.h. Shapes are drawn in software once (anti-aliased with signed distances, icons supersampled),
// stored as white textures and tinted with colour mod, so one texture serves every colour.
#include "dot.h"
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_CAP 768
#define DOT_COLON_FILL .8f   // Doto dot diameter / dot pitch (ROND=100 instance)
typedef struct { Uint32 hash; char *key; SDL_Texture *t; int w, h, ox, oy; Uint32 used; } Ent;

enum { F_DOTO, F_SEMI, F_MED, F_REG, F_MONO, NFONTS };
static const char *FONT_FILE[NFONTS] = { "Doto-Black.ttf", "Geist-SemiBold.ttf", "Geist-Medium.ttf",
                                         "Geist-Regular.ttf", "GeistMono-Medium.ttf" };
static const struct { int font, px; float lh, track; int upper; } ST[DOT_NSTYLES] = {
  [DOT_DISPLAY_XL] = { F_DOTO, 92, 1, 0, 0 },     [DOT_DISPLAY_L] = { F_DOTO, 70, 1, 0, 0 },
  [DOT_DISPLAY_M] = { F_DOTO, 48, 1, 0, 0 },      [DOT_TITLE] = { F_SEMI, 32, 1.15f, 0, 0 },
  [DOT_BODY] = { F_MED, 25, 1.2f, 0, 0 },         [DOT_BODY_S] = { F_REG, 24, 1.3f, 0, 0 },
  [DOT_LABEL_T] = { F_MONO, 22, 1, .14f, 1 },     [DOT_VALUE] = { F_MONO, 22, 1, 0, 0 },
  [DOT_UI] = { F_MED, 24, 1.2f, 0, 0 },           [DOT_CHIP_T] = { F_MED, 23, 1.2f, 0, 0 },
  [DOT_SMALL] = { F_REG, 22, 1.2f, 0, 0 },        [DOT_KEY_T] = { F_MONO, 22, 1, 0, 0 },
  [DOT_KEY_WIDE] = { F_MONO, 22, 1, .06f, 0 },    [DOT_PILL_MONO] = { F_MONO, 22, 1, .1f, 1 },
  [DOT_PANEL_T] = { F_MONO, 26, 1, .14f, 1 },
};

struct Dot {
  SDL_Renderer *r;
  TTF_Font *font[DOT_NSTYLES];
  int asc[DOT_NSTYLES], fh[DOT_NSTYLES];
  Ent cache[CACHE_CAP];
  int ncache;
  Uint32 frame, accent;
  int night, ground, alpha;
  // Doto colon geometry per display style: dot diameter, centre heights above the baseline, advance
  float colon_d[3], colon_lo[3], colon_hi[3];
  int colon_adv[3];
};

// --- cache ------------------------------------------------------------------------------------------------------
static Uint32 fnv(const char *s) { Uint32 h = 2166136261u; while (*s) h = (h ^ (Uint8)*s++) * 16777619u; return h; }

static Ent *cache_get(Dot *d, const char *key) {
  Uint32 h = fnv(key);
  for (int i = 0; i < d->ncache; i++)
    if (d->cache[i].hash == h && !strcmp(d->cache[i].key, key)) { d->cache[i].used = d->frame; return &d->cache[i]; }
  return NULL;
}

// Takes the surface (straight alpha ARGB8888) and frees it.
static Ent *cache_put(Dot *d, const char *key, SDL_Surface *s, int ox, int oy) {
  Ent *e;
  if (d->ncache < CACHE_CAP) e = &d->cache[d->ncache++];
  else {
    e = &d->cache[0];
    for (int i = 1; i < CACHE_CAP; i++) if (d->cache[i].used < e->used) e = &d->cache[i];
    free(e->key); SDL_DestroyTexture(e->t);
  }
  e->hash = fnv(key); e->key = strdup(key); e->used = d->frame;
  e->t = SDL_CreateTextureFromSurface(d->r, s);
  SDL_SetTextureBlendMode(e->t, SDL_BLENDMODE_BLEND);
  e->w = s->w; e->h = s->h; e->ox = ox; e->oy = oy;
  SDL_FreeSurface(s);
  return e;
}

static void blit(Dot *d, Ent *e, int x, int y, Uint32 rgb, int alpha) {
  if (!e || !e->t) return;
  SDL_SetTextureColorMod(e->t, rgb >> 16 & 255, rgb >> 8 & 255, rgb & 255);
  SDL_SetTextureAlphaMod(e->t, alpha * d->alpha / 255);
  SDL_Rect dst = { x + e->ox, y + e->oy, e->w, e->h };
  SDL_RenderCopy(d->r, e->t, NULL, &dst);
}

// --- software shapes --------------------------------------------------------------------------------------------
static SDL_Surface *surf(int w, int h) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w > 0 ? w : 1, h > 0 ? h : 1, 32, SDL_PIXELFORMAT_ARGB8888);
  memset(s->pixels, 0, (size_t)s->pitch * s->h);
  return s;
}
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

static float sd_rrect(float px, float py, float x, float y, float w, float h, float r) {
  float qx = fabsf(px - (x + w / 2)) - (w / 2 - r), qy = fabsf(py - (y + h / 2)) - (h / 2 - r);
  float ox = fmaxf(qx, 0), oy = fmaxf(qy, 0);
  return sqrtf(ox * ox + oy * oy) + fminf(fmaxf(qx, qy), 0) - r;
}
static float cov_rrect(float px, float py, float x, float y, float w, float h, float r) {
  return clampf(.5f - sd_rrect(px, py, x, y, w, h, r), 0, 1);
}

// Straight-alpha "over" of one colour at coverage a.
static void over(Uint32 *p, Uint32 rgb, float a) {
  if (a <= 0) return;
  Uint32 dst = *p;
  float da = (dst >> 24) / 255.f, oa = a + da * (1 - a);
  Uint32 o = 0;
  for (int sh = 0; sh < 24; sh += 8) {
    float c = ((rgb >> sh & 255) * a + (dst >> sh & 255) * da * (1 - a)) / oa;
    o |= (Uint32)(c + .5f) << sh;
  }
  *p = o | (Uint32)(oa * 255 + .5f) << 24;
}

// White rounded rect (or ring when t > 0), alpha = coverage.
static Ent *tex_rrect(Dot *d, int w, int h, int r, int t) {
  char key[64]; snprintf(key, sizeof key, "rr:%d:%d:%d:%d", w, h, r, t);
  Ent *e = cache_get(d, key); if (e) return e;
  SDL_Surface *s = surf(w, h);
  for (int y = 0; y < h; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    for (int x = 0; x < w; x++) {
      float c = cov_rrect(x + .5f, y + .5f, 0, 0, w, h, r);
      if (t > 0) c -= cov_rrect(x + .5f, y + .5f, t, t, w - 2 * t, h - 2 * t, r - t > 0 ? r - t : 0);
      if (c > 0) row[x] = (Uint32)(clampf(c, 0, 1) * 255 + .5f) << 24 | 0xffffff;
    }
  }
  return cache_put(d, key, s, 0, 0);
}

// Box blur of an alpha plane, run three times per axis (close to a Gaussian of the given sigma).
static void blur(float *a, int w, int h, float sigma) {
  int rad = (int)(sqrtf(12 * sigma * sigma / 3 + 1) / 2);
  if (rad < 1) return;
  int n = w > h ? w : h;
  float *tmp = malloc(sizeof(float) * n);
  for (int pass = 0; pass < 6; pass++) {
    int horiz = pass < 3, len = horiz ? w : h, lines = horiz ? h : w, step = horiz ? 1 : w;
    for (int l = 0; l < lines; l++) {
      float *p = a + (horiz ? l * w : l);
      float sum = 0;
      for (int i = -rad; i <= rad; i++) if (i >= 0 && i < len) sum += p[i * step];
      for (int i = 0; i < len; i++) {
        tmp[i] = sum / (2 * rad + 1);
        int add = i + rad + 1, sub = i - rad;
        if (add < len) sum += p[add * step];
        if (sub >= 0) sum -= p[sub * step];
      }
      for (int i = 0; i < len; i++) p[i * step] = tmp[i];
    }
  }
  free(tmp);
}

// Black card with the guide's shadow (0 10px 24px rgba(0,0,0,.28)), baked into one texture with a margin.
#define SHADOW_PAD 46
static Ent *tex_card(Dot *d, int w, int h, int r) {
  char key[64]; snprintf(key, sizeof key, "card:%d:%d:%d", w, h, r);
  Ent *e = cache_get(d, key); if (e) return e;
  int P = SHADOW_PAD, W = w + 2 * P, H = h + 2 * P;
  float *a = calloc((size_t)W * H, sizeof(float));
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) a[y * W + x] = .28f * cov_rrect(x + .5f, y + .5f, P, P + 10, w, h, r);
  blur(a, W, H, 12);
  SDL_Surface *s = surf(W, H);
  for (int y = 0; y < H; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    for (int x = 0; x < W; x++) {
      row[x] = 0;
      over(&row[x], 0x000000, a[y * W + x]);
      over(&row[x], DOT_CARD, cov_rrect(x + .5f, y + .5f, P, P, w, h, r));
    }
  }
  free(a);
  return cache_put(d, key, s, -P, -P);
}

// --- icons (24-unit view box, from the style guide's SVG symbols) -----------------------------------------------
// A shape is rings filled even-odd; an icon is the union of its shapes. Markers: RING, SHAPE, CIRC r then centre, END.
typedef struct { float x, y; } P2;
#define RING {-1, 0}
#define SHAPE {-2, 0}
#define CIRC(cx, cy, r) {-3, r}, {cx, cy}
#define END {-9, 0}
#define RECT(x, y, w, h) {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}
static const P2 IC_PLAY[] = { {7, 4.5f}, {7, 19.5f}, {19.5f, 12}, END };
static const P2 IC_PAUSE[] = { RECT(6, 4, 4.2f, 16), SHAPE, RECT(13.8f, 4, 4.2f, 16), END };
static const P2 IC_BACK[] = { {11, 6}, {11, 18}, {2.5f, 12}, SHAPE, {21, 6}, {21, 18}, {12.5f, 12}, END };
static const P2 IC_FWD[] = { {13, 6}, {13, 18}, {21.5f, 12}, SHAPE, {3, 6}, {3, 18}, {11.5f, 12}, END };
static const P2 IC_PREV[] = { RECT(6, 5, 2.5f, 14), SHAPE, {20, 5}, {20, 19}, {9, 12}, END };
static const P2 IC_NEXT[] = { RECT(15.5f, 5, 2.5f, 14), SHAPE, {4, 5}, {4, 19}, {15, 12}, END };
static const P2 IC_FOLDER[] = { {4.5f, 5}, {9.5f, 5}, {11.5f, 7}, {19.5f, 7}, {20.6f, 7.4f}, {21, 8.5f}, {21, 17.5f},
                                {20.6f, 18.6f}, {19.5f, 19}, {4.5f, 19}, {3.4f, 18.6f}, {3, 17.5f}, {3, 6.5f},
                                {3.4f, 5.4f}, END };
static const P2 IC_FILM[] = { RECT(4, 4, 16, 16), RING, RECT(6, 6, 2, 2), RING, RECT(16, 6, 2, 2), RING,
                              RECT(6, 11, 2, 2), RING, RECT(16, 11, 2, 2), RING, RECT(6, 16, 2, 2), RING,
                              RECT(16, 16, 2, 2), RING, RECT(10, 7, 4, 10), END };
static const P2 IC_NOTE[] = { CIRC(7.5f, 17.75f, 3.5f), SHAPE, RECT(9, 4, 2, 14), SHAPE, RECT(9, 4, 9, 4), END };
static const P2 IC_TV[] = { RECT(3, 6, 18, 12), RING, RECT(5, 8, 14, 8), SHAPE, RECT(8, 20, 8, 1.5f), END };
static const P2 IC_SD[] = { {7, 3}, {15, 3}, {19, 7}, {19, 21}, {7, 21}, RING, RECT(9, 5, 1.5f, 3), RING,
                            RECT(12, 5, 1.5f, 3), END };
static const P2 IC_LEFT[] = { {17, 4}, {17, 20}, {5, 12}, END };
static const P2 IC_RIGHT[] = { {7, 4}, {7, 20}, {19, 12}, END };
static const P2 *ICONS[DOT_NICONS] = { NULL, IC_PLAY, IC_PAUSE, IC_BACK, IC_FWD, IC_PREV, IC_NEXT, IC_FOLDER,
                                       IC_FILM, IC_NOTE, IC_TV, IC_SD, IC_LEFT, IC_RIGHT };

static int inside_icon(const P2 *p, float x, float y) {
  while (p->x != -9) {
    if (p->x == -3) {   // circle
      float r = p->y, dx = x - p[1].x, dy = y - p[1].y;
      if (dx * dx + dy * dy <= r * r) return 1;
      p += 2;
    } else {            // rings until SHAPE or END
      int in = 0;
      while (p->x != -2 && p->x != -9) {
        const P2 *a = p; int n = 0;
        while (a[n].x >= 0) n++;
        for (int i = 0, j = n - 1; i < n; j = i++)
          if ((a[i].y > y) != (a[j].y > y) && x < (a[j].x - a[i].x) * (y - a[i].y) / (a[j].y - a[i].y) + a[i].x) in = !in;
        p += n;
        if (p->x == -1) p++;
      }
      if (in) return 1;
    }
    if (p->x == -2) p++;
  }
  return 0;
}

static Ent *tex_icon(Dot *d, int icon, int size) {
  char key[32]; snprintf(key, sizeof key, "ic:%d:%d", icon, size);
  Ent *e = cache_get(d, key); if (e) return e;
  SDL_Surface *s = surf(size, size);
  const int SS = 4;
  float k = 24.f / size;
  for (int y = 0; y < size; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    for (int x = 0; x < size; x++) {
      int n = 0;
      for (int sy = 0; sy < SS; sy++)
        for (int sx = 0; sx < SS; sx++)
          n += inside_icon(ICONS[icon], (x + (sx + .5f) / SS) * k, (y + (sy + .5f) / SS) * k);
      if (n) row[x] = (Uint32)(n * 255 / (SS * SS)) << 24 | 0xffffff;
    }
  }
  return cache_put(d, key, s, 0, 0);
}

// --- text -------------------------------------------------------------------------------------------------------
static int u8len(const char *s) { Uint8 c = *s; return c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4; }

static void upper(char *dst, const char *s, size_t n) {
  size_t i = 0;
  for (; *s && i + 1 < n; s++, i++) {
    Uint8 c = *s;
    if (c >= 'a' && c <= 'z') c -= 32;
    else if (i > 0 && (Uint8)dst[i - 1] == 0xc3 && c >= 0xa0 && c <= 0xbe && c != 0xb7) c -= 0x20;   // æøå, éü ...
    dst[i] = c;
  }
  dst[i] = 0;
}

static int measure(Dot *d, int st, const char *s) {
  TTF_Font *f = d->font[st];
  int w = 0, h;
  if (ST[st].track == 0) { TTF_SizeUTF8(f, s, &w, &h); return w; }
  float tr = ST[st].track * ST[st].px;
  char ch[5];
  for (const char *p = s; *p; p += u8len(p)) {
    int l = u8len(p); memcpy(ch, p, l); ch[l] = 0;
    int cw; TTF_SizeUTF8(f, ch, &cw, &h);
    w += cw + (int)(tr + .5f);
  }
  return w;
}

static SDL_Surface *render(Dot *d, int st, const char *s) {
  SDL_Color white = { 255, 255, 255, 255 };
  TTF_Font *f = d->font[st];
  if (ST[st].track == 0) return TTF_RenderUTF8_Blended(f, s, white);
  SDL_Surface *out = surf(measure(d, st, s), TTF_FontHeight(f));
  int x = 0, tr = (int)(ST[st].track * ST[st].px + .5f);
  char ch[5];
  for (const char *p = s; *p; p += u8len(p)) {
    int l = u8len(p); memcpy(ch, p, l); ch[l] = 0;
    SDL_Surface *g = TTF_RenderUTF8_Blended(f, ch, white);
    if (g) {
      SDL_SetSurfaceBlendMode(g, SDL_BLENDMODE_NONE);
      SDL_Rect r = { x, 0, g->w, g->h };
      SDL_BlitSurface(g, NULL, out, &r);
      x += g->w + tr;
      SDL_FreeSurface(g);
    }
  }
  return out;
}

// Prepares the string: uppercase when the style says so, cut with an ellipsis to fit maxw.
static void prep(Dot *d, int st, const char *s, int maxw, char *buf, size_t n) {
  if (ST[st].upper) upper(buf, s, n); else snprintf(buf, n, "%s", s);
  if (maxw <= 0 || measure(d, st, buf) <= maxw) return;
  size_t len = strlen(buf);
  char tmp[512];
  while (len > 0) {
    do len--; while (len > 0 && ((Uint8)buf[len] & 0xc0) == 0x80);
    while (len > 0 && buf[len - 1] == ' ') len--;
    snprintf(tmp, sizeof tmp, "%.*s\xe2\x80\xa6", (int)len, buf);
    if (measure(d, st, tmp) <= maxw) break;
  }
  snprintf(buf, n, "%s", tmp);
}

static Ent *tex_text(Dot *d, int st, const char *s, int maxw) {
  char key[600]; snprintf(key, sizeof key, "t%d:%d:%s", st, maxw, s);
  Ent *e = cache_get(d, key); if (e) return e;
  char buf[512]; prep(d, st, s, maxw, buf, sizeof buf);
  SDL_Surface *sf = buf[0] ? render(d, st, buf) : NULL;
  if (!sf) sf = surf(1, TTF_FontHeight(d->font[st]));
  return cache_put(d, key, sf, 0, 0);
}

int dot_line_h(Dot *d, int st) { (void)d; return (int)(ST[st].lh * ST[st].px + .5f); }
int dot_text_w(Dot *d, int st, const char *s) { return tex_text(d, st, s, 0)->w; }

// Draws with the glyph box centred on cy (what CSS does inside a line box or a flex row).
static int text_c(Dot *d, int st, const char *s, int x, int cy, Uint32 rgb, int align, int maxw) {
  Ent *e = tex_text(d, st, s, maxw);
  int dx = align == DOT_CENTER ? x - e->w / 2 : align == DOT_RIGHT ? x - e->w : x;
  blit(d, e, dx, cy - d->fh[st] / 2, rgb, 255);
  return e->w;
}

int dot_text(Dot *d, int st, const char *s, int x, int y, Uint32 rgb, int align, int maxw) {
  return text_c(d, st, s, x, y + dot_line_h(d, st) / 2, rgb, align, maxw);
}

static int display_idx(int st) { return st == DOT_DISPLAY_XL ? 0 : st == DOT_DISPLAY_L ? 1 : st == DOT_DISPLAY_M ? 2 : -1; }

int dot_time(Dot *d, int st, const char *s, int x, int y, Uint32 rgb, int align) {
  int k = display_idx(st);
  if (k < 0) return dot_text(d, st, s, x, y, rgb, align, 0);
  char seg[64];
  int total = 0;
  for (const char *p = s; ; ) {
    const char *c = strchr(p, ':'); size_t n = c ? (size_t)(c - p) : strlen(p);
    snprintf(seg, sizeof seg, "%.*s", (int)n, p);
    if (n) total += dot_text_w(d, st, seg);
    if (!c) break;
    total += d->colon_adv[k]; p = c + 1;
  }
  int px = align == DOT_CENTER ? x - total / 2 : align == DOT_RIGHT ? x - total : x;
  int cy = y + dot_line_h(d, st) / 2, base = cy - d->fh[st] / 2 + d->asc[st];
  for (const char *p = s; ; ) {
    const char *c = strchr(p, ':'); size_t n = c ? (size_t)(c - p) : strlen(p);
    snprintf(seg, sizeof seg, "%.*s", (int)n, p);
    if (n) px += dot_text(d, st, seg, px, y, rgb, DOT_LEFT, 0);
    if (!c) break;
    int r = (int)(d->colon_d[k] / 2 + .5f), ccx = px + d->colon_adv[k] / 2;
    dot_circle(d, ccx, base - (int)(d->colon_lo[k] + .5f), r, rgb);
    dot_circle(d, ccx, base - (int)(d->colon_hi[k] + .5f), r, rgb);
    px += d->colon_adv[k]; p = c + 1;
  }
  return total;
}

// --- setup ------------------------------------------------------------------------------------------------------
Dot *dot_init(SDL_Renderer *r, const char *font_dir, Uint32 accent) {
  if (!TTF_WasInit() && TTF_Init() < 0) return NULL;
  Dot *d = calloc(1, sizeof *d);
  d->r = r; d->accent = accent; d->alpha = 255;
  char path[1024];
  for (int i = 0; i < DOT_NSTYLES; i++) {
    snprintf(path, sizeof path, "%s/%s", font_dir, FONT_FILE[ST[i].font]);
    if (!(d->font[i] = TTF_OpenFont(path, ST[i].px))) { dot_quit(d); return NULL; }
    d->asc[i] = TTF_FontAscent(d->font[i]);
    d->fh[i] = TTF_FontAscent(d->font[i]) - TTF_FontDescent(d->font[i]);
  }
  // Colon: Doto's digits are 7 dot rows high; draw two dots of the digits' size on rows 2 and 4 (from the top).
  int sts[3] = { DOT_DISPLAY_XL, DOT_DISPLAY_L, DOT_DISPLAY_M };
  for (int k = 0; k < 3; k++) {
    int minx, maxx, miny, maxy, adv;
    TTF_GlyphMetrics(d->font[sts[k]], '0', &minx, &maxx, &miny, &maxy, &adv);
    float h = maxy - miny, dd = h / (6 / DOT_COLON_FILL + 1), pitch = (h - dd) / 6, bottom = miny + dd / 2;
    d->colon_d[k] = dd;
    d->colon_lo[k] = bottom + 2 * pitch;
    d->colon_hi[k] = bottom + 4 * pitch;
    TTF_GlyphMetrics(d->font[sts[k]], ':', &minx, &maxx, &miny, &maxy, &d->colon_adv[k]);
  }
  return d;
}

void dot_quit(Dot *d) {
  if (!d) return;
  for (int i = 0; i < d->ncache; i++) { free(d->cache[i].key); SDL_DestroyTexture(d->cache[i].t); }
  for (int i = 0; i < DOT_NSTYLES; i++) if (d->font[i]) TTF_CloseFont(d->font[i]);
  free(d);
}

void dot_frame(Dot *d) { d->frame++; }
void dot_set_accent(Dot *d, Uint32 rgb) { d->accent = rgb; }
Uint32 dot_accent(Dot *d) { return d->accent; }
void dot_set_night(Dot *d, int on) { d->night = on; }
void dot_set_ground(Dot *d, int v) { d->ground = v; }

// --- base -------------------------------------------------------------------------------------------------------
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// The ground's colour at a point (no dither): for things that must blend into it, like rounded image corners.
static Uint32 ground_rgb(Dot *d, float x, float y) {
  const Uint32 stop[3] = { DOT_GROUND_HI, DOT_GROUND_MID, DOT_GROUND_LO };
  float cx = d->ground == DOT_GROUND_TOPCENTRE ? DOT_W / 2.f : 0, dx = (x - cx) / (DOT_W * 1.2f), dy = y / (DOT_H * .9f);
  float t = sqrtf(dx * dx + dy * dy), u = t < .7f ? t / .7f : clampf((t - .7f) / .3f, 0, 1);
  Uint32 a = t < .7f ? stop[0] : stop[1], b = t < .7f ? stop[1] : stop[2], o = 0;
  for (int sh = 0; sh < 24; sh += 8) o |= (Uint32)(lerp(a >> sh & 255, b >> sh & 255, u) + .5f) << sh;
  return o;
}

void dot_ground(Dot *d) {
  char key[32]; snprintf(key, sizeof key, "ground:%d", d->ground);
  Ent *e = cache_get(d, key);
  if (!e) {
    // CSS radial-gradient(120% 90% at <centre>, hi, mid 70%, lo), with a 4x4 ordered dither against banding.
    static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const Uint32 stop[3] = { DOT_GROUND_HI, DOT_GROUND_MID, DOT_GROUND_LO };
    float cx = d->ground == DOT_GROUND_TOPCENTRE ? DOT_W / 2.f : 0, rx = DOT_W * 1.2f, ry = DOT_H * .9f;
    SDL_Surface *s = surf(DOT_W, DOT_H);
    for (int y = 0; y < DOT_H; y++) {
      Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
      for (int x = 0; x < DOT_W; x++) {
        float dx = (x + .5f - cx) / rx, dy = (y + .5f) / ry, t = sqrtf(dx * dx + dy * dy);
        Uint32 a = t < .7f ? stop[0] : stop[1], b = t < .7f ? stop[1] : stop[2];
        float u = t < .7f ? t / .7f : clampf((t - .7f) / .3f, 0, 1), dith = bayer[y & 3][x & 3] / 16.f - .5f;
        Uint32 o = 0xff000000;
        for (int sh = 0; sh < 24; sh += 8)
          o |= (Uint32)clampf(lerp(a >> sh & 255, b >> sh & 255, u) + dith + .5f, 0, 255) << sh;
        row[x] = o;
      }
    }
    e = cache_put(d, key, s, 0, 0);
  }
  blit(d, e, 0, 0, 0xffffff, 255);
}

void dot_scrim(Dot *d) {
  SDL_SetRenderDrawBlendMode(d->r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(d->r, 0, 0, 0, DOT_SCRIM_ALPHA * d->alpha / 255);
  SDL_RenderFillRect(d->r, NULL);
}

void dot_rect(Dot *d, int x, int y, int w, int h, int r, Uint32 rgb, int alpha) {
  blit(d, tex_rrect(d, w, h, r, 0), x, y, rgb, alpha);
}
static void ring(Dot *d, int x, int y, int w, int h, int r, int t, Uint32 rgb) {
  blit(d, tex_rrect(d, w, h, r, t), x, y, rgb, 255);
}
void dot_card(Dot *d, int x, int y, int w, int h) { blit(d, tex_card(d, w, h, DOT_R_CARD), x, y, 0xffffff, 255); }
void dot_circle(Dot *d, int cx, int cy, int r, Uint32 rgb) { dot_rect(d, cx - r, cy - r, 2 * r, 2 * r, r, rgb, 255); }
void dot_adot(Dot *d, int x, int y, Uint32 rgb) { dot_rect(d, x, y, 14, 14, 7, rgb, 255); }
void dot_icon(Dot *d, int icon, int cx, int cy, int size, Uint32 rgb) {
  if (icon <= DOT_I_NONE || icon >= DOT_NICONS) return;
  blit(d, tex_icon(d, icon, size), cx - size / 2, cy - size / 2, rgb, 255);
}

void dot_corners(Dot *d, int x, int y, int w, int h, int r, Uint32 bg) {
  char key[32]; snprintf(key, sizeof key, "cn:%d", r);
  Ent *e = cache_get(d, key);
  if (!e) {
    SDL_Surface *s = surf(r, r);
    for (int py = 0; py < r; py++) {
      Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + py * s->pitch);
      for (int px = 0; px < r; px++) {
        float c = 1 - cov_rrect(px + .5f, py + .5f, 0, 0, 2 * r, 2 * r, r);
        if (c > 0) row[px] = (Uint32)(c * 255 + .5f) << 24 | 0xffffff;
      }
    }
    e = cache_put(d, key, s, 0, 0);
  }
  SDL_SetTextureAlphaMod(e->t, d->alpha);
  const SDL_RendererFlip fl[4] = { SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL,
                                   (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL) };
  for (int i = 0; i < 4; i++) {
    SDL_Rect dst = { i & 1 ? x + w - r : x, i & 2 ? y + h - r : y, r, r };
    Uint32 c = bg == DOT_BG_GROUND ? ground_rgb(d, dst.x + r / 2.f, dst.y + r / 2.f) : bg;
    SDL_SetTextureColorMod(e->t, c >> 16 & 255, c >> 8 & 255, c & 255);
    SDL_RenderCopyEx(d->r, e->t, NULL, &dst, 0, NULL, fl[i]);
  }
}

void dot_poster(Dot *d, SDL_Texture *img, int x, int y, int w, int h, int focus, float progress, int seen, int fresh) {
  int r = DOT_R_POSTER;
  if (focus) { dot_rect(d, x - 8, y - 8, w + 16, h + 16, r + 8, DOT_PAPER, 255); dot_rect(d, x - 4, y - 4, w + 8, h + 8, r + 4, DOT_CARD, 255); }
  SDL_Rect box = { x, y, w, h };
  if (img) {
    SDL_SetTextureColorMod(img, 255, 255, 255); SDL_SetTextureAlphaMod(img, d->alpha);
    SDL_RenderCopy(d->r, img, NULL, &box);
  } else {
    SDL_SetRenderDrawBlendMode(d->r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(d->r, DOT_RAISED >> 16, DOT_RAISED >> 8 & 255, DOT_RAISED & 255, 255);
    SDL_RenderFillRect(d->r, &box);
    dot_icon(d, DOT_I_FILM, x + w / 2, y + h / 2, w / 3, DOT_DIM);
  }
  SDL_SetRenderDrawBlendMode(d->r, SDL_BLENDMODE_BLEND);
  if (seen) { SDL_SetRenderDrawColor(d->r, 0, 0, 0, 120); SDL_RenderFillRect(d->r, &box); }
  if (progress > 0) {   // a card strip along the bottom with dot progress
    SDL_Rect strip = { x, y + h - 30, w, 30 };
    SDL_SetRenderDrawColor(d->r, DOT_CARD >> 16, DOT_CARD >> 8 & 255, DOT_CARD & 255, 225);
    SDL_RenderFillRect(d->r, &strip);
    dot_progress(d, x + 12, y + h - 27, w - 24, progress, 0);
  }
  if (fresh) { dot_circle(d, x + w - 19, y + 19, 11, DOT_CARD); dot_adot(d, x + w - 26, y + 12, d->accent); }
  dot_corners(d, x, y, w, h, r, focus ? DOT_CARD : DOT_BG_GROUND);
}

// --- components -------------------------------------------------------------------------------------------------
void dot_header(Dot *d, const char *crumb, const char *count) {
  Uint32 c = d->night ? DOT_LABEL : 0x161616;
  int cw = count ? dot_text_w(d, DOT_LABEL_T, count) : 0;
  if (crumb) dot_text(d, DOT_LABEL_T, crumb, DOT_MARGIN, 30, c, DOT_LEFT, DOT_W - 2 * DOT_MARGIN - cw - 40);
  if (count) dot_text(d, DOT_LABEL_T, count, DOT_W - DOT_MARGIN, 30, c, DOT_RIGHT, 0);
}

void dot_title(Dot *d, const char *s) { dot_text(d, DOT_DISPLAY_M, s, DOT_MARGIN, 28, DOT_TEXT, DOT_LEFT, 700); }

void dot_row(Dot *d, int x, int y, int w, int icon, const char *name, const char *sub, const char *meta,
             int state, int adot) {
  int sel = state & DOT_FOCUS, off = state & DOT_OFF;
  dot_rect(d, x, y, w, DOT_ROW_H, DOT_R_ROW, sel ? DOT_PAPER : DOT_CARD, 255);
  int tx = x + 16, right = x + w - 24, cy = y + DOT_ROW_H / 2;
  if (icon) {
    dot_circle(d, tx + DOT_WELL / 2, cy, DOT_WELL / 2, sel ? DOT_INK : DOT_RAISED);
    dot_icon(d, icon, tx + DOT_WELL / 2, cy, 22, sel ? DOT_PAPER : 0xd8d8d5);
    tx += DOT_WELL + 18;
  } else tx = x + 24;
  if (meta && *meta) right -= text_c(d, DOT_VALUE, meta, right, cy, sel ? 0x333333 : DOT_LABEL, DOT_RIGHT, 0) + 18;
  if (adot) { dot_adot(d, right - 14, cy - 7, d->accent); right -= 14 + 18; }
  Uint32 nc = sel ? DOT_INK : off ? DOT_DIM : DOT_TEXT;
  if (sub) {
    text_c(d, DOT_BODY, name, tx, y + 9 + 15, nc, DOT_LEFT, right - tx);
    text_c(d, DOT_SMALL, sub, tx, y + 43 + 13, sel ? 0x444444 : DOT_LABEL, DOT_LEFT, right - tx);
  } else text_c(d, DOT_BODY, name, tx, cy, nc, DOT_LEFT, right - tx);
}

static int key_w(Dot *d, const char *key) {
  if (!strcmp(key, "<>")) return 2 * 20 + 46;
  return strlen(key) > 1 ? dot_text_w(d, DOT_KEY_WIDE, key) + 40 : DOT_KEY;
}

int dot_key(Dot *d, int x, int y, const char *key, int primary, int off) {
  int w = key_w(d, key), cy = y + DOT_KEY / 2;
  Uint32 bg = primary ? DOT_PAPER : off ? DOT_RAISED : DOT_CARD, fg = primary ? DOT_INK : off == 1 ? DOT_DIM : DOT_TEXT;
  dot_rect(d, x, y, w, DOT_KEY, DOT_KEY / 2, bg, 255);
  if (!strcmp(key, "<>")) {
    dot_icon(d, DOT_I_LEFT, x + 20 + 9, cy, 18, fg);
    dot_icon(d, DOT_I_RIGHT, x + w - 20 - 9, cy, 18, fg);
  } else text_c(d, strlen(key) > 1 ? DOT_KEY_WIDE : DOT_KEY_T, key, x + w / 2, cy, fg, DOT_CENTER, 0);
  return w;
}

void dot_round(Dot *d, int cx, int cy, int icon, int big, int state) {
  int r = (big ? DOT_ROUND_BIG : DOT_ROUND) / 2, lit = big || (state & DOT_ON);   // DOT_ON: lit at its own size
  if (state & DOT_FOCUS) { dot_circle(d, cx, cy, r + 8, DOT_PAPER); dot_circle(d, cx, cy, r + 4, DOT_CARD); }
  dot_circle(d, cx, cy, r, lit ? DOT_PAPER : DOT_RAISED);
  dot_icon(d, icon, cx, cy, big ? 40 : 30, lit ? DOT_INK : (state & DOT_OFF) ? DOT_DIM : DOT_TEXT);
}

int dot_pill(Dot *d, int x, int y, const char *s, int adot, int mono, int align) {
  int st = mono ? DOT_PILL_MONO : DOT_UI;
  int w = 48 + (adot ? 28 : 0) + dot_text_w(d, st, s);
  if (align == DOT_RIGHT) x -= w; else if (align == DOT_CENTER) x -= w / 2;
  dot_rect(d, x, y, w, DOT_PILL_H, DOT_PILL_H / 2, DOT_CARD, 219);
  int tx = x + 24;
  if (adot) { dot_adot(d, tx, y + 21, d->accent); tx += 28; }
  text_c(d, st, s, tx, y + DOT_PILL_H / 2, DOT_TEXT, DOT_LEFT, 0);
  return w;
}

void dot_toast(Dot *d, const char *s, float alpha) {
  int a = d->alpha;
  d->alpha = (int)(clampf(alpha, 0, 1) * a);
  dot_pill(d, DOT_W / 2, 572, s, 1, 0, DOT_CENTER);
  d->alpha = a;
}

int dot_chip(Dot *d, int x, int y, const char *s, int state) {
  int w = dot_text_w(d, DOT_CHIP_T, s) + 40, h = DOT_CHIP_H, r = DOT_R_CHIP;
  int on = state & DOT_ON, off = state & DOT_OFF;
  if (state & DOT_FOCUS) {
    dot_rect(d, x - 6, y - 6, w + 12, h + 12, r + 6, DOT_PAPER, 255);
    dot_rect(d, x - 3, y - 3, w + 6, h + 6, r + 3, DOT_CARD, 255);
  }
  if (on) dot_rect(d, x, y, w, h, r, DOT_PAPER, 255);
  else ring(d, x, y, w, h, r, 2, off ? 0x2a2a2a : DOT_LINE);
  text_c(d, DOT_CHIP_T, s, x + w / 2, y + h / 2, on ? DOT_INK : off ? DOT_DIM : DOT_TEXT, DOT_CENTER, 0);
  return w;
}

int dot_chips(Dot *d, int x, int y, int w, const char **s, int n, int on, int focus) {
  int cx = x, cy = y;
  for (int i = 0; i < n; i++) {
    int cw = dot_text_w(d, DOT_CHIP_T, s[i]) + 40;
    if (cx > x && cx + cw > x + w) { cx = x; cy += DOT_CHIP_H + 10; }
    cx += dot_chip(d, cx, cy, s[i], (i == on ? DOT_ON : 0) | (i == focus ? DOT_FOCUS : 0)) + 10;
  }
  return cy + DOT_CHIP_H - y;
}

void dot_toggle(Dot *d, int x, int y, int on, int focused) {
  Uint32 bg, knob;
  if (on) { bg = focused ? DOT_INK : DOT_PAPER; knob = focused ? DOT_PAPER : DOT_INK; }
  else { bg = focused ? 0xc9c9c6 : DOT_TOGGLE_OFF; knob = focused ? 0x8a8a87 : 0x777777; }
  dot_rect(d, x, y, DOT_TOGGLE_W, DOT_TOGGLE_H, DOT_TOGGLE_H / 2, bg, 255);
  dot_circle(d, x + (on ? 32 : 4) + 14, y + 18, 14, knob);
}

void dot_slider(Dot *d, int x, int y, float frac, int focused) {
  float lit = clampf(frac, 0, 1) * DOT_SLIDER_W;
  for (int i = 0; i * DOT_SLIDER_PITCH < DOT_SLIDER_W; i++) {
    float c = i * DOT_SLIDER_PITCH + DOT_SLIDER_PITCH / 2.f;
    Uint32 col = c <= lit ? (focused ? DOT_INK : DOT_TEXT) : (focused ? 0xb9b9b6 : DOT_TRACK);
    dot_circle(d, x + (int)c, y + 6, DOT_DOT_R, col);
  }
}

void dot_progress(Dot *d, int x, int y, int w, float frac, int knob) {
  float lit = clampf(frac, 0, 1) * w;
  for (int i = 0; i * DOT_DOT_PITCH + DOT_DOT_PITCH / 2 + DOT_DOT_R <= w; i++) {
    float c = i * DOT_DOT_PITCH + DOT_DOT_PITCH / 2.f;
    dot_circle(d, x + (int)c, y + 12, DOT_DOT_R, c <= lit ? DOT_TEXT : DOT_TRACK);
  }
  if (knob == 2) {   // focused: the same paper ring as a focused round button
    dot_circle(d, x + (int)lit, y + 12, DOT_KNOB / 2 + 8, DOT_PAPER);
    dot_circle(d, x + (int)lit, y + 12, DOT_KNOB / 2 + 4, DOT_CARD);
  }
  if (knob) dot_circle(d, x + (int)lit, y + 12, DOT_KNOB / 2, DOT_TEXT);
}

// Highlight and label of an option row; vw = width of the value at the right (the label is cut before it).
static int opt_bg(Dot *d, int x, int y, int w, const char *label, int state, int vw) {
  int f = state & DOT_FOCUS;
  if (f) dot_rect(d, x - 14, y, w + 28, DOT_OPT_H, 22, DOT_PAPER, 255);
  text_c(d, DOT_UI, label, x, y + DOT_OPT_H / 2, f ? DOT_INK : (state & DOT_OFF) ? DOT_DIM : DOT_TEXT, DOT_LEFT,
         w - vw - 24);
  return f;
}
void dot_opt(Dot *d, int x, int y, int w, const char *label, const char *value, int state) {
  int f = opt_bg(d, x, y, w, label, state, value ? dot_text_w(d, DOT_VALUE, value) : 0);
  if (value) text_c(d, DOT_VALUE, value, x + w, y + DOT_OPT_H / 2, f ? 0x333333 : (state & DOT_OFF) ? DOT_DIM : 0xa9a9a6,
                    DOT_RIGHT, 0);
}
void dot_opt_toggle(Dot *d, int x, int y, int w, const char *label, int on, int state) {
  dot_toggle(d, x + w - DOT_TOGGLE_W, y + (DOT_OPT_H - DOT_TOGGLE_H) / 2, on, opt_bg(d, x, y, w, label, state, DOT_TOGGLE_W));
}
void dot_opt_slider(Dot *d, int x, int y, int w, const char *label, float frac, int state) {
  dot_slider(d, x + w - DOT_SLIDER_W, y + DOT_OPT_H / 2 - 6, frac, opt_bg(d, x, y, w, label, state, DOT_SLIDER_W));
}

int dot_panel(Dot *d, int x, int y, int w, int h, const char *label) {
  dot_card(d, x, y, w, h);
  if (!label) return y + DOT_PAD;
  dot_text(d, DOT_PANEL_T, label, x + 30, y + DOT_PAD, DOT_LABEL, DOT_LEFT, w - 60);
  return y + DOT_PAD + 26 + 20;
}
int dot_panel_h(int content_h) { return DOT_PAD + 26 + 20 + content_h + DOT_PAD; }

void dot_dock(Dot *d, const DotHint *h, int n) {
  if (n > 4) n = 4;
  dot_rect(d, DOT_MARGIN, DOT_DOCK_Y, DOT_W - 2 * DOT_MARGIN, DOT_DOCK_H, DOT_DOCK_H / 2, 0xffffff, d->night ? 31 : 41);
  int total = 0, cy = DOT_DOCK_Y + DOT_DOCK_H / 2;
  for (int i = 0; i < n; i++) total += key_w(d, h[i].key) + 14 + dot_text_w(d, DOT_UI, h[i].text) + (i ? 44 : 0);
  int x = DOT_W / 2 - total / 2;
  for (int i = 0; i < n; i++) {
    x += dot_key(d, x, cy - DOT_KEY / 2, h[i].key, i == 0, 0) + 14;
    x += text_c(d, DOT_UI, h[i].text, x, cy, d->night ? DOT_TEXT : 0x141414, DOT_LEFT, 0) + 44;
  }
}
