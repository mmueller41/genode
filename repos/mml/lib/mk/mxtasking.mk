MXTASKING_DIR := $(call select_from_ports,mxtasking)/src/lib/mxtasking

vpath %.cpp $(MXTASKING_DIR)/src/mx

INC_DIR += $(MXTASKING_DIR)/src/mx $(MXTASKING_DIR)/lib
vpath %.h ${INC_DIR}

CC_OPT += -pedantic -Wall -Wextra -Werror \
 -Wno-invalid-offsetof -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization \
 -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Woverloaded-virtual \
 -Wredundant-decls -Wshadow -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef \
 -Wno-unused -Wold-style-cast -Wno-uninitialized -O1 -g3

CC_OPT += $(addprefix -I ,$(INC_DIR))

LIBS += libm libc stdcxx
SHARED_LIB = yes