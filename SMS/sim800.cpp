/*
 * sim900.c
 *
 *  Created on: 5/1/2017
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;


#include "chprintf.h"
#include "tty.h"
#include "gets.h"
#include "string.h"
#include "stdlib.h"
#include "sms.h"
#include "time.h"
#include "calendar.h"
#include "lcd.h"
#include "colas.h"
#include "jaula.h"

//extern event_source_t updateLCD_source;
//extern struct queu_t colaMsgLcd;

time_t GetTimeUnixSec(void);
void limpiaSerie(BaseChannel  *pSD, uint8_t *buffer, uint16_t bufferSize);
void chgetsNoEchoTimeOut(BaseChannel  *pSD, uint8_t *buffer, uint16_t bufferSize,systime_t timeout, uint8_t *huboTimeout);
void buscaParametros(uint8_t *posCadena, uint8_t *numParametros,uint8_t *parametros[]);
void procesaOrden(char *orden, uint8_t *error);
void leeTension(float *vBat);


uint8_t bufferGetsGPRS[200];

/*
 * https://lastminuteengineers.com/sim800l-gsm-module-arduino-tutorial/?utm_content=cmp-true
 */

static const SerialConfig ser_cfg = {
    115200, 0, 0, 0,
};

uint16_t lee2car(uint8_t *buffer, uint16_t posIni,uint16_t minValor, uint16_t maxValor, uint8_t *hayError)
{
    int32_t res;
    uint32_t numero;
    buffer[posIni+2] = 0;
    res = Str2Int(&buffer[posIni], &numero);
    if (res<0) *hayError=1;
    if (numero<minValor || numero>maxValor) *hayError=1;
    return (uint16_t) numero;
}


int8_t sms::sendSMS(char *msg, char *numTelefono)
{
    uint8_t hayError, huboTimeout, ch, bufferSMS[40];
    char bufferSendSMS[50];
    if (msg[0]==0 || numTelefono[0]==0) return -2;
    chsnprintf(bufferSendSMS,sizeof(bufferSendSMS),"AT+CMGS=\"%s\"\n", numTelefono);
    chMtxLock(&MtxEspSim800SMS);
    chnWrite(pSD, (const uint8_t *)bufferSendSMS, strlen(bufferSendSMS));
    while (1==1)
    {
        ch = chgetchTimeOut(pSD, TIME_MS2I(5000), &huboTimeout);
        if (huboTimeout) // timeout
        {
            chMtxUnlock(&MtxEspSim800SMS);
            return -1;
        }
        if (ch=='>') break;
    }

    while (1==1)
    {
        ch = chgetchTimeOut(pSD, TIME_MS2I(1000), &huboTimeout);
        if (huboTimeout) // timeout
        {
            chMtxUnlock(&MtxEspSim800SMS);
            return -1;
        }
        if (ch==' ') break;
    }
    // envio texto
    limpiaSerie(pSD, (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS));
    chnWrite(pSD,(uint8_t *) msg, strlen(msg));   // chprintf((BaseSequentialStream  *) pSD, msg);
    // termino con ^Z
    chsnprintf(bufferSendSMS,sizeof(bufferSendSMS),"%c",(char) 26);  //26 es CTRL-Z
    hayError = modemOrden(pSD,bufferSendSMS, bufferSMS, sizeof(bufferSMS),TIME_MS2I(8000));
    chMtxUnlock(&MtxEspSim800SMS);
    // borra datos mensaje, preparando para el proximo
    borraMsgRespuesta();
    telefonoRecibido[0] = 0;
    return hayError;
}


int8_t sms::sendSMS(void)
{
    if (!msgRespuesta[0])
        return 2;
    if (telefonoRecibido[0])
        return sendSMS(msgRespuesta, telefonoRecibido);
    else
        return sendSMS(msgRespuesta, telefonoAdmin);
}

void sms::avisaBateriaBaja(void)
{
    if (bateriaBajaAvisada)
        return;
    borraMsgRespuesta();
    addMsgRespuesta("BATERIA MUY BAJA!");
    ponEstado();
    despierta();
    sendSMS();
    bateriaBajaAvisada = 1;
}

