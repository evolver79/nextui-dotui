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
  // Sized for the Brick's 400 ppi panel: body text 32 px is a 2 mm letter, nothing smaller than 24 px.
  [DOT_DISPLAY_XL] = { F_DOTO, 92, 1, 0, 0 },     [DOT_DISPLAY_L] = { F_DOTO, 70, 1, 0, 0 },
  [DOT_DISPLAY_M] = { F_DOTO, 52, 1, 0, 0 },      [DOT_TITLE] = { F_SEMI, 40, 1.15f, 0, 0 },
  [DOT_BODY] = { F_MED, 32, 1.2f, 0, 0 },         [DOT_BODY_S] = { F_MED, 28, 1.3f, 0, 0 },
  [DOT_LABEL_T] = { F_MONO, 28, 1, .08f, 1 },     [DOT_VALUE] = { F_MONO, 28, 1, 0, 0 },
  [DOT_UI] = { F_MED, 30, 1.2f, 0, 0 },           [DOT_CHIP_T] = { F_MED, 28, 1.2f, 0, 0 },
  [DOT_SMALL] = { F_MED, 26, 1.2f, 0, 0 },        [DOT_KEY_T] = { F_MONO, 26, 1, 0, 0 },
  [DOT_KEY_WIDE] = { F_MONO, 24, 1, .04f, 0 },    [DOT_PILL_MONO] = { F_MONO, 24, 1, .08f, 1 },
  [DOT_PANEL_T] = { F_MONO, 26, 1, .1f, 1 },
  [DOT_VALUE_L] = { F_MONO, 32, 1, 0, 0 },
};

