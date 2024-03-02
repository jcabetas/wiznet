/*
 * leeEscribePlc.cpp
 *
 *  Created on: 31 jul. 2020
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include <stdio.h>
#include "stdlib.h"
#include "string.h"
#include "tty.h"
#include "gets.h"
#include "ff.h"
#include "w25q16.h"
#include "varsFlash.h"
#include "nextion.h"


extern uint8_t fs_ready;
void initFlash(void);

uint8_t escribePlc2FlashLowLevel(BaseChannel *ttyBC, const char *nombFich)
{
    char  buffer[250];
    uint16_t page;
    uint8_t pageAddress, slotPlc;
    FIL fich;
    FILINFO finfo;
    FRESULT fr;
    uint16_t numLineas;
    BaseSequentialStream *tty = (BaseSequentialStream *) ttyBC;
    if (!fs_ready)
    {
        nextion::enviaLog(tty,"No hay SD");
        return 1;
    }
    slotPlc = 0;
    fr = f_stat((const char *)nombFich, &finfo);
    switch (fr) {
    case FR_OK:
        chsnprintf(buffer,sizeof(buffer),"Size: %d  %d/%02d/%02d, %d:%02d",(uint32_t) finfo.fsize, (finfo.fdate >> 9) + 1980, finfo.fdate >> 5 & 15, finfo.fdate & 31,
                   finfo.ftime >> 11, finfo.ftime >> 5 & 63);
        nextion::enviaLog(tty,buffer);
//
//        chprintf(tty,"Size: %lu\r\n", );
//        chprintf(tty,"Timestamp: %u/%02u/%02u, %02u:%02u\r\n",
//               (finfo.fdate >> 9) + 1980, finfo.fdate >> 5 & 15, finfo.fdate & 31,
//               finfo.ftime >> 11, finfo.ftime >> 5 & 63);
//        chprintf(tty,"Attributes: %c%c%c%c%c\r\n",
//               (finfo.fattrib & AM_DIR) ? 'D' : '-',
//               (finfo.fattrib & AM_RDO) ? 'R' : '-',
//               (finfo.fattrib & AM_HID) ? 'H' : '-',
//               (finfo.fattrib & AM_SYS) ? 'S' : '-',
//               (finfo.fattrib & AM_ARC) ? 'A' : '-');
        break;
    case FR_NO_FILE:
        nextion::enviaLog(tty,"No existe");
        return 2;
    default:
        chsnprintf(buffer,sizeof(buffer),"Error en f_stat (%d)", fr);
        nextion::enviaLog(tty,buffer);
        return 3;
    }
    if (finfo.fsize>((SECTORINICIALVARS-1)*16*256-4))
    {
        chsnprintf(buffer,sizeof(buffer),"Error, demasiado grande");
        nextion::enviaLog(tty,buffer);
        return 4;
    }
    if (f_open(&fich, (const char *) nombFich, FA_READ|FA_OPEN_EXISTING) != FR_OK)
    {
        chsnprintf(buffer,sizeof(buffer),"Error al abrir '%s'",nombFich);
        nextion::enviaLog(tty,buffer);
        return 5;
    }
    page = 16*slotPlc;
    pageAddress = 4; // empiezan los datos en byte 4 (0x1234+long (u16))
    for (uint8_t s=0;s<SECTORINICIALVARS;s++)
        W25Q16_sectorErase(s*16);
    W25Q16_write_u16(16*slotPlc, 0, 0x1234); // variable de control

    W25Q16_initStreamWrite(page, pageAddress);
    uint16_t longPlc = 0;
    numLineas = 0;
    while (f_gets(buffer, sizeof(buffer), &fich) != NULL)
    {
        for (uint16_t i=0;i<strlen(buffer);i++)
        {
            if (buffer[i]!='\n' && buffer[i]!='\r')
            {
                W25Q16_streamWrite(&page, &pageAddress, buffer[i]);
                longPlc++;
            }
        }
        numLineas++;
        W25Q16_streamWrite(&page, &pageAddress, 0); // fin de string
        longPlc++;
        chThdSleepMilliseconds(20); // no entiendo porque, si no existe no escribe todo el fichero
    }
    f_close(&fich);
    W25Q16_closeStreamWrite();
    W25Q16_write_u16(16*slotPlc, 2, longPlc);      // longitud del testo
    chsnprintf(buffer,sizeof(buffer),"Grabados %d bytes (%d lineas) en flash",longPlc,numLineas);
    nextion::enviaLog(tty,buffer);
    return 0;
}


uint8_t escribePlc2Flash(BaseChannel *ttyBC)
{
    char nombFich[30];
    BaseSequentialStream *tty = (BaseSequentialStream *) ttyBC;
    if (!fs_ready)
    {
        chprintf(tty,"No hay SD\r\n");
        return 1;
    }
    chprintf(tty,"Nombre de fichero plc:");
    chgets(ttyBC, (uint8_t *)nombFich,sizeof(nombFich));
    return escribePlc2FlashLowLevel(ttyBC, (const char *)nombFich);
}



// ToDo: poner mutex para acceso a SD
uint8_t escribePlc2SDLowLevel(BaseSequentialStream *tty, const uint8_t *nombFich)
{
    uint16_t page, longFlash;
    uint8_t pageAddress;
    char buffer[250];
    FIL fich;
    UINT bw;         /* File write count */
    if (!fs_ready)
    {
        nextion::enviaLog(tty,"No hay SD");
        return 1;
    }
    uint16_t slotPlc = 0;
    pageAddress = 0;
    if (W25Q16_read_u16(16*slotPlc, 0)!=0x1234)
    {
        nextion::enviaLog(tty,"Error, slot no inicializado");
        initFlash();
        nextion::enviaLog(tty,"Inicializo flash");
        if (W25Q16_read_u16(16*slotPlc, 0)!=0x1234)
            return 2;
    }
    longFlash = W25Q16_read_u16(16*slotPlc, 2);
    if (longFlash==0)
    {
        chsnprintf(buffer,sizeof(buffer),"Error, slot %d sin datos",slotPlc);
        nextion::enviaLog(tty,buffer);
        return 3;
    }
    if (longFlash==0xFFFF)
    {
        chsnprintf(buffer,sizeof(buffer),"Error, slot %d no inicializado",slotPlc);
        nextion::enviaLog(tty,buffer);
        return 4;
    }
    if (f_open(&fich, (const char *) nombFich, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        chsnprintf(buffer,sizeof(buffer),"Error al abrir '%s'",nombFich);
        nextion::enviaLog(tty,buffer);
        return 5;
    }
    page = 16*slotPlc;
    pageAddress = 4; // empiezan los datos en byte 4 (0x1234+long (u16))
    uint16_t longEscrito = 0;
    uint8_t error = 0;
    W25Q16_initStreamRead(page, pageAddress);
    for (uint16_t i=0;i<longFlash;i++)
    {
        uint8_t byteFlash = W25Q16_streamRead(&page, &pageAddress);
        if (byteFlash==0)
        {
            f_write(&fich,"\r\n",2,&bw);
            longEscrito += bw;
            if (bw < 2)
            {
                error = 6;
                break; /* error or disk full */
            }
        }
        else
        {
            f_write(&fich,(const void *)&byteFlash,1,&bw);
            longEscrito += bw;
            if (bw < 1)
            {
                error = 6;
                break; /* error or disk full */
            }
        }
    }
    f_close(&fich);
    W25Q16_closeStreamRead();
    if (!error)
        chsnprintf(buffer,sizeof(buffer),"Grabados %d bytes en %s\r\n",longEscrito,nombFich);
    else
        chsnprintf(buffer,sizeof(buffer),"Error grabando (llevaba %d bytes) en %s\r\n",longEscrito,nombFich);
    nextion::enviaLog(tty,buffer);
    return 0;
}

uint8_t escribePlc2SD(BaseChannel *ttyBC)
{
    char nombFich[30], buffer[250];
    FRESULT fr;
    FILINFO finfo;
    BaseSequentialStream *tty = (BaseSequentialStream *) ttyBC;
    if (!fs_ready)
    {
        nextion::enviaLog(tty,"No hay SD");
        return 1;
    }
    chprintf(tty,"Nombre de fichero plc:");
    chgets(ttyBC, (uint8_t *)nombFich,sizeof(nombFich));
    chprintf(tty,"\r\n");
    fr = f_stat((const char *)nombFich, &finfo);
    if (fr==FR_OK)
    {
        chprintf(tty,"Existe fichero %s, lo sobreescribo? (no/si):", nombFich);
        chgets(ttyBC, (uint8_t *)buffer,sizeof(buffer));
        chprintf(tty,"\r\n");
        if (strcasecmp(buffer,"si"))
            return 4;
    }
    return escribePlc2SDLowLevel(tty, (const uint8_t *) nombFich);
}

