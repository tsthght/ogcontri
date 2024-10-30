
MODULES = og_savior


EXTENSION = og_savior
DATA = og_savior--0.0.1.sql


ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/og_savior
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

exclude_option=-fPIE
override CPPFLAGS := $(filter-out -fPIE, $(CPPFLAGS)) -fPIC