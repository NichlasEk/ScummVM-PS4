// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SCUMMVM_PS4_PIGLET_CONTEXT_H
#define SCUMMVM_PS4_PIGLET_CONTEXT_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <orbis/Pigletv2VSH.h>

/* Explicit, optional runtime: no Piglet import dependency in the executable.
 * This remains separate from the default SDL/software renderer until hardware
 * context, shader and pixel tests pass. */
typedef struct Ps4Piglet Ps4Piglet;
Ps4Piglet *ps4_piglet_load(FILE *log);
int ps4_piglet_create(Ps4Piglet *p, unsigned width, unsigned height);
void *ps4_piglet_proc(Ps4Piglet *p, const char *name);
int ps4_piglet_swap(Ps4Piglet *p);
void ps4_piglet_close(Ps4Piglet *p);
#endif
