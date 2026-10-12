#ifndef RESUME_UI_H
#define RESUME_UI_H

#include "library.h"

/* Shown before a game that has an auto save state: continue where the kid left off, or start over. */
enum { RESUME_NONE, RESUME_CONTINUE, RESUME_FRESH, RESUME_BACK };

void resume_ui_enter(Game *game);
int resume_ui_update(void);
void resume_ui_render(void);

#endif
