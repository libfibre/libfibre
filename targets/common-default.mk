######################################################################
# Parsing routines, to extract info out of Makefile content during the
# recursion.

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = libname
# $(4) = source
define parse_source
  $(eval TMPD := $(dir $(4)))
  $(eval TMPF := $(notdir $(4)))
  $(eval FOO := $(2)_$(3)_$(TMPF))
  $(eval SOURCE_$(3) += $(FOO))
  $(eval $(FOO)_dir := $(1))
  $(eval $(FOO)_dirid := $(2))
  $(eval $(FOO)_title := $(3))
  $(eval $(FOO)_src := $(TMPF))
  $(eval $(FOO)_out := $(OUT_OBJ)/$(strip $(patsubst %.c,%.o,$(filter %.c,$(FOO))) $(patsubst %.s,%.o,$(filter %.s,$(FOO)))))
  $(eval $(FOO)_dep := $(patsubst %.o,%.d,$($(FOO)_out)))
  $(eval OUTS_OBJ += $(filter-out $(OUTS_OBJ),$($(FOO)_out)))
  $(eval OUTS_DEP += $(filter-out $(OUTS_DEP),$($(FOO)_dep)))
  $(FOO)_display := $($(FOO)_dir)/$($(FOO)_src)
  ifeq (./,$(TMPD))
    $(FOO)_path := $(TOP_SRC)/$(1)/$(TMPF)
  else
    $(FOO)_path := $(4)
    $(FOO)_display += (indirect)
  endif
endef

# $(1) = directory, relative to $(TOP_SRC)
# $(2) = dirid
# $(3) = title
# $(4) = filename
# $(5) = type, LIB or BIN
define parse_tgt
  $(eval myinstall := $(strip $($(3)_install)))
  $(eval $(3)_name := $(3))
  $(eval $(3)_out := $(OUT_$(5))/$(4))
  $(eval $(3)_type := $(5))
  $(eval $(3)_make_dirs += $(filter-out $($(3)_make_dirs),$(1)))
  $(eval OUTS += $(filter-out $(OUTS),$($(3)_out)))
  $(foreach x,$(filter %.c %.s,$($(3)_SOURCES)),$(eval $(call parse_source,$(1),$(2),$(3),$(x))))
  ifneq (none,$(myinstall))
    ifeq (,$(myinstall))
      $(3)_inst_path := $(INSTALL_$(5))
    else
      $(3)_inst_path := $(myinstall)
    endif
    $(3)_inst_flags := $(INSTALL_$(5)_FLAGS)
    INSTALLS += $(filter-out $(INSTALLS),$(3))
  endif
endef

define parse_lib
  $(eval LIBS += $(filter-out $(LIBS),$(3)))
  $(eval $(call parse_tgt,$(1),$(2),$(3),lib$(3).a,LIB))
endef

define parse_bin
  $(eval BINS += $(filter-out $(BINS),$(3)))
  $(eval $(call parse_tgt,$(1),$(2),$(3),$(3),BIN))
endef

####################################################################
# Dependency-generation routines, invoked after recursive parsing of
# Makefile.am files has completed.

# $(1) = symbol name for object foo (dirid_title_source)
define gen_source
  $(eval DEPS += $($(1)_dep))
$($(1)_out): $($(1)_path) $(TOP_SRC)/$($(1)_dir)/Makefile.am $($($(1)_title)_PREREQS) | $(OUT_OBJ)
	$$(Q)echo " [CC] $($(1)_title):$($(1)_display)"
	$$(Q)touch $($(1)_dep)
	$$(Q)$(CC) -MMD -MP -MF$($(1)_dep) $(CFLAGS) $($($(1)_title)_CFLAGS) -c $$< -o $$@
endef

# $(1) = name of library
define gen_lib
  $(eval OBJS := $(foreach foo,$(SOURCE_$(1)),$($(foo)_out)))
$($(1)_out): $(OBJS) $(foreach m,$($(1)_make_dirs),$(TOP_SRC)/$(m)/Makefile.am) | $(OUT_LIB)
	$$(Q)echo " [AR] $(1)"
	$$(Q)$(RM) $$@
	$$(Q)$(AR) $(ARFLAGS) $$@ $(OBJS)
  $(foreach foo,$(SOURCE_$(1)),$(eval $(call gen_source,$(foo))))
endef

# $(1) = name of binary
define gen_bin
  $(eval OBJS := $(foreach foo,$(SOURCE_$(1)),$($(foo)_out)))
$($(1)_out): $(OBJS) $(foreach m,$($(1)_make_dirs),$(TOP_SRC)/$(m)/Makefile.am) $(foreach lib,$($(1)_LDADD),$($(lib)_out)) | $(OUT_BIN)
	$$(Q)echo " [LINK] $(1)"
	$$(Q)$(LINK) $(LINKFLAGS) $(OBJS) $(foreach lib,$($(1)_LDADD),$($(lib)_out)) $($(1)_LINKFLAGS) $(foreach lib,$($(1)_LDADD),$($(lib)_LINKFLAGS)) -o $$@
  $(foreach foo,$(SOURCE_$(1)),$(eval $(call gen_source,$(foo))))
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
