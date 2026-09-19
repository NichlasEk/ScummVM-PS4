// SPDX-License-Identifier: GPL-3.0-or-later
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
static int test_open(const char *path, int flags, ...);
static int test_close(int fd);
#define open test_open
#define close test_close
#include "../../compat/ps4/storage.c"
#undef open
#undef close
static int direct_usb, kernel_ok, aliases, mounts, releases, cred_reads, restores;
static uint64_t clock_us;
static int test_open(const char *path, int flags, ...) {
 (void)flags;
 if (direct_usb && strcmp(path,"/mnt/usb0")==0) return 10;
 if ((aliases & 1) && strcmp(path,"/usb0")==0) return 11;
 return -1;
}
static int test_close(int fd) { assert(fd==10 || fd==11); return 0; }
uint64_t sceKernelGetProcessTime(void) { return clock_us; }
int jbc_get_cred(jbc_cred *cred) { ++cred_reads; memset(cred,0,sizeof(*cred)); return kernel_ok ? 0 : -1; }
int jbc_set_cred(const jbc_cred *cred) { (void)cred; ++restores; return 0; }
int jbc_mount_in_sandbox(const char *source, const char *name) {
 ++mounts;
 if (!strcmp(source,"/mnt/usb0") && !strcmp(name,"usb0")) { aliases |= 1; return 0; }
 return 2;
}
int jbc_unmount_in_sandbox(const char *name) { assert(!strcmp(name,"usb0")); ++releases; aliases &= ~1; return 0; }
int main(void) {
 direct_usb=1;
 ps4_refresh_usb_mounts(); assert(!cred_reads && !mounts);
 direct_usb=0; clock_us+=2000000;
 ps4_refresh_usb_mounts(); assert(cred_reads==1 && !mounts);
 kernel_ok=1; clock_us+=2000000;
 ps4_refresh_usb_mounts(); assert(aliases==1 && mounts==8 && restores==1);
 ps4_refresh_usb_mounts(); assert(mounts==8); // no repeated work in the same browse refresh
 ps4_release_usb_mounts(); assert(!aliases && releases==1);
 ps4_release_usb_mounts(); assert(releases==1); // never unmount someone else's path
 puts("PS4 storage control-flow tests PASS (kernel service mocked)");
 return 0;
}
