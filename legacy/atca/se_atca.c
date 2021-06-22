#include "se_atca.h"
#include "atca_api.h"
#include "atca_config.h"

#include "memory.h"
#include "rng.h"
#include "sha2.h"
#include "util.h"

typedef struct {
  bool cached;
  uint8_t cache_pin[32];
} PIN_CACHED;

static bool se_has_pin = false;

static ATCAPairingInfo *pair_info =
    (ATCAPairingInfo *)(FLASH_STORAGE_SECRETS_INFO);

static PIN_CACHED pin_cache = {0};

static void pin_cacheSave(uint8_t pin[32]) {
  pin_cache.cached = true;
  memcpy(pin_cache.cache_pin, pin, 32);
}

static void pin_cacheGet(uint8_t pin[32]) {
  pin_cache.cached = true;
  memcpy(pin, pin_cache.cache_pin, 32);
}

static void pin_hash(const char *pin, uint32_t pin_len, uint8_t result[32]) {
  SHA256_CTX ctx = {0};

  sha256_Init(&ctx);
  sha256_Update(&ctx, (uint8_t *)pin, pin_len);
  sha256_Update(&ctx, pair_info->hash_mix, sizeof(pair_info->hash_mix));
  sha256_Final(&ctx, result);
}

char *se_get_version(void) { return "1.0.0"; }

bool se_get_sn(char **serial) {
  (void)serial;
  return true;
}

bool se_setSeedStrength(uint32_t strength) {
  ATCAUserState state;
  atca_pair_unlock();
  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);
  state.strength = strength;
  atca_pair_unlock();
  if (ATCA_SUCCESS == atca_write_enc(SLOT_USER_SATATE, 0, (uint8_t *)&state,
                                     pair_info->protect_key,
                                     SLOT_IO_PROTECT_KEY)) {
    return true;
  }
  return false;
}

bool se_getSeedStrength(uint32_t *strength) {
  ATCAUserState state;
  atca_pair_unlock();
  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);
  *strength = state.strength;
  return true;
}

void pin_updateCounter(void) { atca_update_counter(); }

bool se_hasPin(void) {
  ATCAUserState state;
  atca_pair_unlock();

  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);
  if (state.pin_set) {
    se_has_pin = true;
  } else {
    pin_cacheSave(pair_info->init_pin);
  }
  return se_has_pin;

  // if (ATCA_SUCCESS == atca_mac_slot(SLOT_USER_PIN, pair_info->init_pin)) {
  //   pin_cacheSave(pair_info->init_pin);
  //   pin_updateCounter();
  //   return false;
  // } else {
  //   se_has_pin = true;
  //   return true;
  // }
}

bool se_verifyPin(const char *pin) {
  uint8_t hash_pin[32] = {0};
  if (!se_has_pin) {
    return true;
  } else {
    pin_hash(pin, strlen(pin), hash_pin);
    atca_pair_unlock();
    if (ATCA_SUCCESS == atca_mac_slot(SLOT_USER_PIN, hash_pin)) {
      pin_cacheSave(hash_pin);
      pin_updateCounter();
      return true;
    } else {
      return false;
    }
  }
}

bool se_setPin(const char *pin) {
  uint8_t hash_pin[32] = {0};
  ATCAUserState state;
  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);
  pin_hash(pin, strlen(pin), hash_pin);
  atca_pair_unlock();
  if (ATCA_SUCCESS == atca_write_enc(SLOT_USER_PIN, 0, hash_pin,
                                     pair_info->protect_key,
                                     SLOT_IO_PROTECT_KEY)) {
    pin_cacheSave(hash_pin);
    pin_updateCounter();
    if (!state.pin_set) {
      state.pin_set = true;
      atca_pair_unlock();
      if (ATCA_SUCCESS == atca_write_enc(SLOT_USER_SATATE, 0, (uint8_t *)&state,
                                         pair_info->protect_key,
                                         SLOT_IO_PROTECT_KEY)) {
        se_has_pin = true;
        return true;
      }
    }
  }
  return false;
}

bool se_reset_pin(void) {
  atca_pair_unlock();
  if (ATCA_SUCCESS == atca_write_enc(SLOT_USER_PIN, 0, pair_info->init_pin,
                                     pair_info->protect_key,
                                     SLOT_IO_PROTECT_KEY)) {
    pin_cacheSave(pair_info->init_pin);
    pin_updateCounter();
    se_has_pin = false;
    return true;
  } else {
    return false;
  }
}

bool se_changePin(const char *old_pin, const char *new_pin) {
  if (se_verifyPin(old_pin)) {
    return se_setPin(new_pin);
  }
  return false;
}

bool se_isInitialized(void) {
  ATCAUserState state;
  atca_pair_unlock();

  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);

  return state.initialized;

  // uint8_t secret[32] = {0};
  // if (!se_hasPin()) {
  //   if (ATCA_SUCCESS == atca_read_enc(SLOT_USER_SECRET, 0, secret,
  //                                     pair_info->protect_key,
  //                                     SLOT_IO_PROTECT_KEY)) {
  //     if (check_all_zeros(secret, 32)) {
  //       return false;
  //     } else {
  //       return true;
  //     }
  //   } else {
  //   }
  // }
  // return false;
}

bool se_importSeed(uint8_t *seed) {
  uint8_t pin[32];
  ATCAUserState state;
  atca_read_slot_data(SLOT_USER_SATATE, (uint8_t *)&state);
  pin_cacheGet(pin);
  atca_pair_unlock();
  if (ATCA_SUCCESS == atca_mac_slot(SLOT_USER_PIN, pin)) {
    if (ATCA_SUCCESS == atca_write_enc(SLOT_USER_SECRET, 0, seed,
                                       pair_info->protect_key,
                                       SLOT_IO_PROTECT_KEY)) {
      if (!state.initialized) {
        state.initialized = true;
        atca_pair_unlock();
        if (ATCA_SUCCESS ==
            atca_write_enc(SLOT_USER_SATATE, 0, (uint8_t *)&state,
                           pair_info->protect_key, SLOT_IO_PROTECT_KEY)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool se_export_seed(uint8_t *seed) {
  uint8_t pin[32];
  pin_cacheGet(pin);
  atca_pair_unlock();
  if (ATCA_SUCCESS == atca_mac_slot(SLOT_USER_PIN, pin)) {
    if (ATCA_SUCCESS == atca_read_enc(SLOT_USER_SECRET, 0, seed,
                                      pair_info->protect_key,
                                      SLOT_IO_PROTECT_KEY)) {
      return true;
    }
  }
  return false;
}
void se_reset_state(void) {
  uint8_t zeros[32] = {0};
  atca_pair_unlock();
  atca_write_enc(SLOT_USER_SATATE, 0, (uint8_t *)&zeros, pair_info->protect_key,
                 SLOT_IO_PROTECT_KEY);
}

void se_reset_storage(void) {
  uint8_t zeros[32] = {0};
  se_reset_pin();
  se_importSeed(zeros);
  se_reset_state();
}

uint32_t se_pinFailedCounter(void) { return atca_get_failed_counter(); }

bool se_isFactoryMode(void) { return false; }

bool se_device_init(uint8_t mode, const char *passphrase) {
  (void)mode;
  (void)passphrase;
  return true;
}
