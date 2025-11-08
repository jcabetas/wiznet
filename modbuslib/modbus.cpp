/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "string.h"
#include "modbus.h"
#include "registros.h"
#include "w25q16.h"
#include <stdio.h>

// https://github.com/alex31/bras_robot_e407_lcd4ds/blob/master/modbus/rtu/mbrtu.c
// https://www.modbustools.com/modbus.html
// exception codes https://product-help.schneider-electric.com/ED/ES_Power/NT-NW_Modbus_IEC_Guide/EDMS/DOCA0054EN/DOCA0054xx/Master_NS_Modbus_Protocol/Master_NS_Modbus_Protocol-5.htm

//extern holdingRegister *baudHRSlave;

//class holdingRegister;

//mutex_t holdingRegister::mtxUsandoW25q16;


modbus::modbus(void)
{
    chMtxObjectInit(&mtxUsandoW25q16);
    // inicializamos listado HR
    holdingRegisterInt *defaultHR = new holdingRegisterInt(0, "HR no valido",0,0,0, false);  // por si usamos alguno sin inicializar
    for (uint16_t i=0;i<MAXHOLDINGREGISTERS;i++)
        listHoldingRegister[i] = defaultHR;
    numHoldingRegistros = 0;
    // idem input registers
    inputRegister *defaultIR = new inputRegister("IR no valido");                      // idem
    for (uint16_t i=0;i<MAXINPUTREGISTERS;i++)
        listInputRegister[i] = defaultIR;
    numInputRegistros = 0;
}

//holdingRegister *modbus::addHoldingRegister(const char *nombrePar,const char *opcionesStr[3],
//                                 const uint32_t int2ext[], uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabablePar)
//{
//    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
//        return NULL;
//    holdingRegister *nuevoHR = new holdingRegister(numHoldingRegistros, nombrePar, opcionesStr, int2ext, opcMin, opcMax, opcDefault, grabablePar);
//    listHoldingRegister[numHoldingRegistros] = nuevoHR;
//    numHoldingRegistros++;
//    return nuevoHR;
//}




inputRegister *modbus::addInputRegister(const char *nombrePar)
{
    if (numInputRegistros>=MAXINPUTREGISTERS)
        return NULL;
    inputRegister *nuevoIR = new inputRegister(nombrePar);
    listInputRegister[numInputRegistros] = nuevoIR;
    numInputRegistros++;
    return nuevoIR;
}

uint8_t modbus::setHR(uint16_t numHR, uint16_t valor)
{
    if (numHR>=numHoldingRegistros)
        return 2;
    uint8_t hayError = listHoldingRegister[numHR]->setValorInterno(valor);
    if (hayError)
        return 3;
    if (listHoldingRegister[numHR]->esGrabable())
        escribeHR(false);
    return 0;
}

holdingRegister *modbus::getHoldingRegister(uint8_t numHR)
{
    if (numHR>=numHoldingRegistros)
        return NULL;
    return listHoldingRegister[numHR];
}

uint8_t modbus::getNumHR(void)
{
    return numHoldingRegistros;
}

inputRegister *modbus::getInputRegister(uint8_t numIR)
{
    if (numIR>=numInputRegistros)
        return NULL;
    return listInputRegister[numIR];
}

uint8_t modbus::getNumIR(void)
{
    return numInputRegistros;
}

//uint8_t modbus::getHR(uint16_t numHR, uint32_t *valor)
//{
//    if (numHR>=numHoldingRegistros)
//        return 1;
//    *valor = listHoldingRegister[numHR]->getValor();
//    return 0;
//}

uint8_t modbus::getHRInterno(uint16_t numHR, uint16_t *valor)
{
    if (numHR>=numHoldingRegistros)
        return 1;
    *valor = listHoldingRegister[numHR]->getValorInterno();
    return 0;
}


uint8_t modbus::escribeHR(bool useDefaultValues)
{
  uint16_t valor;
  chMtxLock(&mtxUsandoW25q16);
  uint16_t hayW25q16 = W25Q16_start();
  if (hayW25q16) // hay w25q16 instalado?
  {
      W25Q16_sectorErase(0);
      for (uint8_t v=0; v<numHoldingRegistros; v++)
      {
          holdingRegister *hr = listHoldingRegister[v];
          if (hr==NULL)
              continue;
          if (useDefaultValues)
              valor = hr->getValorInternoDefecto();
          else
              valor = hr->getValorInterno();
          W25Q16_write_u16(0, 2+2*v, valor);
      }
      W25Q16_write_u16(0, 0, 0x7851);
  }
  spiStop(&SPID1);
  chMtxUnlock(&mtxUsandoW25q16);
  return hayW25q16;
  //leeVariables();
}

uint8_t modbus::leeHR(void)
{
  //chprintf(ttyCOM,"Leo variables\n");
  chMtxLock(&mtxUsandoW25q16);
  uint16_t hayW25q16 = W25Q16_start();
  if (hayW25q16)
  {
      //chprintf(ttyCOM,"- Detectado flash\n");
      uint16_t claveEEprom = W25Q16_read_u16(0, 0);
      if (claveEEprom != 0x7851)
      {
          //chprintf(ttyCOM,"- No esta inicializada... la reseteamos\n");
          // escribimos valores iniciales, tengo que liberar memoria
          spiStop(&SPID1);
          chMtxUnlock(&mtxUsandoW25q16);
          escribeHR(true); //reseteaEeprom();
          // volvemos a pillar la memoria
          chMtxLock(&mtxUsandoW25q16);
          W25Q16_start();
      }
      for (uint8_t v=0;v<numHoldingRegistros;v++)
      {
          holdingRegister *hr = listHoldingRegister[v];
          if (hr->esGrabable())
          {
              uint16_t valorLeido = W25Q16_read_u16(0, 2+v*2);
              hr->setValorInternoSinGrabar(valorLeido);
          }
      }
  }
  spiStop(&SPID1);
  chMtxUnlock(&mtxUsandoW25q16);
  return hayW25q16;
}

