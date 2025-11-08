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

uint16_t CRC16(const uint8_t *nData, uint16_t wLength);

static const char *descMedida120CT[] = {"V","I","P","Px2","Px3","Q","kWh","kVArh"};
static const uint16_t address120CT[] = {0, 0x6, 0x0C, 0x0C, 0x0C, 0x18, 0x156, 0x158};


sdm120ct::sdm120ct(modbusMaster *modbusMaestroPtr, const char *nombrePar, uint16_t dirPar)
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

void sdm120ct::addDs(uint16_t ds)
{
    for (uint8_t m=1;m<=numMedidas;m++)
    {
        if (dsDesdeUpdate[m] < dsUpdateMaxMed[m])
        {
            dsDesdeUpdate[m] += ds;
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
uint8_t sdm120ct::attachMedida(float *ptrMedPar, const char *tipoMedida, uint8_t dsUpdatePar, const char *descrPar)
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
            dsUpdateMaxMed[numMedidas] = dsUpdatePar;
            dsDesdeUpdate[numMedidas] = dsUpdatePar + 1;  // para que se lea inmediatamente al comienzo
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
//    uint16_t bytesReceived;
//    uint16_t msgCRC, rxCRC;
    uint8_t  *ptrChr;
//    uint8_t buffer[20];
    uint8_t bufferRx[20];

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
    modbusPtr->enviaMBfunc04(direccion, addressReg, 2, bufferRx, sizeof(bufferRx), 500, &msDelay, error);

//    buffer[0] = direccion;
//    buffer[1] = 0x04;
//    buffer[2] = (addressReg&0xFF00)>>8;
//    buffer[3] = addressReg&0xFF;
//    buffer[4] = 0;
//    buffer[5] = 2;
//
//    msgCRC = CRC16(buffer, 6);
//    buffer[6] = msgCRC & 0xFF;
//    buffer[7] = (msgCRC & 0xFF00) >>8;
//    bufferRx[0] = 0;
//    bufferRx[1] = 0;
//    modbusPtr->chprintStrRs485(buffer, 8, chTimeMS2I(100));
//    *error = modbus::chReadStrRs485(bufferRx, 9, &bytesReceived, chTimeMS2I(100));
//    if (*error!=0 || bytesReceived!=9)
//    {
//        *error = -1;
//        *valor = 0.0f;
//        chLcdprintfFila(2,"Error modbus     ",valor2);
//        return;
//    }
//    msgCRC = CRC16(bufferRx, bytesReceived-2);
//    rxCRC = (bufferRx[bytesReceived-1]<<8) + bufferRx[bytesReceived-2];
//    if (msgCRC!=rxCRC || direccion!= bufferRx[0] || bufferRx[1]!=0x04)
//    {
//        *error = -2;
//        *valor = 0.0f;
//        return;
//    }
    if (*error!=0)
        return;
    ptrChr = (uint8_t *)&valor2;
    *ptrChr++ = bufferRx[6];
    *ptrChr++ = bufferRx[5];
    *ptrChr++ = bufferRx[4];
    *ptrChr = bufferRx[3];
    *error = 0;
    *valor = valor2;
    chLcdprintfFila(2,"V: %.1f   ",valor2);
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
        if (dsDesdeUpdate[m-1] >= dsUpdateMaxMed[m-1])
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
                        *ptrMed[m-1] = 0.0f;
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
            *ptrMed[m-1] = valor;
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






