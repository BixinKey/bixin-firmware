#include "w25qxx.h"
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include <stdint.h>
#include <string.h>
#include "util.h"

#if (_W25QXX_DEBUG == 1)
#include <stdio.h>
#endif

#define W25QXX_DUMMY_BYTE 0xA5

w25qxx_t w25qxx;

void spi_delay(unsigned int delay_ms) {
  uint32_t timeout = delay_ms * 30000;

  while (timeout--) {
    __asm__("nop");
  }
}

#define w25qxx_delay(delay) spi_delay(delay)

void gpio_write_pin(uint32_t port, uint32_t gpio, uint32_t value) {
  if (value == GPIO_PIN_RESET)
    gpio_clear(port, gpio);
  else
    gpio_set(port, gpio);
}

void hal_spi_transmit(uint32_t base, uint8_t *pData, uint16_t Size,
                      uint32_t Timeout) {
  if (pData == NULL) return;

  Timeout = Timeout;

  for (int i = 0; i < Size; i++) {
    spi_send(base, pData[i]);
  }

  while (!(SPI_SR(base) & SPI_SR_TXE))
    ;

  while ((SPI_SR(base) & SPI_SR_BSY))
    ;
}

void hal_spi_receive(uint32_t base, uint8_t *pData, uint16_t Size,
                     uint32_t Timeout) {
  if (pData == NULL) return;

  Timeout = Timeout;

  for (int i = 0; i < Size; i++) {
    pData[i] = (uint8_t)spi_xfer(base, W25QXX_DUMMY_BYTE);
  }
}

uint8_t w25qxx_spi(uint8_t Data) {
  uint8_t ret = 0;

  ret = (uint8_t)spi_xfer(_W25QXX_SPI, Data);
  return ret;
}

uint32_t w25qxx_read_id(void) {
  uint32_t temp = 0, temp0 = 0, temp1 = 0, temp2 = 0;

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x9F);

  temp0 = w25qxx_spi(W25QXX_DUMMY_BYTE);
  temp1 = w25qxx_spi(W25QXX_DUMMY_BYTE);
  temp2 = w25qxx_spi(W25QXX_DUMMY_BYTE);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  temp = (temp0 << 16) | (temp1 << 8) | temp2;
  return temp;
}

void w25qxx_read_uniq_id(void) {
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x4B);

  for (uint8_t i = 0; i < 4; i++) w25qxx_spi(W25QXX_DUMMY_BYTE);

  for (uint8_t i = 0; i < 8; i++)
    w25qxx.uniq_id[i] = w25qxx_spi(W25QXX_DUMMY_BYTE);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
}

void w25qxx_write_enable(void) {
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);
  w25qxx_spi(0x06);
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
  w25qxx_delay(1);
}

void w25qxx_write_disable(void) {
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);
  w25qxx_spi(0x04);
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
  w25qxx_delay(1);
}

uint8_t w25qxx_read_status_register(uint8_t select) {
  uint8_t status = 0;

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  if (select == 1) {
    w25qxx_spi(0x05);
    status = w25qxx_spi(W25QXX_DUMMY_BYTE);
    w25qxx.status_register1 = status;
  } else if (select == 2) {
    w25qxx_spi(0x35);
    status = w25qxx_spi(W25QXX_DUMMY_BYTE);
    w25qxx.status_register2 = status;
  } else {
    w25qxx_spi(0x15);
    status = w25qxx_spi(W25QXX_DUMMY_BYTE);
    w25qxx.status_register3 = status;
  }

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
  return status;
}

void w25qxx_write_status_register(uint8_t select, uint8_t data) {
  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  if (select == 1) {
    w25qxx_spi(0x01);
    w25qxx.status_register1 = data;
  } else if (select == 2) {
    w25qxx_spi(0x31);
    w25qxx.status_register2 = data;
  } else {
    w25qxx_spi(0x11);
    w25qxx.status_register3 = data;
  }

  w25qxx_spi(data);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
}

void w25qxx_wait_for_write_end(void) {
  w25qxx_delay(1);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x05);

  do {
    w25qxx.status_register1 = w25qxx_spi(W25QXX_DUMMY_BYTE);
    w25qxx_delay(1);
  } while ((w25qxx.status_register1 & 0x01) == 0x01);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);
}

