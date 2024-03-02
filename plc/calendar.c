



/*#include <ch.h>
#include "board.h"
#undef TRUE
#undef FALSE
#include "stm32f10x_rtc.h"
#include "stm32f10x_rcc.h"
//#include "stm32f10x_pwr.h"
#include "calendar.h"
#include "gets.h"
#include "printf.h"
#include "lcd.h"

#undef DEBUG
#define _RTC


#define PWR_OFFSET               (PWR_BASE - PERIPH_BASE)
#define CR_OFFSET                (PWR_OFFSET + 0x00)
#define DBP_BitNumber            0x08
#define CR_DBP_BB                (PERIPH_BB_BASE + (CR_OFFSET * 32) + (DBP_BitNumber * 4))
#define RCC_OFFSET                (RCC_BASE - PERIPH_BASE)
#define BDCR_OFFSET              (RCC_OFFSET + 0x20)
#define BDCR_ADDRESS             (PERIPH_BASE + BDCR_OFFSET)
#define RTCEN_BitNumber           0x0F
#define BDCR_RTCEN_BB             (PERIPH_BB_BASE + (BDCR_OFFSET * 32) + (RTCEN_BitNumber * 4))
*/


/* Minuto del dia cuando amanece */
const uint16_t minAmanecerPorSemana[] = {446, 443, 438, 432, 426, 418, 410, 401, 392, 383, 374, 364, 354, 345, 335, 326, 317, 309, 301, 294,
										 288, 283, 279, 276, 275, 276, 278, 281, 286, 292, 298, 306, 314, 323, 332, 341, 351, 361, 370, 380, 389,
										 398, 407, 415, 423, 430, 436, 441, 445, 447, 448, 448, 446, 443};
const uint16_t minAnochecerPorSemana[] = {1024, 1027, 1031, 1037, 1044, 1051, 1059, 1068, 1077, 1086, 1096, 1105, 1115, 1125, 1134, 1143, 1152,
										  1161, 1168, 1176, 1182, 1187, 1191, 1193, 1194, 1194, 1192, 1188, 1184, 1178, 1171, 1163, 1155, 1146,
										  1137, 1128, 1118, 1109, 1099, 1090, 1080, 1071, 1062, 1054, 1046, 1039, 1033, 1028, 1025, 1022, 1021,
										  1021, 1023, 1026};
/*
extern uint32_t secsUTM;
extern struct sdate fechaLocal;
extern Mutex MtxSecs;

/*
 * Variables que deben ser actualizadas cuando arranca calendario y pasadas las 24h de cada dia.
 */
uint16_t minUTMAmanecer,minUTMAnochecer;
uint32_t segundosUTMCambioInv2Ver, segundosUTMCambioVer2Inv;

void hallaMinAmanecer(void);

/*******************************************************************************
* Function Name  : CheckLeap
* Description    : Checks whether the passed year is Leap or not.
* Input          : None
* Output         : None
* Return         : 1: leap year
*                  0: not leap year
*******************************************************************************/
uint8_t CheckLeap(uint16_t uint16_t_Year)
{
  if((uint16_t_Year%400)==0)
  {
    return LEAP;
  }
  else if((uint16_t_Year%100)==0)
  {
    return NOT_LEAP;
  }
  else if((uint16_t_Year%4)==0)
  {
    return LEAP;
  }
  else
  {
    return NOT_LEAP;
  }
}

/*******************************************************************************
* Function Name  :WeekDay
* Description    :Determines the weekday
* Input          :Year,Month and Day
* Output         :None
* Return         :Returns the CurrentWeekDay Number 0- Sunday 6- Saturday
*******************************************************************************/
uint16_t WeekDay(struct sdate d)
{
	uint16_t uint16_t_Temp1,uint16_t_Temp2,uint16_t_Temp3,uint16_t_Temp4,uint16_t_CurrentWeekDay;
	uint16_t cYear = d.y;
	uint8_t cMonth = d.m;
	if(d.m < 3)
	{
		cMonth += 12;
		cYear -= 1;
	}

	uint16_t_Temp1=(6*(cMonth + 1))/10;
	uint16_t_Temp2=cYear/4;
	uint16_t_Temp3=cYear/100;
	uint16_t_Temp4=cYear/400;
	uint16_t_CurrentWeekDay=d.d + (2 * cMonth) + uint16_t_Temp1 \
	 + cYear + uint16_t_Temp2 - uint16_t_Temp3 + uint16_t_Temp4 +1;
	uint16_t_CurrentWeekDay = uint16_t_CurrentWeekDay % 7;

	return(uint16_t_CurrentWeekDay);
}


