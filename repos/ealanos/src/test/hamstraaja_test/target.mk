TARGET = hamstraaja_test
SRC_CC = main.cc
LIBS = base
INC_DIR = $(PRG_DIR)
INC_DIR += $(REP_DIR)/include
INC_DIR += $(BASE_DIR)/src/include/

CC_OPT += -Wno-error=effc++

