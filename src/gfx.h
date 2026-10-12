#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <SDL.h>
#include <SDL_ttf.h>

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

extern SDL_Surface *gfx_screen;
extern TTF_Font *font_huge, *font_big, *font_med, *font_small;

void gfx_init(SDL_Surface *screen, const char *font_path);
void gfx_quit(void);

/* All drawing goes to the current target (the screen by default). */
void gfx_set_target(SDL_Surface *target);

void gfx_clear(uint32_t rgb);
void gfx_fill_rect(SDL_Rect r, uint32_t rgb);
void gfx_fill_rounded(SDL_Rect r, int radius, uint32_t rgb);
void gfx_fill_rounded_alpha(SDL_Rect r, int radius, uint32_t rgb, uint8_t alpha);
void gfx_vgradient(SDL_Rect r, uint32_t top, uint32_t bottom);

SDL_Surface *gfx_new_surface(int w, int h);
SDL_Surface *gfx_load_image(const char *path);
SDL_Surface *gfx_scale(SDL_Surface *src, int w, int h);
void gfx_round_corners(SDL_Surface *s, int radius);

void gfx_blit(SDL_Surface *s, int x, int y);
void gfx_blit_alpha(SDL_Surface *s, int x, int y, uint8_t alpha);

int gfx_text(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align);
int gfx_text_fit(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align, int max_w);
int gfx_text_width(TTF_Font *font, const char *text);

uint32_t gfx_hsv(float h, float s, float v);

/* Pictograms for readers who cannot read yet: a play triangle and a circular "start over" arrow. */
SDL_Surface *gfx_icon_play(int size, uint32_t rgb);
SDL_Surface *gfx_icon_restart(int size, uint32_t rgb);
SDL_Surface *gfx_icon_speaker(int size, uint32_t rgb, int waves);

/* A button hint: a coloured disc with the letter and the action next to it. align places the whole hint. */
void gfx_button_hint(int x, int y, const char *letter, uint32_t rgb, const char *action, int align);

#endif
