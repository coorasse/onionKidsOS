/* Preloaded into RetroArch for arcade games in kids mode (see onion/game.sh). RetroArch on the Miyoo
 * reads the pad through Onion's SDL 1.2, so the SELECT key, which is the coin button, can be taken out
 * of the event queue here. While /tmp/kidsos_no_coins exists (coinmon creates it once the daily coins
 * are spent) SELECT never reaches the game and the credits it already has are all it gets. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <SDL.h>

#define LOCK_FLAG "/tmp/kidsos_no_coins"
#define COIN_KEY SDLK_RCTRL /* SELECT on the Miyoo Mini */

static int locked(void)
{
	return access(LOCK_FLAG, F_OK) == 0;
}

static int is_coin(const SDL_Event *e)
{
	return (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) && e->key.keysym.sym == COIN_KEY;
}

int SDL_PeepEvents(SDL_Event *events, int numevents, SDL_eventaction action, Uint32 mask)
{
	static int (*real)(SDL_Event *, int, SDL_eventaction, Uint32);
	if (!real) real = dlsym(RTLD_NEXT, "SDL_PeepEvents");
	int n = real(events, numevents, action, mask);
	if (action != SDL_GETEVENT || n <= 0 || !locked()) return n;
	int kept = 0;
	for (int i = 0; i < n; i++)
		if (!is_coin(&events[i])) events[kept++] = events[i];
	return kept;
}

int SDL_PollEvent(SDL_Event *event)
{
	static int (*real)(SDL_Event *);
	if (!real) real = dlsym(RTLD_NEXT, "SDL_PollEvent");
	for (;;) {
		int got = real(event);
		if (!got || !event || !locked() || !is_coin(event)) return got;
	}
}

/* RetroArch's sdl_dingux driver does not read the events, it looks at SDL's key state array after
 * pumping them, so the coin key is cleared there as well */
Uint8 *SDL_GetKeyState(int *numkeys)
{
	static Uint8 *(*real)(int *);
	if (!real) real = dlsym(RTLD_NEXT, "SDL_GetKeyState");
	Uint8 *state = real(numkeys);
	if (state && locked()) state[COIN_KEY] = 0;
	return state;
}
