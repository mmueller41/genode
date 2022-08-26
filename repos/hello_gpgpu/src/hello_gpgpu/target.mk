TARGET = hello_gpgpu
SRC_CC = main.cc \
		test.cc polybench.cc \
		CL/cl.cc CL/cl_genode.cc \
		benchmark/2mm/2mm.cc \
		benchmark/3mm/3mm.cc \
		benchmark/atax/atax.cc \
		benchmark/bicg/bicg.cc \
		benchmark/doitgen/doitgen.cc \
		benchmark/gemm/gemm.cc \
		benchmark/gemver/gemver.cc \
		benchmark/gesummv/gesummv.cc \
		benchmark/mvt/mvt.cc \
		benchmark/syr2k/syr2k.cc \
		benchmark/syrk/syrk.cc \
		benchmark/correlation/correlation.cc \
		benchmark/covariance/covariance.cc \
		benchmark/adi/adi.cc \
		benchmark/convolution-2d/2DConvolution.cc \
		benchmark/convolution-3d/3DConvolution.cc \
		benchmark/fdtd-2d/fdtd2d.cc \
		benchmark/jacobi-1d-imper/jacobi1D.cc \
		benchmark/jacobi-2d-imper/jacobi2D.cc \
		benchmark/gramschmidt/gramschmidt.cc \
		benchmark/lu/lu.cc \

LIBS   = base libc libm

CC_CXX_WARN_STRICT =
