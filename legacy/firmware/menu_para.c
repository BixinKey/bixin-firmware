#include "menu_para.h"
#include "ble.h"
#include "config.h"
#include "gettext.h"
#include "util.h"

extern uint8_t ui_language;

static char* format_time(uint32_t ms) {
  static char line[sizeof("4294967296 minutes?")] = {0};

  const char* unit = _("second");
  uint32_t num = ms / 1000U;

  if (ms >= 60 * 60 * 1000) {
    unit = _("hour");
    num /= 60 * 60U;
  } else if (ms >= 60 * 1000) {
    unit = _("minute");
    num /= 60U;
  }

  uint2str(num, line);
  strlcat(line, " ", sizeof(line));
  strlcat(line, unit, sizeof(line));
  if (num > 1 && ui_language == 0) {
    strlcat(line, "s", sizeof(line));
  }
  return line;
}

char* menu_para_ble_state(void) {
  return ble_get_switch() ? _("On") : _("Off");
}

char* menu_para_language(void) {
  return ui_language ? _("English") : "English";
}

char* menu_para_shutdown(void) {
  return format_time(config_getAutoLockDelayMs());
}

char* menu_para_autolock(void) { return format_time(config_getSleepDelayMs()); }

void menu_para_set_ble(int index) {
  bool ble_state = index ? false : true;
  if (ble_state != ble_get_switch()) {
    change_ble_sta(ble_state);
  }
}

void menu_para_set_language(int index) {
  const char* lang[2] = {"en-US", "zh-CN"};
  if (ui_language != index) config_setLanguage(lang[index]);
}

void menu_para_set_shutdown(int index) {
  uint32_t ms[4] = {10 * 60 * 1000, 30 * 60 * 1000, 60 * 60 * 1000,
                    2 * 60 * 60 * 1000};
  config_setAutoLockDelayMs(ms[index]);
}

void menu_para_set_sleep(int index) {
  uint32_t ms[4] = {60 * 1000, 2 * 60 * 1000, 5 * 60 * 1000, 10 * 60 * 100};
  config_setSleepDelayMs(ms[index]);
}