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

extern holdingRegister *idMBSlave;



modbusSlave::modbusSlave(SerialDriver *SDpar)
{
    SD = SDpar;
    // inicializamos listado HR
    holdingRegister *defaultHR = new holdingRegister("HR no valido",NULL,NULL,0,0,0);  // por si usamos alguno sin inicializar
    for (uint16_t i=0;i<MAXHOLDINGREGISTERS;i++)
        listHoldingRegister[i] = defaultHR;
    numHoldingRegistros = 0;
    // idem input registers
    inputRegister *defaultIR = new inputRegister("IR no valido");                      // idem
    for (uint16_t i=0;i<MAXINPUTREGISTERS;i++)
        listInputRegister[i] = defaultIR;
    numInputRegistros = 0;
}



holdingRegister *modbusSlave::addHoldingRegister(const char *nombrePar,const char *opcionesStr[3],
                                 const uint32_t int2ext[], uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegister *nuevoHR = new holdingRegister(nombrePar, opcionesStr, int2ext, opcMin, opcMax, opcDefault);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}


inputRegister *modbusSlave::addInputRegister(const char *nombrePar)
{
    if (numInputRegistros>=MAXINPUTREGISTERS)
        return NULL;
    inputRegister *nuevoIR = new inputRegister(nombrePar);
    listInputRegister[numInputRegistros] = nuevoIR;
    numInputRegistros++;
    return nuevoIR;
}

uint8_t modbusSlave::setHR(uint16_t numHR, uint16_t valor)
{
    if (numHR>=numHoldingRegistros)
        return 1;
    return listHoldingRegister[numHR]->setValorInterno(valor);
}

uint8_t modbusSlave::getHR(uint16_t numHR, uint32_t *valor)
{
    if (numHR>=numHoldingRegistros)
        return 1;
    *valor = listHoldingRegister[numHR]->getValor();
    return 0;
}

uint8_t modbusSlave::getHRInterno(uint16_t numHR, uint16_t *valor)
{
    if (numHR>=numHoldingRegistros)
        return 1;
    *valor = listHoldingRegister[numHR]->getValor();
    return 0;
}

//  Read holding registers
//  Slave Address 01
//  Function 03
//  Starting Address High 00
//  Starting Address Low  00
//  Number of regs High   00
//  Number of regs Low    02
//  Error Check Low       71
//  Error Check High      CB
bool modbusSlave::interpretoFunction03(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived)
{
    uint8_t buffTx[30];
    if (bytesReceived != 8)
        return false;
    uint16_t startAddress = (buffer[2]<<8) + buffer[3];
    uint16_t numbOfPoints = (buffer[4]<<8) + buffer[5];
    uint16_t endAddress = startAddress + numbOfPoints - 1;
    if (endAddress >= NUMHOLDINGREGS)
        return errorMB(myId, buffer, 2);
    // prepara respuesta
    buffTx[0] = myId;
    buffTx[1] = 3;
    buffTx[2] = numbOfPoints*2U;
    for (uint8_t r=0; r<numbOfPoints;r++)
    {
        uint16_t valor = listHoldingRegister[r+startAddress]->getValorInterno();
        buffTx[3+r*2] = (valor & 0xFF00) >> 8;
        buffTx[4+r*2] = (valor & 0xFF);
    }
    uint16_t msgCRC = modbus::CRC16(buffTx, 3+2*numbOfPoints);
    buffTx[3+2*numbOfPoints] = (msgCRC & 0xFF);
    buffTx[4+2*numbOfPoints] = (msgCRC & 0xFF00) >> 8;
    envioStrModbus(buffTx, 5+2*numbOfPoints, chTimeMS2I(500));
    return true;
}

// ver https://www.fernhillsoftware.com/help/drivers/modbus/modbus-protocol.html#readInputRegs
// Read input registers
//  Slave Address         01
//  Function              04
//  Starting Address High 00
//  Starting Address Low  00
//  Number of regs High   00
//  Number of regs Low    02
//  Error Check Low       71
//  Error Check High      CB
bool modbusSlave::interpretoFunction04(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived)
{
    uint8_t buffTx[30];
    if (bytesReceived != 8)
        return false;
    uint16_t startAddress = (buffer[2]<<8) + buffer[3];
    uint16_t numbOfPoints = (buffer[4]<<8) + buffer[5];
    uint16_t endAddress = startAddress + numbOfPoints - 1;
    if (endAddress >=NUMINPUTREGS)
        return errorMB(myId, buffer, 2);
    buffTx[0] = myId;
    buffTx[1] = 4;
    buffTx[2] = numbOfPoints*2U;
    for (uint8_t r=0; r<numbOfPoints;r++)
    {
        uint16_t valor = listInputRegister[r+startAddress]->getValor();
        buffTx[3+r*2] = (valor & 0xFF00) >> 8;
        buffTx[4+r*2] = (valor & 0xFF);
    }
    uint16_t msgCRC = modbus::CRC16(buffTx, 3+2*numbOfPoints);
    buffTx[3+2*numbOfPoints] = (msgCRC & 0xFF);
    buffTx[4+2*numbOfPoints] = (msgCRC & 0xFF00) >> 8;
    envioStrModbus(buffTx, 5+2*numbOfPoints, chTimeMS2I(500));
    return true;
}

