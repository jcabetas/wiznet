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

#include "string.h"
#include "stdlib.h"
#include "tty.h"
#include "colas.h"
#include "modbus.h"
#include "registros.h"



modbusMaster::modbusMaster(SerialDriver *SDpar, ioline_t rxtxLinePar)
{
    SD = SDpar;
    rxtxLine = rxtxLinePar;
    /*
     *     dispositivo *listDispositivosMB[MAXDISPOSITIVOS];
    uint8_t  errorEnDispMB[MAXDISPOSITIVOS];
    uint32_t  numErrores[MAXDISPOSITIVOS];
    uint32_t  numMs[MAXDISPOSITIVOS];
    uint32_t  numAccesos[MAXDISPOSITIVOS];
    float tMedio[MAXDISPOSITIVOS];
     */
    for (uint8_t i=0;i<MAXDISPOSITIVOS;i++)
    {
        listDispositivosMB[i] = NULL;
        errorEnDispMB[i] = 0;
        numErrores[i] = 0;
        numMs[i] = 0;
        numAccesos[i] = 0;
        tMedio[i] = 0.0f;
    }
    numDispositivosMB = 0;
}


// Read input registers (FUNC04) o holding registers (FUN05)
// los datos vuelven en bufferRx
void modbusMaster::enviaMBfunc(uint8_t funcion, uint8_t dirMB, uint16_t addressReg, uint8_t numRegs, uint8_t bufferRx[],
                               uint16_t sizeofbufferRx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error)
{
    uint8_t buffer[10];
    uint16_t msgCRC, rxCRC, bytesReceived;
    if (sizeofbufferRx <= (7 + 2*numRegs))
    {
        *error = 3;
        return;
    }
    buffer[0] = dirMB;
    buffer[1] = funcion;
    buffer[2] = (addressReg&0xFF00)>>8;
    buffer[3] = addressReg&0xFF;
    buffer[4] = (numRegs & 0xFF00) >>8;
    buffer[5] = numRegs & 0xFF;

    msgCRC = CRC16(buffer, 6);
    buffer[6] = msgCRC & 0xFF;
    buffer[7] = (msgCRC & 0xFF00) >>8;
    bufferRx[0] = 0;
    bufferRx[1] = 0;
   // envio peticion a esclavo
    envioStrModbus(buffer, 8, chTimeMS2I(100));
    systime_t start = chVTGetSystemTime();
    // leemos respuesta
    bool hayMsg = readModbus(bufferRx, sizeofbufferRx,&bytesReceived, chTimeMS2I(msDelayMax));
    sysinterval_t duracion = chVTTimeElapsedSinceX(start);
    if (!hayMsg || bytesReceived!=(5+2*numRegs))
    {
        *error = 2;
        return;
    }
    msgCRC = CRC16(bufferRx, bytesReceived-2);
    rxCRC = (bufferRx[bytesReceived-1]<<8) + bufferRx[bytesReceived-2];
    if (msgCRC!=rxCRC || dirMB!= bufferRx[0] || bufferRx[1]!=funcion)
    {
        *error = 3;
        return;
    }
    *msDelay = chTimeI2MS(duracion);
    // quito los tres primeros caracteres del buffer.
    for (uint8_t i=3;i<bytesReceived-2;i++)
        bufferRx[i-3] = bufferRx[i];
    *error = 0;
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


uint8_t modbusMaster::addDisp(dispositivo *disp)
{
    /*
     *     dispositivo *listDispositivosMB[MAXDISPOSITIVOS];
    uint8_t  errorEnDispMB[MAXDISPOSITIVOS];
    uint32_t  numErrores[MAXDISPOSITIVOS];
    uint32_t  numMs[MAXDISPOSITIVOS];
    uint32_t  numAccesos[MAXDISPOSITIVOS];
    float tMedio[MAXDISPOSITIVOS];
    uint16_t numDispositivosMB;
     *
     */
    if (numDispositivosMB>=MAXDISPOSITIVOS)
        return 1;
    listDispositivosMB[numDispositivosMB] = disp;
    errorEnDispMB[numDispositivosMB] = 0;
    numErrores[numDispositivosMB] = 0;
    numMs[numDispositivosMB] = 0;
    numAccesos[numDispositivosMB] = 0;
    numDispositivosMB++;
    return 0;
}

uint8_t modbusMaster::diNumDisp(void)
{
    return numDispositivosMB;
}


uint8_t modbusMaster::lee(uint8_t numDispositivo, uint16_t msDelay)
{
    if (numDispositivo>=numDispositivosMB)
        return 1;
    dispositivo *disp = listDispositivosMB[numDispositivo];
    if (disp==NULL)
        return 2;
    disp->addms(msDelay);
    disp->usaBus();
    return 0;
}

// despues de leer un numero de registros, decodificamos un registro
// 012345 (tres registros, maxima posIni=4)
//
uint16_t modbusMaster::decodeU16(uint8_t *bufferDatos, uint8_t posIni, uint8_t numRegsEnBuffer, uint8_t *error)
{
    *error = 0;
    if (posIni > 2*numRegsEnBuffer - 2)
    {
        *error = 1;
        return 0;
    }
    return (bufferDatos[posIni]<<8) + bufferDatos[posIni+1];
}
