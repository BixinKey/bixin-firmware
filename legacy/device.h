#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
  uint32_t factory_test;
  uint32_t lock_flag;
  uint8_t serial_no[16];
  uint8_t hardware_version[16];
  uint8_t manufacturer[16];
  uint8_t factory_firmware[16];
  uint8_t factory_time[16];
} DeviceInfo;

#endif