ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION = GPIO resource manager for Raspberry Pi 3/4
endef
INSTALLDIR=sbin
NAME=rpi_gpio

#EXTRA_INCVPATH=
#EXTRA_LIBVPATH=
LIBS=login secpol

include $(MKFILES_ROOT)/qtargets.mk
