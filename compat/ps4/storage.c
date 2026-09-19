// SPDX-License-Identifier: GPL-3.0-or-later
// Keep ordinary app paths/settings intact; expose USB through read-only nullfs.
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include "libjbc.h"
static unsigned mounted;
static uint64_t last_attempt;
static int have_attempted;
static int readable_directory(const char *path) {
 int fd = open(path, O_RDONLY | O_DIRECTORY);
 if (fd < 0) return 0;
 close(fd); return 1;
}
void ps4_refresh_usb_mounts(void) {
 uint64_t now = sceKernelGetProcessTime();
 if (have_attempted && now - last_attempt < 2000000) return;
 have_attempted = 1; last_attempt = now;
 int missing = 0, direct = 0;
 for (int i = 0; i < 8; ++i) {
  char path[32], alias[16];
  snprintf(path, sizeof(path), "/mnt/usb%d", i);
  snprintf(alias, sizeof(alias), "/usb%d", i);
  if (readable_directory(path)) { direct = 1; continue; }
  if (readable_directory(alias)) continue;
  missing |= 1 << i;
 }
 // Directly mounted USB already works (including the emulator testbench).
 if (!missing || direct) return;
 jbc_cred original;
 if (jbc_get_cred(&original)) {
  printf("[ps4-usb] sandbox access unavailable; no supported homebrew kernel service\n");
  return;
 }
 for (int i = 0; i < 8; ++i) {
  if (!(missing & (1 << i))) continue;
  char path[32], name[16], alias[16];
  snprintf(path, sizeof(path), "/mnt/usb%d", i);
  snprintf(name, sizeof(name), "usb%d", i);
  snprintf(alias, sizeof(alias), "/usb%d", i);
  int result = jbc_mount_in_sandbox(path, name);
  printf("[ps4-usb] map %s -> %s result=%d readable=%d\n", path, alias, result, readable_directory(alias));
  if (!result) mounted |= 1 << i;
 }
 // Defensive restoration also covers a partial credential operation.
 if (jbc_set_cred(&original)) printf("[ps4-usb] ERROR restoring process credentials\n");
}
void ps4_release_usb_mounts(void) {
 for (int i = 0; i < 8; ++i) {
  if (mounted & (1 << i)) {
   char name[16]; snprintf(name, sizeof(name), "usb%d", i);
   printf("[ps4-usb] unmap %s result=%d\n", name, jbc_unmount_in_sandbox(name));
  }
 }
 mounted = 0;
}