// mira si el valor del holdingRegister es diferente a la flash
uint8_t modbus::grabaHR(holdingRegister *holdReg)
{
  if (!holdReg->esGrabable())
      return false;
  chMtxLock(&mtxUsandoW25q16);
  uint16_t hayW25q16 = W25Q16_start();
  if (!hayW25q16)
      return 1; // no merece la pena escribir
  uint16_t claveEEprom = W25Q16_read_u16(0, 0);
  if (claveEEprom != 0x7851)
  {
      spiStop(&SPID1);
      chMtxUnlock(&mtxUsandoW25q16);
      escribeHR(true); //reseteaEeprom();
      // volvemos a pillar la memoria
      chMtxLock(&mtxUsandoW25q16);
      W25Q16_start();
  }
  uint16_t posicionHR = holdReg->getPosicion();
  uint16_t valorFlash = W25Q16_read_u16(0, 2+posicionHR*2);
  spiStop(&SPID1);
  chMtxUnlock(&mtxUsandoW25q16);
  if (valorFlash != holdReg->getValorInterno())
      escribeHR(false);
  return 0;
}

// Envio buffer preparado por modbus
// suponemos que el bus esta libre
int16_t modbus::envioStrModbus(uint8_t *str, uint16_t lenStr, sysinterval_t timeout)
{
    eventmask_t evt;
    event_listener_t endEot_event;
    chEvtRegisterMaskWithFlags (chnGetEventSource(SD),&endEot_event, EVENT_MASK (0),CHN_TRANSMISSION_END);
    palSetLine(rxtxLine);
//    chThdSleepMilliseconds(2);
    chThdSleepMicroseconds(usIntermsg);
    sdWrite(SD, str,lenStr);
    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, timeout);
        if (evt==0) // Timeout
            break;
        if (evt & EVENT_MASK(0)) // Evento fin de transmision, limpio RX/TX
            break;
    }
    chEvtUnregister(chnGetEventSource(SD),&endEot_event);
    palClearLine(rxtxLine);
    if (evt==0)
        return 1;
    return 0;
}

//  Send error response
//  Slave Address 01
//  Function 03+128
//  Error Check Low       71
//  Error Check High      CB
bool modbus::errorMB(uint8_t idModbus, uint8_t *buffer, uint8_t exceptionCode)
{
    uint8_t buffTx[8];
    buffTx[0] = idModbus;
    buffTx[1] = buffer[1] + 128;
    buffTx[2] = exceptionCode;
    uint16_t msgCRC = modbus::CRC16(buffTx, 3);
    buffTx[3] = (msgCRC & 0xFF);
    buffTx[4] = (msgCRC & 0xFF00) >> 8;
    envioStrModbus(buffTx, 5, chTimeMS2I(500));
    return false;
}


// timeout entre MB frames (3.5 veces caracter)
// para baudios>19200 es 1,75 ms
uint32_t ponUsInterMsg(uint16_t baudios)
{
    if( baudios > 19200U )
        return 1750U;
    else
        return (uint32_t) (38500000/baudios);
}

// timeout entre caracteres (1.5 veces tiempo de un caracter)
// lo subo a 3 veces, que es lo que me funciona bien
uint32_t ponUsMaxChr(uint16_t baudios)
{
    return (uint32_t) (33000000/baudios); // antes 16500000
}


void modbus::esperaSilencioModbus(void)
{
    uint8_t buffer[2];
    size_t nb;
    palClearLine(rxtxLine);
    do
    {
        nb = sdReadTimeout(SD,buffer,1,TIME_US2I(usIntermsg));
        if (nb==0) return;
    } while (true);
}


// suponemos que el bus esta libre
bool modbus::readModbus(uint8_t *buffer, uint16_t sizeOfBuffer, uint16_t *bytesReceived, sysinterval_t timeout)
{
    size_t nb;
    palClearLine(rxtxLine);
    // al primer caracter le concedo el tiempo de timeout
    nb = sdReadTimeout(SD,buffer,1,timeout);
    *bytesReceived = nb;
    if (*bytesReceived != 1)
        return false;
    // he leido un caracter, sigo con los siguientes
    do {
        nb = sdReadTimeout(SD,&buffer[*bytesReceived],1,chTimeUS2I(usMaxChar));
        if (nb == 0)
            break;
        *bytesReceived += nb;
        if (*bytesReceived>=sizeOfBuffer)
            return false; // mensaje demasiado largo
    } while (true);
    if (*bytesReceived >= 4) // minimo tamaño de frame modbus
        return true;
    else
        return false;
}



void modbus::startMBSerial(uint32_t baudios)
{
    SerialConfig configSD;
    configSD.speed = baudios;
    configSD.cr1 = 0;
    configSD.cr2 = USART_CR2_STOP1_BITS;// | USART_CR2_LINEN;;
    configSD.cr3 = 0;
    sdStart(SD, &configSD);
    usIntermsg = ponUsInterMsg(baudios);
    usMaxChar = ponUsMaxChr(baudios);
}

