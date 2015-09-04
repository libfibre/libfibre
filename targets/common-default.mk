######################################################################
# Parsing routines, to extract info out of Makefile content during the
# recursion.

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = libname
# $(4) = source
define parse_lib_source
  $(eval FOO := $(2)_$(3)_$(4))
  $(eval LIB_$(3)_SOURCE += $(FOO))
  $(eval $(FOO)_dir := $(1))
  $(eval $(FOO)_dirid := $(2))
  $(eval $(FOO)_lib := $(3))
  $(eval $(FOO)_src := $(4))
  $(eval $(FOO)_out := $(OUT_OBJ)/$(patsubst %.c,%.o,$(FOO)))
  $(eval $(FOO)_dep := $$(patsubst %.o,%.d,$($(FOO)_out)))
  $(eval OUTS_OBJ += $(filter-out $(OUTS_OBJ),$($(FOO)_out)))
  $(eval OUTS_DEP += $(filter-out $(OUTS_DEP),$($(FOO)_dep)))
endef

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = binname
# $(4) = source
define parse_bin_source
  $(eval FOO := $(2)_$(3)_$(4))
  $(eval BIN_$(3)_SOURCE += $(FOO))
  $(eval $(FOO)_dir := $(1))
  $(eval $(FOO)_dirid := $(2))
  $(eval $(FOO)_bin := $(3))
  $(eval $(FOO)_src := $(4))
  $(eval $(FOO)_out := $(OUT_OBJ)/$(patsubst %.c,%.o,$(FOO)))
  $(eval $(FOO)_dep := $$(patsubst %.o,%.d,$($(FOO)_out)))
  $(eval OUTS_OBJ += $(filter-out $(OUTS_OBJ),$($(FOO)_out)))
  $(eval OUTS_DEP += $(filter-out $(OUTS_DEP),$($(FOO)_dep)))
endef

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = libname
define parse_lib
  $(eval myinstall := $(strip $($(3)_install)))
  $(eval LIBS += $(filter-out $(LIBS),$(3)))
  $(eval $(3)_name := lib$(3).a)
  $(eval $(3)_out := $(OUT_LIB)/$($(3)_name))
  $(eval $(3)_type := LIB)
  $(eval $(3)_make_dirs += $(filter-out $($(3)_make_dirs),$(1)))
  $(eval OUTS += $(filter-out $(OUTS),$($(3)_out)))
  $(foreach x,$(filter %.c,$($(3)_SOURCES)),$(eval $(call parse_lib_source,$(1),$(2),$(3),$(x))))
  ifneq (none,$(myinstall))
    ifeq (,$(myinstall))
      $(3)_inst_path := $(INSTALL_LIB)
    else
      $(3)_inst_path := $(myinstall)
    endif
    $(3)_inst_flags := $(INSTALL_LIB_FLAGS)
    INSTALLS += $(filter-out $(INSTALLS),$(3))
  endif
endef

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = binname
define parse_bin
  $(eval myinstall := $(strip $($(3)_install)))
  $(eval BINS += $(filter-out $(BINS),$(3)))
  $(eval $(3)_name := $(3))
  $(eval $(3)_out := $(OUT_BIN)/$($(3)_name))
  $(eval $(3)_type := BIN)
  $(eval $(3)_make_dirs += $(filter-out $($(3)_make_dirs),$(1)))
  $(eval OUTS += $(filter-out $(OUTS),$($(3)_out)))
  $(foreach x,$(filter %.c,$($(3)_SOURCES)),$(eval $(call parse_bin_source,$(1),$(2),$(3),$(x))))
  ifneq (none,$(myinstall))
    ifeq (,$(myinstall))
      $(3)_inst_path := $(INSTALL_BIN)
    else
      $(3)_inst_path := $(myinstall)
    endif
    $(3)_inst_flags := $(INSTALL_BIN_FLAGS)
    INSTALLS += $(filter-out $(INSTALLS),$(3))
  endif
endef

