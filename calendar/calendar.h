#ifndef _CALENDAR_H_
#define _CALENDAR_H_

#include <ch.h>

#define SECONDS_IN_DAY 86400
#define DEFAULT_YEAR 2008
#define LEAP 1
#define NOT_LEAP 0

void completeYdayWday(struct tm *tim);
void rtcSetTM(RTCDriver *rtcp, struct tm *tim, uint16_t ds, uint8_t esHoraVerano);
void rtcGetTM(RTCDriver *rtcp, struct tm *tim, uint16_t *ds);

enum getPeriodoTarifa { punta, llano, valle };

struct fechaHora
{
    time_t secsUnix;
    uint16_t dsUnix;
};

class calendar
{
private:
    static uint16_t minAmanecer, minAnochecer;
    static time_t fechaCambioVer2Inv, fechaCambioInv2Ver;
    static struct fechaHora fechaHoraNow;
    static struct tm fechaNow;
    static float latitudRad, longitudRad;
public:
    static void ajustaFechasCambHorario(void);
    static void ajustaHorasLuz(void);
    static void ajustaHorasLuz(struct tm *now);
    static void rtcSetFecha(struct tm *fecha, uint16_t ds);
    static void init(void);
    static void setLatLong(float latitudRad, float longitudRad);
    static void updateEveryDs(void);
    static void getFecha(struct tm *fecha);
    static void getFechaHora(struct fechaHora *fechaHora);
    static time_t getSecUnix(void);
    static time_t getSecUnix(struct tm *fecha);
    static enum getPeriodoTarifa getPeriodoTarifa(void);
    static uint8_t getDOW(void);
    static uint32_t dsDiff(struct fechaHora *fechHoraOld);
    static uint32_t sDiff(time_t *timetOld);
    static uint32_t sDiff(struct fechaHora *fechaHora);
    static void checkDstFlagVerano(void);
    static uint8_t esHoraVerano();
    static uint8_t esDeNoche(void);
    static void printHoras(char *buff, uint16_t longBuff);
    static void updateUnixTime(void);
    static void enviaFechayHoraPorCAN(struct tm *fechaLocal, uint16_t ds);
    static void cambiaFecha(uint16_t *anyo, uint8_t *mes, uint8_t *dia, uint8_t *hora, uint8_t *min, uint8_t *seg, uint8_t *ds);
    static void cambiaFechaTM(uint8_t anyo, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t min, uint8_t seg, uint8_t dsPar);
    static void trataOrdenNextion(char *vars[], uint16_t numPars);
};


//void RTC_Configuration(void);
//uint16_t WeekDay(struct tm *d);
//uint8_t diasEnMes(uint8_t uint8_t_Month,uint16_t uint16_t_Year);
//uint32_t segundosEnAno(uint16_t uint16_t_Year);
//uint32_t gday(struct tm *d);
//uint8_t fechaLegal(struct tm *d);
//uint8_t esDeNoche(void);
//void d2f(uint32_t secs,struct tm *d);
//void today(struct tm *fecha);
//void incrementaFechaLocal1sec(void);
//void hallaSecsCambHorario(struct tm *fecha, uint32_t *secsInv2Ver, uint32_t *secsVer2Inv);
//void fechaUTM2Local(struct tm *fechaUTM, struct tm *fechaLocal);
//void leeFecha(void);
//void ponFechaLCD(void);
//void ponHoraLCD(void);
//void initCalendar(void);
//void arrancaCalendario(void);
//void actualizaDatosDiarios(void);
#endif
