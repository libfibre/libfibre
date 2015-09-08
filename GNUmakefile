TOP_SRC := $(shell pwd)

TARGET ?= default-gcc
TARGET_FILE := targets/$(TARGET).mk

TOP_OUT ?= $(TOP_SRC)/output_$(TARGET)
OUT_OBJ := $(TOP_OUT)/obj
OUT_LIB := $(TOP_OUT)/lib
OUT_BIN := $(TOP_OUT)/bin
OUT_DIRS := $(OUT_OBJ) $(OUT_LIB) $(OUT_BIN)

TOP_INSTALL ?= $(TOP_SRC)/install_$(TARGET)
INSTALL_LIB ?= $(TOP_INSTALL)/lib
INSTALL_BIN ?= $(TOP_INSTALL)/bin
INSTALL_LIB_FLAGS ?= --mode=644
INSTALL_BIN_FLAGS ?= --mode=755

all: build

include $(TARGET_FILE)

ifndef V
  Q := @
endif

# TODO: this misc stuff should probably also go into the targets/fu, but
# I don't want to create disorganisation in there for now. The targets/fu
# should allow a mix of specific and less specific information to be
# combined by appropriate factoring and includes. I've only made an
# initial sketch for now, by creating two targets (default-gcc, and
# debug-gcc), both of which use the same "parse" and "gen" routines
# factored out into a "common-default" include. Stuff like setting RMDIR,
# LINK, pointing CFLAGS to the include dir, LINKFLAGS to the library dir,
# and so forth... that stuff may eventually need to be made modifiable
# by the target, but once that happens, we'll presumably have a better
# idea of the granularity/factoring considerations. For now, keeping it
# here.
RMDIR ?= $(RM) -d
LINK ?= $(CC)
CFLAGS += -I$(TOP_SRC)/include

#############################################################
# Utility function missing from Make. Reverse a list of items
reverse = $(if $(wordlist 2,2,$(1)),$(call reverse,$(wordlist 2,$(words $(1)),$(1))) $(firstword $(1)),$(1))

########################################################
# Parse the per-directory Makefile.am files, recursively

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
define parse_dir
  $(eval lib_LIBRARIES := )
  $(eval bin_BINARIES := )
  $(eval SUBDIRS := )
  $(eval THISDIR := $(TOP_SRC)/$(1))
  $(eval include $(TOP_SRC)/$(1)/Makefile.am)
  $(foreach L,$(lib_LIBRARIES),$(eval $(call parse_lib,$(1),$(2),$(L))))
  $(foreach B,$(bin_BINARIES),$(eval $(call parse_bin,$(1),$(2),$(B))))
  $(foreach s,$(SUBDIRS),$(eval $(call parse_dir,$(1)/$(s),$(2)_$(s))))
endef

$(eval $(call parse_dir,.,))

######################################################
# Declare all the deps based on the results of parsing
$(eval $(call gen_all))

#######################################
# Define the principle makefile targets

build: $(OUTS)

clean:
	$(Q)$(RM) $(OUTS) $(OUTS_OBJ) $(OUTS_DEP)
	$(Q)$(RMDIR) $(call reverse,$(OUT_DIRS))
	$(Q)$(RMDIR) $(TOP_OUT)

install: $(foreach i,$(INSTALLS),$(i)_inst)
uninstall: $(foreach i,$(INSTALLS),$(i)_uninst)
	$(Q)$(RMDIR) $(INSTALL_LIB)
	$(Q)$(RMDIR) $(INSTALL_BIN)
	$(Q)$(RMDIR) $(TOP_INSTALL)

$(OUT_DIRS):
	$(Q)mkdir -p $@

debug:
	@echo "Different debug output available;"
	@echo "  make debug_install  - show installations"
	@echo "  make debug_lib      - show library builds"
	@echo "  make debug_bin      - show binary builds"
	@echo "  make debug_target   - show build targets"
	@echo "  make debug_deps     - show dependency files"

define debug_inst
  echo "Install target: $1";
  echo "      $1_inst_path = $($(1)_inst_path)";
  echo "      $1_inst_flags = $($(1)_inst_flags)";
  echo "      $1_name = $($(1)_name)";
  echo "      $1_out = $($(1)_out)";
endef

debug_install:
	@$(foreach i,$(INSTALLS),$(call debug_inst,$(i)))

define debug_lib_foo
  echo "    source: $($(1)_src) (dirid: $($(1)_dirid))";
endef
define debug_lib
  echo "Library: $(1)";
  $(foreach x,$(LIB_$(1)_SOURCE),$(call debug_lib_foo,$(x)))
endef

debug_lib:
	@$(foreach i,$(LIBS),$(call debug_lib,$(i)))

define debug_bin_foo
  echo "    source: $($(1)_src) (dirid: $($(1)_dirid))";
endef
define debug_bin_ldadd
  echo "     ldadd: $(1) ($($(1)_out))";
endef
define debug_bin
  echo "Executable: $(1)";
  $(foreach x,$(BIN_$(1)_SOURCE),$(call debug_bin_foo,$(x)))
  $(foreach lib,$($(1)_LDADD),$(call debug_bin_ldadd,$(lib)))
endef

debug_bin:
	@$(foreach i,$(BINS),$(call debug_bin,$(i)))

define debug_dep
  echo "    $(1)";
endef

debug_deps:
	@$(foreach d,$(DEPS),$(call debug_dep,$(d)))

#####################################
# Include auto-generated dependencies
-include $(foreach d,$(DEPS),$(d)))
