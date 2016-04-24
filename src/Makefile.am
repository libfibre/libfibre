$(eval FIBRE_ARCH_TOP = $(THISDIR))

lib_LIBRARIES = fibre

fibre_SOURCES = fibre.c
fibre_SOURCES += sel_origin.c sel_scheduler.c
# LINKFLAGS for a *library* aren't used when building the lib, but do get used
# when linking executables that *depend* on this lib...
ifeq ($(FIBRE_ARCH),setjmp)
fibre_LINKFLAGS += -lpthread
endif

FIBRE_ARCH_OUTDIR=$(TOP_OUT)/fibre_arch
FIBRE_ARCH_HEADER=$(FIBRE_ARCH_OUTDIR)/fibre_arch.h
FIBRE_ARCH_SRC=$(FIBRE_ARCH_TOP)/arch-$(FIBRE_ARCH).h
$(eval fibre_CFLAGS = -I$(FIBRE_ARCH_OUTDIR))

$(FIBRE_ARCH_HEADER): $(FIBRE_ARCH_SRC) | $(FIBRE_ARCH_OUTDIR)
	$(Q)echo " [GEN] arch-$(FIBRE_ARCH).h -> fibre_arch.h"
	$(Q)echo "/* DO NOT EDIT, AUTO-GENERATED FILE */" > $(FIBRE_ARCH_HEADER)
	$(Q)echo "/* EDIT $(FIBRE_ARCH_SRC) INSTEAD */" >> $(FIBRE_ARCH_HEADER)
	$(Q)cat $(FIBRE_ARCH_SRC) >> $(FIBRE_ARCH_HEADER)

$(eval OUTS += $(FIBRE_ARCH_HEADER))
$(eval OUT_DIRS += $(FIBRE_ARCH_OUTDIR))
