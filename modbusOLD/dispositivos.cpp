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

#include "modbus.h"
#include "string.h"
#include "stdlib.h"
#include "tty.h"

dispositivo *dispositivo::listDispositivos[MAXDISPOSITIVOS] = {0};
uint8_t dispositivo::numDispositivos = 0;


dispositivo::dispositivo()
{
    /*
     *     static dispositivo *listDispositivos[MAXDISPOSITIVOS];
    static uint8_t numDispositivos;
     */
    if (numDispositivos>=MAXDISPOSITIVOS)
        return;
    listDispositivos[numDispositivos++] = this;
}

dispositivo::~dispositivo()
{
}

void dispositivo::deleteAll(void)
{
    for (int16_t d=0;d<dispositivo::numDispositivos;d++)
        delete listDispositivos[d];
    dispositivo::numDispositivos = 0;
}


