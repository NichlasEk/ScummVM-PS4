/* USB mouse ABI: event deltas, not an emulated analog-stick velocity. */
#ifndef PS4GL_MOUSE_H
#define PS4GL_MOUSE_H
#include <stdint.h>
#include <stddef.h>
typedef struct {
  uint64_t timestamp;
  uint8_t connected, padding[3];
  uint32_t buttons;
  int32_t x, y, wheel, tilt;
  uint8_t reserved[8];
} PS4GLMouseData;
_Static_assert(sizeof(PS4GLMouseData)==40, "Mouse report ABI size");
_Static_assert(offsetof(PS4GLMouseData,buttons)==12, "Mouse report ABI buttons");
/* OpenOrbis's Mouse.h still has placeholder signatures. */
extern int sceMouseInit(void);
extern int sceMouseOpen(int user, int type, int index, const void *param);
extern int sceMouseRead(int handle, PS4GLMouseData *data, int count);
extern int sceMouseClose(int handle);
#endif
