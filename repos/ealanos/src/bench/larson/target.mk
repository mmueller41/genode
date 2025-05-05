TARGET = larson
SRC_CC = larson.cpp
LIBS += base libm libc stdcxx 
CC_OPT += -Wno-error -Wno-permissive -fpermissive -DPRIVATE -Wno-error=conversion -Wno-error=write-strings