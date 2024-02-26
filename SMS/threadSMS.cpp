/*
 * threadSMS.cpp
 *
 *  Created on: 5 feb. 2021
 *      Author: joaquin
 */

#include "ch.h"
#include "hal.h"

#include "chprintf.h"
#include "tty.h"
#include "gets.h"
#include "string.h"
#include "sms.h"
#include "calendar.h"

#include "string.h"
#include "stdlib.h"
#include "lcd.h"
#include "jaula.h"

thread_t *procesoSMS;
event_source_t enviarSMS_source;

void leeTension(float *vBat);

extern "C"{
    uint8_t okSMS(void);
}

uint8_t okSMS(void)
{
    return sms::diSmsReady();
}

/*
 * Proceso SMS
 * Escucha para eventos de enviar SMS, y de vez en cuando busca SMS y la fecha del sistema y estado de comunicaciones
 * El SMS a enviar esta definido en clase "sms" que se pasa por parametro
 */


static THD_WORKING_AREA(waSMS, 2024);

static THD_FUNCTION(threadSMS, arg) {
    (void) arg;
    eventmask_t evt;
    eventflags_t flags;
    uint8_t huboTimeout;
    int16_t dsQuery; // contador para ver estados

    char bufferSendSMS[200], buffer[30];
    uint32_t posSMS;
    time_t ultAjusteHora = 0, ultQuerySMS = 0;

    sms::borraMsgRespuesta();
    dsQuery = -200; // los primeros 20+10 segundos dejo soltar mensajes a SIM800L, y no le hago preguntas

    uint8_t hayError, numParametros, *parametros[10];
    event_listener_t smsSend_listener, receivedData;
    chRegSetThreadName("SMS");

    // prueba dormir y despertar
    sms::sleep();

    chEvtRegisterMask(&enviarSMS_source, &smsSend_listener,EVENT_MASK(0));
    chEvtRegisterMaskWithFlags (chnGetEventSource((SerialDriver *)sms::pSD),&receivedData, EVENT_MASK (1),CHN_INPUT_AVAILABLE);

    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
        {
            chEvtUnregister(&enviarSMS_source, &smsSend_listener);
            chEvtUnregister(chnGetEventSource((SerialDriver *)sms::pSD),&receivedData);//SerialDriver *
            sms::callReady = 0;
            sms::smsReadyVal = 0;
            sms::pinReady = 0;
            sms::estadoCREG = 0;
            sms::proveedor[0] =0;
            chThdExit((msg_t) 1);
        }
        if (evt & EVENT_MASK(0)) // Evento enviar SMS
        {
            sms::despierta();
            sms::sendSMS();
            sms::sleep();
            continue;
        }
        if (evt & EVENT_MASK(1)) // Algo ha entrado por la SIM
        {
            flags = chEvtGetAndClearFlags(&receivedData);
            if (flags & CHN_INPUT_AVAILABLE)
            {
                chgetsNoEchoTimeOut((BaseChannel *)sms::pSD, (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS), TIME_MS2I(100),&huboTimeout);
                if (huboTimeout)
                    goto continuaRx;
                // si es una linea en blanco, lee la siguiente
                if (bufferSendSMS[0]==0)
                {
                    chgetsNoEchoTimeOut((BaseChannel *)sms::pSD, (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS), TIME_MS2I(100),&huboTimeout);
                    if (huboTimeout)
                        goto continuaRx;
                }
                // +CMTI: "SM",5
                if (!strncmp(bufferSendSMS,"+CMTI: ",6))
                {
                    chEvtUnregister(chnGetEventSource((SerialDriver *)sms::pSD), &receivedData);
                    Str2Int((uint8_t *)&bufferSendSMS[12], &posSMS);
                    sms::despierta();
                    sms::leoSmsCMTI();
                    sms::sleep();
                    chEvtRegisterMaskWithFlags (chnGetEventSource ((SerialDriver *)sms::pSD),&receivedData, EVENT_MASK (1),CHN_INPUT_AVAILABLE);
                }
                // +CMT: "+34619262851","19/12/05,21:12:30+04"
                else if (!strncmp(bufferSendSMS,"+CMT: ",6))
                {
                    chEvtUnregister(chnGetEventSource((SerialDriver *)sms::pSD), &receivedData);
                    sms::despierta();
                    sms::leoSMS(bufferSendSMS,sizeof(bufferSendSMS));
                    sms::sleep();
                    chEvtRegisterMaskWithFlags (chnGetEventSource ((SerialDriver *)sms::pSD),&receivedData, EVENT_MASK (1),CHN_INPUT_AVAILABLE);
                }
                else if (!strncmp(bufferSendSMS,"Call Ready",sizeof(bufferSendSMS)))
                {
                    if (sms::callReady != 1)
                        sms::callReady = 1;
                }
                else if (!strncmp(bufferSendSMS,"SMS Ready",sizeof(bufferSendSMS)))
                {
                    if (sms::smsReadyVal != 1)
                    {
                        sms::smsReadyVal = 1;
                    }
                }
                //+CPIN: READY
                else if (!strncmp(bufferSendSMS,"+CPIN:",6))
                {
                    if (!strncmp(bufferSendSMS,"+CPIN: READY",12))
                    {
                        if (sms::pinReady != 1)
                        {
                            sms::pinReady = 1;
                        }
                    }
                    else
                        sms::pinReady = 0;
                }
                else if (!strncmp(bufferSendSMS,"+CREG: ",7))
                {
                    uint8_t newEstadoCREG = bufferSendSMS[7]-'0';
                    if (sms::estadoCREG != newEstadoCREG)
                    {
                        sms::estadoCREG = newEstadoCREG;
                    }
                }
            }
            continuaRx:
            continue;
        }
        // Actualizo datos cada 20 segundos
        if (++dsQuery>=200)
        {
            chEvtUnregister(chnGetEventSource((SerialDriver *)sms::pSD),&receivedData);
            sms::despierta();
            // miro tension
            hayError = modemParametros((BaseChannel *)sms::pSD,"AT+CBC\r\n","+CBC:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
            if (!hayError && numParametros==3)
            {
                sms::porcBateriaSIM800L = atoi((char *) parametros[1]);
                sms::vBat =  atof((char *) parametros[2])/1000.0f;
                if (sms::porcBateriaSIM800L<30 && !sms::bateriaBajaAvisada)
                {
                    // envia mensaje de aviso
                    sms::avisaBateriaBaja();
                }
            }
            // comprueba conexion
            hayError = modemParametros((BaseChannel *)sms::pSD,"AT+CREG?\r\n","+CREG:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(5000),&numParametros, parametros);
            uint8_t newEstadoCREG = bufferSendSMS[7]-'0';
            // si ha cambiado el estado, actualiza Nextion
            if (newEstadoCREG != sms::estadoCREG)
            {
                sms::estadoCREG = newEstadoCREG;
                if (sms::estadoCREG != 1)
                {
                    sms::proveedor[0] = 0;
                }
            }
            time_t unix_time = calendar::GetTimeUnixSec();
            if (sms::estadoCREG != 1 )
            {
#ifdef LCD
                chsnprintf(buffer,sizeof(buffer),"%.3fV %d%% SIN CONEXION",sms::vBat,sms::porcBateriaSIM800L);
                ponEnColaLCD(0,buffer);
                ponEnColaLCD(1,"");
#endif
#ifdef SSD1306
                chsnprintf(buffer,sizeof(buffer),"%3d%%B GSM? ",sms::porcBateriaSIM800L,sms::porcBateriaSIM800L);
                ponEnColaLCD(0,buffer);
#endif
                // si llevo sin conexion mas de 3 minutos, reseteo
                if (unix_time-sms::tiempoLastConexion > 180)
                {
                    sms::tiempoLastConexion = unix_time;
                    sms::initSIM800SMS();
                }
            }
            else
            {
                // sigue habiendo conexion
                sms::tiempoLastConexion = unix_time;
                // miro si hay mensajes cada 10s
                if (unix_time-ultQuerySMS > 10)
                {
                    ultQuerySMS = unix_time;
                    sms::leoSmsCMTI();
                }
                // ajusta hora cada semana
                if (unix_time-ultAjusteHora > 604800)
                {
                    // leo hora
                    sms::ponHoraConGprs();
                    ultAjusteHora = calendar::GetTimeUnixSec();
                }
                // leo proveedor, si no lo tenia claro
                if (!sms::proveedor[0])
                {
                    hayError = modemParametros((BaseChannel *)sms::pSD,"AT+COPS?\r\n","+COPS:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(5000),&numParametros, parametros);
                    //+COPS: 0,0,"vodafone"
                    if (numParametros>=3 && strlen((char *)parametros[2])>3)
                    {
                        strncpy(sms::proveedor,(char *)parametros[2],sizeof(sms::proveedor)-1);
                    }
                }
                // Aprovecho para preguntar RSSI
                hayError = modemParametros((BaseChannel *)sms::pSD,"AT+CSQ\r\n","+CSQ:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(5000),&numParametros, parametros);
                if (hayError==0 && numParametros>=1)
                {
                    sms::rssiGPRS = lee2car(parametros[0],0,0,99,&hayError);
                }
                //at+cbc =>    +CBC: 0,92,4136 0, %carga y mV
                hayError = modemParametros((BaseChannel *)sms::pSD,"AT+CBC\r\n","+CBC:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
                if (!hayError && numParametros==3)
                {
                    sms::porcBateriaSIM800L = atoi((char *) parametros[1]);
                    sms::vBat =  atof((char *) parametros[2])/1000.0f;
                }
#ifdef LCD
                if (sms::rssiGPRS==99)
                    chsnprintf(buffer,sizeof(buffer),"%.3fV %d%%",sms::vBat,sms::porcBateriaSIM800L);
                else
                    chsnprintf(buffer,sizeof(buffer),"%.3fV %d%% %c%d/30",sms::vBat,sms::porcBateriaSIM800L,(uint8_t)(1+(7*sms::rssiGPRS)/26),sms::rssiGPRS);
                ponEnColaLCD(0,buffer);
                ponEnColaLCD(1,sms::proveedor);
#endif
#ifdef SSD1306
                if (sms::rssiGPRS==99)
                    chsnprintf(buffer,sizeof(buffer),"%d%%B NO GSM",sms::porcBateriaSIM800L);
                else
                    chsnprintf(buffer,sizeof(buffer),"%3d%%B %3d%%G",sms::porcBateriaSIM800L,(uint16_t)(sms::rssiGPRS/.3333f));
                ponEnColaLCD(0,buffer);
                ponEnColaLCD(2,sms::proveedor);
#endif
                // miro si hay mensajes
//                modemOrden((BaseChannel *)sms::pSD, "at+cnmi=1,2,0,0,0\r\n", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS), TIME_MS2I(5000));
            }
            dsQuery = 0;
            sms::sleep();
            chEvtRegisterMaskWithFlags (chnGetEventSource((SerialDriver *)sms::pSD),&receivedData, EVENT_MASK (1),CHN_INPUT_AVAILABLE);
        }
    }
}


void sms::initThreadSMS()
{
    if (!procesoSMS)
        procesoSMS = chThdCreateStatic(waSMS, sizeof(waSMS), NORMALPRIO, threadSMS, NULL);
}



void sms::mataSMS(void)
{
    if (procesoSMS!=NULL)
    {
        chThdTerminate(procesoSMS);
        chThdWait(procesoSMS);
        procesoSMS = NULL;
    }
}
