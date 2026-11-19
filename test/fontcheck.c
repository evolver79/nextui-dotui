// Renders each font at its type-scale size to fontcheck.bmp, to check SDL_ttf draws them right.
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
int main(void) {
  SDL_Init(0); TTF_Init();
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1024, 560, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_FillRect(s, NULL, 0xff0f0f10);
  struct { const char *f; int px; const char *t; } L[] = {
    { "fonts/Doto-Black.ttf", 92, "41:12 / 1:52:08" }, { "fonts/Doto-Black.ttf", 48, "OPTIONS  NOW PLAYING" },
    { "fonts/Doto-Bold.ttf", 70, "09:35" }, { "fonts/Geist-SemiBold.ttf", 32, "The Long Harbour" },
    { "fonts/Geist-Medium.ttf", 25, "Northern Lights Timelapse   Rally Highlights" },
    { "fonts/GeistMono-Medium.ttf", 22, "R E S U M E   2021 · 1080P · MKV   37%" } };
  int y = 10;
  for (int i = 0; i < 6; i++) {
    TTF_Font *f = TTF_OpenFont(L[i].f, L[i].px);
    if (!f) { printf("%s: %s\n", L[i].f, TTF_GetError()); return 1; }
    SDL_Color c = { 0xf2, 0xf2, 0xf0, 255 };
    SDL_Surface *t = TTF_RenderUTF8_Blended(f, L[i].t, c);
    SDL_Rect d = { 30, y, 0, 0 }; SDL_BlitSurface(t, NULL, s, &d); y += t->h + 6;
  }
  SDL_SaveBMP(s, "test/fontcheck.bmp"); printf("ok\n"); return 0;
}
