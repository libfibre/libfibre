lib_LIBRARIES = fibre

fibre_SOURCES = fibre.c arch-$(FIBRE_ARCH).c
fibre_SOURCES += sel_origin.c sel_scheduler.c
# LINKFLAGS for a *library* aren't used when building the lib, but do get used
# when linking executables that *depend* on this lib...
ifeq ($(FIBRE_ARCH),setjmp)
fibre_LINKFLAGS += -lpthread
endif