bool w25qxx_init(void) {
  uint32_t id;

  w25qxx.lock = 1;

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_delay(100);

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx Init Begin...\r\n");
#endif

  id = w25qxx_read_id();

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx id:0x%X\r\n", id);
#endif

  switch (id & 0x000000FF) {
    case 0x20:  // w25q512
      w25qxx.id = W25Q512;
      w25qxx.block_count = 1024;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q512\r\n");
#endif
      break;
    case 0x19:  // w25q256
      w25qxx.id = W25Q256;
      w25qxx.block_count = 512;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q256\r\n");
#endif
      break;
    case 0x18:  // w25q128
      w25qxx.id = W25Q128;
      w25qxx.block_count = 256;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q128\r\n");
#endif
      break;
    case 0x17:  // w25q64
      w25qxx.id = W25Q64;
      w25qxx.block_count = 128;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q64\r\n");
#endif
      break;
    case 0x16:  // w25q32
      w25qxx.id = W25Q32;
      w25qxx.block_count = 64;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q32\r\n");
#endif
      break;
    case 0x15:  // w25q16
      w25qxx.id = W25Q16;
      w25qxx.block_count = 32;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q16\r\n");
#endif
      break;
    case 0x14:  // w25q80
      w25qxx.id = W25Q80;
      w25qxx.block_count = 16;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q80\r\n");
#endif
      break;
    case 0x13:  // w25q40
      w25qxx.id = W25Q40;
      w25qxx.block_count = 8;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q40\r\n");
#endif
      break;
    case 0x12:  // w25q20
      w25qxx.id = W25Q20;
      w25qxx.block_count = 4;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q20\r\n");
#endif
      break;
    case 0x11:  // w25q10
      w25qxx.id = W25Q10;
      w25qxx.block_count = 2;
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Chip: w25q10\r\n");
#endif
      break;
    default:
#if (_W25QXX_DEBUG == 1)
      printf("w25qxx Unknown id\r\n");
#endif
      w25qxx.lock = 0;
      return false;
  }

  w25qxx.page_size = 256;
  w25qxx.sector_size = 0x1000;
  w25qxx.sector_count = w25qxx.block_count * 16;
  w25qxx.page_count =
      (w25qxx.sector_count * w25qxx.sector_size) / w25qxx.page_size;
  w25qxx.block_size = w25qxx.sector_size * 16;
  w25qxx.capacity_in_kilobyte =
      (w25qxx.sector_count * w25qxx.sector_size) / 1024;
  w25qxx_read_uniq_id();
  w25qxx_read_status_register(1);
  w25qxx_read_status_register(2);
  w25qxx_read_status_register(3);

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx Page Size: %d Bytes\r\n", w25qxx.page_size);
  printf("w25qxx Page Count: %d\r\n", w25qxx.PageCount);
  printf("w25qxx Sector Size: %d Bytes\r\n", w25qxx.sector_size);
  printf("w25qxx Sector Count: %d\r\n", w25qxx.sector_count);
  printf("w25qxx Block Size: %d Bytes\r\n", w25qxx.block_size);
  printf("w25qxx Block Count: %d\r\n", w25qxx.block_count);
  printf("w25qxx Capacity: %d KiloBytes\r\n", w25qxx.capacity_in_kilobyte);
  printf("w25qxx Init Done\r\n");
#endif

  w25qxx.lock = 0;
  return true;
}

void w25qxx_erase_chip(void) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  uint32_t start_time = HAL_GetTick();
  printf("w25qxx EraseChip Begin...\r\n");
#endif

  w25qxx_write_enable();

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0xC7);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_wait_for_write_end();

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx EraseBlock done after %d ms!\r\n", HAL_GetTick() - start_time);
#endif

  w25qxx_delay(10);
  w25qxx.lock = 0;
}

void w25qxx_erase_sector(uint32_t sector_addr) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  uint32_t start_time = HAL_GetTick();
  printf("w25qxx EraseSector %d Begin...\r\n", sector_addr);
