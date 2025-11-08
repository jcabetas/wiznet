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
#include "registros.h"

//uint16_t CRC16(const uint8_t *nData, uint16_t wLength);

extern holdingRegister *idMBVaconHR;


/*
 * En VACON 100, A negativo, B positivo
 * - Coil register (0001, function 1 read, 5 para write), se refiere a bits en Control word (p.e. RUN)
 * - Discrete input (10001, function 2), es un bit read-only (p.e. run)
 * - Input register (30001, function 4), read only
 * - Holding register (40001, function 3 read, 6 write single, 16 multiple write, 23 read y write) son read/write
 *
 * Los holding registers 40001 se leen con function 3 (16 para varios) en base 0 (HR 40108 se lee en 107)
 *  * Escribir speed reference: function 16 (write multiple registers), address 0x07D0 (2000, control word)
 * Leer speed: function 0x4 (read input registers), (process data out 2103=process data 42103 => leer 2102)
 *
 * Process Data In (function 16 para varios):
 * MODBUS ADDRESS
 * 2000: Run request
 * 2002: speed reference
 *
 * Process data out (function 4):
 * MODBUS ADDRESS
 * 2101: frequency 0.01 Hz
 * 2102: speed rpm (process data 42103)
 * 2103: motor current 0.1A
 * 2104: motor torque 0.1%
 * 2105: motor power 0.1%
 *
 * Posiblemente (address no modbus):
 * Analog input 1: 59 (%*0,01)
 * Idem An2: 60
 * PID out: 23 (%*0.01)
 * PID status: 24 (0: stopped, 1: running, 3: sleep, 4: in dead-band)
 *
 */

/*
 * Estado b1:preparado, b2:En marcha, b3:fallo
 */

//extern holdingRegister *vaconIdHR;
//extern uint16_t idVacon;


static const char *descMedidaVacon[] = {"Hz","rpm", "I", "P%" ,"AI2%","Estado","FaultCode"};
static const uint16_t addressVacon[] = {2102, 2103, 2104, 2106, 59,    42,      2109      };
static const float escalaVacon[] =     {0.01f,1.0f, 0.1f,0.1f ,0.01f,  1.0f,    1.0f       };


vacon::vacon(modbusMaster *modbusPtr, const char *nombrePar)
{
    modbusConectado = modbusPtr;
    strncpy(nombre,nombrePar,sizeof(nombre));
    numMedidas = 0;
    erroresSeguidos = 0;
    modbusPtr->addDisp(this);
};

vacon::~vacon()
{
}


const char *vacon::diTipo(void)
{
    return "vacon";
}

char *vacon::diNombre(void)
{
    return nombre;
}

int8_t vacon::init(void)
{
    // no necesita inicializacion
    return 0;
}

void vacon::addms(uint16_t ms)
{
    for (uint8_t m=1;m<=numMedidas;m++)
    {
        if (msDesdeUpdate[m] < msUpdateMaxMed[m])
        {
            msDesdeUpdate[m] += ms;
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
uint8_t vacon::attachMedida(float *ptrMedPar, const char *tipoMedida, uint16_t msUpdatePar, const char *descrPar)
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
        if (!strcasecmp(descMedidaVacon[tip],tipoMedida))
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
    chprintf((BaseSequentialStream *)&SD1,"Error: Vacon no tiene medida tipo '%s'\n",tipoMedida);
    return 0;
}

void vacon::leer(uint16_t *valorInt, uint16_t addressReg, uint8_t *error)
{
//    uint16_t bytesReceived;
//    uint16_t msgCRC, rxCRC;
//    uint8_t buffer[20];
    uint8_t bufferRx[30];

    /*
     * Example

    The following query will request speed at address 2103 (hay que meter 2102) with node address 1:

    Field Name Example(Hex)
    Slave Address 01
    Function 04
    Starting Address High 0x08
    Starting Address Low 0x36
    Number of Points High 00
    Number of Points Low 01
    Error Check Low 71
    Error Check High CB
     */
    uint8_t idVacon = idMBVaconHR->getValor();
//    buffer[0] =  idVacon;
//    buffer[1] = 0x04;
//    buffer[2] = (addressReg&0xFF00)>>8;
//    buffer[3] = addressReg&0xFF;
//    buffer[4] = 0;
//    buffer[5] = 1;

    modbusConectado->enviaMBfunc(4, idVacon, addressReg, 1, bufferRx, sizeof(bufferRx), 300, &msDelay, error);
//
//    msgCRC = CRC16(buffer, 6);
//    buffer[6] = msgCRC & 0xFF;
//    buffer[7] = (msgCRC & 0xFF00) >>8;
//    bufferRx[0] = 0;
//    bufferRx[1] = 0;
//    modbus::chprintStrRs485(buffer, 8, chTimeMS2I(100));
//    *error = modbus::chReadStrRs485(bufferRx, 7, &bytesReceived, chTimeMS2I(100));
//    if (*error!=0 || bytesReceived!=7)
//    {
//        *error = 1;
//        *valorInt = 0;
//        chLcdprintfFila(2,"Error modbus vacon");
//        return;
//    }
//    msgCRC = CRC16(bufferRx, bytesReceived-2);
//    rxCRC = (bufferRx[bytesReceived-1]<<8) + bufferRx[bytesReceived-2];
//    if (msgCRC!=rxCRC || idVacon!= bufferRx[0] || bufferRx[1]!=0x04)
//    {
//        *error = 2;
//        *valorInt = 0;
//        return;
//    }
    // en 3 y 4 esta el valor entero
    if (!error)
        *valorInt = (bufferRx[3]<<8) + bufferRx[4];
    else
        *valorInt = 0;
    return;
}

void vacon::leerTip(float *valor, uint8_t tipMedida, uint8_t *error)
{
    uint16_t valorInt;
    uint16_t addressReg = addressVacon[tipMedida];
    leer(&valorInt, addressReg, error);
    *valor = valorInt*escalaVacon[tipMedida];
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
uint8_t vacon::usaBus(void)
{
    float valor;
    uint8_t error;
    uint8_t heEnviado = 1;
    for (uint8_t m=1;m<=numMedidas;m++)
    {
        if (msDesdeUpdate[m-1] >= msUpdateMaxMed[m-1])
        {
            uint8_t tip = tipoMed[m-1];
            leerTip(&valor, tip, &error);
            heEnviado = 1;
            if (error!=0)
            {
                erroresSeguidos++;
                // vuelve a intentarlo
                leerTip(&valor, tip, &error);
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
            *ptrMed[m-1] = valor;
        }
    }
    if (heEnviado)
        return 0;
    else
        return 2;
}




