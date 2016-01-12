HOMEDIR := $(realpath $(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
# This little trick finds where this makefile exists.
$(info HOMEDIR set to $(HOMEDIR))

CXX = g++
AR = ar
RANLIB = ranlib

SRC_EXTS = .cpp .cxx .c
LIB_NAME = libgraphgaslite

LIBRARY = $(HOMEDIR)/lib/$(LIB_NAME).a

# gcc 4.8 bug, fixed by -Wl,--no-as-needed, see http://stackoverflow.com/a/19463892
MT_OPTS = -pthread -Wl,--no-as-needed

CXX_LANG = -Wall -Wextra -pedantic -std=c++11 $(MT_OPTS)
CXX_OPT  = -O3 -funroll-loops -march=native -flto
CXX_DBG  = -g -fkeep-inline-functions
INC_OPTS = -I$(HOMEDIR)/include
LIB_OPTS =

# User-supply flags through command line arguments
CL_CXXFLAGS =
CL_LDFLAGS  =

CXXFLAGS = $(CXX_LANG) $(CXX_OPT) $(INC_OPTS) $(CL_CXXFLAGS)
LDFLAGS  = $(CXX_LANG) $(CXX_OPT) $(LIB_OPTS) $(CL_LDFLAGS)

