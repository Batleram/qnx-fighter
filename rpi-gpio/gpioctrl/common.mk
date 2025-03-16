ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

define PINFO
PINFO DESCRIPTION = gpioctrl
endef
INSTALLDIR=
NAME=gpioctrl
USEFILE=

LIBS+=screen cairo pixman-1
POST_INSTALL=$(CP_HOST) -r $(PROJECT_ROOT)/images/*.png $(INSTALL_ROOT_nto)/usr/share/gpioctrl/images/

include $(MKFILES_ROOT)/qtargets.mk
