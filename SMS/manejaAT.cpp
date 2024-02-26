/*
 * manejaAT.cpp
 *
 *  Created on: 7 mar. 2020
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
#include "string.h"
#include "chprintf.h"
#include "calendar.h"
#include "sms.h"
#include "gets.h"


void limpiaSerie(BaseChannel  *pSD, uint8_t *buffer, uint16_t bufferSize)
{
    uint8_t huboTimeout;
    do
    {
        chgetsNoEchoTimeOut(pSD,  buffer, bufferSize, TIME_MS2I(10),&huboTimeout);
    } while (huboTimeout!=1);
}


// envia orden al modem, esperando OK o ERROR
uint8_t modemOrden(BaseChannel  *pSD, const char *orden, uint8_t *buffer, uint8_t bufferSize, sysinterval_t timeout)
{
    uint8_t *posCadena,huboTimeout;
    limpiaSerie(pSD, buffer, bufferSize);
    //  chnWrite(pSD, (uint8_t *)orden, 1);
    chprintf((BaseSequentialStream  *) pSD, orden);
    while (1==1)
    {
        chgetsNoEchoTimeOut(pSD,buffer,bufferSize, timeout,&huboTimeout);
        if (huboTimeout==1)
        {
            //timeout
            return 1;
        }
        posCadena = (uint8_t *) strstr((char *)buffer, "OK");
        if (posCadena!=NULL)
            return 0;
        posCadena = (uint8_t *) strstr((char *)buffer, "ERROR");
        if (posCadena!=NULL)
            return 2;
    }
}



// envia orden al modem, esperando un parametro de respuesta
uint8_t modemParametro(BaseChannel  *pSD, const char *orden,  uint8_t *buffer, uint16_t bufferSize, sysinterval_t timeout)
{
    uint8_t huboTimeout;
    limpiaSerie(pSD, buffer, bufferSize);
    chprintf((BaseSequentialStream  *) pSD, orden);
    // echo de la orden
    chgetsNoEchoTimeOut(pSD, buffer, bufferSize, timeout,&huboTimeout);
    // parametro de respuesta
    chgetsNoEchoTimeOut(pSD, buffer, bufferSize, timeout,&huboTimeout);
    if (huboTimeout==1)
        return 1;
    else
        return 0;
}


void buscaParametros(uint8_t *posCadena, uint8_t *numParametros,uint8_t *parametros[])
{
    uint8_t parametroTipoString;
    // empiezo a buscar parametros
    *numParametros = 0;
    while (1==1)
    {
        if (*posCadena==0)
            break;
        // salta espacios
        while (*posCadena == ' ' || *posCadena==',')
        {
            posCadena++;
            if (*posCadena==0)
                break;
        }
        if (*posCadena=='"')
        {
            parametroTipoString = 1; // string
            posCadena++;
        }
        else
            parametroTipoString = 0; // numero
        parametros[*numParametros] = (uint8_t *) posCadena;
        (*numParametros)++;

        while (*posCadena!=0 && ((parametroTipoString && *posCadena!='"') ||
                (!parametroTipoString && (*posCadena!=','))))
            //          (!parametroTipoString && *posCadena>='0' && *posCadena<='9')))
        {
            posCadena++;
        }
        // terminamos de identificar el parametro
        if (*posCadena==0)  // si es el final de la cadena
            break;            // terminamos
        else
        {
            *posCadena = 0;  // marcamos final de parametro
            posCadena++;     // y saltamos
        }
    }
}


// hay que detectar fin de linea!!
uint16_t modemParametros(BaseChannel  *pSD, const char *orden, const char *cadRespuesta,  uint8_t *buffer, uint16_t bufferSize, sysinterval_t timeout,
                         uint8_t *numParametros,uint8_t *parametros[])
{
    uint8_t *posCadena,huboTimeout;
    limpiaSerie(pSD, buffer, bufferSize);
    *numParametros = 0;
    chprintf((BaseSequentialStream  *)pSD, orden);
    while (1==1)
    {
        chgetsNoEchoTimeOut(pSD, buffer, bufferSize, timeout,&huboTimeout);
        if (huboTimeout==1)
        {
            //timeout
            return 1;
        }
        posCadena = (uint8_t *) strstr((char *)buffer, (char *)cadRespuesta);
        if (posCadena!=NULL)
        {
            posCadena += strlen(cadRespuesta);
            buscaParametros(posCadena, numParametros, parametros);
            limpiaBuffer(pSD);
            return 0;
        }
    }
}



