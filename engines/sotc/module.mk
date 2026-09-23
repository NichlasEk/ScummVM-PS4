MODULE := engines/sotc
MODULE_OBJS = sotc.o pak.o metaengine.o startup.o sound.o
ifeq ($(ENABLE_SOTC), DYNAMIC_PLUGIN)
PLUGIN := 1
endif
include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
