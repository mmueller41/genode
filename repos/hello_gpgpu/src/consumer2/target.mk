TARGET = consumer2
SRC_CC = main.cc \
		2DConvolution.cc \
		../hello_gpgpu/polybench.cc \
		../hello_gpgpu/CL/cl.cc ../hello_gpgpu/CL/cl_genode.cc \
		../hello_gpgpu/allocator_stupid.cc

LIBS   = base libc libm

CC_CXX_WARN_STRICT =