#endif

  w25qxx_wait_for_write_end();

  sector_addr = sector_addr * w25qxx.sector_size;

  w25qxx_write_enable();

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x20);

  if (w25qxx.id >= W25Q256) w25qxx_spi((sector_addr & 0xFF000000) >> 24);
  w25qxx_spi((sector_addr & 0xFF0000) >> 16);
  w25qxx_spi((sector_addr & 0xFF00) >> 8);
  w25qxx_spi(sector_addr & 0xFF);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_wait_for_write_end();

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx EraseSector done after %d ms\r\n", HAL_GetTick() - start_time);
#endif

  w25qxx_delay(1);
  w25qxx.lock = 0;
}

void w25qxx_erase_block(uint32_t block_addr) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx EraseBlock %d Begin...\r\n", block_addr);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  w25qxx_wait_for_write_end();

  block_addr = block_addr * w25qxx.sector_size * 16;

  w25qxx_write_enable();

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0xD8);

  if (w25qxx.id >= W25Q256) w25qxx_spi((block_addr & 0xFF000000) >> 24);
  w25qxx_spi((block_addr & 0xFF0000) >> 16);
  w25qxx_spi((block_addr & 0xFF00) >> 8);
  w25qxx_spi(block_addr & 0xFF);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_wait_for_write_end();

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx EraseBlock done after %d ms\r\n", HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx_delay(1);
  w25qxx.lock = 0;
}

uint32_t w25qxx_page_to_sector(uint32_t page_addr) {
  return ((page_addr * w25qxx.page_size) / w25qxx.sector_size);
}

uint32_t w25qxx_page_to_block(uint32_t page_addr) {
  return ((page_addr * w25qxx.page_size) / w25qxx.block_size);
}

uint32_t w25qxx_sector_to_block(uint32_t sector_addr) {
  return ((sector_addr * w25qxx.sector_size) / w25qxx.block_size);
}

uint32_t w25qxx_sector_to_page(uint32_t sector_addr) {
  return (sector_addr * w25qxx.sector_size) / w25qxx.page_size;
}

uint32_t w25qxx_block_to_page(uint32_t block_addre) {
  return (block_addre * w25qxx.block_size) / w25qxx.page_size;
}

bool w25qxx_is_empty_page(uint32_t page_addr, uint32_t offset,
                          uint32_t number) {
  uint8_t buffer[32];
  uint32_t work_addr;
  uint32_t i;

  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

  if (((number + offset) > w25qxx.page_size) || (number == 0))
    number = w25qxx.page_size - offset;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckPage:%d, Offset:%d, Bytes:%d begin...\r\n", page_addr,
         offset, number);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  for (i = offset; i < w25qxx.page_size; i += sizeof(buffer)) {
    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

    work_addr = (i + page_addr * w25qxx.page_size);

    w25qxx_spi(0x0B);

    if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
    w25qxx_spi((work_addr & 0xFF0000) >> 16);
    w25qxx_spi((work_addr & 0xFF00) >> 8);
    w25qxx_spi(work_addr & 0xFF);
    w25qxx_spi(0);

    memset(buffer, 0, 32);
    hal_spi_receive(_W25QXX_SPI, buffer, sizeof(buffer), 100);

    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

    for (uint8_t x = 0; x < sizeof(buffer); x++) {
      if (buffer[x] != 0xFF) goto NOT_EMPTY;
    }
  }

  if ((w25qxx.page_size + offset) % sizeof(buffer) != 0) {
    i -= sizeof(buffer);

    for (; i < w25qxx.page_size; i++) {
      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

      work_addr = (i + page_addr * w25qxx.page_size);

      w25qxx_spi(0x0B);

      if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
      w25qxx_spi((work_addr & 0xFF0000) >> 16);
      w25qxx_spi((work_addr & 0xFF00) >> 8);
      w25qxx_spi(work_addr & 0xFF);
      w25qxx_spi(0);

      memset(buffer, 0, 32);
      hal_spi_receive(_W25QXX_SPI, buffer, 1, 100);

      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

      if (buffer[0] != 0xFF) goto NOT_EMPTY;
    }
  }

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckPage is Empty in %d ms\r\n", HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return true;

NOT_EMPTY:
#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckPage is Not Empty in %d ms\r\n",
         HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return false;
}

