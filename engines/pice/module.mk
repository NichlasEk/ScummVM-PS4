MODULE := engines/pice
MODULE_OBJS = pice.o script.o resource-names.o sprite.o kro.o selftest.o metaengine.o
ifeq ($(ENABLE_PICE), DYNAMIC_PLUGIN)
PLUGIN := 1
endif
include $(srcdir)/rules.mk
DETECT_OBJS += $(MODULE)/detection.o
