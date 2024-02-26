#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "string.h"
#include "chprintf.h"
#include "calendar.h"
#include "gets.h"
#include "stdlib.h"


/* Minuto del dia cuando amanece, esta en minutos UTC */
const uint16_t minAmanecerPorSemana[] = {446, 443, 438, 432, 426, 418, 410, 401, 392, 383, 374, 364, 354, 345, 335, 326, 317, 309, 301, 294,
										 288, 283, 279, 276, 275, 276, 278, 281, 286, 292, 298, 306, 314, 323, 332, 341, 351, 361, 370, 380, 389,
										 398, 407, 415, 423, 430, 436, 441, 445, 447, 448, 448, 446, 443};
const uint16_t minAnochecerPorSemana[] = {1024, 1027, 1031, 1037, 1044, 1051, 1059, 1068, 1077, 1086, 1096, 1105, 1115, 1125, 1134, 1143, 1152,
										  1161, 1168, 1176, 1182, 1187, 1191, 1193, 1194, 1194, 1192, 1188, 1184, 1178, 1171, 1163, 1155, 1146,
										  1137, 1128, 1118, 1109, 1099, 1090, 1080, 1071, 1062, 1054, 1046, 1039, 1033, 1028, 1025, 1022, 1021,
										  1021, 1023, 1026};

uint16_t calendar::minAmanecer = 370;
uint16_t calendar::minAnochecer = 1194;
time_t calendar::segundosUnixCambioInv2Ver = 0;
time_t calendar::segundosUnixCambioVer2Inv = 1;
//uint8_t calendar::esHorarioVerano = 0;

