#define GENODE // use genodes stdint header
#include <driver/gpgpu_driver.h>
#include <stubs.h>

#define ELEMENTS 4096

namespace gpgpu
{

uint32_t* in;
uint32_t* out;

/*
kernel void clmain(global const unsigned int* in, global unsigned int* out)
{
    unsigned int i = get_global_id(0);
    out[i] = in[i];
}
*/

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

void cleanUp()
{
    printk("Yeah.. it finished!");

    // set gpu frequency to minimum
    GPGPU_Driver::getInstance().setMinFreq();

    for(int i = 0; i < ELEMENTS; i++)
    {
        if(out[i] != in[i])
        {
            printk("Error at Item %d (%d != %d)!", i, out[i], in[i]);
        }
    }
        
    // free buffers
    free((void *)in);
    free((void *)out);
}

void run_gpgpu_test()
{
    // create kernel and buffer config struct
    static kernel_config kconf;
    static buffer_config buffconf[2];

    kconf.range[0] = ELEMENTS; // number of executions
    kconf.workgroupsize[0] = 0; // 0 = auto
    kconf.binary = test_Gen9core_gen;
    kconf.finish_callback = cleanUp;

    // allocate buffers
    in = (uint32_t*)uos_aligned_alloc(0x1000, kconf.range[0] * sizeof(uint32_t));
    out = (uint32_t*)uos_aligned_alloc(0x1000, kconf.range[0] * sizeof(uint32_t));

    // config buffers
    kconf.buffCount = 2;
    kconf.buffConfigs = buffconf;
    kconf.buffConfigs[0].buffer = (uint32_t*)virt_to_phys(in);
    kconf.buffConfigs[0].buffer_size = kconf.range[0] * sizeof(uint32_t);
    kconf.buffConfigs[1].buffer = (uint32_t*)virt_to_phys(out);
    kconf.buffConfigs[1].buffer_size = kconf.range[0] * sizeof(uint32_t);

    for(int i = 0; i < ELEMENTS; i++)
    {
        in[i] = 0x42;
    }

    // set maximum freuqency
    GPGPU_Driver& gpgpudriver = GPGPU_Driver::getInstance();
    gpgpudriver.setMaxFreq();

    // start gpu task
    gpgpudriver.enqueueRun(kconf);

    // wait for its end
    while(!kconf.finished);
}

}
