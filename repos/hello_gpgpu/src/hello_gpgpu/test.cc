#include <base/log.h>
#include <base/allocator_avl.h>
#define CL_TARGET_OPENCL_VERSION 100
#include "CL/cl.h"

static unsigned char test_Gen9core_gen[] = {
  0x43, 0x54, 0x4e, 0x49, 0x2e, 0x04, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x4c, 0x04, 0x96, 0x2a, 0x25, 0xad, 0x06, 0x1f,
  0x99, 0x00, 0x72, 0x8d, 0x08, 0x00, 0x00, 0x00, 0xac, 0x03, 0x00, 0x00,
  0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
  0x88, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x63, 0x6c, 0x6d, 0x61,
  0x69, 0x6e, 0x00, 0x00, 0x01, 0x00, 0x60, 0x00, 0x0c, 0x02, 0x60, 0x20,
  0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x80, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x30, 0x00, 0x10, 0x00, 0x16, 0xc0, 0x04, 0xc0, 0x04,
  0x41, 0x00, 0x00, 0x00, 0x2c, 0x0a, 0x80, 0x20, 0x10, 0x01, 0x00, 0x0a,
  0x64, 0x00, 0x00, 0x00, 0x01, 0x4d, 0x00, 0x20, 0x07, 0x7f, 0x03, 0x00,
  0x40, 0x00, 0x80, 0x00, 0x28, 0x0a, 0xa0, 0x20, 0x80, 0x00, 0x00, 0x12,
  0x20, 0x00, 0xb1, 0x00, 0x40, 0x20, 0x80, 0x00, 0x28, 0x0a, 0x20, 0x21,
  0x80, 0x00, 0x00, 0x12, 0x40, 0x00, 0xb1, 0x00, 0x40, 0x96, 0x01, 0x20,
  0x07, 0x05, 0x05, 0x07, 0x40, 0x20, 0x80, 0x00, 0x28, 0x0a, 0x20, 0x21,
  0x20, 0x01, 0x8d, 0x0a, 0xe0, 0x00, 0x00, 0x00, 0x09, 0x00, 0x80, 0x00,
  0x28, 0x0a, 0xa0, 0x20, 0xa0, 0x00, 0x8d, 0x1e, 0x02, 0x00, 0x02, 0x00,
  0x09, 0x20, 0x80, 0x00, 0x28, 0x0a, 0x20, 0x21, 0x20, 0x01, 0x8d, 0x1e,
  0x02, 0x00, 0x02, 0x00, 0x31, 0x00, 0x80, 0x0c, 0x68, 0x02, 0x60, 0x21,
  0xa0, 0x00, 0x00, 0x06, 0x00, 0x5e, 0x20, 0x04, 0x31, 0x20, 0x80, 0x0c,
  0x68, 0x02, 0xa0, 0x21, 0x20, 0x01, 0x00, 0x06, 0x00, 0x5e, 0x20, 0x04,
  0x33, 0x00, 0x80, 0x0c, 0x70, 0xb0, 0x00, 0x00, 0xa2, 0x00, 0x00, 0x00,
  0x01, 0x5e, 0x02, 0x04, 0x33, 0x20, 0x80, 0x0c, 0x70, 0xd0, 0x00, 0x00,
  0x22, 0x01, 0x00, 0x00, 0x01, 0x5e, 0x02, 0x04, 0x31, 0x00, 0x60, 0x07,
  0x04, 0x02, 0x00, 0x20, 0xe0, 0x0f, 0x00, 0x06, 0x10, 0x00, 0x00, 0x82,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x00, 0xc0, 0xff, 0x83, 0x00, 0x00, 0x00, 0x03, 0x7f, 0x00, 0xff, 0x1f,
  0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xff, 0x83, 0x00, 0x00, 0x00, 0x03,
  0x7f, 0x00, 0xff, 0x1f, 0x00, 0x00, 0xe0, 0x0f, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x40, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x80, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
  0x80, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x14, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x2b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00,
  0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
  0x24, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
  0x28, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00,
  0x0c, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00,
  0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
  0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x4e, 0x4f, 0x4e, 0x45, 0x00, 0x00, 0x00, 0x00,
  0x69, 0x6e, 0x00, 0x00, 0x75, 0x69, 0x6e, 0x74, 0x2a, 0x3b, 0x38, 0x00,
  0x63, 0x6f, 0x6e, 0x73, 0x74, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00,
  0x48, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
  0x08, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x67, 0x6c, 0x6f, 0x62, 0x61, 0x6c,
  0x00, 0x00, 0x00, 0x00, 0x4e, 0x4f, 0x4e, 0x45, 0x00, 0x00, 0x00, 0x00,
  0x6f, 0x75, 0x74, 0x00, 0x75, 0x69, 0x6e, 0x74, 0x2a, 0x3b, 0x38, 0x00,
  0x4e, 0x4f, 0x4e, 0x45, 0x00, 0x00, 0x00, 0x00
};
static unsigned int test_Gen9core_gen_len = 1568;

