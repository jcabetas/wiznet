/*
 * procesaOrden.c
 *
 *  Created on: 7/8/2017
 *      Author: joaquin
 */

/*
 * procesaOrden.c
 *
 *  Created on: 6/8/2017
 *      Author: joaquin
 */

/**
 * @file stm32/sms/parsing.c
 * @brief Ejecuta un SMS recibido
 * @addtogroup SMSs
 * @{
 */
#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include "string.h"
#include "gets.h"
#include "chprintf.h"
#include "ctype.h"
#include "sms.h"
#include "varsFlash.h"
#include "lcd.h"

void leeTension(float *vBat);
float hallaCapBat(float *vBat);

extern uint16_t porcBateriaSIM800L, mVSIM800L;
extern event_source_t enviarSMS_source;
extern uint8_t estadoJaulaClosed, estadoJaulaOpened;

extern event_source_t activate_source, desactivate_source;

extern char nomEstado[5][20];// = {"Iniciando","Espero activacion","Midiendo altura","Espero gato","Gato en jaula"};
enum estado_t {esperoThreads=1, esperoBoton, esperoEstabilidad, esperoGato, gatoEnJaula};
extern enum estado_t estado;


void parseStr(char *cadena,char **parametros, const char *tokens,uint16_t *numParam)
{
    char *puntOut,*puntStr;
    *numParam=0;
    puntOut = cadena;
    do
    {
        puntStr = strsep(&puntOut,tokens);
        if (puntStr)
        {
            parametros[*numParam] = puntStr;
            (*numParam)++;
        }
    } while (puntStr);
}



/**
 * @brief Quita espacios de delante y de atras de una cadena
 *
 * @param str Texto a modificar
 * @note Modifica el texto original
 */
char *trimall(char *str)
{
    char *posInicio, *posFinal;

    posInicio = str;
    while (*posInicio==' ') posInicio++;
    posFinal = posInicio + strlen(posInicio)-1;
    while (*posFinal==' ' && posFinal>=posInicio)
    {
        *posFinal = (char) 0;
        posFinal--;
    }
    return posInicio;
}



void sms::procesaOrdenAsignacion(char *orden, char *puntSimbIgual)
{
    char buff[50], *puntValor;
    int32_t err;
    char bufferSetPin[50];
    // orden de asignacion
    *puntSimbIgual = (char) 0;
    trimall(orden);
    puntValor = trimall(puntSimbIgual+1);
    //
    if (!strcasecmp(orden,"telefono"))
    {
        ponEstado();
        uint16_t sectorNumTelef = SECTORNUMTELEF;
        err = escribeStr50(&sectorNumTelef, "numTelef", puntValor);
        if (err==0)
        {
            chsnprintf(buff,sizeof(buff),"Telefono envio:%s\n",puntValor);
            addMsgRespuesta(buff);
            chsnprintf(buff,sizeof(buff),"Telefono %s",puntValor);
            ponEnColaLCD(1,buff);
            ponEnColaLCD(2,"grabado!");
        }
        else
        {
            addMsgRespuesta("Error al grabar num. telef.!!");
            ponEnColaLCD(1,"Error grabando telf!");
            ponEnColaLCD(2,"");
        }
        return;
    }
    if (!strcasecmp(orden,"pin"))
    {
        ponEstado();
        //        AT+CPIN=“1234”
        //        OK
        //        AT+CLCK=“SC”,0,“1234”,1
        //        OK
        chsnprintf(buff,sizeof(buff),"AT+CLCK=\"SC\",1,\"%s\"\r\n",puntValor);
        hayError = modemOrden(pSD,buff,(uint8_t *) bufferSetPin, sizeof(bufferSetPin),TIME_MS2I(200));
        if (hayError)
        {
            ponEnColaLCD(1,"Error ajustando PIN");
            chThdSleepMilliseconds(4000);
            return;
        }
        else
        {
            chsnprintf(buff,sizeof(buff),"Nuevo PIN: %s",puntValor);
            ponEnColaLCD(1,buff);
            chThdSleepMilliseconds(2000);
        }
        chsnprintf(buff,sizeof(buff),"AT+CPIN=\"%s\"\r\n",puntValor);
        hayError = modemOrden(pSD,buff, (uint8_t *) bufferSetPin, sizeof(bufferSetPin), TIME_MS2I(1000));
        uint16_t sectorPin = SECTORPIN;
        err = escribeStr50(&sectorPin, "numPin", puntValor);
        chsnprintf(buff,sizeof(buff),"pin:%s\n",puntValor);
        addMsgRespuesta(buff);
        chsnprintf(buff,sizeof(buff),"PIN %s grabado!",puntValor);
        ponEnColaLCD(1,buff);
        chThdSleepMilliseconds(500);
        return;
    }
}




