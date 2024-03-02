/*
  W25Q16.h - Arduino library for communicating with the Winbond W25Q16 Serial Flash.
  Created by Derek Evans, July 17, 2016.
*/

#ifndef W25Q16_h
#define W25Q16_h

#define WRITE_ENABLE 0x06
#define WRITE_DISABLE 0x04
#define PAGE_PROGRAM 0x02
#define READ_STATUS_REGISTER_1 0x05
#define READ_DATA 0x03
#define SECTOR_ERASE 0x20
#define CHIP_ERASE 0xC7
#define POWER_DOWN 0xB9
#define RELEASE_POWER_DOWN 0xAB
#define MANUFACTURER_ID 0x90

uint16_t W25Q16_start(void);
void W25Q16_write(uint16_t page, uint8_t pageAddress, uint8_t val);
void W25Q16_write_u16(uint16_t page, uint8_t pageAddress, uint16_t val);
uint8_t W25Q16_read(uint16_t page, uint8_t pageAddress);
uint16_t W25Q16_read_u16(uint16_t page, uint8_t pageAddress);
void W25Q16_initStreamWrite(uint16_t page, uint8_t pageAddress);
void W25Q16_streamWrite(uint16_t *page, uint8_t *pageAddress, uint8_t val);
void W25Q16_closeStreamWrite(void);
void W25Q16_initStreamRead(uint16_t page, uint8_t pageAddress);
uint8_t W25Q16_streamRead(uint16_t *page, uint8_t *pageAddress);
void W25Q16_closeStreamRead(void);
void W25Q16_powerDown(void);
void W25Q16_releasePowerDown(void);
void W25Q16_chipErase(void);
void W25Q16_sectorErase(uint16_t page);
uint16_t W25Q16_manufacturerID(void);
void sleepW25q16(void);


void W25Q16_writeEnable(void);
void W25Q16_writeDisable(void);
void W25Q16_notBusy(void);


#define w25q16_PAGESIZE 0xFF
#define w25q16_NUMPAGES 0x2000

#endif /* W25Q16_h */