uint8_t sms::initSIM800SMS(void)
{
    uint8_t hayError, numIntentos;
    uint8_t numParametros, *parametros[10];
    char buffer[30], bufferSendSMS[150];

    // soft init
    callReady = 0;
    smsReadyVal = 0;
    pinReady = 0;
    estadoCREG = 99;
    rssiGPRS = 99;
    telefonoEnvio[0] = 0;
    proveedor[0] = 0;
    durmiendo = 1;
    tiempoLastConexion = calendar::GetTimeUnixSec();

    // hardware init
    palClearPad(GPIOB, GPIOB_TX1);
    palSetPad(GPIOB, GPIOB_RX1);
    palSetPadMode(GPIOB, GPIOB_TX1, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOB, GPIOB_RX1, PAL_MODE_ALTERNATE(7));
    //palClearLine(LINE_RESETSIM800);
    //chThdSleepMilliseconds(150);
    //palSetLine(LINE_RESETSIM800);
    sdStart((SerialDriver *) pSD,&ser_cfg);

    // por si estaba durmiendo
    despierta();

    ponEnColaLCD(0,"Arranco GSM...");
//    message.fila = 0;
//    strncpy(message.msg,"Arranco GSM...",sizeof(message.msg));
//    hayError = putQueu(&colaMsgLcd, &message);
//    chEvtBroadcast(&updateLCD_source);

    // miro a ver si me responde
    numIntentos = 0;
    do
    {
        hayError = modemOrden(pSD, "AT\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(500));
        if (!hayError) break;
        hayError = modemOrden(pSD,"AT\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(500));
        if (!hayError) break;
        osalThreadSleepMilliseconds(1300);
        modemOrden(pSD,"+++", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
        osalThreadSleepMilliseconds(1300);
    } while (hayError && numIntentos++<3);
    if (hayError==1)
    {
        return 1;
    }
    // si me costo que me entendieran, fijo los baudios
    if (numIntentos>0)
    {
        hayError = modemOrden(pSD,"AT+IPR=115200\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
        hayError = modemOrden(pSD,"AT&W\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
    }
    // SIM insertada?
    hayError = modemParametros(pSD,"AT+CSMINS?\r\n","+CSMINS:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
    if (numParametros==2 && atoi((char *) parametros[1])==0)
    {
        ponEnColaLCD(0,"SIM no insertada!!");
        chThdSleepMilliseconds(5000);
        return 1;
    }
    // estado pin
    hayError = modemParametros(pSD,"AT+CPIN?\r\n","+CPIN:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
    if (numParametros==1 && !strncmp((char *) parametros[0],"SIM PIN",7)) // la SIM esta esperando el PIN
    {
        hayError = modemParametros(pSD,"AT+SPIC\r\n","+SPIC:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
        if (numParametros>=1)
        {
            uint8_t numIntentosrestantes = atoi((char *) parametros[0]);
            chsnprintf(buffer,sizeof(buffer),"Quedan %d intentos PIN\n\r",atoi((char *) parametros[0]));
            if (numIntentosrestantes>=2)
            {
                chsnprintf(buffer,sizeof(buffer),"AT+CPIN=\"%s\"\r\n",pin);
                hayError = modemOrden(pSD,buffer, bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
                if (hayError)
                {
                    ponEnColaLCD(0,"PIN problematico...");
                    chThdSleepMilliseconds(2000);
                    return 1;
                }
            }
            else
            {
                ponEnColaLCD(0,"PIN problematico...");
                chThdSleepMilliseconds(2000);
                return 1;
            }
        }
    }
    // comprobar que el PIN funciono
    chThdSleepMilliseconds(200);
    hayError = modemParametros(pSD,"AT+CPIN?\r\n","+CPIN:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
    if (numParametros!=1 || strcmp((char *) parametros[0],"READY")) // esta el PIN listo?
    {
        ponEnColaLCD(0,"PIN no funciona...");
        chThdSleepMilliseconds(4000);
        return 1;
    }
    //  modemOrden(pSD, "AT+CREG=1\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
    hayError = modemParametros(pSD,"AT+CMGF=?\r\n","+CMGF:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
    // Esperar registro en red
    uint8_t regOk = 0;
    for (uint8_t numInt=0;numInt<40; numInt++)
    {
        hayError = modemParametros(pSD,"AT+CREG?\r\n","+CREG:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
        if (numParametros==2)
        {
            uint8_t newEstadoCREG = bufferSendSMS[7]-'0';
            sms::estadoCREG = newEstadoCREG;
            regOk = 1;
            break;
        }
        ponEnColaLCD(0,"Registro en red...");
        chThdSleepMilliseconds(500);
    }
    if (!regOk)
    {
        ponEnColaLCD(0,"NO puedo registrar!");
        chThdSleepMilliseconds(5000);
    }
    // SMS modo texto
    //Miro si puede admitir SMS modo texto AT+CMGF=?
    hayError= modemOrden(pSD, "AT+CMGF=1\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
    // borro todos los mensajes sin leer
    modemOrden(pSD, "AT+CMGD=1,4\r\n", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS), TIME_MS2I(5000));
    // SMS notification: memorizo aunque me llegaran notificaciones CMTI
    modemOrden(pSD, "at+cnmi=2,1,0,0,0\r\n", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS), TIME_MS2I(5000));
    // Espero a tener RSSI
    do
    {
        hayError = modemParametros(pSD,"AT+CSQ\r\n","+CSQ:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(5000),&numParametros, parametros);
        if (hayError==0 && numParametros>=1)
        {
            rssiGPRS = lee2car(parametros[0],0,0,99,&hayError);
            if (rssiGPRS>0)
                break;
        }
        ponEnColaLCD(0,"Espero RSSI...");
        chThdSleepMilliseconds(500);
    } while (1==1);
    // me conecto
    hayError= modemOrden(pSD, "AT+COPS=0,0\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
    // leo proveedor, si no lo tenia claro
    do
    {
        hayError = modemParametros(pSD,"AT+COPS?\r\n","+COPS:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(5000),&numParametros, parametros);
        //+COPS: 0,0,"vodafone"
        if (numParametros>=3 && strlen((char *)parametros[2])>3)
        {
            strncpy(proveedor,(char *)parametros[2],sizeof(proveedor)-1);
            if (proveedor[0])
                break;
        }
        ponEnColaLCD(0,"Espero proveedor...");
        chThdSleepMilliseconds(500);
    } while (1==1);

    //at+cbc =>    +CBC: 0,92,4136 0, %carga y mV
    hayError = modemParametros(pSD,"AT+CBC\r\n","+CBC:", (uint8_t *) bufferSendSMS, sizeof(bufferSendSMS),TIME_MS2I(200),&numParametros, parametros);
    if (!hayError && numParametros==3)
    {
        //leeTension(&vBat);
        porcBateriaSIM800L = atoi((char *) parametros[1]);
        vBat =  atof((char *) parametros[2])/1000.0f;

#ifdef LCD
                if (sms::rssiGPRS==99)
                    chsnprintf(buffer,sizeof(buffer),"%.3fV %d%%",sms::vBat,sms::porcBateriaSIM800L);
                else
                    chsnprintf(buffer,sizeof(buffer),"%.3fV %d%% %c%d/30",sms::vBat,sms::porcBateriaSIM800L,(uint8_t)(1+(7*sms::rssiGPRS)/26),sms::rssiGPRS);
                ponEnColaLCD(0,buffer);
#endif
#ifdef SSD1306
                if (sms::rssiGPRS==99)
                    chsnprintf(buffer,sizeof(buffer),"%3d%%B NO GSM",sms::porcBateriaSIM800L);
                else
                    chsnprintf(buffer,sizeof(buffer),"%3d%%B %3d%%G",sms::porcBateriaSIM800L,(uint16_t)(sms::rssiGPRS/.3333f));
                ponEnColaLCD(0,buffer);
#endif
//
//        if (rssiGPRS==99)
//            chsnprintf(buffer,sizeof(buffer),"%.3fV %d%%",vBat,porcBateriaSIM800L);
//        else
//            chsnprintf(buffer,sizeof(buffer),"%.3fV %d%% %c%d/30",vBat,porcBateriaSIM800L,(uint8_t)(1+(7*rssiGPRS)/26),rssiGPRS);
//        ponEnColaLCD(0,buffer);
    }
    ponEnColaLCD(1,proveedor);
    // me pongo a dormir
    sleep();
    chsnprintf(buffer,sizeof(buffer),"Aviso %9s",telefonoAdmin);
    ponEnColaLCD(1,buffer);
    return 0;
}


/*
+CMTI: "SM",1

+CMTI: "SM",2     => envia dos veces porque hab�a en dos memorias diferentes. Si no enredamos solo debe llegar uno
at+cmgl="ALL"
+CMGL: 1,"REC UNREAD","+34619262851","","21/01/02,19:43:14+04"
Vamos a ver hoy

+CMGL: 2,"REC UNREAD","+34619262851","","21/01/02,20:37:26+04"
Vamos a ver

OK

 */
void sms::leoSmsCMTI(void)
{
    uint8_t huboTimeout, hayError, hayMensaje;
    uint8_t bufferGetsGPRS[20], numSMSaBorrar;
    char bufferCMGL[120], bufferNxt[100], *posCadena, *posSMS; //bufferSMS[120],
    uint8_t numParametros, *parametros[10];
    struct tm fechaSMS;
    numSMSaBorrar = 0;

    do
    {
        hayMensaje = 0;
        // envio orden de volcar mensajes
        chprintf((BaseSequentialStream *)pSD, "at+cmgl=\"ALL\"\r\n");
        // no hago caso del eco inicial
        chgetsNoEchoTimeOut((BaseChannel *) pSD, (uint8_t *) bufferCMGL, sizeof(bufferCMGL), TIME_MS2I(500),&huboTimeout);
        // solo nos vamos a fijar en el primer mensaje. Lo leemos, saltamos el resto, procesamos y borramos
        chgetsNoEchoTimeOut((BaseChannel *) pSD, (uint8_t *) bufferCMGL, sizeof(bufferCMGL), TIME_MS2I(500),&huboTimeout);
        if (huboTimeout==1)
            return;
        // puede llegar o una cabecera, o OK
        if (!strncmp((char *) bufferCMGL,"OK",2))
            return;
        // cabecera del tipo
        // +CMGL: 1,"REC UNREAD","+34619262851","","21/01/02,19:43:14+04"
        posCadena = strstr(bufferCMGL, (char *)"+CMGL:");
        if (posCadena==NULL)
            break;
        posCadena += 6;  // avanzo "+CMGL:"
        buscaParametros((uint8_t *) posCadena, &numParametros, parametros);
        if (numParametros<5)
            break;
        numSMSaBorrar = atoi((char *) parametros[0]);
        horaSMStoTM((uint8_t *)parametros[4], &fechaSMS);
        // leo mensaje, pueden venir en varias lineas hasta que llegue OK o +CMGL. Voy acumulando
        posSMS = mensajeRecibido;
        do
        {
            chgetsNoEchoTimeOut((BaseChannel *) pSD, (uint8_t *) posSMS, sizeof(mensajeRecibido)-(posSMS - mensajeRecibido), TIME_MS2I(500),&huboTimeout);
            if (huboTimeout==1 || !strncmp((char *) posSMS,"OK",2) || !strncmp((char *) posSMS,"+CMGL",5))
                break;
            hayMensaje = 1;
            posSMS += strlen((char *)posSMS);
        } while (1==1);
        // marco el final del mensaje (no incorporo el OK o +CMGL)
        *posSMS = 0;
        // salto el resto del envio
        limpiaBuffer((BaseChannel *) pSD);

        if (hayMensaje)
        {
            strncpy(telefonoRecibido,(char *)parametros[2],sizeof(telefonoRecibido));
            chsnprintf(bufferNxt,sizeof(bufferNxt),"SMS de %s, enviado el %d/%d %d:%02d:%02d",parametros[2],
                       fechaSMS.tm_mday,fechaSMS.tm_mon+1,fechaSMS.tm_hour,fechaSMS.tm_min,fechaSMS.tm_sec);
            chsnprintf(bufferNxt,sizeof(bufferNxt),"Mensaje: %s",mensajeRecibido);
            chsnprintf(bufferNxt,sizeof(bufferNxt),"SMS:%s",mensajeRecibido);
            //ponEnColaLCD(1,bufferNxt);
            // borro el mensaje
            chsnprintf(bufferNxt,sizeof(bufferNxt),"at+cmgd=%d\r\n",numSMSaBorrar);
            hayError = modemOrden((BaseChannel *) pSD, bufferNxt, bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(500));
            if (!hayError)
            {
                borraMsgRespuesta();
                procesaOrden(mensajeRecibido, &hayError);
                ponEstado();
                sendSMS();
            }
        }
    } while (hayMensaje);
}


void sms::leoSMS(char *bufferSendSMS, uint16_t buffSize)
{
    uint8_t huboTimeout,i;
    char buffer[81];
    struct tm fechaSMS;
    // +CMT: "+34619262851","","20/03/07,08:40:04+04"
    for (i=0;i<sizeof(telefonoRecibido) && i<buffSize-7;i++)
    {
        if (bufferSendSMS[i+7]=='\"')
            break;
        telefonoRecibido[i] = bufferSendSMS[i+7];
    }
    telefonoRecibido[i] = 0;
    // decodifico fecha
    horaSMStoTM((uint8_t *)&bufferSendSMS[25], &fechaSMS);
    chsnprintf(buffer,sizeof(buffer),"SMS de %s",telefonoRecibido);
    //msgParaLCD(buffer, 1000);
    chsnprintf(buffer,sizeof(buffer),"%d/%d/%d %d:%d:%d",
                    fechaSMS.tm_mday, fechaSMS.tm_mon+1, fechaSMS.tm_year+1900,
                    fechaSMS.tm_hour, fechaSMS.tm_min, fechaSMS.tm_sec);
    //msgParaLCD(buffer, 1000);
    // mensaje (puede haber varias lineas)
    mensajeRecibido[0] = 0;
    // reaprovecho bufferSendSMS
    do
    {
        chgetsNoEchoTimeOut((BaseChannel  *)&SD2, (uint8_t *) bufferSendSMS, buffSize, TIME_MS2I(500),&huboTimeout);
        if (bufferSendSMS[0])
        {
            if (mensajeRecibido[0])
                strncat(mensajeRecibido,";",2);
            strncat(mensajeRecibido,bufferSendSMS,sizeof(mensajeRecibido)-strlen(bufferSendSMS)-1);
        }
        else
            break;
    } while (TRUE);
    interpretaSMS((uint8_t *)mensajeRecibido);
    //chEvtBroadcast(&enviarSMS_source);
}

void sms::sleep(void)
{
    uint8_t buffer[20];
    if (!bajoConsumo)
        return;
    if (!durmiendo)
    {
        uint8_t hayError = modemOrden(pSD, "AT+CSCLK=2\r\n", buffer, sizeof(buffer), TIME_MS2I(500));
        if (!hayError)
            durmiendo = 1;
    }
}

void sms::despierta(void)
{
    uint8_t buffer[20];
    if (!durmiendo)
        return;
    chprintf((BaseSequentialStream *)pSD, "despierta!!\r\n");
    chThdSleepMilliseconds(200);
    uint8_t hayError = modemOrden(pSD, "AT+CSCLK=0\r\n", buffer, sizeof(buffer), TIME_MS2I(500));
    if (!hayError)
        durmiendo = 0;
}

/*
 * Convierto fecha SMS a struct tm
 * Formato: 19/12/05,21:12:30+04
 * Retorna 1 si hay error
 */
uint8_t sms::horaSMStoTM(uint8_t *cadena, struct tm *fecha)
{
    uint8_t hayError;
    int16_t difGMT;
    // 04/01/01,00:10:57+04
    hayError = 0;
    difGMT = lee2car(cadena,19,0,96,&hayError);
    if (cadena[18]=='-') difGMT = -difGMT;
    fecha->tm_year = lee2car(cadena,0,17,99,&hayError)+2000-1900;
    fecha->tm_mon = lee2car(cadena,3,1,12,&hayError)-1;
    fecha->tm_mday = lee2car(cadena,6,1,31,&hayError);
    fecha->tm_hour = lee2car(cadena,9,0,23,&hayError);
    fecha->tm_min = lee2car(cadena,12,0,59,&hayError);
    fecha->tm_sec = lee2car(cadena,15,0,59,&hayError);
    uint8_t numCuartos = lee2car(cadena,18,0,8,&hayError);
    if (numCuartos==4) fecha->tm_isdst = 0; // horario de invierno
    if (numCuartos==8) fecha->tm_isdst = 1; // horario de verano
    return hayError;
}


uint8_t sms::ponHoraConGprs(void)
{
    uint8_t hayError, numParametros, *parametros[10];
    char buffer[40];
    struct tm fechaGPRS, fechaUTM;
    time_t secs;
    RTCDateTime timespec;
    /*
     *  bufferGetsGPRS = "+CCLK: \"17/08/03,20:39:29+08",
     *   parametros[0] = "17/08/04,11:18:30+08", // el 8 es la diferencia con PST en unidades de 15 minutos!
     *                    01234567890123456789
     */
    /*
     * Miro si está activado leer hora
     */
    chMtxLock(&MtxEspSim800SMS);
    hayError = modemParametros(pSD,"AT+CLTS?\r\n","+CLTS:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(1000),&numParametros, parametros);
    chMtxUnlock(&MtxEspSim800SMS);
    if (hayError==0 && numParametros==1 && strcmp((char *)parametros[0],"0")==0)
    {
        chMtxLock(&MtxEspSim800SMS);
        hayError = modemOrden(pSD, "AT+CLTS=1;&W\r\n", bufferGetsGPRS, sizeof(bufferGetsGPRS), TIME_MS2I(1000));
        chMtxUnlock(&MtxEspSim800SMS);
        if (hayError!=0) return 1;
        osalThreadSleepMilliseconds(100);
    }
    chMtxLock(&MtxEspSim800SMS);
    hayError = modemParametros(pSD,"AT+CCLK?\r\n","+CCLK:", bufferGetsGPRS, sizeof(bufferGetsGPRS),TIME_MS2I(1000),&numParametros, parametros);
    chMtxUnlock(&MtxEspSim800SMS);
    // +CCLK: "21/02/01,21:25:04+04"
    if (hayError==1)
        return 1;
    hayError = horaSMStoTM(&bufferGetsGPRS[8], &fechaGPRS);
    if (hayError) // fecha mal, no funciona CCLK. Podria auto-enviarme un SMS
    {
        return 1;
    }
    calendar::fechaLocal2UTM(&fechaGPRS, &fechaUTM, &secs);
    rtcConvertStructTmToDateTime(&fechaUTM, 0, &timespec);
    rtcSetTime(&RTCD1, &timespec);
    chsnprintf(buffer,sizeof(buffer),"Fecha GPRS:%d/%d/%d %d:%d:%d",
                    fechaGPRS.tm_mday, fechaGPRS.tm_mon+1, fechaGPRS.tm_year+1900,
                    fechaGPRS.tm_hour, fechaGPRS.tm_min, fechaGPRS.tm_sec);
    return 0;
}



uint8_t sms::diSmsReady(void)
{
    return (estadoCREG==1 || estadoCREG==5);
}