void sms::ponEstado(void)
{
    char buff[30];
    if (estadoPuesto==1)
        return;
    if (estado < sizeof(nomEstado))
        addMsgRespuesta(nomEstado[estado-1]);
    chsnprintf(buff,sizeof(buff),"Vbat:%.3fV (%d%%)",sms::vBat,sms::porcBateriaSIM800L);
    addMsgRespuesta(buff);
    chsnprintf(buff,sizeof(buff),"Avisare a %s",telefonoAdmin);
    addMsgRespuesta(buff);
    estadoPuesto = 1;
}


/**
 * @brief Procesa una orden
 *
 * @param orden Orden a ejecutar
 *
 */
char bufferOrden[40];
void sms::procesaOrden(char *orden, uint8_t *error)
{
    char *puntSimb, *puntOrden;

    puntOrden = trimall(orden);
    strncpy(bufferOrden,puntOrden,sizeof(bufferOrden));
    puntSimb = strchr(bufferOrden,' ');
    if (puntSimb != NULL)
        *puntSimb = (char) 0;

    if (!strncasecmp(bufferOrden,"status",strlen(bufferOrden)) || !strcasecmp(orden,"estado"))
    {
        ponEstado();
//        if (puntSimb != NULL)
//        {
//            char *puntValor = trimall(puntSimb+1);
//            //procesaStatusYAlgo(puntValor);
//            return;
//        }
//        procesaStatus();
        return;
    }
    if (!strncasecmp(bufferOrden,"ayuda",strlen(bufferOrden)))
    {
        ponEstado();
        addMsgRespuesta("Ordenes:\nestado\ndesactivar\nactivar\ntelefono=XXX\npin=YYY");
        return;
    }
    if (!strncasecmp(orden,"desactivar",strlen(orden)))
    {
        chEvtBroadcast(&desactivate_source);
        chThdSleepMilliseconds(500);
        addMsgRespuesta("Desactivo jaula");
        ponEstado();
        return;
    }
    if (!strncasecmp(orden,"activar",strlen(orden)))
    {
        chEvtBroadcast(&activate_source);
        addMsgRespuesta("Activo jaula (puede tardar)");
        chThdSleepMilliseconds(5000);
        return;
    }
    // contiene = : ?
    puntSimb = strchr(orden,'=');
    if (puntSimb==NULL)
        puntSimb = strchr(orden,':');
    if (puntSimb != NULL)
    {
        // es una orden de asignacion
        procesaOrdenAsignacion(puntOrden, puntSimb);
        return;
    }
    // si llego aqui, es que no he interpretado la orden
    *error = 1;
}


/**
 * @brief interpreta las ordenes de un SMS
 *
 * @param textoSMS Texto a ejecutar
 * @param enviaRespuestaPorSMS Indica si tiene que enviar respuesta por SMS
 * @note divide el SMS en trozos, y procesa orden a orden
 * origenSMS = +CMT: \"+34619262851\",\"\",\"17/08/06,15:34:39+08\"\r\n
 */
void sms::interpretaSMS(uint8_t *textoSMS)
{
    char *ordenes[15];
    uint16_t numOrdenes, i;
    uint8_t error;

    msgRespuesta[0] = 0;
    // compruebo que no hay caracteres raros
    for (i=0;i<strlen((char *)textoSMS);i++)
        if (!isprint((int) textoSMS[i]))
        {
            return;
        }
    // divido ordenes

    error = 0;
    parseStr((char *)textoSMS,ordenes,",.;",&numOrdenes);
    for (i=0;i<numOrdenes;i++)
        procesaOrden(ordenes[i],&error);
    if (error)
    {
        chsnprintf(bufferOrden,sizeof(bufferOrden),"No entiendo '%s'",textoSMS);
        addMsgRespuesta(bufferOrden);
    }
    ponEstado();
}
