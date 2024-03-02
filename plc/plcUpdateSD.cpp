#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include <stdio.h>
#include "stdlib.h"
#include "string.h"
#include "tty.h"
#include "gets.h"
#include "bloques.h"
#include "dispositivos.h"
#include "ff.h"
#include "w25q16.h"
#include "nextion.h"
#include "radio.h"
#include "sms.h"

extern uint8_t hayCambios;
extern uint8_t fs_ready;
extern thread_t *procesoPlc;

void divideString(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[]);
void rtcGetTM(RTCDriver *rtcp, struct tm *tim, uint16_t *ds);
uint8_t escribePlc2SDLowLevel(BaseSequentialStream *tty, const uint8_t *nombFich);
uint8_t escribePlc2FlashLowLevel(BaseChannel *ttyBC, const char *nombFich);
void initPlc(BaseSequentialStream *tty);
uint8_t mataPlc(BaseSequentialStream *tty);
void borraDatos(void);

extern "C"
{
  void initNextion(void);
  void initSerial(void);
}

uint8_t trataBloque(BaseSequentialStream *tty,char *buffer, uint16_t numLinea);


uint8_t leePlcSD(BaseSequentialStream *tty,const char *nomFich)
{
    uint8_t hayError;
    uint16_t numLinea;
    char buffer[250];
    FIL fich;
    // abro fichero
    if (!fs_ready)
    {
        nextion::enviaLog(tty,"No hay SD");
        return 1;
    }
    if (f_open(&fich, nomFich, FA_READ|FA_OPEN_EXISTING) != FR_OK)
    {
        chsnprintf(buffer,sizeof(buffer),"Error al abrir %s",nomFich);
        nextion::enviaLog(tty,buffer);
        return 2;
    }
    parametro::deleteAll();
    parametroFlash::deleteAll();
    bloque::deleteAll();
    campoNextion::deleteAll();
    nombres::init();
    estados::init();
    hayError = 0;
    numLinea = 0;
    nextion::hayActividad();

    while (f_gets(buffer, sizeof(buffer), &fich) != NULL && !hayError)
    {
        hayError = trataBloque(tty, buffer, ++numLinea);
    }
    f_close(&fich);
    if (hayError)
    {
        nextion::enviaLog(tty,"ABORTADO por errores");
        estados::printSize(tty);
        bloque::printBloques(tty);
        return 3;
    }
    nextion::enviaLog(tty,"Terminado de leer");
    //bloque::printBloques(tty);

    if (hayError)
    {
        nextion::enviaLog(tty,"Abortado por errores");
        if (tty != NULL)
        {
            estados::printSize(tty);
            bloque::printBloques(tty);
        }
        return 5;
    }
    if (tty!=NULL)
    {
        chprintf(tty, "Terminado de leer\r\n");
        parametro::printAll(tty);
        estados::printSize(tty);
        bloque::printBloques(tty);
    }
    return 0;
}


uint8_t leePlcFlash(BaseSequentialStream *tty, uint8_t sector)
{
    uint16_t varCheck, page, longPLC, posFlash, posBuff, numLinea;
    uint8_t  pageAddress, byte;
    uint8_t  hayError;
    char buffer[250];
    if (sector>1)
     {
         nextion::enviaLog(tty,"Error: sector PLC>1 ??");
         return 1;
     }
     page = 16*sector;
     varCheck = W25Q16_read_u16(page, 0);
     if (varCheck==0xFFFF)
     {
         nextion::enviaLog(tty,"sector sin datos PLC");
         return 2;
     }
     if (varCheck!=0x1234)
     {
         nextion::enviaLog(tty,"Sector PLC no inicializado");
         return 3;
     }
     longPLC = W25Q16_read_u16(page, 2);
     if (tty!=NULL)
         chprintf(tty,"PLC#%d.  #bytes:%d\n\r", sector, longPLC);
     if (longPLC==0)
         return 4;
     pageAddress = 4;
     nombres::init();
     estados::init();
//     borraDatos();
     hayError = 0;
     posFlash = 0;
     posBuff = 0;
     numLinea = 0;
     nextion::hayActividad();
     W25Q16_initStreamRead(page, pageAddress);
     while (!hayError && ++posFlash<=longPLC)
     {
         byte = W25Q16_streamRead(&page, &pageAddress);
         buffer[posBuff] = byte;
         if (++posBuff>=sizeof(buffer))
         {
             hayError = 1;
             nextion::enviaLog(tty,"Entrada muy larga");
             break;
         }
         if (byte==0 || posFlash>=longPLC)
         {
             numLinea++;
             W25Q16_closeStreamRead();
             if (byte!=0)
                 buffer[posBuff] = 0;
             hayError = trataBloque(tty, buffer, numLinea);
             W25Q16_initStreamRead(page, pageAddress);
             posBuff = 0;
         }
     }
     W25Q16_closeStreamRead();
     if (hayError)
     {
         nextion::enviaLog(tty,"** Abortado por errores");
         estados::printSize(tty);
         bloque::printBloques(tty);
         return 5;
     }
     if (tty!=NULL)
         chprintf(tty, "Terminado de leer\r\n");
     if (tty!=NULL)
     {
         parametro::printAll(tty);
         estados::printSize(tty);
         bloque::printBloques(tty);
     }
     return 0;
}