struct Dot {
  SDL_Renderer *r;
  TTF_Font *font[DOT_NSTYLES];
  int asc[DOT_NSTYLES], fh[DOT_NSTYLES];
  Ent cache[CACHE_CAP];
  int ncache;
  Uint32 frame, accent;
  Uint32 (*backdrop)(float x, float y);   // set: what is behind instead of the ground (for corners)
  Uint32 g[3];   // the ground's stops (the theme's)
  int night, ground, alpha, still;
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
  // Glass, but not glass: the body is translucent (the ground's colour comes through), with a soft light sheen at the
  // top and a thin light rim brighter along the top edge. The shadow only shows outside the card.
  for (int y = 0; y < H; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    float t = clampf((y - P) / (float)h, 0, 1);
    for (int x = 0; x < W; x++) {
      float in = cov_rrect(x + .5f, y + .5f, P, P, w, h, r);
      float rim = in - cov_rrect(x + .5f, y + .5f, P + 1.5f, P + 1.5f, w - 3, h - 3, r - 1.5f);
      row[x] = 0;
      over(&row[x], 0x000000, a[y * W + x] * (1 - in));
      over(&row[x], DOT_GLASS, in * DOT_GLASS_A / 255.f);
      over(&row[x], 0xffffff, in * clampf(.07f * (1 - t / .45f), 0, 1));
      over(&row[x], 0xffffff, rim * (.22f - .14f * t));
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
static const P2 IC_STAR[] = { {12.00f, 2.80f}, {14.41f, 9.48f}, {21.51f, 9.71f}, {15.90f, 14.07f}, {17.88f, 20.89f}, {12.00f, 16.90f}, {6.12f, 20.89f}, {8.10f, 14.07f}, {2.49f, 9.71f}, {9.59f, 9.48f}, END };
static const P2 IC_LEFT[] = { {17, 4}, {17, 20}, {5, 12}, END };
static const P2 IC_RIGHT[] = { {7, 4}, {7, 20}, {19, 12}, END };
static const P2 *ICONS[DOT_NICONS] = { NULL, IC_PLAY, IC_PAUSE, IC_BACK, IC_FWD, IC_PREV, IC_NEXT, IC_FOLDER,
                                       IC_FILM, IC_NOTE, IC_TV, IC_SD, IC_LEFT, IC_RIGHT, IC_STAR };

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

// Text too long for its room, scrolling (a focused title): it holds for DOT_SCROLL_HOLD ms, then moves left at
// DOT_SCROLL_PX px a second until it has come round again (a gap, then the start), and holds again. Each text keeps its
// own clock while it's drawn; not drawn for a moment, it starts again from the beginning. Still (screenshots): cut.
#define SCROLL_GAP 64
static int text_scroll_c(Dot *d, int st, const char *s, int x, int cy, Uint32 rgb, int maxw) {
  int w = dot_text_w(d, st, s);
  if (maxw <= 0 || w <= maxw || d->still) return text_c(d, st, s, x, cy, rgb, DOT_LEFT, maxw);
  static struct { unsigned key; Uint32 since, seen; } slot[6];
  unsigned key = (unsigned)st * 2654435761u;
  for (const char *c = s; *c; c++) key = key * 33u + (unsigned char)*c;
  Uint32 now = SDL_GetTicks();
  int k = -1, old = 0;
  for (int i = 0; i < 6; i++) {
    if (slot[i].key == key && now - slot[i].seen < 250) { k = i; break; }
    if (slot[i].seen < slot[old].seen) old = i;
  }
  if (k < 0) { k = old; slot[k].key = key; slot[k].since = now; }
  slot[k].seen = now;
  int loop = w + SCROLL_GAP;
  Uint32 cycle = DOT_SCROLL_HOLD + (Uint32)(loop * 1000 / DOT_SCROLL_PX), t = (now - slot[k].since) % cycle;
  int off = t < DOT_SCROLL_HOLD ? 0 : (int)((t - DOT_SCROLL_HOLD) * DOT_SCROLL_PX / 1000);
  SDL_Rect was, room = { x, cy - d->fh[st], maxw, 2 * d->fh[st] }, c;
  int clipped = SDL_RenderIsClipEnabled(d->r);
  if (clipped) { SDL_RenderGetClipRect(d->r, &was); if (!SDL_IntersectRect(&was, &room, &c)) return maxw; } else c = room;
  SDL_RenderSetClipRect(d->r, &c);
  text_c(d, st, s, x - off, cy, rgb, DOT_LEFT, 0);
  if (off) text_c(d, st, s, x - off + loop, cy, rgb, DOT_LEFT, 0);
  SDL_RenderSetClipRect(d->r, clipped ? &was : NULL);
  return maxw;
}
int dot_text_scroll(Dot *d, int st, const char *s, int x, int y, Uint32 rgb, int maxw) {
  return text_scroll_c(d, st, s, x, y + dot_line_h(d, st) / 2, rgb, maxw);
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
int dot_screen_w = 1024, dot_screen_h = 768;
void dot_screen(int w, int h) { if (w > 0 && h > 0) { dot_screen_w = w; dot_screen_h = h; } }
void dot_screen_detect(void) {
  int w = 0, h = 0;
  const char *e = getenv("DOT_SCREEN");   // for shots, and to try a size on a desktop
  if (e && sscanf(e, "%dx%d", &w, &h) == 2 && w >= 640 && h >= 480) { dot_screen(w, h); return; }
  SDL_DisplayMode m;
  if (SDL_GetCurrentDisplayMode(0, &m) == 0) {
    if (m.w < m.h) { int t = m.w; m.w = m.h; m.h = t; }   // a portrait panel (the Smart Pro's is 720x1280 inside)
    if ((m.w == 1024 && m.h == 768) || (m.w == 1280 && m.h == 720)) { dot_screen(m.w, m.h); return; }
  }
  dot_screen(1024, 768);
}

Dot *dot_init(SDL_Renderer *r, const char *font_dir, Uint32 accent) {
  if (!TTF_WasInit() && TTF_Init() < 0) return NULL;
  Dot *d = calloc(1, sizeof *d);
  d->r = r; d->accent = accent; d->alpha = 255;
  d->g[0] = DOT_GROUND_HI; d->g[1] = DOT_GROUND_MID; d->g[2] = DOT_GROUND_LO;
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
void dot_set_theme(Dot *d, const DotTheme *t) {
  d->g[0] = t->ground_hi; d->g[1] = t->ground_mid; d->g[2] = t->ground_lo; d->accent = t->accent;
}
void dot_set_backdrop(Dot *d, Uint32 (*fn)(float x, float y)) { d->backdrop = fn; }

// --- base -------------------------------------------------------------------------------------------------------
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// The ground's colour at a point (no dither): for things that must blend into it, like rounded image corners.
static Uint32 ground_rgb(Dot *d, float x, float y) {
  if (d->backdrop) return d->backdrop(x, y);
  const Uint32 *stop = d->g;
  float cx = d->ground == DOT_GROUND_TOPCENTRE ? DOT_W / 2.f : 0, dx = (x - cx) / (DOT_W * 1.2f), dy = y / (DOT_H * .9f);
  float t = sqrtf(dx * dx + dy * dy), u = t < .7f ? t / .7f : clampf((t - .7f) / .3f, 0, 1);
  Uint32 a = t < .7f ? stop[0] : stop[1], b = t < .7f ? stop[1] : stop[2], o = 0;
  for (int sh = 0; sh < 24; sh += 8) o |= (Uint32)(lerp(a >> sh & 255, b >> sh & 255, u) + .5f) << sh;
  return o;
}

void dot_ground(Dot *d) {
  char key[48]; snprintf(key, sizeof key, "ground:%d:%06x:%06x:%06x", d->ground, (unsigned)d->g[0], (unsigned)d->g[1], (unsigned)d->g[2]);
  Ent *e = cache_get(d, key);
  if (!e) {
    // CSS radial-gradient(120% 90% at <centre>, hi, mid 70%, lo), with a 4x4 ordered dither against banding.
    static const int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    const Uint32 *stop = d->g;
    float cx = d->ground == DOT_GROUND_TOPCENTRE ? DOT_W / 2.f : 0, rx = DOT_W * 1.2f, ry = DOT_H * .9f;
    SDL_Surface *s = surf(DOT_W, DOT_H);
    for (int y = 0; y < DOT_H; y++) {
      Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
      for (int x = 0; x < DOT_W; x++) {
        float dx = (x + .5f - cx) / rx, dy = (y + .5f) / ry, t = sqrtf(dx * dx + dy * dy);
        Uint32 a = t < .7f ? stop[0] : stop[1], b = t < .7f ? stop[1] : stop[2];
        float u = t < .7f ? t / .7f : clampf((t - .7f) / .3f, 0, 1), dith = (bayer[y & 3][x & 3] / 16.f - .5f) * 3;   // +-1.5: 6-bit panels
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
void dot_card(Dot *d, int x, int y, int w, int h) {
  if (d->night) dot_rect(d, x, y, w, h, DOT_R_CARD, DOT_CARD, 255);   // over video: solid, so nothing shows through
  blit(d, tex_card(d, w, h, DOT_R_CARD), x, y, 0xffffff, 255);
}
// A glass row or pill: translucent body and a light hairline rim.
static void glass(Dot *d, int x, int y, int w, int h, int r) {
  blit(d, tex_rrect(d, w, h, r, 0), x, y, DOT_GLASS, DOT_GLASS_A * d->alpha / 255);
  blit(d, tex_rrect(d, w, h, r, 1), x, y, 0xffffff, 34 * d->alpha / 255);
}
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
    Uint32 c = bg;
    if (bg == DOT_BG_GROUND || bg == DOT_BG_GLASS) c = ground_rgb(d, dst.x + r / 2.f, dst.y + r / 2.f);
    if (bg == DOT_BG_GLASS) {   // the card over that ground (no sheen: pictures sit below the top)
      Uint32 m = 0; float ga = DOT_GLASS_A / 255.f;
      for (int sh = 0; sh < 24; sh += 8) m |= (Uint32)lerp(c >> sh & 255, DOT_CARD >> sh & 255, ga) << sh;
      c = m;
    }
    SDL_SetTextureColorMod(e->t, c >> 16 & 255, c >> 8 & 255, c & 255);
    SDL_RenderCopyEx(d->r, e->t, NULL, &dst, 0, NULL, fl[i]);
  }
}

// The focus ring round a cover: paper 8 px out, a card-coloured gap 4 px wide inside it.
static void poster_ring(Dot *d, int x, int y, int w, int h) {
  int r = DOT_R_POSTER;
  dot_rect(d, x - 8, y - 8, w + 16, h + 16, r + 8, DOT_PAPER, 255); dot_rect(d, x - 4, y - 4, w + 8, h + 8, r + 4, DOT_CARD, 255);
}
void dot_poster(Dot *d, SDL_Texture *img, int x, int y, int w, int h, int focus, float progress, int seen, int fresh) {
  int r = DOT_R_POSTER;
  if (focus) poster_ring(d, x, y, w, h);
  SDL_Rect box = { x, y, w, h };
  if (img) {
    SDL_SetTextureColorMod(img, 255, 255, 255); SDL_SetTextureAlphaMod(img, d->alpha);
    SDL_SetTextureBlendMode(img, d->alpha < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);   // fading (a peek): blended
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
  Uint32 c = d->night ? DOT_LABEL : DOT_ON_GROUND;
  int cw = count ? dot_text_w(d, DOT_LABEL_T, count) : 0;
  if (crumb) dot_text(d, DOT_LABEL_T, crumb, DOT_MARGIN, 30, c, DOT_LEFT, DOT_W - 2 * DOT_MARGIN - cw - 40);
  if (count) dot_text(d, DOT_LABEL_T, count, DOT_W - DOT_MARGIN, 30, c, DOT_RIGHT, 0);
}

void dot_title(Dot *d, const char *s) { dot_text(d, DOT_DISPLAY_M, s, DOT_MARGIN, 28, DOT_TEXT, DOT_LEFT, 700); }


void dot_peek(Dot *d, int y) { SDL_Rect c = { 0, y, DOT_W, DOT_PEEK }; SDL_RenderSetClipRect(d->r, &c); }
void dot_peek_end(Dot *d) { SDL_RenderSetClipRect(d->r, NULL); }

void dot_row(Dot *d, int x, int y, int w, int icon, const char *name, const char *sub, const char *meta,
             int state, int adot) {
  int sel = state & DOT_FOCUS, off = state & DOT_OFF;
  if (sel) dot_rect(d, x, y, w, DOT_ROW_H, DOT_R_ROW, DOT_PAPER, 255); else glass(d, x, y, w, DOT_ROW_H, DOT_R_ROW);
  int tx = x + 16, right = x + w - 24, cy = y + DOT_ROW_H / 2;
  if (icon) {
    if (sel) dot_circle(d, tx + DOT_WELL / 2, cy, DOT_WELL / 2, DOT_INK);
    else dot_rect(d, tx, cy - DOT_WELL / 2, DOT_WELL, DOT_WELL, DOT_WELL / 2, 0xffffff, 26 * d->alpha / 255);
    dot_icon(d, icon, tx + DOT_WELL / 2, cy, 26, sel ? DOT_PAPER : 0xd8d8d5);
    tx += DOT_WELL + 18;
  } else tx = x + 24;
  if (meta && *meta) right -= text_c(d, DOT_VALUE, meta, right, cy, sel ? 0x333333 : DOT_LABEL, DOT_RIGHT, 0) + 18;
  if (adot) { dot_adot(d, right - 14, cy - 7, d->accent); right -= 14 + 18; }
  if (state & DOT_STAR) { dot_icon(d, DOT_I_STAR, right - 11, cy, 22, sel ? DOT_INK : DOT_TEXT); right -= 22 + 16; }
  Uint32 nc = sel ? DOT_INK : off ? DOT_DIM : DOT_TEXT;
  // the focused row's name scrolls when it's too long for the row
  if (sub) {
    if (sel) text_scroll_c(d, DOT_BODY, name, tx, y + 32, nc, right - tx); else text_c(d, DOT_BODY, name, tx, y + 32, nc, DOT_LEFT, right - tx);
    text_c(d, DOT_SMALL, sub, tx, y + 68, sel ? 0x444444 : DOT_LABEL, DOT_LEFT, right - tx);
  } else if (sel) text_scroll_c(d, DOT_BODY, name, tx, cy, nc, right - tx);
  else text_c(d, DOT_BODY, name, tx, cy, nc, DOT_LEFT, right - tx);
}

static int key_w(Dot *d, const char *key) {
  if (!strcmp(key, "<>")) return 2 * 20 + 46;
  return strlen(key) > 1 ? dot_text_w(d, DOT_KEY_WIDE, key) + 40 : DOT_KEY;
}

int dot_key(Dot *d, int x, int y, const char *key, int primary, int off) {
  int w = key_w(d, key), cy = y + DOT_KEY / 2;
  Uint32 bg = primary ? DOT_PAPER : off ? DOT_RAISED : DOT_CARD, fg = primary ? DOT_INK : off == 1 ? DOT_DIM : DOT_TEXT;
  if (!primary && off >= 2) dot_rect(d, x, y, w, DOT_KEY, DOT_KEY / 2, 0xffffff, 40 * d->alpha / 255);   // on dark glass
  else dot_rect(d, x, y, w, DOT_KEY, DOT_KEY / 2, bg, 255);
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
  if (adot) { dot_adot(d, tx, y + (DOT_PILL_H - 14) / 2, d->accent); tx += 28; }
  text_c(d, st, s, tx, y + DOT_PILL_H / 2, DOT_TEXT, DOT_LEFT, 0);
  return w;
}

void dot_toast(Dot *d, const char *s, float alpha) {
  int a = d->alpha;
  d->alpha = (int)(clampf(alpha, 0, 1) * a);
  dot_pill(d, DOT_W / 2, DOT_DOCK_Y - 92, s, 1, 0, DOT_CENTER);   // 572 on the Brick
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
  else blit(d, tex_rrect(d, w, h, r, 2), x, y, 0xffffff, (off ? 30 : 64) * d->alpha / 255);   // a light outline: reads on glass
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
  else { bg = focused ? 0xc9c9c6 : DOT_TOGGLE_OFF; knob = focused ? 0x8a8a87 : 0x9a9a97; }
  if (!on && !focused) dot_rect(d, x, y, DOT_TOGGLE_W, DOT_TOGGLE_H, DOT_TOGGLE_H / 2, 0xffffff, 46 * d->alpha / 255);   // on glass
  else dot_rect(d, x, y, DOT_TOGGLE_W, DOT_TOGGLE_H, DOT_TOGGLE_H / 2, bg, 255);
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
  // Full bleed along the bottom, edge to edge, no corners or rim: dark glass the ground shows through.
  dot_rect(d, 0, DOT_DOCK_Y, DOT_W, DOT_H - DOT_DOCK_Y, 0, DOT_GLASS, 140 * d->alpha / 255);
  int total = 0, cy = DOT_DOCK_Y + DOT_DOCK_H / 2;
  int gap = 44, room = DOT_W - 2 * DOT_MARGIN - 48;
  for (int i = 0; i < n; i++) total += key_w(d, h[i].key) + 14 + dot_text_w(d, DOT_UI, h[i].text);
  if (n > 1 && total + gap * (n - 1) > room) gap = (room - total) / (n - 1) < 20 ? 20 : (room - total) / (n - 1);
  total += gap * (n - 1);
  int x = DOT_W / 2 - total / 2;
  for (int i = 0; i < n; i++) {
    x += dot_key(d, x, cy - DOT_KEY / 2, h[i].key, i == 0, 3) + 14;
    x += text_c(d, DOT_UI, h[i].text, x, cy, DOT_TEXT, DOT_LEFT, 0) + gap;
  }
}

// --- motion -------------------------------------------------------------------------------------------------------
void dot_set_still(Dot *d, int on) { d->still = on; }
float dot_alpha(Dot *d) { return d->alpha / 255.0f; }
void dot_set_alpha(Dot *d, float a) { d->alpha = a <= 0 ? 0 : a >= 1 ? 255 : (int)(a * 255); }
float dot_ease_out(float t) { t = clampf(t, 0, 1); return 1 - (1 - t) * (1 - t) * (1 - t); }

int dot_rise(Dot *d, float in, float out) {
  if (d->still) { in = 1; out = 0; }
  float a = dot_ease_out(in), b = dot_ease_out(out);
  dot_set_alpha(d, a * (1 - b));
  return (int)lroundf(DOT_RISE * (1 - a) - DOT_RISE * b);
}

float dot_slide(Dot *d, DotSlide *s, int up) {
  Uint32 now = SDL_GetTicks();
  float dt = (float)(now - s->at);
  if (!s->at || now - s->at > 200) { s->v = 0; dt = 16; }
  s->at = now;
  float step = dt / DOT_SLIDE_MS;
  s->v = d->still ? (up ? 1 : 0) : up ? clampf(s->v + step, 0, 1) : clampf(s->v - step, 0, 1);
  return dot_ease_out(s->v);
}

const void *dot_swap(Dot *d, DotSwap *s, const void *now_on, int showing, float *alpha) {
  Uint32 now = SDL_GetTicks();
  *alpha = 1;
  if (!showing || !s->to || d->still) { s->from = s->to = now_on; s->at = 0; return now_on; }
  float t = s->at ? (now - s->at) / (float)DOT_SWAP_MS : 1;
  if (now_on != s->to) {
    if (t >= 1) { s->from = s->to; s->at = now; }                                                // a change starts
    else if (t >= .5f) { s->from = s->to; s->at = now - (Uint32)((1 - t) * DOT_SWAP_MS); }       // again, mid-way
    s->to = now_on;
    t = (now - s->at) / (float)DOT_SWAP_MS;
  }
  if (t >= 1) { s->at = 0; return s->to; }
  *alpha = t < .5f ? 1 - 2 * t : 2 * t - 1;
  return t < .5f ? s->from : s->to;
}

static float ease_to(float v, float to, float k) { float dv = to - v; return fabsf(dv) < .002f ? to : v + dv * k; }
// The peek fades out downwards: drawn again in strips this high, each more transparent than the one above it.
#define PEEK_STRIP 4
static void list_pass(Dot *d, DotList *l, SDL_Rect clip, int n, int first, int last, int cursor, int x, int w, DotRowFn row, void *ud) {
  const int P = DOT_ROW_H + 12, Y = DOT_CONTENT_TOP;
  SDL_RenderSetClipRect(d->r, &clip);
  for (int i = first; i <= last; i++) row(ud, i, (int)lroundf(Y + (i - l->top) * P), 0);
  // the paper at its own place, with the rows under it drawn focused, clipped to it
  int hy = (int)lroundf(Y + (l->cur - l->top) * P);
  SDL_Rect h = { 0, hy, DOT_W, DOT_ROW_H }, c;
  if (cursor >= 0 && cursor < n && SDL_IntersectRect(&clip, &h, &c)) {
    SDL_RenderSetClipRect(d->r, &c);
    dot_rect(d, x, hy, w, DOT_ROW_H, DOT_R_ROW, DOT_PAPER, 255);
    for (int i = first; i <= last; i++) {
      int y = (int)lroundf(Y + (i - l->top) * P);
      if (y + DOT_ROW_H > hy && y < hy + DOT_ROW_H) row(ud, i, y, 1);
    }
  }
}
// The fades: a zone from y0 to y1 drawn again in strips, opacity going from `from` (at y0) to `to` (at y1).
typedef void (*PassFn)(Dot *d, SDL_Rect clip, void *ctx);
static void fade_zone(Dot *d, int y0, int y1, float from, float to, PassFn pass, void *ctx) {
  int a0 = d->alpha;
  for (int sy = y0; sy < y1; sy += PEEK_STRIP) {
    int h = y1 - sy < PEEK_STRIP ? y1 - sy : PEEK_STRIP;
    float t = (sy + h / 2.0f - y0) / (y1 - y0), f = from + (to - from) * t;
    if (from < to) f = f * f * f;   // fading upwards, under a header: gone well before the header's text
    d->alpha = (int)(a0 * (f < 0 ? 0 : f > 1 ? 1 : f));
    pass(d, (SDL_Rect){ 0, sy, DOT_W, h }, ctx);
  }
  d->alpha = a0;
}
typedef struct { DotList *l; int n, first, last, cursor, x, w; DotRowFn row; void *ud; } ListCtx;
static void list_pass_fn(Dot *d, SDL_Rect clip, void *c) {
  ListCtx *k = c; list_pass(d, k->l, clip, k->n, k->first, k->last, k->cursor, k->x, k->w, k->row, k->ud);
}
void dot_list(Dot *d, DotList *l, int key, int n, int rows, int top, int cursor, int x, int w, int peek, DotRowFn row, void *ud) {
  const int P = DOT_ROW_H + 12, Y = DOT_CONTENT_TOP;
  Uint32 now = SDL_GetTicks();
  if (l->key != key || d->still || !l->at || now - l->at > 200 || fabsf(l->cur - cursor) > rows + 1) { l->top = top; l->cur = cursor; }
  else {
    float k = 1 - expf(-(float)(now - l->at) / DOT_LIST_MS);
    l->top = ease_to(l->top, top, k); l->cur = ease_to(l->cur, cursor, k);
  }
  l->key = key; l->at = now;
  int below = (peek & DOT_PEEK_BELOW) && n > rows, above = (peek & DOT_PEEK_ABOVE) && (top > 0 || l->top > 0);
  int first = (int)floorf(l->top) - (above ? 1 : 0), last = (int)ceilf(l->top) + rows;
  if (first < 0) first = 0;
  if (last > n - 1) last = n - 1;
  ListCtx k = { l, n, first, last, cursor, x, w, row, ud };
  list_pass(d, l, (SDL_Rect){ 0, Y, DOT_W, rows * P - (below ? 0 : 12) }, n, first, last, cursor, x, w, row, ud);
  if (below) fade_zone(d, Y + rows * P, Y + rows * P + DOT_PEEK, 1, 0, list_pass_fn, &k);   // the next row's top
  if (above) fade_zone(d, Y - 12 - DOT_PEEK, Y, 0, 1 + 12.0f / DOT_PEEK, list_pass_fn, &k);   // the row before's bottom
  SDL_RenderSetClipRect(d->r, NULL);
}

static void grid_pass(Dot *d, DotGrid *g, SDL_Rect clip, int n, int cols, int nrows, int first, int last, int cursor,
                      int x, int y, int cw, int ch, int gap, int settled, DotCellFn cell, void *ud) {
  const int PX = cw + gap, PY = ch + gap;
  SDL_RenderSetClipRect(d->r, &clip);
  if (!settled) poster_ring(d, x + (int)lroundf(g->col * PX), y + (int)lroundf((g->row - g->top) * PY), cw, ch);
  for (int r = first < 0 ? 0 : first; r <= last && r < nrows; r++)
    for (int c = 0; c < cols && r * cols + c < n; c++) {
      int i = r * cols + c;
      cell(ud, i, x + c * PX, y + (int)lroundf((r - g->top) * PY), settled && i == cursor);
    }
}
typedef struct { DotGrid *g; int n, cols, nrows, first, last, cursor, x, y, cw, ch, gap, settled; DotCellFn cell; void *ud; } GridCtx;
static void grid_pass_fn(Dot *d, SDL_Rect clip, void *c) {
  GridCtx *k = c;
  grid_pass(d, k->g, clip, k->n, k->cols, k->nrows, k->first, k->last, k->cursor, k->x, k->y, k->cw, k->ch, k->gap, k->settled, k->cell, k->ud);
}
void dot_grid(Dot *d, DotGrid *g, int key, int n, int cols, int rows, int top, int cursor, int x, int y, int cw, int ch,
              int gap, int peek, DotCellFn cell, void *ud) {
  if (cols < 1 || n < 1) return;
  const int PY = ch + gap;
  float trow = (float)(top / cols), ccol = (float)(cursor % cols), crow = (float)(cursor / cols);
  Uint32 now = SDL_GetTicks();
  if (g->key != key || d->still || !g->at || now - g->at > 200 || fabsf(g->row - crow) > rows + 1 || fabsf(g->top - trow) > rows + 1) {
    g->top = trow; g->col = ccol; g->row = crow;
  } else {
    float k = 1 - expf(-(float)(now - g->at) / DOT_LIST_MS);
    g->top = ease_to(g->top, trow, k); g->col = ease_to(g->col, ccol, k); g->row = ease_to(g->row, crow, k);
  }
  g->key = key; g->at = now;
  int nrows = (n + cols - 1) / cols;
  int below = (peek & DOT_PEEK_BELOW) && nrows > (int)trow + rows, above = (peek & DOT_PEEK_ABOVE) && (trow > 0 || g->top > 0);
  int settled = g->col == ccol && g->row == crow;
  int first = (int)floorf(g->top) - (above ? 1 : 0), last = (int)ceilf(g->top) + rows;
  int bottom = y + rows * PY - gap + (below ? gap : 8);   // 8 above and below: the ring round a cover
  GridCtx k = { g, n, cols, nrows, first, last, cursor, x, y, cw, ch, gap, settled, cell, ud };
  grid_pass(d, g, (SDL_Rect){ 0, y - 8, DOT_W, bottom - (y - 8) }, n, cols, nrows, first, last, cursor, x, y, cw, ch, gap, settled, cell, ud);
  if (below) fade_zone(d, bottom, bottom + DOT_PEEK, 1, 0, grid_pass_fn, &k);
  if (above) fade_zone(d, y - gap - DOT_PEEK, y - 8, 0, 1 + (gap - 8.0f) / DOT_PEEK, grid_pass_fn, &k);
  SDL_RenderSetClipRect(d->r, NULL);
}

// --- over video -----------------------------------------------------------------------------------------------------
void dot_band(Dot *d, int y, int h) {
  dot_rect(d, 0, y, DOT_W, h, 0, DOT_CARD, 240);
  dot_rect(d, 0, y, DOT_W, 1, 0, 0xffffff, 30);
}

static void centre_card(Dot *d, int icon, const char *label, const char *value, int extra, int lift, int *ox, int *oy, int *wx, int *wy);
int dot_notice(Dot *d, int icon, const char *label, const char *value, int at, int n, Uint32 since) {
  Uint32 age = SDL_GetTicks() - since;
  if (!d->still && age >= DOT_NOTICE_MS) return 0;
  float in = d->still ? 1 : dot_ease_out(age / 200.0f);
  float out = d->still || age < DOT_NOTICE_MS - 300 ? 0 : clampf((age - (DOT_NOTICE_MS - 300)) / 300.0f, 0, 1);
  int a0 = d->alpha, tx, ty, wx, wy;
  dot_set_alpha(d, in * (1 - out));
  centre_card(d, icon, label, value, n > 1 ? 14 + 2 * DOT_DOT_R : 0, (int)lroundf(20 * (1 - in)), &tx, &ty, &wx, &wy);
  for (int i = 0; n > 1 && i < n && i < 12; i++)
    dot_circle(d, tx + DOT_DOT_R + i * DOT_DOT_PITCH, ty + 14 + DOT_DOT_R, DOT_DOT_R, i == at - 1 ? DOT_TEXT : DOT_TRACK);
  d->alpha = a0;
  return 1;
}

// The card a notice or busy uses: icon well (accent, r 36) with the label over the value; returns its height.
// *ox, *oy: under the value (for dots); *wx, *wy: the well's centre (for the busy ring). The well (r 36) sits in a
// 100 px square, so the ring (r 50) has room; the text 28 px after it.
#define WELL_BOX 100
static void centre_card(Dot *d, int icon, const char *label, const char *value, int extra, int lift, int *ox, int *oy, int *wx, int *wy) {
  int vw = dot_text_w(d, DOT_TITLE, value), lw = dot_text_w(d, DOT_LABEL_T, label), tw = vw > lw ? vw : lw;
  int w = 30 + WELL_BOX + 28 + (tw > 240 ? tw : 240) + 40;
  if (w > DOT_CONTENT_W) w = DOT_CONTENT_W;
  int lab = dot_line_h(d, DOT_LABEL_T), tit = dot_line_h(d, DOT_TITLE);
  int h = 30 + lab + 8 + tit + extra + 30;
  if (h < 30 + WELL_BOX + 30) h = 30 + WELL_BOX + 30;
  int x = (DOT_W - w) / 2, y = (DOT_H - h) / 2 + lift, cx = x + 30 + WELL_BOX / 2, cy = y + h / 2;
  dot_rect(d, x, y, w, h, DOT_R_CARD, DOT_CARD, 255);
  dot_circle(d, cx, cy, 36, d->accent);
  dot_icon(d, icon, cx, cy, 34, DOT_INK);
  int tx = x + 30 + WELL_BOX + 28, ty = y + (h - (lab + 8 + tit + extra)) / 2;
  dot_text(d, DOT_LABEL_T, label, tx, ty, DOT_LABEL, DOT_LEFT, 0);
  dot_text(d, DOT_TITLE, value, tx, ty + lab + 8, DOT_TEXT, DOT_LEFT, x + w - 40 - tx);
  *ox = tx; *oy = ty + lab + 8 + tit; *wx = cx; *wy = cy;
}

void dot_busy(Dot *d, DotSlide *s, int show, int icon, const char *label, const char *value, int spin) {
  float v = dot_slide(d, s, show);
  if (v <= 0) return;
  int a0 = d->alpha, tx, ty, cx, cy;
  dot_set_alpha(d, v);
  centre_card(d, icon, label, value, 0, (int)lroundf(20 * (1 - v)), &tx, &ty, &cx, &cy);
  // the ring: 12 dots round the well, one lit running round with a fading tail (a turn a second)
  float head = d->still ? 3 : (SDL_GetTicks() % 1000) / 1000.0f * 12;
  for (int i = 0; i < 12; i++) {
    float ang = i / 12.0f * 6.2831853f - 1.5707963f;
    int px = cx + (int)lroundf(cosf(ang) * 47), py = cy + (int)lroundf(sinf(ang) * 47);
    float behind = fmodf(head - i + 12, 12);   // 0 = the head
    int lit = spin ? (behind < 4 ? (int)(255 * (1 - behind / 4)) : 0) : 0;
    dot_rect(d, px - 3, py - 3, 6, 6, 3, DOT_TRACK, 255);
    if (lit) dot_rect(d, px - 3, py - 3, 6, 6, 3, DOT_TEXT, lit);
  }
  d->alpha = a0;
}

void dot_stats(Dot *d, char lines[][96], int n) {
  // 28 px type on a 40 px pitch; labels (7 letters at most) in a 160 px column; at the content's margins + 30
  int lh = 40, th = dot_line_h(d, DOT_VALUE), bh = (n - 1) * lh + th, y0 = (DOT_H - bh) / 2;
  int lx = DOT_MARGIN + 30, vx = lx + 160, right = DOT_W - DOT_MARGIN - 30;
  dot_rect(d, 0, 0, DOT_W, DOT_H, 0, DOT_CARD, 225);
  for (int i = 0; i < n; i++) {
    char l[96]; snprintf(l, sizeof l, "%s", lines[i]);
    char *bar = strchr(l, '|'); if (bar) *bar++ = 0;
    int y = y0 + i * lh;
    dot_text(d, DOT_LABEL_T, l, lx, y, DOT_LABEL, DOT_LEFT, vx - lx - 12);
    if (bar) dot_text(d, DOT_VALUE, bar, vx, y, DOT_TEXT, DOT_LEFT, right - vx);
  }
}

// --- switch, picker, subtitles ----------------------------------------------------------------------------------
void dot_switch(Dot *d, DotSwitch *s, int x, int y, int w, const char *const *labels, int n, int on, int focused) {
  if (n < 1) return;
  Uint32 now = SDL_GetTicks();
  if (d->still || !s->at || now - s->at > 200) s->pos = on;
  else { float k = 1 - expf(-(float)(now - s->at) / DOT_LIST_MS); s->pos = ease_to(s->pos, on, k); }
  s->at = now;
  int h = DOT_CHIP_H, seg = w / n;
  dot_rect(d, x, y, w, h, h / 2, 0xffffff, 26 * d->alpha / 255);   // the track: white 10% on the card
  if (focused) blit(d, tex_rrect(d, w, h, h / 2, 2), x, y, 0xffffff, 90 * d->alpha / 255);
  dot_rect(d, x + (int)lroundf(s->pos * seg) + 4, y + 4, seg - 8, h - 8, (h - 8) / 2, DOT_PAPER, 255);
  for (int i = 0; i < n; i++) {
    float near = 1 - clampf(fabsf(s->pos - i), 0, 1);   // under the paper: ink
    Uint32 c = near > .5f ? DOT_INK : DOT_TEXT;
    text_c(d, DOT_CHIP_T, labels[i], x + i * seg + seg / 2, y + h / 2, c, DOT_CENTER, seg - 24);
  }
}

void dot_picker(Dot *d, DotPicker *p, int show, const char *const *tabs, int ntabs, int tab,
                const char *const *items, int n, int cursor, int in_use) {
  float v = dot_slide(d, &p->slide, show);
  if (v <= 0) return;
  const int ROWS = 5, w = 620, x = (DOT_W - w) / 2;
  int rows = n < ROWS ? n : ROWS;
  if (cursor < p->top) p->top = cursor;
  if (cursor >= p->top + ROWS) p->top = cursor - ROWS + 1;
  if (p->top > n - rows) p->top = n - rows > 0 ? n - rows : 0;
  int head = ntabs > 1 ? DOT_CHIP_H + 24 : 0;
  int h = DOT_PAD + head + (rows ? rows * DOT_OPT_PITCH - (DOT_OPT_PITCH - DOT_OPT_H) : 0) + DOT_PAD + 6;
  int y = (DOT_H - h) / 2 + (int)lroundf(20 * (1 - v));
  int a0 = d->alpha;
  dot_set_alpha(d, v);
  dot_rect(d, x, y, w, h, DOT_R_CARD, DOT_CARD, 255);
  int cy = y + DOT_PAD;
  if (ntabs > 1) { dot_switch(d, &p->sw, x + 30, cy, w - 60, tabs, ntabs, tab, 0); cy += head; }
  for (int i = 0; i < rows; i++) {
    int k = p->top + i;
    dot_opt(d, x + 30, cy + i * DOT_OPT_PITCH, w - 60, items[k], NULL, k == cursor ? DOT_FOCUS : 0);
    if (k == in_use) dot_adot(d, x + w - 30 - 14, cy + i * DOT_OPT_PITCH + (DOT_OPT_H - 14) / 2, d->accent);
  }
  if (n > ROWS) {   // more than fit: where you are, as dots down the right edge
    for (int i = 0; i < n && i < 24; i++)
      dot_circle(d, x + w - 12, y + h / 2 - (n - 1) * 9 + i * 18, 3, i == cursor ? DOT_TEXT : DOT_TRACK);
  }
  d->alpha = a0;
}

void dot_subtitle(Dot *d, const char *text, int bottom) {
  if (!text || !text[0]) return;
  const int st = DOT_BODY, maxw = DOT_W - 2 * 72, padx = 16, pady = 4;
  int lh = dot_line_h(d, st) + 2 * pady;
  // word-wrap every line into at most 4 screen lines
  char out[6][256]; int no = 0;
  char buf[512]; snprintf(buf, sizeof buf, "%s", text);
  char *save = NULL;
  for (char *line = strtok_r(buf, "\n", &save); line && no < 6; line = strtok_r(NULL, "\n", &save)) {
    char cur[256] = "";
    char *ws = NULL;
    for (char *word = strtok_r(line, " ", &ws); word; word = strtok_r(NULL, " ", &ws)) {
      char tryl[256]; snprintf(tryl, sizeof tryl, "%s%s%s", cur, cur[0] ? " " : "", word);
      if (cur[0] && dot_text_w(d, st, tryl) > maxw - 2 * padx) { if (no < 6) snprintf(out[no++], 256, "%s", cur); snprintf(cur, sizeof cur, "%s", word); }
      else snprintf(cur, sizeof cur, "%s", tryl);
    }
    if (cur[0] && no < 6) snprintf(out[no++], 256, "%s", cur);
  }
  int first = no > 4 ? no - 4 : 0;
  for (int i = first; i < no; i++) {
    int tw = dot_text_w(d, st, out[i]), bw = tw + 2 * padx;
    int y = bottom - (no - i) * lh;
    dot_rect(d, (DOT_W - bw) / 2, y, bw, lh, 10, 0x000000, 190);
    dot_text(d, st, out[i], DOT_W / 2, y + pady, DOT_TEXT, DOT_CENTER, maxw);
  }
}