bool w25qxx_is_empty_sector(uint32_t sector_addr, uint32_t offset,
                            uint32_t number) {
  uint8_t buffer[32];
  uint32_t work_addr;
  uint32_t i;

  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

  if ((number > w25qxx.sector_size) || (number == 0))
    number = w25qxx.sector_size;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckSector:%d, Offset:%d, Bytes:%d begin...\r\n", sector_addr,
         offset, number);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  for (i = offset; i < w25qxx.sector_size; i += sizeof(buffer)) {
    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

    work_addr = (i + sector_addr * w25qxx.sector_size);

    w25qxx_spi(0x0B);

    if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
    w25qxx_spi((work_addr & 0xFF0000) >> 16);
    w25qxx_spi((work_addr & 0xFF00) >> 8);
    w25qxx_spi(work_addr & 0xFF);
    w25qxx_spi(0);

    hal_spi_receive(_W25QXX_SPI, buffer, sizeof(buffer), 100);

    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

    for (uint8_t x = 0; x < sizeof(buffer); x++) {
      if (buffer[x] != 0xFF) goto NOT_EMPTY;
    }
  }

  if ((w25qxx.sector_size + offset) % sizeof(buffer) != 0) {
    i -= sizeof(buffer);
    for (; i < w25qxx.sector_size; i++) {
      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

      work_addr = (i + sector_addr * w25qxx.sector_size);

      w25qxx_spi(0x0B);

      if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
      w25qxx_spi((work_addr & 0xFF0000) >> 16);
      w25qxx_spi((work_addr & 0xFF00) >> 8);
      w25qxx_spi(work_addr & 0xFF);
      w25qxx_spi(0);

      memset(buffer, 0, 32);
      hal_spi_receive(_W25QXX_SPI, buffer, 1, 100);

      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

      if (buffer[0] != 0xFF) goto NOT_EMPTY;
    }
  }

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckSector is Empty in %d ms\r\n",
         HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return true;

NOT_EMPTY:
#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckSector is Not Empty in %d ms\r\n",
         HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return false;
}

bool w25qxx_is_empty_block(uint32_t block_addr, uint32_t offset,
                           uint32_t number) {
  uint8_t buffer[32];
  uint32_t work_addr;
  uint32_t i;

  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

  if ((number > w25qxx.block_size) || (number == 0)) number = w25qxx.block_size;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckBlock:%d, Offset:%d, Bytes:%d begin...\r\n", block_addr,
         offset, number);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  for (i = offset; i < w25qxx.block_size; i += sizeof(buffer)) {
    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

    work_addr = (i + block_addr * w25qxx.block_size);

    w25qxx_spi(0x0B);

    if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
    w25qxx_spi((work_addr & 0xFF0000) >> 16);
    w25qxx_spi((work_addr & 0xFF00) >> 8);
    w25qxx_spi(work_addr & 0xFF);
    w25qxx_spi(0);

    memset(buffer, 0, 32);
    hal_spi_receive(_W25QXX_SPI, buffer, sizeof(buffer), 100);

    gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

    for (uint8_t x = 0; x < sizeof(buffer); x++) {
      if (buffer[x] != 0xFF) goto NOT_EMPTY;
    }
  }

  if ((w25qxx.block_size + offset) % sizeof(buffer) != 0) {
    i -= sizeof(buffer);
    for (; i < w25qxx.block_size; i++) {
      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

      work_addr = (i + block_addr * w25qxx.block_size);

      w25qxx_spi(0x0B);

      if (w25qxx.id >= W25Q256) w25qxx_spi((work_addr & 0xFF000000) >> 24);
      w25qxx_spi((work_addr & 0xFF0000) >> 16);
      w25qxx_spi((work_addr & 0xFF00) >> 8);
      w25qxx_spi(work_addr & 0xFF);
      w25qxx_spi(0);

      memset(buffer, 0, 32);
      hal_spi_receive(_W25QXX_SPI, buffer, 1, 100);

      gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

      if (buffer[0] != 0xFF) goto NOT_EMPTY;
    }
  }

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckBlock is Empty in %d ms\r\n", HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return true;

NOT_EMPTY:
#if (_W25QXX_DEBUG == 1)
  printf("w25qxx CheckBlock is Not Empty in %d ms\r\n",
         HAL_GetTick() - start_time);
  w25qxx_delay(100);
#endif

  w25qxx.lock = 0;
  return false;
}

