CFLAGS := -pthread -ggdb3 -Wall
CFLAGS += -Wshadow -Wstrict-prototypes -Wwrite-strings -Wdeclaration-after-statement
CFLAGS += -DFIBRE_RUNTIME_CHECK

AR := ar
ARFLAGS := rcs

ifndef FIBRE_ARCH
$(error FIBRE_ARCH needs to be defined; e.g. ucontext, setjmp, x86)
endif

# Slurp in the standard routines for parsing Makefile content and
# generating dependencies
include targets/common-default.mk
