
include ../../makefile.inc

SRC_DIR = .
BIN_DIR = bin

APPS = \
	   pagerank \
	   sssp \


ifdef ATLASPATH
APPS += \
		als\

CXXFLAGS += -I$(ATLASPATH)/include
LDFLAGS += -L$(ATLASPATH)/lib -llapack -lf77blas -lcblas -latlas
endif

default: $(addprefix $(BIN_DIR)/,$(APPS))

include ../common_harness/makefile.harness

$(foreach APP,$(APPS),$(eval $(call compile_rule,$(APP))))

