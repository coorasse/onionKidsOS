#ifndef KID_UI_H
#define KID_UI_H

#include <stdint.h>
#include "library.h"

enum { KID_NONE, KID_LAUNCH, KID_EXIT_SYSTEM };

void kid_ui_enter(void);
int kid_ui_update(uint32_t dt);
void kid_ui_render(void);
Game *kid_ui_selected(void);
void kid_ui_notice_no_coins(void);
/* something is moving on screen (scroll, hold progress, notice) */
int kid_ui_animating(void);

#endif
