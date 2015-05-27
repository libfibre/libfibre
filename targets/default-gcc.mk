CFLAGS := -pthread -O2 -Wall
CFLAGS += -Wshadow -Wstrict-prototypes -Wwrite-strings -Wdeclaration-after-statement

AR := ar
ARFLAGS := rcs

FIBRE_ARCH := ucontext

# Slurp in the standard routines for parsing Makefile content and
# generating dependencies
include targets/common-default.mk
