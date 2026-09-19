// SPDX-License-Identifier: GPL-3.0-or-later
// Keep ordinary app paths/settings intact; expose USB through read-only nullfs.
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <orbis/libkernel.h>
#include "libjbc.h"
static void storage_log(const char *format, ...) {
 va_list args, copy;
 va_start(args, format); va_copy(copy, args);
 vprintf(format, args);
 FILE *log = fopen("/data/scummvm/usb-diagnostics.log", "a");
 if (log) { vfprintf(log, format, copy); fclose(log); }
 va_end(copy); va_end(args);
}
#define printf storage_log
static char usb_status[96] = "USB: not scanned";
const char *ps4_usb_status_text(void) { return usb_status; }
static void set_status(const char *text, int code) {
 snprintf(usb_status, sizeof(usb_status), "%s [%d]", text, code);
 printf("[ps4-usb] %s\n", usb_status);
}
static unsigned mounted;
static uint64_t last_attempt;
static int have_attempted;
// Mount-point directories can exist with no device attached. A game browser
// must verify a real directory entry, not just that open(O_DIRECTORY) succeeds.
int ps4_usb_has_entries(const char *path) {
 int fd = open(path, O_RDONLY | O_DIRECTORY);
 if (fd < 0) return 0;
 unsigned char buffer[4096];
 int found = 0;
 for (;;) {
  int bytes = getdents(fd, (struct dirent *)buffer, sizeof(buffer));
  if (bytes <= 0 || bytes > (int)sizeof(buffer)) break;
  unsigned pos = 0;
  while (pos < (unsigned)bytes) {
   unsigned left = (unsigned)bytes - pos;
   if (left < 9) goto done;
   unsigned size = buffer[pos+4] | ((unsigned)buffer[pos+5] << 8);
   unsigned length = buffer[pos+7];
   if (size < 9 || size > left || !length || length > size-9 ||
       buffer[pos+8+length] || memchr(buffer+pos+8, 0, length) ||
       memchr(buffer+pos+8, '/', length)) goto done;
   const char *name = (const char *)buffer+pos+8;
   if (!((length == 1 && name[0] == '.') ||
         (length == 2 && name[0] == '.' && name[1] == '.'))) {
    found = 1; goto done;
   }
   pos += size;
  }
 }
done:
 close(fd);
 return found;
}
void ps4_refresh_usb_mounts(void) {
 uint64_t now = sceKernelGetProcessTime();
 if (have_attempted && now - last_attempt < 2000000) return;
 have_attempted = 1; last_attempt = now;
 int missing = 0, reachable = 0;
 for (int i = 0; i < 8; ++i) {
  char path[32], alias[16];
  snprintf(path, sizeof(path), "/mnt/usb%d", i);
  snprintf(alias, sizeof(alias), "/scummvm_usb%d", i);
  if (ps4_usb_has_entries(path)) { ++reachable; continue; }
  if (ps4_usb_has_entries(alias)) { ++reachable; continue; }
  missing |= 1 << i;
 }
 // Directly mounted USB already works (including the emulator testbench).
 if (!missing || reachable) { set_status("USB ready", 0); return; }
 // Only map missing slots if no directly accessible game-bearing USB exists.
 for (int i = 0; i < 8; ++i) {
  char path[32]; snprintf(path, sizeof(path), "/mnt/usb%d", i);
  if (ps4_usb_has_entries(path)) { set_status("USB ready", 0); return; }
 }
 jbc_cred original;
 if (jbc_get_cred(&original)) {
  set_status("USB U1: homebrew storage service unavailable", 1);
  return;
 }
 // Inspect the real mount sources while temporarily outside the sandbox.
 // Restore before calling libjbc's sandbox-mount helper.
 jbc_cred root = original;
 if (jbc_jailbreak_cred(&root) || jbc_set_cred(&root)) {
  jbc_set_cred(&original);
  set_status("USB U2: cannot access console storage", 2);
  return;
 }
 unsigned present = 0;
 for (int i = 0; i < 8; ++i) {
  char path[32]; snprintf(path, sizeof(path), "/mnt/usb%d", i);
  int populated = ps4_usb_has_entries(path);
  printf("[ps4-usb] source %s populated=%d\n", path, populated);
  if (populated) present |= 1 << i;
 }
 if (jbc_set_cred(&original)) {
  set_status("USB U6: storage access restore failed", 6);
  return;
 }
 if (!present) { set_status("USB U3: no readable files found on USB", 3); return; }
 int usable = 0, last_error = 0;
 for (int i = 0; i < 8; ++i) {
  if (!(present & (1 << i))) continue;
  if (!(missing & (1 << i))) continue;
  char path[32], name[16], alias[16];
  snprintf(path, sizeof(path), "/mnt/usb%d", i);
  snprintf(name, sizeof(name), "scummvm_usb%d", i);
  snprintf(alias, sizeof(alias), "/scummvm_usb%d", i);
  int result = jbc_mount_in_sandbox(path, name);
  printf("[ps4-usb] map %s -> %s result=%d readable=%d\n", path, alias, result, ps4_usb_has_entries(alias));
  if (!result) mounted |= 1 << i;
  if (ps4_usb_has_entries(alias)) ++usable;
  else last_error = result;
 }
 // Defensive restoration also covers a partial credential operation.
 if (jbc_set_cred(&original)) set_status("USB U6: storage access restore failed", 6);
 else if (usable) set_status("USB ready", 0);
 else set_status("USB U4: drive found, cannot read mapped files", last_error);
}
void ps4_release_usb_mounts(void) {
 for (int i = 0; i < 8; ++i) {
  if (mounted & (1 << i)) {
   char name[16]; snprintf(name, sizeof(name), "scummvm_usb%d", i);
   printf("[ps4-usb] unmap %s result=%d\n", name, jbc_unmount_in_sandbox(name));
  }
 }
 mounted = 0;
}
