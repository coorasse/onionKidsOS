#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include "platform.h"
#include "gfx.h"
#include "library.h"
#include "kid_ui.h"
#include "resume_ui.h"
#include "osd.h"
#include "coins.h"

enum { ST_KID, ST_RESUME, ST_PLAYING };

/* Scripted input for headless screenshots:
 *   --script "right,right,shot:kid,hold:select+start:3200,shot:parent,down,a,b"
 * press tokens hold the button for one frame then settle for 450ms,
 * hold:<buttons>:<ms> keeps them down, wait:<ms> idles, shot:<name> saves a PNG. */
typedef struct {
	char *tokens[512];
	int count, pos;
	uint32_t hold_left, hold_mask, last_mask;
} Script;

static uint32_t parse_buttons(char *spec)
{
	static const struct { const char *name; uint32_t bit; } names[] = {
		{"up", BTN_UP}, {"down", BTN_DOWN}, {"left", BTN_LEFT}, {"right", BTN_RIGHT},
		{"a", BTN_A}, {"b", BTN_B}, {"x", BTN_X}, {"y", BTN_Y}, {"l1", BTN_L1}, {"r1", BTN_R1},
		{"start", BTN_START}, {"select", BTN_SELECT}, {"menu", BTN_MENU},
		{"volup", BTN_VOL_UP}, {"voldown", BTN_VOL_DOWN}, {NULL, 0},
	};
	uint32_t mask = 0;
	for (char *tok = strtok(spec, "+"); tok; tok = strtok(NULL, "+")) {
		int found = 0;
		for (int i = 0; names[i].name; i++)
			if (strcasecmp(tok, names[i].name) == 0) mask |= names[i].bit, found = 1;
		if (!found) fprintf(stderr, "unknown button '%s'\n", tok);
	}
	return mask;
}

static void script_parse(Script *s, char *arg)
{
	for (char *tok = strtok(arg, ","); tok && s->count < 512; tok = strtok(NULL, ",")) {
		while (*tok == ' ' || *tok == '\t') tok++;
		char *end = tok + strlen(tok);
		while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
		if (*tok) s->tokens[s->count++] = tok;
	}
}

static uint32_t script_step(Script *s, const char **shot, int *done)
{
	*shot = NULL;
	if (s->hold_left > 0) {
		s->hold_left -= s->hold_left < 16 ? s->hold_left : 16;
		return s->last_mask = s->hold_mask;
	}
	if (s->pos >= s->count) {
		*done = 1;
		return 0;
	}
	char *tok = s->tokens[s->pos++];
	if (strncmp(tok, "shot:", 5) == 0) {
		*shot = tok + 5;
		return s->last_mask;
	}
	if (strncmp(tok, "wait:", 5) == 0) {
		s->hold_left = (uint32_t)atoi(tok + 5);
		s->hold_mask = 0;
		return s->last_mask = 0;
	}
	if (strncmp(tok, "hold:", 5) == 0) {
		char *spec = tok + 5, *colon = strrchr(spec, ':');
		s->hold_left = colon ? (uint32_t)atoi(colon + 1) : 1000;
		if (colon) *colon = 0;
		s->hold_mask = parse_buttons(spec);
		return s->last_mask = s->hold_mask;
	}
	s->hold_left = 450;
	s->hold_mask = 0;
	return s->last_mask = parse_buttons(tok);
}

static void render_playing(Game *g)
{
	gfx_clear(0x1C1C1E);
#ifdef PLATFORM_DESKTOP
	const char *why = "Desktop build: nothing is launched";
#else
	const char *why = "No emulator found for this system";
#endif
	gfx_text(font_med, why, 0x8E8E93, 320, 150, ALIGN_CENTER);
	gfx_text_fit(font_big, g->name, 0xFFFFFF, 320, 200, ALIGN_CENTER, 600);
	char buf[600];
	snprintf(buf, sizeof(buf), "%s  -  %s", g->system_tag, g->rel);
	gfx_text_fit(font_small, buf, 0x8E8E93, 320, 250, ALIGN_CENTER, 600);
	if (coins_is_arcade(g->system_tag)) {
		snprintf(buf, sizeof(buf), coins_left() > 0 ? "Coins left today: %d of %d   (SELECT inserts a coin)"
		                                          : "Coins left today: %d of %d   (SELECT does nothing now)",
		         coins_left(), coins_limit());
		gfx_text(font_small, buf, 0xF5B800, 320, 300, ALIGN_CENTER);
	}
	gfx_text(font_small, "Press B to go back", 0x8E8E93, 320, 420, ALIGN_CENTER);
}

static void on_terminate(int sig)
{
	(void)sig;
	pad.quit = 1;
}

/* Starts the game and returns the state the UI continues in; ST_EXIT when the host took over. */
enum { ST_EXIT = -1 };
static int launch(Game *g, int mode, int scripted)
{
	int launched = scripted ? LAUNCH_FAKE : PLAT_launch(g, mode);
	if (launched == LAUNCH_HANDOVER) return ST_EXIT;
	if (launched == LAUNCH_RETURNED) {
		game_resume_forget(g);
		kid_ui_enter();
		return ST_KID;
	}
	return ST_PLAYING;
}

