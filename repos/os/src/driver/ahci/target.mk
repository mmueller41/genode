TARGET   = ahci
SRC_CC  += main.cc
INC_DIR += $(PRG_DIR)
LIBS    += base

vpath %.cc $(PRG_DIR)
