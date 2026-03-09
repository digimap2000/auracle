/*
 * Auracle runtime mode control for the nRF5340 side.
 *
 * This isolates transport-driven policy from the Nordic sample logic so the
 * desktop can switch the device between passive monitoring and active sink
 * behavior without scattering mode checks across the codebase.
 */

#ifndef _AURACLE_MODE_H_
#define _AURACLE_MODE_H_

#include <stdbool.h>

enum auracle_mode {
	AURACLE_MODE_MONITOR = 0,
	AURACLE_MODE_SINK = 1,
};

const char *auracle_mode_name(enum auracle_mode mode);
enum auracle_mode auracle_mode_get(void);
bool auracle_mode_allows_broadcast_auto_sync(void);

int auracle_mode_from_str(const char *name, enum auracle_mode *mode);
int auracle_mode_apply(void);
int auracle_mode_set(enum auracle_mode mode);

#endif /* _AURACLE_MODE_H_ */
