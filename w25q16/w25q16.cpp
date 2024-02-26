#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include <w25q16/w25q16.h>
#include "string.h"

#define tty2 (BaseSequentialStream *)&SD2


uint8_t bufferTx[70];

/*
*  Purpose :   Initializes the W25Q16 by setting the input slave select pin
*              as OUTPUT and writing it HIGH.  Also initializes the SPI bus,
*              sets the SPI bit order to MSBFIRST and the SPI data mode to
*              SPI_MODE3, ensures the flash is not in low power mode, and
*              that flash write is disabled.
*/
void W25Q16_init(void)
{
  W25Q16_releasePowerDown();
  W25Q16_manufacturerID();
  W25Q16_writeDisable();
}


/*
 *  Purpose :   Reads a byte from the flash page and page address.  The W25Q16 has
 *              8192 pages with 256 bytes in a page.  Both page and byte addresses
 *              start at 0. Page ends at address 8191 and page address ends at 255.
 *
 */

uint8_t W25Q16_read(uint16_t page, uint8_t pageAddress) {
  uint8_t txbf[5], rxbf[5];
  rxbf[4] = 0;
  spiAcquireBus(&SPID1);
  txbf[0] = READ_DATA;
  txbf[1] = (page >> 8) & 0xFF;
  txbf[2] = page & 0xFF;
  txbf[3] = pageAddress;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 5, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
  return rxbf[4];
}


/*
 *  Purpose :   Reads uint16_t from the flash page and page address.  The W25Q16 has
 *              8192 pages with 256 bytes in a page.  Both page and byte addresses
 *              start at 0. Page ends at address 8191 and page address ends at 255.
 *              First byte is high value
 */

uint16_t W25Q16_read_u16(uint16_t page, uint8_t pageAddress) {
  uint8_t txbf[6], rxbf[6];
  rxbf[4] = 0;
  rxbf[5] = 0;
  spiAcquireBus(&SPID1);
  txbf[0] = READ_DATA;
  txbf[1] = (page >> 8) & 0xFF;
  txbf[2] = page & 0xFF;
  txbf[3] = pageAddress;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 6, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
  return (rxbf[4]<<8) + rxbf[5];
}


/*
 *  Purpose :   Writes a byte to the flash page and page address.  The W25Q16 has
 *              8192 pages with 256 bytes in a page.  Both page and byte addresses
 *              start at 0. Page ends at address 8191 and page address ends at 255.
 */
void W25Q16_write(uint16_t page, uint8_t pageAddress, uint8_t val) {
    uint8_t txbf[6], rxbf[6];
    W25Q16_writeEnable();

    spiAcquireBus(&SPID1);
    txbf[0] = PAGE_PROGRAM;
    txbf[1] = (page >> 8) & 0xFF;
    txbf[2] = page & 0xFF;
    txbf[3] = pageAddress;
    txbf[4] = val;
    spiSelect(&SPID1);
    spiExchange(&SPID1, 5, txbf, rxbf);
    spiUnselect(&SPID1);
    spiReleaseBus(&SPID1);
    W25Q16_notBusy();
    W25Q16_writeDisable();
}


/*
 *  Purpose :   Writes uint16_t to the flash page and page address.  The W25Q16 has
 *              8192 pages with 256 bytes in a page.  Both page and byte addresses
 *              start at 0. Page ends at address 8191 and page address ends at 255.
 *              First byte is high value
 */
void W25Q16_write_u16(uint16_t page, uint8_t pageAddress, uint16_t val) {
    uint8_t txbf[7], rxbf[7];
    W25Q16_writeEnable();

    spiAcquireBus(&SPID1);
    txbf[0] = PAGE_PROGRAM;
    txbf[1] = (page >> 8) & 0xFF;
    txbf[2] = page & 0xFF;
    txbf[3] = pageAddress;
    txbf[4] = val>>8;
    txbf[5] = val & 0xFF;
    spiSelect(&SPID1);
    spiExchange(&SPID1, 6, txbf, rxbf);
    spiUnselect(&SPID1);
    spiReleaseBus(&SPID1);
    W25Q16_notBusy();
    W25Q16_writeDisable();
}

/*
 *  Purpose :   Initializes flash for stream write, e.g. write more than one byte
 *              consecutively.  Both page and byte addresses start at 0. Page
 *              ends at address 8191 and page address ends at 255.
 *
 */
void W25Q16_initStreamWrite(uint16_t page, uint8_t pageAddress) {
    uint8_t txbf[4], rxbf[4];
    W25Q16_writeEnable();
    spiAcquireBus(&SPID1);
    txbf[0] = PAGE_PROGRAM;
    txbf[1] = (page >> 8) & 0xFF;
    txbf[2] = page & 0xFF;
    txbf[3] = pageAddress;
    spiSelect(&SPID1);
    spiExchange(&SPID1, 4, txbf, rxbf);
}


/*
 *  Purpose :   Writes a byte to the W25Q16.  Must be first called after
 *              initStreamWrite and then consecutively to write multiple bytes.
 *
 */
void W25Q16_streamWrite(uint16_t *page, uint8_t *pageAddress, uint8_t val) {

  uint8_t txbf[1], rxbf[1];
  txbf[0] = val;
  spiExchange(&SPID1, 1, txbf, rxbf);
  (*pageAddress)++;
  if (*pageAddress==0) // cambio de pagina
  {
    (*page)++;
    W25Q16_closeStreamWrite();
    W25Q16_initStreamWrite(*page, *pageAddress);
  }
}


/*
 *  Purpose :   Close the stream write. Must be called after the last call to
 *              streamWrite.
 */

