#include "platform.h"

Input pad;

static uint32_t down_since[BTN_COUNT];
static uint32_t last_repeat[BTN_COUNT];

void pad_update(uint32_t held)
{
	pad.pressed = held & ~pad.held;
	pad.released = pad.held & ~held;
	pad.held = held;
}

void input_update_repeat(uint32_t now)
{
	const uint32_t delay = 350, interval = 70;
	pad.repeat = pad.pressed;
	for (int i = 0; i < BTN_COUNT; i++) {
		uint32_t bit = 1u << i;
		if (pad.pressed & bit) {
			down_since[i] = now;
			last_repeat[i] = now;
		} else if ((pad.held & bit) && now - down_since[i] > delay && now - last_repeat[i] >= interval) {
			pad.repeat |= bit;
			last_repeat[i] = now;
		}
	}
}
