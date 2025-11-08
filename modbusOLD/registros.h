/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#ifndef REGISTROS_H_
#define REGISTROS_H_

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#define MAXHOLDINGREGISTERS 40
#define MAXINPUTREGISTERS 40

#define HR_MODORADIO 2
#define HR_IDMODBUSVACON 9
#define HR_IDMODBUSLLAMADOR 22

class holdingRegister
{
private:
    char nombre[25];
    uint16_t valInterno;
    uint16_t valDefecto;
    uint16_t valMin;
    uint16_t valMax;
    const uint32_t *valInt2Ext;
    const char *descValInt[3];
public:
    holdingRegister(const char *nombre,const char *opcionesStr[3],const uint32_t int2ext[], uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault);
    uint16_t getValor(void);
    uint32_t getValorInterno(void);
    uint8_t setValorInterno(uint16_t valor);
};

class inputRegister
{
private:
    char nombre[20];
    uint16_t valInterno;
public:
    inputRegister(const char *nombre);
    uint16_t getValor(void);
    void setValor(uint16_t valor); // devuelve codigo de error
};

void error(void);

#endif /* REGISTROS_H_ */
