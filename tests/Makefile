#
# Makefile for building unit tests with Google Test.
#

include ../makefile.inc

## Google Test setting.
GTEST_REPO = ../googletest
GTEST_BASE = $(GTEST_REPO)/googletest
GTEST_PATH = $(GTEST_BASE)/include
GTEST_LIB  = $(GTEST_BASE)/make/gtest_main.a

## Google Test additional flags.
CXX_DEBUG = -g
override CXXFLAGS += $(CXX_LANG) $(CXX_DEBUG) $(INC_OPTS) \
	-I$(GTEST_PATH)
override LDFLAGS += $(CXX_LANG) $(CXX_DEBUG) $(LIB_OPTS) \
	$(GTEST_LIB) -lpthread


## Tests
TESTS += \
		 comm_sync


## Test build rules
BIN_DIR = bin
BINS = $(foreach TEST,$(TESTS),$(BIN_DIR)/$(TEST))
OBJS = $(foreach TEST,$(TESTS),$(TEST).o)
DEPS = $(OBJS:.o=.d)

all: $(GTEST_LIB) $(BINS)

define compile_rule
$(BIN_DIR)/$1: $1.o $(BIN_DIR)
	$$(CXX) $$< -o $$@ $$(CXXFLAGS) $$(LDFLAGS)
endef
$(foreach TEST,$(TESTS),$(eval $(call compile_rule,$(TEST))))

# .cpp.o and .c.o means the same as %.o:%.cpp and %.o:%.c
$(SRC_EXTS:=.o):
	$(CXX) $< -c -o $@ $(CXXFLAGS) -MP -MMD

$(BIN_DIR):
	@mkdir -p $@

clean:
	rm -f $(OBJS) $(DEPS) $(BINS) *~
	rm -rf $(BIN_DIR)


## Google Test setup and clean
$(GTEST_LIB):
	@if [ ! -d "$(GTEST_REPO)" ]; then git clone git@github.com:google/googletest.git $(GTEST_REPO); fi
	cd $(GTEST_BASE)/make; make

clean_gtest:
	rm -rf $(GTEST_REPO)


.PHONY: clean clean_gtest

-include $(DEPS)

