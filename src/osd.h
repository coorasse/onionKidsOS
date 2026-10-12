#ifndef OSD_H
#define OSD_H

#include <stdint.h>

/* Big volume indicator drawn over whatever screen is showing, for a moment after a volume key. */
void osd_update(uint32_t dt);
void osd_render(void);
int osd_visible(void);

#endif
