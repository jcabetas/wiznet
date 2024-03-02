#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "bloques.h"
#include "string.h"
#include <stdint.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "nextion.h"
#include "calendar.h"
#include "dispositivos.h"
#include "sms.h"

time_t GetTimeUnixSec(void);
void rtcGetTM(RTCDriver *rtcp, struct tm *tim, uint16_t *ds);

/*
 *     uint16_t idPage, idYear, idMes, idDia, idHora, idMin;
 */
uint8_t fecha::actualizacionPendiente = 0;

// FECHA base year mes dia [hora min]
fecha::fecha(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=5 && numPar!=7)
    {
        nextion::enviaLog(tty,"#param FECHA");
        *hayError = 1;
        return; // error
    }
    idPage = nombres::incorpora(pars[1]);
    idYear = nombres::incorpora(pars[2]);
    idMes = nombres::incorpora(pars[3]);
    idDia = nombres::incorpora(pars[4]);
    if (numPar==7)
    {
        idHora = nombres::incorpora(pars[5]);
        idMin = nombres::incorpora(pars[6]);
    }
    else
    {
        idHora = 0;
        idMin = 0;
    }
    actualizacionPendiente =0;
};

fecha::~fecha()
{
}

const char *fecha::diTipo(void)
{
	return "fecha";
}

const char *fecha::diNombre(void)
{
	return "fecha";
}

int8_t fecha::init(void)
{
    calendar::init();
    fecha::actualizaNextion(0);
    return 0;
}


void fecha::addTime(uint16_t , uint8_t , uint8_t , uint8_t seg, uint8_t ds)
{
    calendar::updateEveryDs();
    if (ds==0)
    {
        if (seg==0 || actualizacionPendiente) // actualiza cada minuto, que no es mucho
        {
            fecha::actualizaNextion(0);
            actualizacionPendiente = 0;
        }
    }
}


void fecha::actualizaCuandoPuedas(void)
{
    actualizacionPendiente = 1;
}
//void rtcShow(BaseSequentialStream *tty)
//{
//  struct tm timp;// = {0};
//  char buffer[60];
//  uint16_t ds;
//  time_t unix_time;
//  /* switch off wakeup */
//    rtcSTM32SetPeriodicWakeup(&RTCD1, NULL);
//    unix_time = calendar::GetTimeUnixSec();
//    if (unix_time == -1){
//      nextion::enviaLog(tty,"incorrect time in RTC cell");
//    }
//    else {
//      chsnprintf(buffer,sizeof(buffer),"UxTime:%d",unix_time);
//      nextion::enviaLog(tty,buffer);
//      rtcGetTM(&RTCD1, &timp, &ds);
//      chsnprintf(buffer,sizeof(buffer),"%s",asctime(&timp));
//      nextion::enviaLog(tty,buffer);
//    }
//}




void fecha::actualizaNextion(uint8_t )
{
    char buffer[20];
    struct tm fechaLocal;
    calendar::getFecha(&fechaLocal);
    chsnprintf(buffer,sizeof(buffer),"%2d",fechaLocal.tm_mday);
    enviaTxt(idPage, idDia, buffer);
    chsnprintf(buffer,sizeof(buffer),"%02d",fechaLocal.tm_mon+1);
    enviaTxt(idPage, idMes, buffer);
    chsnprintf(buffer,sizeof(buffer),"%4d",fechaLocal.tm_year+1900);
    enviaTxt(idPage, idYear, buffer);
    if (idHora>0)
    {
        chsnprintf(buffer,sizeof(buffer),"%2d",fechaLocal.tm_hour);
        enviaTxt(idPage, idHora, buffer);
        chsnprintf(buffer,sizeof(buffer),"%02d",fechaLocal.tm_min);
        enviaTxt(idPage, idMin, buffer);
    }
}

void fecha::trataOrdenNextion(char *vars[], uint16_t numPars)
{
    struct tm fechaLocal;
    uint16_t ds;
    // Orden desde Nextion: @orden,fecha,year,2020
    if (numPars==2)//
    {
        calendar::getFecha(&fechaLocal);
        if (!strcasecmp(vars[0],"year"))
            fechaLocal.tm_year = atoi(vars[1]) - 1900;
        else if ((!strcasecmp(vars[0],"mes")))
            fechaLocal.tm_mon = atoi(vars[1]) - 1;
        else if ((!strcasecmp(vars[0],"dia")))
            fechaLocal.tm_mday = atoi(vars[1]);
        else if ((!strcasecmp(vars[0],"hora")))
            fechaLocal.tm_hour = atoi(vars[1]);
        else if ((!strcasecmp(vars[0],"min")))
            fechaLocal.tm_min = atoi(vars[1]);
        else if ((!strcasecmp(vars[0],"seg")))
            fechaLocal.tm_sec = atoi(vars[1]);
        calendar::rtcSetFecha(&fechaLocal,0);
        actualizaNextion(0);
        if (cargador::cochesConectados()>0)
        {
            rtcGetTM(&RTCD1, &fechaLocal, &ds);         // leo hora local con ds
            calendar::enviaFechayHoraPorCAN(&fechaLocal,ds);
        }
    }
}

void fecha::print(BaseSequentialStream *tty)
{
    char buffer[10];
    chsnprintf(buffer,sizeof(buffer),"FECHA");
    if (tty!=NULL)
        nextion::enviaLog(tty,buffer);
}

void fecha::statusSMS(sms *smsPtr)
{
    char buffer[30];
    struct tm timp;
    uint16_t ds;
    rtcGetTM(&RTCD1, &timp, &ds);
    chsnprintf(buffer,sizeof(buffer),"%2d/%02d/%2d %d:%02d:%02d",timp.tm_mday,timp.tm_mon+1,timp.tm_year+1900,timp.tm_hour,timp.tm_min,timp.tm_sec);
    smsPtr->addMsgRespuesta(buffer);
}



