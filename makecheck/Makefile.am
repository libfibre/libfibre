SUBDIRS = subdir1 subdir2

lib_LIBRARIES = mc
bin_BINARIES = mc_install_me_normal
bin_BINARIES += mc_install_me_abnormal
bin_BINARIES += mc_dont_install_me

mc_SOURCES = a.c b.c

mc_install_me_normal_SOURCES = mc_install.c

mc_install_me_abnormal_SOURCES = mc_install.c
mc_install_me_abnormal_install = $(TOP_INSTALL)/odd_place

mc_dont_install_me_SOURCES = mc_install.c
mc_dont_install_me_install = none