#define ELEMENTS 4096
void run_gpgpu_test(Genode::Allocator_avl& alloc)
{
    clInitGenode(alloc);
	const int num = 0x42;
    uint32_t* m_in;
    volatile uint32_t* m_out;

    // allocate buffers
    m_in = (uint32_t*)alloc.alloc(ELEMENTS * sizeof(uint32_t));
    m_out = (volatile uint32_t*)alloc.alloc(ELEMENTS * sizeof(uint32_t));

    for(int i = 0; i < ELEMENTS; i++)
    {
        m_in[i] = num;
        m_out[i] = 0;
    }

    cl_platform_id platform_id;
    cl_device_id device_id;   
    cl_uint num_devices;
    cl_uint num_platforms;
    cl_int errcode;
    cl_context clContext;
    cl_kernel clKernel;
    cl_command_queue clCommandQue;
    cl_program clProgram;
    cl_mem clInBuff;
    cl_mem clOutBuff;

    // init opencl stuff
    errcode = clGetPlatformIDs(1, &platform_id, &num_platforms);
	if(errcode != CL_SUCCESS) Genode::log("Error in number of platforms");
	errcode = clGetDeviceIDs( platform_id, CL_DEVICE_TYPE_GPU, 1, &device_id, &num_devices);
	if(errcode != CL_SUCCESS) Genode::log("Error in number of devices");
	clContext = clCreateContext( NULL, 1, &device_id, NULL, NULL, &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in creating context");
	clCommandQue = clCreateCommandQueue(clContext, device_id, 0, &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in creating command queue");

    // allocate opencl buffers
	clInBuff = clCreateBuffer(clContext, CL_MEM_READ_WRITE, ELEMENTS * sizeof(uint32_t), NULL, &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in creating buffer");
	clOutBuff = clCreateBuffer(clContext, CL_MEM_READ_WRITE, ELEMENTS * sizeof(uint32_t), NULL, &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in creating buffer");

    // init buffers
	errcode = clEnqueueWriteBuffer(clCommandQue, clInBuff, CL_TRUE, 0, ELEMENTS * sizeof(uint32_t), m_in, 0, NULL, NULL);
	if(errcode != CL_SUCCESS) Genode::log("Error in writing to buffer");
	errcode = clEnqueueWriteBuffer(clCommandQue, clOutBuff, CL_TRUE, 0, ELEMENTS * sizeof(uint32_t), (uint32_t*)m_out, 0, NULL, NULL);
	if(errcode != CL_SUCCESS) Genode::log("Error in writing to buffer");

	// create a program from the kernel source
    const size_t kernel_size = test_Gen9core_gen_len;
    const unsigned char* kernel_bin = test_Gen9core_gen;
	clProgram = clCreateProgramWithBinary(clContext, 1, &device_id, &kernel_size, &kernel_bin, NULL, &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in loading binary");

    // build the program
	errcode = clBuildProgram(clProgram, 1, &device_id, NULL, NULL, NULL);
	if(errcode != CL_SUCCESS) Genode::log("Error in building program");
		
	// create the OpenCL kernel
	clKernel = clCreateKernel(clProgram, "clmain", &errcode);
	if(errcode != CL_SUCCESS) Genode::log("Error in creating kernel");

    // set kernel args
	errcode = clSetKernelArg(clKernel, 0, sizeof(cl_mem), (void *)&clInBuff);
	if(errcode != CL_SUCCESS) Genode::log("Error in setting kernel arg");
	errcode = clSetKernelArg(clKernel, 1, sizeof(cl_mem), (void *)&clOutBuff);
	if(errcode != CL_SUCCESS) Genode::log("Error in setting kernel arg");

    // launch the kernel
    size_t globalWorkSize = ELEMENTS;
	errcode = clEnqueueNDRangeKernel(clCommandQue, clKernel, 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);
	if(errcode != CL_SUCCESS) Genode::log("Error in launching kernel");

    // wait for finish
    clFinish(clCommandQue);

    // read result back
    errcode = clEnqueueReadBuffer(clCommandQue, clOutBuff, CL_TRUE, 0, ELEMENTS * sizeof(uint32_t), (void*)m_out, 0, NULL, NULL);
	if(errcode != CL_SUCCESS) Genode::log("Error in reading GPU mem");

    uint32_t errors = 0;
    for(int i = 0; i < ELEMENTS; i++)
    {
        if(m_out[i] != num)
        {
            //LOG_INFO("Error at Item " << i << " val: " << m_out[i]);
            errors++;
        }
    }
	Genode::log("Task has finished with ", errors, " errors!");

    // free stuff
    errcode = clReleaseKernel(clKernel);
	if(errcode != CL_SUCCESS) Genode::log("Error in releasing kernel");
    errcode = clReleaseMemObject(clInBuff);
	if(errcode != CL_SUCCESS) Genode::log("Error in releasing mem obj");
    errcode = clReleaseMemObject(clOutBuff);
	if(errcode != CL_SUCCESS) Genode::log("Error in releasing mem obj");
    errcode = clReleaseCommandQueue(clCommandQue);
	if(errcode != CL_SUCCESS) Genode::log("Error in releasing command queue");
    errcode = clReleaseContext(clContext);
	if(errcode != CL_SUCCESS) Genode::log("Error in releasing context");
  
    // free buffers
   alloc.free(m_in);
   alloc.free((void*)m_out);
}