void w25qxx_write_byte(uint8_t buffer, uint32_t write_addr) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  uint32_t start_time = HAL_GetTick();
  printf("w25qxx WriteByte 0x%02X at address %d begin...", buffer, write_addr);
#endif

  w25qxx_wait_for_write_end();

  w25qxx_write_enable();

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x02);

  if (w25qxx.id >= W25Q256) w25qxx_spi((write_addr & 0xFF000000) >> 24);
  w25qxx_spi((write_addr & 0xFF0000) >> 16);
  w25qxx_spi((write_addr & 0xFF00) >> 8);
  w25qxx_spi(write_addr & 0xFF);
  w25qxx_spi(buffer);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_wait_for_write_end();

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx WriteByte done after %d ms\r\n", HAL_GetTick() - start_time);
#endif

  w25qxx.lock = 0;
}

void w25qxx_write_page(uint8_t *buffer, uint32_t page_addr, uint32_t offset,
                       uint32_t number) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

  if (((number + offset) > w25qxx.page_size) || (number == 0))
    number = w25qxx.page_size - offset;

  if ((offset + number) > w25qxx.page_size) number = w25qxx.page_size - offset;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx WritePage:%d, Offset:%d ,Writes %d Bytes, begin...\r\n",
         page_addr, offset, number);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  w25qxx_wait_for_write_end();

  w25qxx_write_enable();

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x02);

  page_addr = (page_addr * w25qxx.page_size) + offset;

  if (w25qxx.id >= W25Q256) w25qxx_spi((page_addr & 0xFF000000) >> 24);
  w25qxx_spi((page_addr & 0xFF0000) >> 16);
  w25qxx_spi((page_addr & 0xFF00) >> 8);
  w25qxx_spi(page_addr & 0xFF);

  hal_spi_transmit(_W25QXX_SPI, buffer, number, 100);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

  w25qxx_wait_for_write_end();

#if (_W25QXX_DEBUG == 1)
  start_time = HAL_GetTick() - start_time;
  for (uint32_t i = 0; i < number; i++) {
    if ((i % 8 == 0) && (i > 2)) {
      printf("\r\n");
      w25qxx_delay(10);
    }
    printf("0x%02X,", buffer[i]);
  }
  printf("\r\n");
  printf("w25qxx WritePage done after %d ms\r\n", start_time);
  w25qxx_delay(100);
#endif

  w25qxx_delay(1);
  w25qxx.lock = 0;
}

void w25qxx_write_sector(uint8_t *buffer, uint32_t sector_addr, uint32_t offset,
                         uint32_t number) {
  uint32_t start_page;
  int32_t bytes_to_write;
  uint32_t local_offset;

  if ((number > w25qxx.sector_size) || (number == 0))
    number = w25qxx.sector_size;

#if (_W25QXX_DEBUG == 1)
  printf("+++w25qxx WriteSector:%d, Offset:%d ,Write %d Bytes, begin...\r\n",
         sector_addr, offset, number);
  w25qxx_delay(100);
#endif

  if (offset >= w25qxx.sector_size) {
#if (_W25QXX_DEBUG == 1)
    printf("---w25qxx WriteSector Faild!\r\n");
    w25qxx_delay(100);
#endif
    return;
  }

  if ((offset + number) > w25qxx.sector_size)
    bytes_to_write = w25qxx.sector_size - offset;
  else
    bytes_to_write = number;

  start_page = w25qxx_sector_to_page(sector_addr) + (offset / w25qxx.page_size);
  local_offset = offset % w25qxx.page_size;

  do {
    w25qxx_write_page(buffer, start_page, local_offset, bytes_to_write);
    start_page++;
    bytes_to_write -= w25qxx.page_size - local_offset;
    buffer += w25qxx.page_size - local_offset;
    local_offset = 0;
  } while (bytes_to_write > 0);

#if (_W25QXX_DEBUG == 1)
  printf("---w25qxx WriteSector Done\r\n");
  w25qxx_delay(100);
#endif
}