void today(struct sdate *fecha)
{
    chMtxLock(&MtxSecs);
    fecha->y = fechaLocal.y;
    fecha->m = fechaLocal.m;
    fecha->d = fechaLocal.d;
    fecha->h = fechaLocal.h;
    fecha->min = fechaLocal.min;
    fecha->s = fechaLocal.s;
    chMtxUnlock();
}

void fechaLocal2UTM(struct sdate *fechaLocal, struct sdate *fechaUTM, uint32_t *secsUTM, uint32_t secsInv2Ver, uint32_t secsVer2Inv)
{
	uint32_t secsLocal;
	secsLocal = gday(fechaLocal);
	// pruebo a ver si es invierno
	*secsUTM = secsLocal - 3600;
	if (*secsUTM>secsInv2Ver && *secsUTM<secsVer2Inv)
		*secsUTM = secsLocal - 7200;
	d2f(*secsUTM,fechaUTM);
}

void fechaUTM2Local(struct sdate *fechaUTM, struct sdate *fechaLocal)
{
	uint32_t secsUTM, secsLocal;
	secsUTM = gday(fechaUTM);
	// pruebo a ver si es invierno
	secsLocal = secsUTM + 3600;
	if (secsLocal>segundosUTMCambioInv2Ver && secsLocal<segundosUTMCambioVer2Inv)
		secsLocal = secsUTM + 7200;
	d2f(secsLocal,fechaLocal);
}

uint8_t diasEnMes(uint8_t uint8_t_Month,uint16_t uint16_t_Year)
{
	const uint8_t monthsDays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
	if (uint8_t_Month<1 || uint8_t_Month>12) return (uint8_t) 0;
	uint8_t dias = monthsDays[uint8_t_Month-1];
	if (uint8_t_Month==2 && CheckLeap(uint16_t_Year)==LEAP)
		return 29;
	else
		return dias;
}

uint32_t segundosEnAno(uint16_t uint16_t_Year)
{
	if (CheckLeap(uint16_t_Year)!=LEAP)
		return 365*SECONDS_IN_DAY;
	else
		return 366*SECONDS_IN_DAY;
}





uint32_t gday(struct sdate *d)
{
	uint32_t segundos = 0;
	uint16_t year = DEFAULT_YEAR;
	uint8_t mes = 1;
	for (;year<d->y;year++)
	   segundos += segundosEnAno(year);
	for (;mes<d->m;mes++)
	   segundos += SECONDS_IN_DAY*diasEnMes(mes,year);
	segundos += (d->d-1)*SECONDS_IN_DAY+d->h*3600+d->min*60+d->s;
	return segundos;
}

uint8_t fechaLegal(struct sdate *d)
{
	if (d->y<DEFAULT_YEAR || d->y>DEFAULT_YEAR+135) return 0;
	if (d->m<1 || d->m>12) return 0;
	if (d->h>23) return 0;
	if (d->min>59) return 0;
	if (d->s>59) return 0;
	if (d->d<1 || d->d>diasEnMes(d->m,d->y)) return 0;
	return 1;
}

void d2f(uint32_t secs,struct sdate *d)
{
	d->y = DEFAULT_YEAR;
	while (secs>=segundosEnAno(d->y))
	{
		secs -= segundosEnAno(d->y);
		d->y++;
	}
	d->m = 1;
	while (secs>SECONDS_IN_DAY*diasEnMes(d->m,d->y))
	{
		secs -= SECONDS_IN_DAY*diasEnMes(d->m,d->y);
		d->m++;
	}
	d->d = 1+(secs / SECONDS_IN_DAY);
	secs -= (d->d-1)*SECONDS_IN_DAY;
	d->h = secs /3600;
	secs -= d->h*3600;
	d->min = secs /60;
	secs -= d->min*60;
	d->s = secs;
	d->DOW = WeekDay(*d);
}



