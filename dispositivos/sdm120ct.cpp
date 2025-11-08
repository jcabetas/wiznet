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
#include "lcd.h"
#include "stdio.h"
#include "externRegistros.h"

uint16_t CRC16(const uint8_t *nData, uint16_t wLength);

static const char *descMedida120CT[] = {"V","I","P","Px2","Px3","Q","kWh","kVArh"};
static const uint16_t address120CT[] = {0, 0x6, 0x0C, 0x0C, 0x0C, 0x18, 0x156, 0x158};


sdm120ct::sdm120ct(modbusMaster *modbusMaestroPtr, const char *nombrePar, uint8_t dirPar)
{
    strncpy(nombre,nombrePar,sizeof(nombre));
    direccion = dirPar;
    numMedidas = 0;
    erroresSeguidos = 0;
    modbusPtr = modbusMaestroPtr;
    modbusPtr->addDisp(this);
};

sdm120ct::~sdm120ct()
{
}


const char *sdm120ct::diTipo(void)
{
    return "sdm120";
}

char *sdm120ct::diNombre(void)
{
    return nombre;
}

int8_t sdm120ct::init(void)
{
    // no necesita inicializacion
    return 0;
}

void sdm120ct::addms(uint16_t ms)
{
    for (uint8_t m=1;m<=numMedidas;m++)
    {
        if (msDesdeUpdate[m-1] < msUpdateMaxMed[m-1])
        {
            msDesdeUpdate[m-1] += ms;
        }
    }
}

/*
    float *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint8_t dsUpdateMaxMed[MAXMEDIDAS];
    uint16_t dsDesdeUpdate[MAXMEDIDAS];
 */
uint8_t sdm120ct::attachMedida(medida *ptrMedPar, const char *tipoMedida, uint16_t msUpdatePar, const char *descrPar)
{
    if (numMedidas>=MAXMEDIDAS)
    {
        chprintf((BaseSequentialStream *)&SD1,"Error: demasiadas medidas en dispositivo\n");
        return 1;
    }
    if (ptrMedPar==NULL)
    {
        chprintf((BaseSequentialStream *)&SD1,"Error: attachMedida con puntero NULL\n");
        return 2;
    }
    // busco la direccion que corresponde con la medida buscada
    for (uint8_t tip=0;tip<8;tip++)
    {
        if (!strcasecmp(descMedida120CT[tip],tipoMedida))
        {
            tipoMed[numMedidas] = tip;
            strncpy(descrMed[numMedidas], descrPar, sizeof(descrMed[numMedidas]));
            ptrMed[numMedidas] = ptrMedPar;
            msUpdateMaxMed[numMedidas] = msUpdatePar;
            msDesdeUpdate[numMedidas] = msUpdatePar + 1;  // para que se lea inmediatamente al comienzo
            numMedidas++;
            return 0;
        }
    }
    chprintf((BaseSequentialStream *)&SD1,"Error: sdm120CT no tiene medida tipo '%s'\n",tipoMedida);
    return 0;
}

void sdm120ct::leer(float *valor, uint16_t addressReg, uint8_t *error)
{
    float valor2;
    uint8_t  *ptrChr;
    uint8_t bufferRx[20];

    modbusPtr->enviaMBfunc(4, direccion, addressReg, 2, bufferRx, sizeof(bufferRx), 500, &msDelay, error);
    if (*error!=0)
    {
        return;
    }
    ptrChr = (uint8_t *)&valor2;
    *ptrChr++ = bufferRx[3];
    *ptrChr++ = bufferRx[2];
    *ptrChr++ = bufferRx[1];
    *ptrChr = bufferRx[0];
    *error = 0;
    *valor = valor2;
    return;
}

// retorna 1 si hay error
// mejor: 0: no error, 1: error, 2: no se ha usado
/*
    float *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint8_t dsUpdateMaxMed[MAXMEDIDAS];
    uint16_t dsDesdeUpdate[MAXMEDIDAS];
 */
uint8_t sdm120ct::usaBus(void)
{
    float valor;
    uint8_t error;
    uint8_t heEnviado = 1;
    for (uint8_t m=1;m<=numMedidas;m++)
    {
        if (msDesdeUpdate[m-1] >= msUpdateMaxMed[m-1])
        {
            uint8_t tip = tipoMed[m-1];
            leer(&valor, address120CT[tip], &error);
            heEnviado = 1;
            if (error!=0)
            {
                erroresSeguidos++;
                // vuelve a intentarlo
                leer(&valor, address120CT[tip], &error);
                if (error!=0)
                {
                    erroresSeguidos++;
                    if (erroresSeguidos>4)
                    {
                        ptrMed[m-1]->setValidez(false);
                    }
                    return 2;
                }
            }
            else
                erroresSeguidos = 0;
            // quizas hay que multiplicar valor (p.e. Px2)
            if (tip==3)
                valor *= 2.0f;
            if (tip==4)
                valor *= 3.0f;
//            med->setValidez(1); ya se valida al hacer el set
            ptrMed[m-1]->setValor(valor);
            msDesdeUpdate[m-1] = 0;
        }
    }
    if (heEnviado)
        return 0;
    else
        return 2;
}

void sdm120ct::changeID(uint8_t oldId, uint8_t newId, uint8_t *error)
{
    float newIdFloat;
    uint8_t  *ptrChr;
    uint8_t buffer[20];
    /*
     * Example

    The following query will request Volts from an instrument with node address 1:

    Field Name Example(Hex)
    Slave Address 01
    Function 04
    Starting Address High 00
    Starting Address Low 00
    Number of Points High 00
    Number of Points Low 02
    Error Check Low 71
    Error Check High CB
     */
    newIdFloat = (float) newId;
//    buffer[0] = oldId;//direccion;
//    buffer[1] = 0x10; // write holding registers
//    buffer[2] = 0;//(addressReg&0xFF00)>>8;
//    buffer[3] = 0x14;//addressReg&0xFF;
//    buffer[4] = 0;
//    buffer[5] = 2; // number of registers low
//    buffer[6] = 4; // count bytes

    ptrChr = (uint8_t *)&newIdFloat;
    buffer[3] = *ptrChr++;
    buffer[2] = *ptrChr++;
    buffer[1] = *ptrChr++;
    buffer[0] = *ptrChr++;

    modbusPtr->enviaMBfunc10(oldId, 0x14, 2, (uint16_t *) &buffer, 2, 500, &msDelay, error);
    // la respuesta se hará con otros baudios, con lo que se producirá un error seguro

    return;
}






