CFLAGS := -pthread -O2 -Wall
CFLAGS += -Wshadow -Wstrict-prototypes -Wwrite-strings

AR := ar
ARFLAGS := rcs

ifndef FIBRE_ARCH
$(error FIBRE_ARCH needs to be defined; e.g. ucontext, setjmp, x86)
endif

#FIBRE_ARCH := ucontext
#FIBRE_ARCH := setjmp
#FIBRE_ARCH := x86

# Slurp in the standard routines for parsing Makefile content and
# generating dependencies
include targets/common-default.mk
