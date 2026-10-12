#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <SDL.h>
#include <SDL_ttf.h>

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

/* The KidsOS palette ("Blocks"): ink outlines and hard shadows on cream, toy primaries as accents. */
#define INK 0x2B2B35
#define PANEL 0x3A3A46
#define CREAM 0xFFF8EC
#define MUTED 0xB8B0A4
#define YELLOW 0xF7C948
#define RED 0xE8453C
#define BLUE 0x2F6FE4
#define GREEN 0x3DBE5B
#define ORANGE 0xFF8A3D

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
/* A rounded rectangle with a border: the border colour first, the fill inset by border_px. */
void gfx_fill_rounded_border(SDL_Rect r, int radius, uint32_t fill, uint32_t border_rgb, int border_px);
/* A grid of small dots over the whole target, the pattern behind every kids screen. */
void gfx_dots(uint32_t rgb, int step, float radius, uint8_t alpha);
/* The cream, dotted backdrop of the kids screens, drawn once. */
SDL_Surface *gfx_paper(void);

SDL_Surface *gfx_new_surface(int w, int h);
SDL_Surface *gfx_load_image(const char *path);
SDL_Surface *gfx_scale(SDL_Surface *src, int w, int h);
void gfx_round_corners(SDL_Surface *s, int radius);
/* A copy of an ARGB surface turned by degrees (positive = clockwise), large enough to hold all of it. */
SDL_Surface *gfx_rotate(SDL_Surface *src, float degrees);

void gfx_blit(SDL_Surface *s, int x, int y);
void gfx_blit_alpha(SDL_Surface *s, int x, int y, uint8_t alpha);
/* For each row of an ARGB surface, the first and last column of its run of fully opaque pixels
 * (2 ints per row, last < first when there is none); free() it. */
int *gfx_opaque_spans(SDL_Surface *s);
/* Like gfx_blit_alpha, with the surface scaled to w x h on the way (nearest neighbour, no intermediate
 * copy). With spans from gfx_opaque_spans the opaque runs are copied in bulk instead of pixel by pixel,
 * which is several times faster on the device. */
void gfx_blit_alpha_scaled(SDL_Surface *s, const int *spans, int x, int y, int w, int h, uint8_t alpha);

int gfx_text(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align);
int gfx_text_fit(TTF_Font *font, const char *text, uint32_t rgb, int x, int y, int align, int max_w);
int gfx_text_width(TTF_Font *font, const char *text);

uint32_t gfx_hsv(float h, float s, float v);

/* Pictograms for readers who cannot read yet: a play triangle and a circular "start over" arrow. */
SDL_Surface *gfx_icon_play(int size, uint32_t rgb);
SDL_Surface *gfx_icon_restart(int size, uint32_t rgb);
SDL_Surface *gfx_icon_speaker(int size, uint32_t rgb, int waves);
/* A Mario-style coin, gold or grey. */
SDL_Surface *gfx_icon_coin(int size, int grey);

/* A button hint: a coloured disc with the letter and the action next to it. align places the whole hint. */
void gfx_button_hint(int x, int y, const char *letter, uint32_t rgb, const char *action, int align);

#endif