void w25qxx_write_block(uint8_t *buffer, uint32_t block_addr, uint32_t offset,
                        uint32_t number) {
  uint32_t start_page;
  int32_t bytes_to_write;
  uint32_t local_offset;

  if ((number > w25qxx.block_size) || (number == 0)) number = w25qxx.block_size;

#if (_W25QXX_DEBUG == 1)
  printf("+++w25qxx WriteBlock:%d, Offset:%d ,Write %d Bytes, begin...\r\n",
         block_addr, offset, number);
  w25qxx_delay(100);
#endif

  if (offset >= w25qxx.block_size) {
#if (_W25QXX_DEBUG == 1)
    printf("---w25qxx WriteBlock Faild!\r\n");
    w25qxx_delay(100);
#endif
    return;
  }

  if ((offset + number) > w25qxx.block_size)
    bytes_to_write = w25qxx.block_size - offset;
  else
    bytes_to_write = number;

  start_page = w25qxx_block_to_page(block_addr) + (offset / w25qxx.page_size);
  local_offset = offset % w25qxx.page_size;

  do {
    w25qxx_write_page(buffer, start_page, local_offset, bytes_to_write);
    start_page++;
    bytes_to_write -= w25qxx.page_size - local_offset;
    buffer += w25qxx.page_size - local_offset;
    local_offset = 0;
  } while (bytes_to_write > 0);

#if (_W25QXX_DEBUG == 1)
  printf("---w25qxx WriteBlock Done\r\n");
  w25qxx_delay(100);
#endif
}

void w25qxx_read_byte(uint8_t *buffer, uint32_t Bytes_Address) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  uint32_t start_time = HAL_GetTick();
  printf("w25qxx ReadByte at address %d begin...\r\n", Bytes_Address);
#endif

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x0B);

  if (w25qxx.id >= W25Q256) w25qxx_spi((Bytes_Address & 0xFF000000) >> 24);
  w25qxx_spi((Bytes_Address & 0xFF0000) >> 16);
  w25qxx_spi((Bytes_Address & 0xFF00) >> 8);
  w25qxx_spi(Bytes_Address & 0xFF);
  w25qxx_spi(0);

  *buffer = w25qxx_spi(W25QXX_DUMMY_BYTE);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx ReadByte 0x%02X done after %d ms\r\n", *buffer,
         HAL_GetTick() - start_time);
#endif

  w25qxx.lock = 0;
}

void w25qxx_read_bytes(uint8_t *buffer, uint32_t read_addr, uint32_t number) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

#if (_W25QXX_DEBUG == 1)
  uint32_t start_time = HAL_GetTick();
  printf("w25qxx ReadBytes at Address:%d, %d Bytes  begin...\r\n", read_addr,
         number);
#endif

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x0B);

  if (w25qxx.id >= W25Q256) w25qxx_spi((read_addr & 0xFF000000) >> 24);
  w25qxx_spi((read_addr & 0xFF0000) >> 16);
  w25qxx_spi((read_addr & 0xFF00) >> 8);
  w25qxx_spi(read_addr & 0xFF);
  w25qxx_spi(0);

  hal_spi_receive(_W25QXX_SPI, buffer, number, 2000);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

#if (_W25QXX_DEBUG == 1)
  start_time = HAL_GetTick() - start_time;
  for (uint32_t i = 0; i < number; i++) {
    if ((i % 8 == 0) && (i > 2)) {
      printf("\r\n");
      w25qxx_delay(10);
    }
    printf("0x%02X,", buffer[i]);
  }
  printf("\r\n");
  printf("w25qxx ReadBytes done after %d ms\r\n", start_time);
  w25qxx_delay(100);
#endif

  w25qxx_delay(1);
  w25qxx.lock = 0;
}

void w25qxx_read_page(uint8_t *buffer, uint32_t page_addr, uint32_t offset,
                      uint32_t number) {
  while (w25qxx.lock == 1) w25qxx_delay(1);

  w25qxx.lock = 1;

  if ((number > w25qxx.page_size) || (number == 0)) number = w25qxx.page_size;

  if ((offset + number) > w25qxx.page_size) number = w25qxx.page_size - offset;

#if (_W25QXX_DEBUG == 1)
  printf("w25qxx ReadPage:%d, Offset:%d ,Read %d Bytes, begin...\r\n",
         page_addr, offset, number);
  w25qxx_delay(100);
  uint32_t start_time = HAL_GetTick();
#endif

  page_addr = page_addr * w25qxx.page_size + offset;

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_RESET);

  w25qxx_spi(0x0B);

  if (w25qxx.id >= W25Q256) w25qxx_spi((page_addr & 0xFF000000) >> 24);
  w25qxx_spi((page_addr & 0xFF0000) >> 16);
  w25qxx_spi((page_addr & 0xFF00) >> 8);
  w25qxx_spi(page_addr & 0xFF);
  w25qxx_spi(0);

  hal_spi_receive(_W25QXX_SPI, buffer, number, 100);

  gpio_write_pin(_W25QXX_CS_GPIO, _W25QXX_CS_PIN, GPIO_PIN_SET);

