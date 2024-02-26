#ifndef _CALENDAR_H_
#define _CALENDAR_H_

#include <ch.h>
#include "time.h"

#define SECONDS_IN_DAY 86400
#define DEFAULT_YEAR 2008
#define LEAP 1
#define NOT_LEAP 0

class calendar
{
private:
    static uint16_t minAmanecer, minAnochecer;                           // hora UTM
    static time_t segundosUnixCambioInv2Ver, segundosUnixCambioVer2Inv;  // hora UTM
    //static uint8_t  esHorarioVerano;
    static void ajustaSecsCambHorario(struct tm *now);
    static void ajustaHorasLuz(struct tm *now);
public:
    static void initRTC(void);
    static uint8_t esHoraVerano(void);
    static uint8_t esDeNoche(void);
    static void getTimeTM(struct tm *tim);
    static time_t getSecUnix(void);
    static void SetTimeUnixSec(time_t unix_time, int8_t flagVerano);
    static void ajustaHorasLuz(void);
//    static void ajustaFlagHorarioVerano(void);
    static void fechaLocal2UTM(struct tm *fechaLocal, struct tm *fechaUTM, time_t *secsUTM);
    static time_t GetTimeUnixSec(void);
    static void fechaUTM2Local(struct tm *fechaUTM, struct tm *fechaLocal, time_t *secsLocal);
    static void cambiaFechaLocal(uint16_t *anyo, uint8_t *mes, uint8_t *dia, uint8_t *hora, uint8_t *min, uint8_t *seg);
    static void trataOrdenNextion(char *vars[], uint16_t numPars);
};


void RTC_Configuration(void);
uint16_t WeekDay(struct tm *d);
uint8_t diasEnMes(uint8_t uint8_t_Month,uint16_t uint16_t_Year);
uint32_t segundosEnAno(uint16_t uint16_t_Year);
uint32_t gday(struct tm *d);
uint8_t fechaLegal(struct tm *d);
uint8_t esDeNoche(void);
void d2f(uint32_t secs,struct tm *d);
void today(struct tm *fecha);
void incrementaFechaLocal1sec(void);
void hallaSecsCambHorario(struct tm *fecha, uint32_t *secsInv2Ver, uint32_t *secsVer2Inv);
void fechaUTM2Local(struct tm *fechaUTM, struct tm *fechaLocal);
void leeFecha(void);
void ponFechaLCD(void);
void ponHoraLCD(void);
void initCalendar(void);
void arrancaCalendario(void);
void actualizaDatosDiarios(void);
#endif
