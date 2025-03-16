ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION = Python module implementing the SMBus interface
endef
INSTALLDIR=/usr/lib/python3.11
NAME=smbus
SONAME=smbus.so

EXTRA_INCVPATH=$(QNX_TARGET)/usr/include/python3.11 $(QNX_TARGET)/usr/include/$(CPUVARDIR)/python3.11
LIBS=python3.11

include $(MKFILES_ROOT)/qtargets.mk
