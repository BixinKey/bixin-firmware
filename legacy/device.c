#include "device.h"
#include "atca_app.h"
#include "atca_config.h"
#include "memory.h"
#include "util.h"

#define FLASH_TRUE_FLAG 0x5A5AA5A5

static DeviceInfo *device_info = (DeviceInfo *)(FLASH_STORAGE_DEVICE_INFO);
static ATCAPairingInfo *pair_info =
    (ATCAPairingInfo *)(FLASH_STORAGE_SECRETS_INFO);

void device_init(void) {
  if (device_info->factory_test != FLASH_TRUE_FLAG) {
  }
  atca_config_init();
}