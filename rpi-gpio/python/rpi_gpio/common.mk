ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION = Python module implementing a client for the GPIO resource manager
endef
INSTALLDIR=/usr/lib/python3.11
NAME=rpi_gpio
SONAME=rpi_gpio.so

EXTRA_INCVPATH=$(QNX_TARGET)/usr/include/python3.11 $(QNX_TARGET)/usr/include/$(CPUVARDIR)/python3.11
LIBS=python3.11

include $(MKFILES_ROOT)/qtargets.mk
