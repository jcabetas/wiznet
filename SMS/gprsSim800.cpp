/*
 * gprsSim800.cpp
 *
 *  Created on: 7 mar. 2020
 *      Author: joaquin
 */
#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "comunicaciones.h"


uint8_t conectarSim800TCPMqqt(BaseChannel  *pSD, uint8_t useSSL, uint8_t verbose)
{
    uint8_t hayError, numParametros, *parametros[10];
    char buffer[80];
    mqqtOk = 0;
    // configuro con SSL
    if (useSSL)
    {
        hayError = modemParametro(pSD,"AT+CIPSSL=1\n", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(2000));
        if (verbose && hayError==1)
        {
            msgParaLCD("No se activa SSL!",1000);
        }
    }
    // me responde a veces PDP DEACT\r\n\r\nCONNECT FAIL
    chsnprintf(buffer,sizeof(buffer),"AT+CIPSTART=\"TCP\",\"%s\",\"%d\"\r\n",mqttServerValor(),mqqtPortValor());
    hayError = modemParametros(pSD,buffer,"CONNECT", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    if (hayError!=0 || (numParametros>=1 && strcmp((char *)parametros[0],"FAIL")))
    {
        if (verbose)
            msgParaLCD("Error al conectar!  ", 1000);
        return 8;
    }
    else
    {
        if (verbose)
        {
            msgParaLCD("Conectado a MQTT", 500);
        }
    }
    mqqtOk = 1;
    return 0;
}


uint8_t initSIM800GPRS(BaseChannel  *pSD, uint8_t verbose)
{
    uint8_t hayError, numParametros, *parametros[10], numTry;
    char buffer[80];
    char IPMAIN[20];

    gprsOk = 0;
    reseteaGPRS(pSD, verbose);
    // Esta registrado?
    numTry = 0;
    do
    { // responde CREG: 0,1
        hayError = modemParametros(pSD,"AT+CREG?\r\n","+CREG:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(3000),&numParametros, parametros);
        if (hayError==0 && numParametros>=2 && strcmp((char *)parametros[1],"1")==0)
            break;
        if (numTry++>3)
        {
            if (verbose)
                msgParaLCD("GPRS no se registra!",2000);
            return 2;
        }
    } while (numTry<3);

    // Esta activo GPRS?
    numTry = 0;
    do
    {
        // debe responder +CGATT: 1
        hayError = modemParametros(pSD,"AT+CGATT?\r\n","+CGATT:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(3000),&numParametros, parametros);
        if (hayError==0 && strcmp((char *)parametros[0],"1")==0)
            break;
        if (numTry++>3)
        {
            if (verbose)
                msgParaLCD("GPRS no activo!",2000);
            return 3;
        }
    } while (numTry<3);

    // contesta IP INITIAL
    hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    hayError = modemOrden(pSD,"AT+CIPMODE=1\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(1000));
    if (hayError!=0)
    {
        if (verbose)
            msgParaLCD("Error CIPMODE!",2000);
        return 4;
    }

    hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);

    chsnprintf(buffer,sizeof(buffer),"AT+CSTT=\"%s\"\r\n", apnValor());
    hayError = modemOrden(pSD,buffer, bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(2000));

    hayError = modemParametros(pSD,"AT+CSTT?\r\n","+CSTT:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(2000),&numParametros, parametros);
    if (hayError!=0)
    {
        if (verbose)
            msgParaLCD("Error APN",2000);
        return 5;
    }

    // contesta IP START
    hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    hayError = modemParametro(pSD,"AT+CIICR\r\n",  bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(4000));
    if (hayError!=0)
    {
        if (verbose)
            msgParaLCD("No se activa GPRS!",2000);
        return 6;
    }
    // contesta IP GPRSACT... pero PRIMERO IP START... hay que ir preguntando
    // quizas sobra
    numTry = 0;
    do
    {
        hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(2000),&numParametros, parametros);
        if (hayError==0 && strcmp((char *)parametros[0],"IP GPRSACT")==0)
            break;
        osalThreadSleepMilliseconds(50);
        if (++numTry>20)
        {
            if (verbose)
                msgParaLCD("No se activa GPRS!",2000);
            return 7;
        }
    } while (1==1);

    hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    hayError = modemParametro(pSD,"AT+CIFSR\n", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000));
    if (hayError==0)
    {
        if (verbose && strlen((char *)bufferGetsGPRS)>0)
        {
            msgParaLCD((char *)bufferGetsGPRS,1000);
            //        chLcdprintfFila(2,"IP %s",bufferGetsGPRS);
        }
    }
    else
    {
        if (verbose)
            msgParaLCD("No se activa GPRS!",2000);
        return 7;
    }

    // responde IP STATUS
    hayError = modemParametros(pSD,"AT+CIPSTATUS\r\n","STATE: ", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    chsnprintf(buffer,sizeof(buffer),"AT+CDNSGIP=%s\r\n", mqttServerValor());
    hayError = modemParametros(pSD,buffer,"+CDNSGIP:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(5000),&numParametros, parametros);
    if (hayError==0)
    {
        if (verbose && numParametros>=3 && parametros[2]!=NULL)
        {
            msgParaLCD((char *)parametros[2],1000);
            //      chLcdprintfFila(3,"MQTT %15s",parametros[2]);
            strncpy(IPMAIN, (char *)parametros[2],sizeof(IPMAIN));
        }
    }
    else
    {
        if (verbose)
            msgParaLCD("Error CDNS",1000);
    }
    gprsOk = 1;
    return 0;
}

