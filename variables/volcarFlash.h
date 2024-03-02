/*
 * volcarFlash.h
 *
 *  Created on: 26 jul. 2020
 *      Author: jcabe
 */

#ifndef W25Q16_VOLCARFLASH_H_
#define W25Q16_VOLCARFLASH_H_

void dumpFlash(BaseSequentialStream *tty, uint16_t page, uint8_t pageAddress, uint16_t longitud);
uint8_t printVar(BaseSequentialStream *tty, uint16_t sector);
uint8_t printPlcDeFlash(BaseSequentialStream *tty, uint16_t sector);
void volcarFlash(BaseSequentialStream *tty);


#endif /* W25Q16_VOLCARFLASH_H_ */
