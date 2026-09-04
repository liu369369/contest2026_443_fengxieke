############################################################################
# FireEye - Electric Bicycle Charging Safety Monitor
# Makefile
############################################################################

include $(APPDIR)/Make.defs

# Application name
PROGNAME = fireeye
PRIORITY = $(CONFIG_FIREYEYE_PRIORITY)
STACKSIZE = $(CONFIG_FIREYEYE_STACKSIZE)

# Source files
CSRCS = src/fireeye_main.c \
        src/fireeye_filter.c \
        src/fireeye_threshold.c \
        src/fireeye_fsm.c

# Application entry point
MAINSRC = src/fireeye_main.c

include $(APPDIR)/Application.mk
