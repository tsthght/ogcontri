# contrib/pg_stat_statements/Makefile

MODULES = og_full_sql


EXTENSION = og_full_sql
DATA = og_full_sql--1.sql

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/og_full_sql
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif

exclude_option=-fPIE
override CPPFLAGS := $(filter-out -fPIE, $(CPPFLAGS)) -fPIC