void leeFecha(void)
{
	struct sdate fechaLeida, fechaUTM;
	int16_t numero;
	ponEnLCD(0, "");
	ponEnLCD(1, "");
	ponEnLCD(2, "");
	ponEnLCD(3, "");
	numero = leeNumeroMsg(fechaLocal.y, 0, 1,"anyo: ");
	if (numero<0) return; else fechaLeida.y = (uint16_t) numero;
	numero = leeNumeroMsg(fechaLocal.m, 0, 1,"mes: ");
	if (numero<0) return; else fechaLeida.m = (uint8_t) numero;
	numero = leeNumeroMsg(fechaLocal.d, 0, 1,"dia: ");
	if (numero<0) return; else fechaLeida.d = (uint8_t) numero;
	numero = leeNumeroMsg(fechaLocal.h, 0, 1,"hora: ");
	if (numero<0) return; else fechaLeida.h = (uint8_t) numero;
	numero = leeNumeroMsg(fechaLocal.min, 0, 1,"minuto: ");
	if (numero<0) return; else fechaLeida.min = (uint8_t) numero;
	fechaLeida.s = 0;
	if (fechaLegal(&fechaLeida))
	{
		hallaSecsCambHorario(&fechaLeida, &segundosUTMCambioInv2Ver, &segundosUTMCambioVer2Inv);
	    chMtxLock(&MtxSecs);
		fechaLocal2UTM(&fechaLeida, &fechaUTM, &secsUTM, segundosUTMCambioInv2Ver, segundosUTMCambioVer2Inv);
		RTC_SetCounter(secsUTM);
		fechaLocal.y = fechaLeida.y;
	    fechaLocal.m = fechaLeida.m;
	    fechaLocal.d = fechaLeida.d;
	    fechaLocal.h = fechaLeida.h;
	    fechaLocal.min = fechaLeida.min;
	    fechaLocal.s = fechaLeida.s;
	    fechaLocal.DOW = WeekDay(fechaLocal);
	    chMtxUnlock();
	    hallaMinAmanecer();
	    ponFechaLCD();
        chThdSleepMilliseconds(2000);
	}
	else
	{
		ponEnLCD(3, "Fecha ilegal!!");
		chThdSleepMilliseconds(1000);
	}
}


void hallaMinAmanecer(void)
{
	// partiendo de secsUTM, hallo el resto de datos
	// uint16_t minUTMAmanecer,minUTMAnochecer,diaSemanaLocal;
	uint32_t sec1EneroUTM, secElapsed, semanaAnyo;
	struct sdate inicioAnyo, nowUTM;
	d2f(secsUTM, &nowUTM);
	fechaUTM2Local(&nowUTM, &fechaLocal);
	inicioAnyo.y = nowUTM.y;
	inicioAnyo.m = 1;
	inicioAnyo.d = 1;
	inicioAnyo.h = 0;
	inicioAnyo.min = 0;
	inicioAnyo.s = 0;
	sec1EneroUTM = gday(&inicioAnyo);
	secElapsed = secsUTM - sec1EneroUTM;
	semanaAnyo = secElapsed/604800; // segundosporsemana = 3600*24*7
	if (semanaAnyo>=sizeof(minAmanecerPorSemana))
		minUTMAmanecer = 439; // por decir algo
	else
		minUTMAmanecer = minAmanecerPorSemana[semanaAnyo];
	if (semanaAnyo>=sizeof(minAnochecerPorSemana))
		minUTMAnochecer = 1030;
	else
		minUTMAnochecer = minAnochecerPorSemana[semanaAnyo];
}


void hallaSecsCambHorario(struct sdate *fecha, uint32_t *secsInv2Ver, uint32_t *secsVer2Inv)
{
	uint16_t diaSemana;

	// busco el último domingo de octubre
	struct sdate fechaCambio;
	fechaCambio.y = fecha->y;
	fechaCambio.m = 3;
	fechaCambio.d = 31;
	fechaCambio.h = 1;
	fechaCambio.min = 0;
	fechaCambio.s = 0;
	diaSemana = WeekDay(fechaCambio); // 0- Sunday 6- Saturday
	*secsInv2Ver = gday(&fechaCambio); // despues hay que descontar precisamente el dia de la semana (para pillar el domingo)
	*secsInv2Ver -= diaSemana*86400; // 86400 = 24*60*60
	// repetimos para el cambio de octubre
	fechaCambio.m = 10;
	diaSemana = WeekDay(fechaCambio); // 0- Sunday 6- Saturday
	*secsVer2Inv = gday(&fechaCambio); // despues hay que descontar precisamente el dia de la semana (para pillar el domingo)
	*secsVer2Inv -= diaSemana*86400; // 86400 = 24*60*60
}

void incrementaFechaLocal1sec(void)
{
	uint8_t diasMes;
	if (secsUTM==segundosUTMCambioInv2Ver)
		fechaLocal.h++;
	if (secsUTM==segundosUTMCambioVer2Inv)
		fechaLocal.h--;
	fechaLocal.s++;
	if (fechaLocal.s < 60) return;
	fechaLocal.s = 0;
	fechaLocal.min++;
	if (fechaLocal.min < 60) return;
	fechaLocal.min = 0;
	fechaLocal.h++;
	//chprintf("Secs interno:%d  cuenta:%d\n",RTC_GetCounter(),secsUTM);
	if (fechaLocal.h < 24) return;
	// salto de dia
    RTC_SetCounter(secsUTM); // cada dia actualiza el reloj interno
	fechaLocal.DOW++;
	if (fechaLocal.DOW > 6) fechaLocal.DOW = 0;
	fechaLocal.d++;
	diasMes = diasEnMes(fechaLocal.m, fechaLocal.y);
	hallaMinAmanecer();
	if (fechaLocal.d <= diasMes) return;
	// salto de mes
	fechaLocal.d = 1;
	fechaLocal.m++;
	if (fechaLocal.m <= 12) return;
	// salto de año
	fechaLocal.m = 1;
	fechaLocal.y++;
	hallaSecsCambHorario(&fechaLocal, &segundosUTMCambioInv2Ver, &segundosUTMCambioVer2Inv);
}