time_t timegm(const struct tm *tm)
{
    // Month-to-day offset for non-leap-years.
    static const int month_day[12] =
    {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

    // Most of the calculation is easy; leap years are the main difficulty.
    int16_t month = tm->tm_mon % 12;
    uint16_t year = tm->tm_year + tm->tm_mon / 12;
    if (month < 0) {   // Negative values % 12 are still negative.
        month += 12;
        --year;
    }

    // This is the number of Februaries since 1900.
    const uint16_t year_for_leap = (month > 1) ? year + 1 : year;

    time_t rt = tm->tm_sec                             // Seconds
        + 60 * (tm->tm_min                          // Minute = 60 seconds
        + 60 * (tm->tm_hour                         // Hour = 60 minutes
        + 24 * (month_day[month] + tm->tm_mday - 1  // Day = 24 hours
        + 365 * (year - 70)                         // Year = 365 days
        + (year_for_leap - 69) / 4                  // Every 4 years is     leap...
        - (year_for_leap - 1) / 100                 // Except centuries...
        + (year_for_leap + 299) / 400)));           // Except 400s.
    return rt < 0 ? -1 : rt;
}

time_t calendar::GetTimeUnixSec(void) {
  struct tm tim;
  RTCDateTime timespec;
  rtcGetTime(&RTCD1, &timespec); // lee del RTC fecha y hora
  rtcConvertDateTimeToStructTm(&timespec, &tim, NULL); // convierte a time_t de unix
  return mktime(&tim);
}

void calendar::fechaLocal2UTM(struct tm *fechaLocal, struct tm *fechaUTM, time_t *secsUTM) //, uint32_t secsInv2Ver, uint32_t secsVer2Inv)
{
    // si lo llamas entre las 2 y 3 am del cambio de verano a invierno, es indeterminado (pasa dos veces)
    time_t secsLocal;
    secsLocal = mktime(fechaLocal);  // gday(fechaLocal); // timegm pasa de hora UTM a secLocal, mktime calcula los sec sin pasar a local
	if (secsLocal-3600<segundosUnixCambioInv2Ver || secsLocal-7200>=segundosUnixCambioVer2Inv)
		*secsUTM = secsLocal - 3600;   // invierno
	else
        *secsUTM = secsLocal - 7200;   // verano
	gmtime_r(secsUTM, fechaUTM);      //  d2f(*secsUTM,fechaUTM);
}

void calendar::fechaUTM2Local(struct tm *fechaUTM, struct tm *fechaLocal, time_t *secsLocal)
{
	*secsLocal = mktime(fechaUTM);
	if (*secsLocal>segundosUnixCambioInv2Ver && *secsLocal<segundosUnixCambioVer2Inv)
		*secsLocal += 7200;
	else
	    *secsLocal += 3600;
	gmtime_r(secsLocal,fechaLocal);
}

// devuelve fecha/hora UTC en formato tm
void calendar::getTimeTM(struct tm *tim) {
  RTCDateTime timespec;
  rtcGetTime(&RTCD1, &timespec); // lee del RTC fecha y hora
  rtcConvertDateTimeToStructTm(&timespec, tim, NULL); // convierte a time_t de unix
}


// ajusta hora interna. La entrada sera hora local
// calcular flag verano: -1=auto, 0,1: usar
void calendar::SetTimeUnixSec(time_t unix_time, int8_t flagVerano) {
  struct tm tim;
  struct tm *canary;
  RTCDateTime timespec;
  /* If the conversion is successful the function returns a pointer
     to the object the result was written into.*/
  canary = localtime_r(&unix_time, &tim);
  if (flagVerano==-1)
  {
      if (unix_time>calendar::segundosUnixCambioInv2Ver && unix_time<=calendar::segundosUnixCambioVer2Inv)
          tim.tm_isdst =1;
      else
          tim.tm_isdst =1;
  }
  else
      tim.tm_isdst = flagVerano;
  osalDbgCheck(&tim == canary);
  rtcConvertStructTmToDateTime(&tim, 0, &timespec);
  rtcSetTime(&RTCD1, &timespec);
}


// comprueba que si estamos en verano esta el estado bien
// al pasar el calendario a UTM no hace falta
//void calendar::ajustaFlagHorarioVerano(void)
//{
//    struct tm now;
//    RTCDateTime timespec;
//    time_t secNow;
//    rtcGetTime(&RTCD1, &timespec); // lee del RTC fecha y hora
//    rtcConvertDateTimeToStructTm(&timespec, &now, NULL); // convierte a time_t de unix
//    ajustaSecsCambHorario(&now);
//    secNow = mktime(&now);
//    if (secNow>=calendar::segundosUnixCambioInv2Ver && secNow<=calendar::segundosUnixCambioVer2Inv)
//    {
//        // toca hora de verano
//        if (timespec.dstflag!=1)
//        {
//            // Estaba en invierno, adelanto una hora y ajusto flag
//            secNow -=3600;
//            SetTimeUnixSec(secNow,1);
//        }
//    }
//    else
//    {
//        // toca hora de invierno
//        if (timespec.dstflag!=0)
//        {
//            // Estaba en verano, atraso una hora y ajusto flag
//            secNow +=3600;
//            SetTimeUnixSec(secNow, 0);
//        }
//    }
//}


void calendar::cambiaFechaLocal(uint16_t *anyo, uint8_t *mes, uint8_t *dia, uint8_t *hora, uint8_t *min, uint8_t *seg)
{
    struct tm fechaUTM, fechaLocal;
    RTCDateTime timespec;
    time_t secs;

    getTimeTM(&fechaUTM);                // leo hora UTM
    fechaUTM2Local(&fechaUTM, &fechaLocal, &secs);
    if (anyo!=NULL && *anyo>2020 && *anyo<3000) // actualizo datos con lo que hayan pasado
        fechaLocal.tm_year = *anyo-1900;
    if (mes!=NULL && *mes>=1 && *mes<=12)
        fechaLocal.tm_mon = *mes-1;
    if (dia!=NULL && *dia>=1 && *dia<=31)
        fechaLocal.tm_mday = *dia;
    if (hora!=NULL && *hora<=23)
        fechaLocal.tm_hour = *hora;
    if (min!=NULL && *min<=59)
        fechaLocal.tm_min = *min;
    if (seg!=NULL && *seg<=59)
        fechaLocal.tm_sec = *seg;
    fechaLocal2UTM(&fechaLocal, &fechaUTM, &secs);
    ajustaSecsCambHorario(&fechaUTM); // funciona igual con UTM o fecha local
    rtcConvertStructTmToDateTime(&fechaUTM, 0, &timespec);
    rtcSetTime(&RTCD1, &timespec);
    ajustaHorasLuz(&fechaUTM);
    ajustaSecsCambHorario(&fechaUTM);
}

void calendar::trataOrdenNextion(char *vars[], uint16_t numPars)
{
    time_t secs;
    struct tm fechaUTM, fechaLocal;
    RTCDateTime timespec;
    // Orden desde Nextion: @orden,fecha,year,2020
    if (numPars==2)//
    {
        getTimeTM(&fechaUTM); //GetTimeTm(&timp,&ms);
        fechaUTM2Local(&fechaUTM, &fechaLocal, &secs);
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
        fechaLocal2UTM(&fechaLocal, &fechaUTM, &secs);
        rtcConvertStructTmToDateTime(&fechaUTM, 0, &timespec);
        rtcSetTime(&RTCD1, &timespec);
        ajustaHorasLuz(&fechaUTM);
        ajustaSecsCambHorario(&fechaUTM);
    }
}

void calendar::ajustaHorasLuz(struct tm *now)
{
	time_t sec1Enero, secsNow, secElapsed;
	uint16_t semanaAnyo;
	struct tm inicioAnyo;
	secsNow = mktime(now);
	inicioAnyo.tm_year = now->tm_year;
	inicioAnyo.tm_mon = 0;
	inicioAnyo.tm_mday = 1;
	inicioAnyo.tm_hour = 0;
	inicioAnyo.tm_min = 0;
	inicioAnyo.tm_sec = 0;
	sec1Enero = mktime(&inicioAnyo);
	secElapsed = secsNow - sec1Enero;
	semanaAnyo = secElapsed/604800; // segundosporsemana = 3600*24*7
	if (semanaAnyo>=sizeof(minAmanecerPorSemana))
	    minAmanecer = 439; // por decir algo
	else
	    minAmanecer = minAmanecerPorSemana[semanaAnyo];
	if (semanaAnyo>=sizeof(minAnochecerPorSemana))
	    minAnochecer = 1030;
	else
	    minAnochecer = minAnochecerPorSemana[semanaAnyo];
}

void calendar::ajustaHorasLuz(void)
{
    struct tm now;
    getTimeTM(&now);
    ajustaHorasLuz(&now);
}

void calendar::ajustaSecsCambHorario(struct tm *now)
{
	// busco el ultimo domingo de marzo a las 02horas (01 en UTM)
	struct tm fechaCambio;

	fechaCambio.tm_year = now->tm_year;
	fechaCambio.tm_mon = 2; // enero = 0
	fechaCambio.tm_mday = 31;
	fechaCambio.tm_hour = 1;
	fechaCambio.tm_min = 0;
	fechaCambio.tm_sec = 0;
	segundosUnixCambioInv2Ver = mktime(&fechaCambio);
	segundosUnixCambioInv2Ver -= fechaCambio.tm_wday*86400; // para descontar precisamente el dia de la semana (para pillar el domingo); 86400 = 24*60*60
	// repetimos para el cambio de octubre
	fechaCambio.tm_mon = 9;
	segundosUnixCambioVer2Inv = mktime(&fechaCambio); // despues hay que descontar precisamente el dia de la semana (para pillar el domingo)
	segundosUnixCambioVer2Inv -= fechaCambio.tm_wday*86400; // 86400 = 24*60*60
}


uint8_t calendar::esDeNoche(void)
{
    struct tm now;
    uint16_t minutosNow;
    getTimeTM(&now);
    minutosNow = 60*now.tm_hour + now.tm_min;
    if (minutosNow<minAmanecer || minutosNow>minAnochecer)
    	return 1;
    else
    	return 0;
}


void calendar::initRTC(void)
{
    struct tm now;
    getTimeTM(&now);
    ajustaSecsCambHorario(&now);
    ajustaHorasLuz(&now);
}