int main(int argc, char **argv)
{
	Script script = {0};
	int scripted = 0;
	const char *shot_dir = "shots";
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--script") == 0 && i + 1 < argc) {
			script_parse(&script, argv[++i]);
			scripted = 1;
		} else if (strcmp(argv[i], "--shots") == 0 && i + 1 < argc) {
			shot_dir = argv[++i];
		} else {
			fprintf(stderr, "usage: %s [--script tokens] [--shots dir]\n", argv[0]);
			return 2;
		}
	}
	if (scripted) setenv("SDL_VIDEODRIVER", "dummy", 1);
	setvbuf(stdout, NULL, _IOLBF, 0); /* the device keeps stdout in a log file */

	SDL_Surface *screen = PLAT_init();
	/* after PLAT_init so SDL's own handlers do not replace it: a plain kill must leave the console clean */
	signal(SIGTERM, on_terminate);
	signal(SIGINT, on_terminate);
	gfx_init(screen, PLAT_font_path());

	const char *root = PLAT_sdcard_root();
	coins_init(root);
	library_scan(root);
	library_load_enabled(root);
	printf("library: %d systems, %d games (%s)\n", lib.system_count, lib.game_count, root);
	if (!scripted) PLAT_audio_init();

	int state = ST_KID;
	Game *playing = NULL;
	kid_ui_enter();

	uint32_t now = scripted ? 0 : PLAT_ticks(), last = now, last_cover_check = now;
	int last_state = -1;
	while (!pad.quit) {
		const char *shot = NULL;
		if (scripted) {
			int done = 0;
			uint32_t held = script_step(&script, &shot, &done);
			if (done) break;
			pad_update(held);
			now += 16;
		} else {
			PLAT_poll_input();
			now = PLAT_ticks();
			PLAT_power_update(now);
		}
		uint32_t dt = now - last;
		last = now;
		if (dt > 100) dt = 100;
		input_update_repeat(now);
		osd_update(dt);

		/* the loop runs at 60 Hz for input, but drawing a frame costs more than the frame itself on the
		 * device: skip it while nothing moves and no button changed */
		int animating = state == ST_KID ? kid_ui_animating() : 0;
		int covers_changed = 0;
		if (state == ST_KID && now - last_cover_check >= 1000) {
			last_cover_check = now;
			covers_changed = library_covers_refresh();
		}
		int dirty = scripted || state != last_state || pad.pressed || pad.released || pad.repeat || animating ||
		            osd_visible() || covers_changed;
		last_state = state;
		if (!dirty) {
			PLAT_delay(pad.held ? 16 : 32);
			continue;
		}

		switch (state) {
		case ST_KID: {
			int action = kid_ui_update(dt);
			if (action == KID_LAUNCH) {
				playing = kid_ui_selected();
				if (game_resume_available(playing)) {
					resume_ui_enter(playing);
					state = ST_RESUME;
				} else {
					state = launch(playing, LAUNCH_DEFAULT, scripted);
					if (state == ST_EXIT) goto out;
				}
			} else if (action == KID_EXIT_SYSTEM) {
				PLAT_exit_to_system();
				goto out;
			}
			if (state == ST_RESUME) resume_ui_render();
			else kid_ui_render();
			break;
		}
		case ST_RESUME: {
			int result = resume_ui_update();
			if (result == RESUME_BACK) {
				state = ST_KID;
			} else if (result != RESUME_NONE) {
				state = launch(playing, result == RESUME_CONTINUE ? LAUNCH_RESUME : LAUNCH_FRESH, scripted);
				if (state == ST_EXIT) goto out;
			}
			if (state == ST_RESUME) resume_ui_render();
			else if (state == ST_KID) kid_ui_render();
			else render_playing(playing);
			break;
		}
		case ST_PLAYING:
			/* desktop stand-in for coinmon: SELECT is the coin button, ignored once the coins are spent */
			if ((pad.pressed & BTN_SELECT) && coins_is_arcade(playing->system_tag) && coins_left() > 0) coins_spend();
			if (pad.pressed & (BTN_B | BTN_MENU)) {
				state = ST_KID;
				kid_ui_enter();
			}
			if (state == ST_KID) kid_ui_render();
			else render_playing(playing);
			break;
		}

		osd_render();
		PLAT_flip(screen);
		if (shot) {
			char path[1024];
			snprintf(path, sizeof(path), "%s/%s.png", shot_dir, shot);
			if (PLAT_save_screenshot(screen, path) == 0) printf("saved %s\n", path);
			else fprintf(stderr, "could not save %s\n", path);
		}
		if (!scripted) {
			uint32_t elapsed = PLAT_ticks() - now;
			if (elapsed < 16) PLAT_delay(16 - elapsed);
		}
	}
out:
	gfx_quit();
	PLAT_quit();
	return 0;
}
