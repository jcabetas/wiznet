/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "modbus.h"
#include "registros.h"


// https://github.com/alex31/bras_robot_e407_lcd4ds/blob/master/modbus/rtu/mbrtu.c
// https://www.modbustools.com/modbus.html
// exception codes https://product-help.schneider-electric.com/ED/ES_Power/NT-NW_Modbus_IEC_Guide/EDMS/DOCA0054EN/DOCA0054xx/Master_NS_Modbus_Protocol/Master_NS_Modbus_Protocol-5.htm

extern holdingRegister *baudHR;



// Envio buffer preparado por modbus
// suponemos que el bus esta libre
int16_t modbus::envioStrModbus(uint8_t *str, uint16_t lenStr, sysinterval_t timeout)
{
    eventmask_t evt;
    event_listener_t endEot_event;
    chEvtRegisterMaskWithFlags (chnGetEventSource(&SD2),&endEot_event, EVENT_MASK (0),CHN_TRANSMISSION_END);
    palSetLine(LINE_TXRX);
    chThdSleepMilliseconds(2);
    sdWrite(&SD2, str,lenStr);
    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, timeout);
        if (evt==0) // Timeout
            break;
        if (evt & EVENT_MASK(0)) // Evento fin de transmision, limpio RX/TX
            break;
    }
    chEvtUnregister(chnGetEventSource(&SD2),&endEot_event);
    palClearLine(LINE_TXRX);
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
uint32_t ponUsMaxChr(uint16_t baudios)
{
    return (uint32_t) (16500000/baudios);
}


void modbus::esperaSilencioModbus(void)
{
    uint8_t buffer[2];
    size_t nb;
    palClearLine(LINE_TXRX);
    do
    {
        nb = sdReadTimeout(&SD2,buffer,1,usIntermsg);
        if (nb==0) return;
    } while (true);
}


// suponemos que el bus esta libre
bool modbus::readModbus(uint8_t *buffer, uint16_t sizeOfBuffer, uint16_t *bytesReceived, sysinterval_t timeout)
{
    size_t nb;
    palClearLine(LINE_TXRX);
    // al primer caracter le concedo el tiempo de timeout
    nb = sdReadTimeout(&SD2,buffer,1,timeout);
    *bytesReceived = nb;
    if (*bytesReceived != 1)
        return false;
    // he leido un caracter, sigo con los siguientes
    do {
        nb = sdReadTimeout(&SD2,&buffer[*bytesReceived],1,chTimeUS2I(usMaxChar));
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


void modbus::startMBSerial(void)
{
    uint32_t baudios;
    SerialConfig configSD2;
    baudios = baudHR->getValor(); //holdingRegisters[HR_BAUDIOS];
    configSD2.speed = baudios;
    configSD2.cr1 = 0;
    configSD2.cr2 = USART_CR2_STOP1_BITS;// | USART_CR2_LINEN;;
    configSD2.cr3 = 0;
    sdStart(SD, &configSD2);
    ponUsInterMsg(baudios);
    ponUsMaxChr(baudios);
}