// Write Single Holding Register
//  Slave Address   11
//  Function        06
//  Reg. Address Hi 00
//  Reg. Address Lo 01
//  Write Data Hi   00
//  Write Data Lo   03
//  Error Check Lo  9A
//  Error Check Hi  9B
bool modbusSlave::interpretoFunction06(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived)
{
    uint8_t buffTx[30];
    if (bytesReceived != 8)
        return false;
    uint16_t regAddress = (buffer[2]<<8) + buffer[3];
    uint16_t data = (buffer[4]<<8) + buffer[5];
    if (regAddress >= NUMHOLDINGREGS)
        return errorMB(myId, buffer, 1);
    uint8_t error = setHR(regAddress, data);
    //
    if (error)
        return errorMB(myId, buffer, 1);
    // responde
    buffTx[0] = myId;
    buffTx[1] = 6;
    buffTx[2] = buffer[2];
    buffTx[3] = buffer[3];
    buffTx[4] = buffer[4];
    buffTx[5] = buffer[5];
    uint16_t msgCRC = modbus::CRC16(buffTx, 6);
    buffTx[6] = (msgCRC & 0xFF);
    buffTx[7] = (msgCRC & 0xFF00) >> 8;
    envioStrModbus(buffTx, 8, chTimeMS2I(500));
    return true;
}


// Write multiple Holding Register
//    Slave Address                 11
//    Function                      10
//    Starting Address Hi           00
//    Starting Address Lo           01
//    Quantity of Registers Hi      00
//    Quantity of Registers Lo      02
//    Byte Count                    04
//    Data Hi                       00
//    Data Lo                       0A
//    Data Hi                       01
//    Data Lo                       02
//    Error Check Lo                C6
//    Error Check Hi                F0
bool modbusSlave::interpretoFunction16(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived)
{
    uint8_t buffTx[30];
    uint16_t regStartAddress = (buffer[2]<<8) + buffer[3];
    uint16_t numRegs = (buffer[4]<<8) + buffer[5];
    uint16_t numBytes = buffer[6];
    if (2*numRegs!=numBytes)
        return errorMB(myId, buffer, 1);
    if (regStartAddress+numRegs -1 >= NUMHOLDINGREGS)
        return errorMB(myId, buffer, 1);
    if (bytesReceived != 9+2*numRegs)
        return errorMB(myId, buffer, 1);
    for (int16_t reg=0;reg<numRegs;reg++)
    {
        uint16_t data = (buffer[7+2*reg]<<8) + buffer[8+2*reg];
        uint8_t error = setHR(reg+regStartAddress, data);
        if (error)
            return errorMB(myId, buffer, 1);
    }
    // responde
    //    Slave Address                 11
    //    Function                      10
    //    Starting Address Hi           00
    //    Starting Address Lo           01
    //    Quantity of Registers Hi      00
    //    Quantity of Registers Lo      02
    //    Error Check Lo                12
    //    Error Check Hi                98

    buffTx[0] = myId;
    buffTx[1] = 16;
    buffTx[2] = buffer[2];
    buffTx[3] = buffer[3];
    buffTx[4] = buffer[4];
    buffTx[5] = buffer[5];
    uint16_t msgCRC = modbus::CRC16(buffTx, 6);
    buffTx[6] = (msgCRC & 0xFF);
    buffTx[7] = (msgCRC & 0xFF00) >> 8;
    envioStrModbus(buffTx, 8, chTimeMS2I(500));
    return true;
}

bool modbusSlave::interpretaMsg(uint8_t *buffer, uint16_t bytesReceived)
{
    // va dirigido a nosotros?
    uint8_t myId =idMBSlave->getValor();
    if (buffer[0] != myId)
        return false;
    // check CRC
    uint16_t msgCRC = modbus::CRC16(buffer, bytesReceived-2);
    uint16_t rxCRC = (buffer[bytesReceived-1]<<8) + buffer[bytesReceived-2];
    if (msgCRC != rxCRC)
        return false;
    uint8_t funct = buffer[1];
    switch(funct) {
      case 3:
          return interpretoFunction03(myId, buffer, bytesReceived);
          break;
      case 4:
          return interpretoFunction04(myId, buffer, bytesReceived);
          break;
      case 6:
          return interpretoFunction06(myId, buffer, bytesReceived);
          break;
      case 16:
          return interpretoFunction16(myId, buffer, bytesReceived);
          break;
      default:
          return errorMB(myId, buffer,1);
    }
}
