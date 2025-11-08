/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "variables.h"
#include "string.h"
#include "stdlib.h"
#include "tty.h"
#include "variables.h"
#include "colas.h"
#include "modbus.h"
#include "registros.h"



void modbusMaster::enviaMBfunc04(uint8_t dirMB, uint16_t addressReg, uint8_t numRegs, uint8_t bufferRx[], uint16_t sizeofbufferRx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error)
{
    uint8_t buffer[10];
    uint16_t msgCRC, rxCRC, bytesReceived;
    if (sizeofbufferRx <= (7 + 2*numRegs))
    {
        *error = -3;
        return;
    }
    buffer[0] = dirMB;
    buffer[1] = 0x04;
    buffer[2] = (addressReg&0xFF00)>>8;
    buffer[3] = addressReg&0xFF;
    buffer[4] = (numRegs & 0xFF00) >>8;
    buffer[5] = numRegs & 0xFF;

    msgCRC = CRC16(buffer, 6);
    buffer[6] = msgCRC & 0xFF;
    buffer[7] = (msgCRC & 0xFF00) >>8;
    bufferRx[0] = 0;
    bufferRx[1] = 0;
   // modbus::chprintStrRs485(buffer, 8, chTimeMS2I(100));
    envioStrModbus(buffer, 8, chTimeMS2I(100));
    systime_t start = chVTGetSystemTime();
    *error = modbus::chReadStrRs485(bufferRx, 1, &bytesReceived, chTimeMS2I(msDelayMax));
    if (*error!=0 || bytesReceived!=1)
    {
        *error = 1;
        return;
    }
    sysinterval_t duracion = chVTTimeElapsedSinceX(start);
    *error = modbus::chReadStrRs485(&bufferRx[1], 4+2*numRegs, &bytesReceived, chTimeMS2I(10));
    if (*error!=0 || bytesReceived!=(4+2*numRegs))
    {
        *error = 2;
        return;
    }
    msgCRC = CRC16(bufferRx, bytesReceived-1);
    rxCRC = (bufferRx[bytesReceived]<<8) + bufferRx[bytesReceived-1];
    if (msgCRC!=rxCRC || dirMB!= bufferRx[0] || bufferRx[1]!=0x04)
    {
        *error = 3;
        return;
    }
    *msDelay = chTimeI2MS(duracion);
    return;
}


// Function 16 (10hex) Write Multiple Registers
// Writes values into a sequence of holding registers
void modbusMaster::enviaMBfunc10(uint8_t dirMB, uint16_t startAddressReg, uint8_t numRegs, uint16_t RegsTx[], uint16_t numRegsEnTx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error)
{
    uint8_t buffer[30];
    uint8_t bufferRx[30];
    uint16_t msgCRC, rxCRC, bytesReceived;
    if (numRegs > numRegsEnTx)
    {
        *error = 3;
        return;
    }
    buffer[0] = dirMB;
    buffer[1] = 0x10;
    buffer[2] = (startAddressReg&0xFF00)>>8;
    buffer[3] = startAddressReg&0xFF;
    buffer[4] = (numRegs & 0xFF00) >>8;
    buffer[5] = numRegs & 0xFF;
    for (int8_t i=0;i<numRegs;i++)
    {
        buffer[6+2*i] = (RegsTx[i] & 0xFF00) >>8;
        buffer[7+2*i] = RegsTx[i] & 0xFF;
    }
    msgCRC = CRC16(buffer, 6+2*numRegs);
    buffer[6+2*numRegs] = msgCRC & 0xFF;
    buffer[7+2*numRegs] = (msgCRC & 0xFF00) >>8;
    bufferRx[0] = 0;
    bufferRx[1] = 0;
    envioStrModbus(buffer, 8+2*numRegs, chTimeMS2I(100));
    systime_t start = chVTGetSystemTime();
    *error = modbus::chReadStrRs485(bufferRx, 1, &bytesReceived, chTimeMS2I(msDelayMax));
    if (*error!=0 || bytesReceived!=1)
    {
        *error = 1;
        return;
    }
    sysinterval_t duracion = chVTTimeElapsedSinceX(start);
    *error = modbus::chReadStrRs485(&bufferRx[1], 8, &bytesReceived, chTimeMS2I(10));
    if (*error!=0 || bytesReceived!=8)
    {
        *error = 2;
        return;
    }
    msgCRC = CRC16(bufferRx, bytesReceived-1);
    rxCRC = (bufferRx[bytesReceived]<<8) + bufferRx[bytesReceived-1];
    if (msgCRC!=rxCRC || dirMB!= bufferRx[0] || bufferRx[1]!=0x04)
    {
        *error = 3;
        return;
    }
    *msDelay = chTimeI2MS(duracion);
    return;
}
