#ifndef _W25QXX_H
#define _W25QXX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// config info
#define _W25QXX_SPI SPI2
#define _W25QXX_CS_GPIO GPIOB
#define _W25QXX_CS_PIN GPIO12
#define _W25QXX_DEBUG 0

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1

typedef enum {
  W25Q10 = 1,
  W25Q20,
  W25Q40,
  W25Q80,
  W25Q16,
  W25Q32,
  W25Q64,
  W25Q128,
  W25Q256,
  W25Q512,
} W25QXX_ID_t;

typedef struct {
  W25QXX_ID_t id;
  uint8_t uniq_id[8];
  uint16_t page_size;
  uint32_t page_count;
  uint32_t sector_size;
  uint32_t sector_count;
  uint32_t block_size;
  uint32_t block_count;
  uint32_t capacity_in_kilobyte;
  uint8_t status_register1;
  uint8_t status_register2;
  uint8_t status_register3;
  uint8_t lock;
} w25qxx_t;

extern w25qxx_t w25qxx;

// in Page, Sector and block read/write functions, can put 0 to read maximum
// bytes
bool w25qxx_init(void);

void w25qxx_erase_chip(void);
void w25qxx_erase_sector(uint32_t sector_addr);
void w25qxx_erase_block(uint32_t block_addr);

uint32_t w25qxx_page_to_sector(uint32_t page_addr);
uint32_t w25qxx_page_to_block(uint32_t page_addr);
uint32_t w25qxx_sector_to_block(uint32_t sector_addr);
uint32_t w25qxx_sector_to_page(uint32_t sector_addr);
uint32_t w25qxx_block_to_page(uint32_t block_addr);

bool w25qxx_is_empty_page(uint32_t page_addr, uint32_t offset, uint32_t number);
bool w25qxx_is_empty_sector(uint32_t sector_addr, uint32_t offset,
                            uint32_t number);
bool w25qxx_is_empty_block(uint32_t block_addr, uint32_t offset,
                           uint32_t number);

void w25qxx_write_byte(uint8_t buffer, uint32_t bytes_addr);
void w25qxx_write_page(uint8_t *buffer, uint32_t page_addr, uint32_t offset,
                       uint32_t number);
void w25qxx_write_sector(uint8_t *buffer, uint32_t sector_addr, uint32_t offset,
                         uint32_t number);
void w25qxx_write_block(uint8_t *buffer, uint32_t block_addr, uint32_t offset,
                        uint32_t number);

void w25qxx_read_byte(uint8_t *buffer, uint32_t bytes_addr);
void w25qxx_read_bytes(uint8_t *buffer, uint32_t read_addr, uint32_t number);
void w25qxx_read_page(uint8_t *buffer, uint32_t page_addr, uint32_t offset,
                      uint32_t number);
void w25qxx_read_sector(uint8_t *buffer, uint32_t sector_addr, uint32_t offset,
                        uint32_t number);
void w25qxx_read_block(uint8_t *buffer, uint32_t block_addr, uint32_t offset,
                       uint32_t number);
void flast_test(void);

#ifdef __cplusplus
}
#endif

#endif