uint8_t esHorarioInvierno(void)
{
	uint32_t segundos;
    chMtxLock(&MtxSecs);
    segundos = secsUTM;
    chMtxUnlock();
	if (segundos>=segundosUTMCambioInv2Ver && segundos<segundosUTMCambioVer2Inv)
		return 0;
	else
		return 1;
}


/*
 * uint16_t minUTMAmanecer,minUTMAnochecer,diaSemanaLocal;
uint32_t segundosUTMCambioInv2Ver, segundosUTMCambioVer2Inv;
 */
uint8_t esDeNoche(void)
{
	uint16_t minAhoraLocal,minAhoraUTM;
    chMtxLock(&MtxSecs);
    minAhoraLocal = 60*((uint16_t)fechaLocal.h)+((uint16_t)fechaLocal.min);
    chMtxUnlock();
	if (esHorarioInvierno())
		minAhoraUTM = minAhoraLocal - 60;
	else
		minAhoraUTM = minAhoraLocal - 120;
    if (minAhoraUTM<minUTMAmanecer || minAhoraUTM>minUTMAnochecer)
    	return 1;
    else
    	return 0;
}


void ponFechaLCD(void)
{
	char buf[20];
    //chMtxLock(&MtxSecs);
	chsprintf(buf, "%02d:%02d:%02d %d/%d/%d",fechaLocal.h,fechaLocal.min,fechaLocal.s,fechaLocal.d,fechaLocal.m,fechaLocal.y);
    //chMtxUnlock();
    ponEnLCD(3,buf);
	chThdSleepMilliseconds(2000);
}


void RTC_Configuration(void)
{
	struct sdate fechaUTM;
	/* Enable PWR and BKP clocks */
	//RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	RCC->APB1ENR |= (RCC_APB1Periph_PWR | RCC_APB1Periph_BKP);

	/* Allow access to BKP Domain */
	//PWR_BackupAccessCmd(ENABLE);
	*(vu32 *) CR_DBP_BB = (u32)ENABLE;

	/* Reset Backup Domain */
	//BKP_DeInit();

	/* Enable LSE */
	//RCC_LSEConfig(RCC_LSE_ON);
	*(vu8 *) BDCR_ADDRESS = RCC_LSE_OFF;
	/* Reset LSEBYP bit */
	*(vu8 *) BDCR_ADDRESS = RCC_LSE_OFF;
	/* Configure LSE (RCC_LSE_OFF is already covered by the code section above) */
	*(vu8 *) BDCR_ADDRESS = RCC_LSE_ON;

	/* Wait till LSE is ready */
	//  while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET)
	//{}
	while (!(RCC->BDCR & RCC_FLAG_LSERDY)) ;

	/* Select LSE as RTC Clock Source */
	//RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	RCC->BDCR |= RCC_RTCCLKSource_LSE;

	/* Enable RTC Clock */
	//RCC_RTCCLKCmd(ENABLE);
	*(vu32 *) BDCR_RTCEN_BB = (u32)ENABLE;

	/* Wait for RTC registers synchronization */
	RTC_WaitForSynchro();

	/* Wait until last write operation on RTC registers has finished */
	RTC_WaitForLastTask();

	/* Enable the RTC Second */
	RTC_ITConfig(RTC_IT_SEC, ENABLE);

	/* Wait until last write operation on RTC registers has finished */
	RTC_WaitForLastTask();

	/* Set RTC prescaler: set RTC period to 1sec */
	RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */

	/* Wait until last write operation on RTC registers has finished */
	RTC_WaitForLastTask();
	/* lee el estado del contador */
    chMtxLock(&MtxSecs);
    secsUTM = RTC_GetCounter();
    d2f(secsUTM,&fechaUTM);
    fechaUTM2Local(&fechaUTM, &fechaLocal);
	hallaSecsCambHorario(&fechaLocal, &segundosUTMCambioInv2Ver, &segundosUTMCambioVer2Inv);
    hallaMinAmanecer();
    chMtxUnlock();
}