void W25Q16_closeStreamWrite(void) {
    spiUnselect(&SPID1);
    spiReleaseBus(&SPID1);
    W25Q16_notBusy();
    W25Q16_writeDisable();
}



/*
 *  Purpose :   Initializes flash for stream read, e.g. read more than one byte
 *              consecutively.  Both page and byte addresses start at 0. Page
 *              ends at address 8191 and page address ends at 255.
 *
 */
void W25Q16_initStreamRead(uint16_t page, uint8_t pageAddress) {
    uint8_t txbf[6], rxbf[6];
    spiAcquireBus(&SPID1);
    txbf[0] = READ_DATA;
    txbf[1] = (page >> 8) & 0xFF;
    txbf[2] = page & 0xFF;
    txbf[3] = pageAddress;
    spiSelect(&SPID1);
    spiExchange(&SPID1, 4, txbf, rxbf);
}


/*
 *  Purpose :   Reads a byte from the W25Q16.  Must be first called after
 *              initStreamRead and then consecutively to write multiple bytes.
 *
 */
uint8_t W25Q16_streamRead(uint16_t *page, uint8_t *pageAddress) {
  uint8_t txbf[1], rxbf[1];
  spiExchange(&SPID1, 1, txbf, rxbf);
  (*pageAddress)++;
  if (*pageAddress==0) // cambio de pagina
  {
    (*page)++;
    W25Q16_closeStreamRead();
    W25Q16_initStreamRead(*page, *pageAddress);
  }
  return rxbf[0];
}

/*
 *  Purpose :   Reads a byte from the W25Q16 and rewind the pointer
 *
 */
uint8_t W25Q16_streamPeek(uint16_t *page, uint8_t *pageAddress) {
  uint8_t txbf[1], rxbf[1];
  spiExchange(&SPID1, 1, txbf, rxbf);
  W25Q16_closeStreamRead();
  W25Q16_initStreamRead(*page, *pageAddress);
  return rxbf[0];
}

/*
 *  Purpose :   Close the stream read. Must be called after the last call to
 *              streamRead. */

void W25Q16_closeStreamRead(void) {
    spiUnselect(&SPID1);
    spiReleaseBus(&SPID1);
    W25Q16_notBusy();
}


/*
 *   Purpose :   Puts the flash in its low power mode.
 *
 */
void W25Q16_powerDown(void) {
  uint8_t txbf[1], rxbf[1];
  spiAcquireBus(&SPID1);
  txbf[0] = POWER_DOWN;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
}

/*
 *  Purpose :   Releases the flash from its low power mode.  Flash cannot be in
 *              low power mode to perform read/write operations.
 *
 */
void W25Q16_releasePowerDown(void) {
  uint8_t txbf[1], rxbf[1];
  spiAcquireBus(&SPID1);
  txbf[0] = RELEASE_POWER_DOWN;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
}


/*
 *   Purpose:   Erases sector (4k) from the flash.
 *
 */
void W25Q16_sectorErase(uint16_t page)
{
  uint8_t txbf[4], rxbf[4];
  W25Q16_writeEnable();
  spiAcquireBus(&SPID1);
  txbf[0] = SECTOR_ERASE;
  txbf[1] = (page >> 8) & 0xFF;
  txbf[2] = page & 0xFF;
  txbf[3] = 0;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 4, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
  W25Q16_writeDisable();
}

/*
 *   Purpose :   Erases all data from the flash.
 *
 */
void W25Q16_chipErase(void)
{
  uint8_t txbf[1], rxbf[1];
  W25Q16_writeEnable();
  spiAcquireBus(&SPID1);
  txbf[0] = CHIP_ERASE;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
  W25Q16_writeDisable();
}


/*
 *   Purpose :   Reads the manufacturer ID from the W25Q16.  Should return 0xEF.
 *
 */
uint16_t W25Q16_manufacturerID(void) {

  uint8_t txbf[6], rxbf[6];
  spiAcquireBus(&SPID1);
  txbf[0] = MANUFACTURER_ID;
  txbf[1] = 0;
  txbf[2] = 0;
  txbf[3] = 0;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 6, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
  return (rxbf[4]<<8) + rxbf[5];
}


/*
 *  Purpose :   Halts operation until the flash is finished with its
 *              write/erase operation. Bit 0 of Status Register 1 of the
 *              W25Q16 is 1 if the chip is busy with a write/erase operation.
 *
 */
void W25Q16_notBusy(void) {
  uint8_t txbf[1], rxbf[1];
  spiAcquireBus(&SPID1);
  txbf[0] = READ_STATUS_REGISTER_1;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  txbf[0] = 0;
  do
  {
    spiExchange(&SPID1, 1, txbf, rxbf);
    osalThreadSleepMilliseconds(1);
  } while (rxbf[0]&1);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
}


/*
 *  Purpose :   Sets Bit 1 of Status Register 1.  Bit 1 is the write enable
 *              latch bit of the status register. This bit must be set prior
 *              to every write/erase operation.
 *
 */
void W25Q16_writeEnable(void) {
  uint8_t txbf[1], rxbf[1];
  spiAcquireBus(&SPID1);
  txbf[0] = WRITE_ENABLE;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
}



/*
 *  Purpose :   Clears Bit 1 of Status Register 1.  Bit 1 is the write enable
 *              latch bit of the status register.  Clearing this bit prevents
 *              the flash from being written or erased.
 *
 */
void W25Q16_writeDisable(void) {
  uint8_t txbf[1], rxbf[1];
  spiAcquireBus(&SPID1);
  txbf[0] = WRITE_DISABLE;
  spiSelect(&SPID1);
  spiExchange(&SPID1, 1, txbf, rxbf);
  spiUnselect(&SPID1);
  spiReleaseBus(&SPID1);
  W25Q16_notBusy();
}
