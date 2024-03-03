#ifndef _CALENDAR_H_
#define _CALENDAR_H_

#include <ch.h>

#define SECONDS_IN_DAY 86400
#define DEFAULT_YEAR 2008
#define LEAP 1
#define NOT_LEAP 0

void completeYdayWday(struct tm *tim);
void rtcSetTM(RTCDriver *rtcp, struct tm *tim, uint16_t ds);
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
    static struct fechaHora fechaHoraNow;
    static struct tm fechaNow;
    static uint16_t diaCalculado;
public:
    // funciones necesarias para leer y ajustar fechas
    static void completeYdayWday(struct tm *tim);
    static time_t getSecUnix(struct tm *fecha);
    static void setLatLong(float latitudRad, float longitudRad);
    // funciones de leey y ajustar fechas
    static void rtcGetFecha(void);
    static void rtcSetFecha(struct tm *fecha, uint16_t ds);
    static void cambiaFecha(uint16_t *anyo, uint8_t *mes, uint8_t *dia, uint8_t *hora, uint8_t *min, uint8_t *seg, uint8_t *ds);
    static void cambiaFechaTM(uint8_t anyo, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t min, uint8_t seg, uint8_t dsPar);
    // estas funciones requieren haber hecho rtcGetFecha() previamente
    static time_t getSecUnix(void);
    static void gettm(struct tm *fecha);
    static void getFechaHora(struct fechaHora *fechaHora);
    static uint8_t getDOW(void);
    // funciones auxiliares
    static void iniciaSecAdaptacion(void);
    static uint32_t dsDiff(struct fechaHora *fechHoraOld);
    static uint32_t sDiff(time_t *timetOld);
    static uint32_t sDiff(struct fechaHora *fechaHora);
    static uint8_t esDeNoche(void);
    static void addDs(int16_t dsAdd);
    static void printFecha(char *buff, uint16_t longBuff);
    static void vuelcaFecha(void);
    static void updateUnixTime(void);
};
#endif
