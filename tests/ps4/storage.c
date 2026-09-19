// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <dirent.h>
static int test_getdents(int fd, struct dirent *out, size_t size);
static int test_open(const char *path, int flags, ...);
static int test_close(int fd);
#define getdents test_getdents
#define open test_open
#define close test_close
#include "../../compat/ps4/storage.c"
#undef getdents
#undef open
#undef close
static int direct_usb, kernel_ok, aliases, mounts, releases, cred_reads, restores, privileged, empty_slots, available;
static uint64_t clock_us;
static int test_open(const char *path, int flags, ...) {
 (void)flags;
 if ((direct_usb || (privileged && available)) && strcmp(path,"/mnt/usb0")==0) return 10;
 if (empty_slots && strstr(path,"/mnt/usb")==path) return 12;
 if ((aliases & 1) && strcmp(path,"/scummvm_usb0")==0) return 11;
 return -1;
}
static int test_close(int fd) { assert(fd==10 || fd==11 || fd==12); return 0; }
static int test_getdents(int fd, struct dirent *out, size_t size) {
 assert(size >= 16);
 if (fd == 12) return 0; // An openable mount-point directory is not a device.
 unsigned char *b = (unsigned char *)out;
 memset(b,0,16); b[4]=16; b[7]=7; memcpy(b+8,"scummvm",7);
 return 16;
}
int jbc_jailbreak_cred(jbc_cred *cred) { cred->uid=123; return 0; }
uint64_t sceKernelGetProcessTime(void) { return clock_us; }
int jbc_get_cred(jbc_cred *cred) { ++cred_reads; memset(cred,0,sizeof(*cred)); return kernel_ok ? 0 : -1; }
int jbc_set_cred(const jbc_cred *cred) { privileged=(cred->uid==123); if (!privileged) ++restores; return 0; }
int jbc_mount_in_sandbox(const char *source, const char *name) {
 ++mounts;
 if (!strcmp(source,"/mnt/usb0") && !strcmp(name,"scummvm_usb0")) { aliases |= 1; return 0; }
 return 2;
}
int jbc_unmount_in_sandbox(const char *name) { assert(!strcmp(name,"scummvm_usb0")); ++releases; aliases &= ~1; return 0; }
int main(void) {
 direct_usb=1;
 ps4_refresh_usb_mounts(); assert(!cred_reads && !mounts);
 direct_usb=0; clock_us+=2000000;
 ps4_refresh_usb_mounts(); assert(cred_reads==1 && !mounts);
 kernel_ok=1; empty_slots=1; clock_us+=2000000;
 ps4_refresh_usb_mounts(); assert(!mounts && !aliases && !privileged);
 available=1; clock_us+=2000000;
 ps4_refresh_usb_mounts(); assert(aliases==1 && mounts==1 && restores>=2);
 ps4_refresh_usb_mounts(); assert(mounts==1); // no repeated work in the same browse refresh
 ps4_release_usb_mounts(); assert(!aliases && releases==1);
 ps4_release_usb_mounts(); assert(releases==1); // never unmount someone else's path
 puts("PS4 storage control-flow tests PASS (kernel service mocked)");
 return 0;
}
