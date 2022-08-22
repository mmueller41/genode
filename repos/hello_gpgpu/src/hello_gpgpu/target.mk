TARGET = hello_gpgpu
SRC_CC = main.cc test.cc polybench.cc CL/cl.cc benchmark/2mm/2mm.cc benchmark/3mm/3mm.cc benchmark/atax/atax.cc benchmark/bicg/bicg.cc benchmark/doitgen/doitgen.cc benchmark/gemm/gemm.cc benchmark/gemver/gemver.cc benchmark/gesummv/gesummv.cc benchmark/mvt/mvt.cc benchmark/syr2k/syr2k.cc benchmark/syrk/syrk.cc 
LIBS   = base libc 

CC_CXX_WARN_STRICT =