####################################################################
# Dependency-generation routines, invoked after recursive parsing of
# Makefile.am files has completed.

# $(1) = symbol name for object foo (dirid_libname_source)
define gen_lib_source
  $(eval DEPS += $($(1)_dep))
$($(1)_out): $(TOP_SRC)/$($(1)_dir)/$($(1)_src) $(TOP_SRC)/$($(1)_dir)/Makefile.am | $(OUT_OBJ)
	$$(Q)echo " [CC] $($(1)_lib):$($(1)_dir)/$($(1)_src)"
	$$(Q)touch $($(1)_dep)
	$$(Q)$(CC) -MMD -MP -MF$($(1)_dep) $(CFLAGS) $($($(1)_lib)_CFLAGS) -c $$< -o $$@
endef

# $(1) = symbol name for object foo (dirid_binname_source)
define gen_bin_source
  $(eval DEPS += $($(1)_dep))
$($(1)_out): $(TOP_SRC)/$($(1)_dir)/$($(1)_src) $(TOP_SRC)/$($(1)_dir)/Makefile.am | $(OUT_OBJ)
	$$(Q)echo " [CC] $($(1)_bin):$($(1)_dir)/$($(1)_src)"
	$$(Q)touch $($(1)_dep)
	$$(Q)$(CC) -MMD -MP -MF$($(1)_dep) $(CFLAGS) $($($(1)_bin)_CFLAGS) -c $$< -o $$@
endef

# $(1) = name of library
define gen_lib
  $(eval OBJS := $(foreach foo,$(LIB_$(1)_SOURCE),$($(foo)_out)))
$($(1)_out): $(OBJS) $(foreach m,$($(1)_make_dirs),$(TOP_SRC)/$(m)/Makefile.am) | $(OUT_LIB)
	$$(Q)echo " [AR] $(1)"
	$$(Q)$(RM) $$@
	$$(Q)$(AR) $(ARFLAGS) $$@ $(OBJS)
  $(foreach foo,$(LIB_$(1)_SOURCE),$(eval $(call gen_lib_source,$(foo))))
endef

# $(1) = name of binary
define gen_bin
  $(eval OBJS := $(foreach foo,$(BIN_$(1)_SOURCE),$($(foo)_out)))
$($(1)_out): $(OBJS) $(foreach m,$($(1)_make_dirs),$(TOP_SRC)/$(m)/Makefile.am) $(foreach lib,$($(1)_LDADD),$($(lib)_out)) | $(OUT_BIN)
	$$(Q)echo " [LINK] $(1)"
	$$(Q)$(LINK) $(LINKFLAGS) $(OBJS) $(foreach lib,$($(1)_LDADD),$($(lib)_out)) $($(1)_LINKFLAGS) $(foreach lib,$($(1)_LDADD),$($(lib)_LINKFLAGS)) -o $$@
  $(foreach foo,$(BIN_$(1)_SOURCE),$(eval $(call gen_bin_source,$(foo))))
endef

# $(1) = name of installable object
define gen_install
$(1)_inst: $($(1)_inst_path)/$($(1)_name)
$($(1)_inst_path)/$($(1)_name): $($(1)_out)
	$$(Q)echo " [INSTALL:$($(1)_type)] $(1)"
	$$(Q)install -D $($(1)_inst_flags) $($(1)_out) $($(1)_inst_path)/$($(1)_name)
$(1)_uninst:
	$$(Q)echo " [UNINSTALL:$($(1)_type)] $(1)"
	$$(Q)$(RM) $($(1)_inst_path)/$($(1)_name)
endef

define gen_libs
  $(foreach L,$(LIBS),$(eval $(call gen_lib,$(L))))
endef

define gen_bins
  $(foreach B,$(BINS),$(eval $(call gen_bin,$(B))))
endef

define gen_installs
  $(foreach i,$(INSTALLS),$(eval $(call gen_install,$(i))))
endef

define gen_all
  $(eval $(call gen_libs))
  $(eval $(call gen_bins))
  $(eval $(call gen_installs))
endef
