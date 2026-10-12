#include <math.h>
#include "resume_ui.h"
#include "gfx.h"
#include "platform.h"

#define GREEN 0x3DBE5B
#define ORANGE 0xFF8A3D

static Game *game;
static int choice; /* 0 = continue, 1 = start over */
static SDL_Surface *background, *icon_play, *icon_restart;
static SDL_Surface *continue_img, *fresh_img; /* scaled render caches */

static void free_caches(void)
{
	if (continue_img) SDL_FreeSurface(continue_img);
	if (fresh_img) SDL_FreeSurface(fresh_img);
	continue_img = fresh_img = NULL;
}

void resume_ui_enter(Game *g)
{
	game = g;
	choice = 0;
	free_caches();
}

int resume_ui_update(void)
{
	if (pad.pressed & BTN_B) return RESUME_BACK;
	int before = choice;
	if (pad.pressed & BTN_LEFT) choice = 0;
	if (pad.pressed & BTN_RIGHT) choice = 1;
	if (choice != before) PLAT_audio_click();
	if (pad.pressed & BTN_A) return choice == 0 ? RESUME_CONTINUE : RESUME_FRESH;
	return RESUME_NONE;
}

static SDL_Surface *fit(SDL_Surface *src, int box, int radius)
{
	float s = fminf((float)box / (float)src->w, (float)box / (float)src->h);
	SDL_Surface *out = gfx_scale(src, (int)((float)src->w * s), (int)((float)src->h * s));
	if (out) gfx_round_corners(out, radius);
	return out;
}

static unsigned name_hash(const char *s)
{
	unsigned h = 5381;
	while (*s) h = h * 33 + (unsigned char)*s++;
	return h;
}

/* A big rounded card with a picture, a coloured badge in its corner and a label underneath. */
static void draw_option(int cx, int cy, SDL_Surface *picture, SDL_Surface *icon, uint32_t accent, const char *label,
                        int selected)
{
	int size = selected ? 270 : 230, radius = size / 12, inner = size - 2 * (size / 14);
	if (selected) {
		SDL_Rect ring = {cx - size / 2 - 8, cy - size / 2 - 8, size + 16, size + 16};
		gfx_fill_rounded(ring, radius + 8, accent);
	}
	SDL_Rect shadow = {cx - size / 2, cy - size / 2 + size / 30, size, size};
	gfx_fill_rounded_alpha(shadow, radius, 0x3A2E1A, 40);
	SDL_Rect card = {cx - size / 2, cy - size / 2, size, size};
	gfx_fill_rounded(card, radius, 0xFFFFFF);

	if (picture) {
		gfx_blit(picture, cx - picture->w / 2, cy - picture->h / 2);
	} else {
		SDL_Rect r = {cx - inner / 2, cy - inner / 2, inner, inner};
		gfx_fill_rounded(r, radius / 2, gfx_hsv((float)(name_hash(game->name) % 360), 0.55f, 0.92f));
	}

	int br = size / 5;
	int bx = cx + size / 2 - br - 6, by = cy + size / 2 - br - 6;
	SDL_Rect bshadow = {bx - br, by - br + 3, 2 * br, 2 * br};
	gfx_fill_rounded_alpha(bshadow, br, 0x3A2E1A, 70);
	SDL_Rect badge = {bx - br, by - br, 2 * br, 2 * br};
	gfx_fill_rounded(badge, br, 0xFFFFFF);
	SDL_Rect fill = {bx - br + 4, by - br + 4, 2 * br - 8, 2 * br - 8};
	gfx_fill_rounded(fill, br - 4, accent);
	gfx_blit(icon, bx - icon->w / 2, by - icon->h / 2);

	gfx_text(selected ? font_big : font_med, label, selected ? 0x333333 : 0x8A8A8A, cx,
	         cy + size / 2 + (selected ? 22 : 30), ALIGN_CENTER);
}

void resume_ui_render(void)
{
	if (!background) {
		background = gfx_new_surface(SCREEN_W, SCREEN_H);
		gfx_set_target(background);
		SDL_Rect full = {0, 0, SCREEN_W, SCREEN_H};
		gfx_vgradient(full, 0xBFE6FF, 0xFFF3D6);
		gfx_set_target(NULL);
		icon_play = gfx_icon_play(46, 0xFFFFFF);
		icon_restart = gfx_icon_restart(50, 0xFFFFFF);
	}
	gfx_blit(background, 0, 0);
	gfx_text_fit(font_big, game->name, 0x333333, 320, 22, ALIGN_CENTER, 600);

	int big_inner = 270 - 2 * (270 / 14), small_inner = 230 - 2 * (230 / 14);
	int c_inner = choice == 0 ? big_inner : small_inner, f_inner = choice == 0 ? small_inner : big_inner;
	if (!continue_img || continue_img->w > c_inner || (continue_img->w < c_inner && continue_img->h < c_inner)) {
		if (continue_img) SDL_FreeSurface(continue_img);
		SDL_Surface *src = game_resume_screen(game);
		if (!src) src = game_cover(game);
		continue_img = src ? fit(src, c_inner, 10) : NULL;
	}
	if (!fresh_img || fresh_img->w > f_inner || (fresh_img->w < f_inner && fresh_img->h < f_inner)) {
		if (fresh_img) SDL_FreeSurface(fresh_img);
		SDL_Surface *src = game_cover(game);
		fresh_img = src ? fit(src, f_inner, 10) : NULL;
	}

	draw_option(175, 250, continue_img, icon_play, GREEN, "Continue", choice == 0);
	draw_option(465, 250, fresh_img, icon_restart, ORANGE, "Start over", choice == 1);
	gfx_button_hint(18, 456, "B", 0xE5533D, "Back", ALIGN_LEFT);
	gfx_button_hint(622, 456, "A", GREEN, "Go", ALIGN_RIGHT);
}