#if (_W25QXX_DEBUG == 1)
  start_time = HAL_GetTick() - start_time;
  for (uint32_t i = 0; i < number; i++) {
    if ((i % 8 == 0) && (i > 2)) {
      printf("\r\n");
      w25qxx_delay(10);
    }
    printf("0x%02X,", buffer[i]);
  }
  printf("\r\n");
  printf("w25qxx ReadPage done after %d ms\r\n", start_time);
  w25qxx_delay(100);
#endif

  w25qxx_delay(1);
  w25qxx.lock = 0;
}

void w25qxx_read_sector(uint8_t *buffer, uint32_t sector_addr, uint32_t offset,
                        uint32_t number) {
  uint32_t start_page;
  int32_t bytes_to_read;
  uint32_t local_offset;

  if ((number > w25qxx.sector_size) || (number == 0))
    number = w25qxx.sector_size;

#if (_W25QXX_DEBUG == 1)
  printf("+++w25qxx ReadSector:%d, Offset:%d ,Read %d Bytes, begin...\r\n",
         sector_addr, offset, number);
  w25qxx_delay(100);
#endif

  if (offset >= w25qxx.sector_size) {
#if (_W25QXX_DEBUG == 1)
    printf("---w25qxx ReadSector Faild!\r\n");
    w25qxx_delay(100);
#endif
    return;
  }

  if ((offset + number) > w25qxx.sector_size)
    bytes_to_read = w25qxx.sector_size - offset;
  else
    bytes_to_read = number;

  start_page = w25qxx_sector_to_page(sector_addr) + (offset / w25qxx.page_size);
  local_offset = offset % w25qxx.page_size;

  do {
    w25qxx_read_page(buffer, start_page, local_offset, bytes_to_read);
    start_page++;
    bytes_to_read -= w25qxx.page_size - local_offset;
    buffer += w25qxx.page_size - local_offset;
    local_offset = 0;
  } while (bytes_to_read > 0);

#if (_W25QXX_DEBUG == 1)
  printf("---w25qxx ReadSector Done\r\n");
  w25qxx_delay(100);
#endif
}

void w25qxx_read_block(uint8_t *buffer, uint32_t block_addr, uint32_t offset,
                       uint32_t number) {
  uint32_t start_page;
  int32_t bytes_to_read;
  uint32_t local_offset;

  if ((number > w25qxx.block_size) || (number == 0)) number = w25qxx.block_size;

#if (_W25QXX_DEBUG == 1)
  printf("+++w25qxx ReadBlock:%d, Offset:%d ,Read %d Bytes, begin...\r\n",
         block_addr, offset, number);
  w25qxx_delay(100);
#endif

  if (offset >= w25qxx.block_size) {
#if (_W25QXX_DEBUG == 1)
    printf("w25qxx ReadBlock Faild!\r\n");
    w25qxx_delay(100);
#endif
    return;
  }

  if ((offset + number) > w25qxx.block_size)
    bytes_to_read = w25qxx.block_size - offset;
  else
    bytes_to_read = number;

  start_page = w25qxx_block_to_page(block_addr) + (offset / w25qxx.page_size);
  local_offset = offset % w25qxx.page_size;

  do {
    w25qxx_read_page(buffer, start_page, local_offset, bytes_to_read);
    start_page++;
    bytes_to_read -= w25qxx.page_size - local_offset;
    buffer += w25qxx.page_size - local_offset;
    local_offset = 0;
  } while (bytes_to_read > 0);

#if (_W25QXX_DEBUG == 1)
  printf("---w25qxx ReadBlock Done\r\n");
  w25qxx_delay(100);
#endif
}