/*
 * Si hay SD:
 * - Si hay PLC en flash:
 *   * verifica que esta volcado en SD (a plcArranque.txt)
 *   * si no está o difiere, escribela
 * - Si existe plc.txt en SD
 *   * Para PLC
 *   * Arranca desde SD y verifica si no hay errores
 *   * Si no hay errores:
 *     a) graba plc actual en plcPrevio.txt
 *     b) graba plc.txt en flash
 *     c) vuelca flash a plcArranque.txt
 */
uint8_t checkSD(BaseSequentialStream *tty)
{
    char buffer[120];
    FILINFO finfo;
    FRESULT fr;
    // abro fichero
    if (!fs_ready)
    {
        nextion::enviaLog(tty,"Info: no hay SD");
        return 1;
    }
    fr = f_stat("plcArranque.txt", &finfo);
    if (fr != FR_OK)
    {
        chsnprintf(buffer,sizeof(buffer),"No existe plcArranque.txt, lo genero");
        nextion::enviaLog(tty,buffer);
        uint8_t error = escribePlc2SDLowLevel(tty, (const uint8_t *) "plcArranque.txt");
        if (error!=0)
        {
            chsnprintf(buffer,sizeof(buffer),"Error %d escribiendo plcArranque.txt", error);
            nextion::enviaLog(tty,buffer);
        }
    }
    fr = f_stat("plc.txt", &finfo);
    if (fr != FR_OK)
    {
        chsnprintf(buffer,sizeof(buffer),"info: no existe plc.txt en SD para actualizar");
        nextion::enviaLog(tty,buffer);
        return 3;
    }
    else
    {
        // ejecuta plc.txt de la tarjeta SD para ver si es valida
        // por si estaba arrancado...
        //mataPlc(tty);
        borraDatos();
        uint8_t error = leePlcSD(tty,(const char *)"plc.txt");
        if (error!=0)
        {
            chsnprintf(buffer,sizeof(buffer),"Error %d plc.txt de SD, no grabamos a flash");
            nextion::enviaLog(tty,buffer);
            borraDatos();
            return 4;
        }
        nextion::enviaLog(tty,"Fichero plc.txt sin errores");
        error = bloque::initBloques();
        if (error!=0)
        {
            nextion::enviaLog(tty,"Error al inicializar");
            borraDatos();
            return 5;
        }
        // plc.txt valido, grabamos a flash
        error = escribePlc2FlashLowLevel((BaseChannel *)tty, "plc.txt");
        if (error!=0)
        {
            chsnprintf(buffer,sizeof(buffer),"Error al grabar a flash");
            nextion::enviaLog(tty,buffer);
            borraDatos();
            return 6;
        }
        chThdSleepMilliseconds(500);
        // ha ido bien, borramos de la SD para evitar escribir cada vez que arranca
        f_unlink("plc.txt");
        // borramos datos
        borraDatos();
        // arranca procesos parados
        initSerial();
        chThdSleepMilliseconds(500);
    }
    return 0;
}


