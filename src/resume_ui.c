#include <math.h>
#include "resume_ui.h"
#include "gfx.h"
#include "platform.h"

static Game *game;
static int choice; /* 0 = continue, 1 = start over */
static SDL_Surface *icon_play, *icon_restart;
static SDL_Surface *continue_img, *fresh_img; /* scaled render caches */
static SDL_Surface *cards[2];                 /* the composed, tilted cards, redrawn when the choice changes */

static void free_cards(void)
{
	for (int i = 0; i < 2; i++) {
		if (cards[i]) SDL_FreeSurface(cards[i]);
		cards[i] = NULL;
	}
}

static void free_caches(void)
{
	if (continue_img) SDL_FreeSurface(continue_img);
	if (fresh_img) SDL_FreeSurface(fresh_img);
	continue_img = fresh_img = NULL;
	free_cards();
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
	if (choice != before) {
		PLAT_audio_click();
		free_cards();
	}
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

#define RING 12
#define DROP 7

/* A big white card with an ink border and the picture inside, a coloured badge in its corner. The chosen
 * one sits in a green ring and leans left, the other one leans right over a hard ink shadow. */
static SDL_Surface *compose_option(SDL_Surface *picture, SDL_Surface *icon, uint32_t accent, int selected)
{
	int size = selected ? 270 : 230, radius = selected ? 26 : 22, inner = size - 2 * (size / 14);
	int pad = selected ? RING : 0, extra = selected ? 2 * RING : DROP;
	SDL_Surface *comp = gfx_new_surface(size + extra, size + extra);
	if (!comp) return NULL;
	gfx_set_target(comp);
	if (selected) {
		SDL_Rect ring = {0, 0, size + 2 * RING, size + 2 * RING};
		gfx_fill_rounded_border(ring, radius + RING, GREEN, INK, 4);
	} else {
		SDL_Rect drop = {DROP, DROP, size, size};
		gfx_fill_rounded(drop, radius, INK);
	}
	SDL_Rect card = {pad, pad, size, size};
	gfx_fill_rounded_border(card, radius, 0xFFFFFF, INK, 4);

	int cx = pad + size / 2, cy = pad + size / 2;
	if (picture) {
		gfx_blit(picture, cx - picture->w / 2, cy - picture->h / 2);
	} else {
		SDL_Rect r = {cx - inner / 2, cy - inner / 2, inner, inner};
		gfx_fill_rounded(r, radius / 2, gfx_hsv((float)(name_hash(game->name) % 360), 0.55f, 0.92f));
	}

	int br = size / 5;
	int bx = cx + size / 2 - br - 6, by = cy + size / 2 - br - 6;
	SDL_Rect badge = {bx - br, by - br, 2 * br, 2 * br};
	gfx_fill_rounded_border(badge, br, 0xFFFFFF, INK, 4);
	SDL_Rect fill = {bx - br + 9, by - br + 9, 2 * br - 18, 2 * br - 18};
	gfx_fill_rounded(fill, br - 9, accent);
	gfx_blit(icon, bx - icon->w / 2, by - icon->h / 2);
	gfx_set_target(NULL);

	SDL_Surface *tilted = gfx_rotate(comp, selected ? -1.5f : 2.0f);
	SDL_FreeSurface(comp);
	return tilted;
}

static void draw_option(int which, int cx, int cy, SDL_Surface *picture, SDL_Surface *icon, uint32_t accent,
                        const char *label, int selected)
{
	if (!cards[which]) cards[which] = compose_option(picture, icon, accent, selected);
	int size = selected ? 270 : 230, off = selected ? 0 : DROP;
	if (cards[which]) gfx_blit(cards[which], cx + off / 2 - cards[which]->w / 2, cy + off / 2 - cards[which]->h / 2);
	gfx_text(selected ? font_big : font_med, label, selected ? INK : 0x7A7A86, cx, cy + size / 2 + (selected ? 22 : 30),
	         ALIGN_CENTER);
}

void resume_ui_render(void)
{
	if (!icon_play) {
		icon_play = gfx_icon_play(46, 0xFFFFFF);
		icon_restart = gfx_icon_restart(50, 0xFFFFFF);
	}
	gfx_blit(gfx_paper(), 0, 0);
	gfx_text_fit(font_big, game->name, INK, 320, 22, ALIGN_CENTER, 600);

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

	draw_option(0, 175, 250, continue_img, icon_play, GREEN, "Continue", choice == 0);
	draw_option(1, 465, 250, fresh_img, icon_restart, ORANGE, "Start over", choice == 1);
	gfx_button_hint(18, 456, "B", RED, "Back", ALIGN_LEFT);
	gfx_button_hint(622, 456, "A", GREEN, "Go", ALIGN_RIGHT);
}
