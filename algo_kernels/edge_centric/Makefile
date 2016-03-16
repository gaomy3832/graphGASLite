
include ../../makefile.inc

SRC_DIR = .
BIN_DIR = bin

APPS = \
	   pagerank \


default: $(addprefix $(BIN_DIR)/,$(APPS))

include ../common_harness/makefile.harness

$(foreach APP,$(APPS),$(eval $(call compile_rule,$(APP))))

