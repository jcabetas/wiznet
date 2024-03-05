/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "modbus.h"
#include "string.h"
#include "stdlib.h"
#include "tty.h"
#include "colas.h"
#include "modbus.h"


uint16_t modbus::baudios = 9600;
uint8_t modbus::definido = 0;
modbus *modbus::modbusPtr = NULL;
thread_t *modbus::procesoModbus = NULL;
dispositivo *modbus::listDispositivosMB[MAXDISPOSITIVOS] = {0};
uint8_t modbus::errorEnDispMB[MAXDISPOSITIVOS] = {0};
uint32_t  modbus::numErrores[MAXDISPOSITIVOS] = {0};
uint32_t  modbus::numMs[MAXDISPOSITIVOS] = {0};
uint32_t  modbus::numAccesos[MAXDISPOSITIVOS] = {0};
float modbus::tMedio[MAXDISPOSITIVOS] = {0};
uint16_t modbus::numDispositivosMB = 0;

extern "C"
{
    void testMB(void);
}


uint16_t CRC16(const uint8_t *nData, uint16_t wLength);
event_source_t testMB_source;


static THD_WORKING_AREA(wamodbus, 3000);
static THD_FUNCTION(modbusThrd, arg) {
    (void)arg;
    event_listener_t el0;
    chRegSetThreadName("modbus");
    uint16_t iter = 0;
    chEvtRegister(&testMB_source, &el0, 0);
    while (true) {
        if (++iter == 100)
        {
            modbus::leeTodos(1); // aunque tenga error, reitero cada 10s
            iter = 0;
        }
        modbus::leeTodos(0);
        //chThdSleepMilliseconds(20);
        if (chThdShouldTerminateX())
        {
            chEvtUnregister(&testMB_source, &el0);
            chThdExit((msg_t) 1);
        }
        chThdSleepMilliseconds(100);
    }
}


/*
- Crear dispositivo que use el MODBUS, p.e. SDM120CT con direccion=2
   SDM120CT MedidorFlexo MB1 2 (nombreDispo, InterfaceModbus, direccion)
*/
/*
class SDM120CT : public bloque
{
    //SDM120CT MedidorFlexo MB1 2
protected:
    uint16_t numModBus;
    uint16_t idNombreDisp;
    uint16_t direccion;
public:
    sdm120ct(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~sdm120ct();
    int16_t chReadStrRs485(uint8_t *buffer, uint16_t numBytesExpected, uint16_t *bytesReceived,  sysinterval_t timeout);
    int16_t chprintStrRs485(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};

 Como funciona:
 - Crear IO modbus, con MODBUS (Numero de modbus, interfaceModbus, baudios)
    MODBUS 1 MB1 9600
 - Crear dispositivo que use el MODBUS, p.e. SDM120CT con direccion=2
    SDM120CT MedidorFlexo MB1 2 (nombreDispo, InterfaceModbus, direccion)
 - Crear medidas, actualizable cada 3s y 20s
    MEDIDA WFlexo MedidorFlexo W 3 (nombreMedida, nombreDispo, nombreDatos, periodicidad)
                                    + fechaUltimaMedida, esValida, errorMedida
    MEDIDA Tension MedidorFlexo V 20s
 - Utilizar la medida
    ASIGNAVAR intensidad WFlexo Tension /
    ASIGNAESTADO encendido.base intensidad 2 >

 Metodo:
 - Sólo hacemos esto a nivel de Modbus
 - Thread a nivel de Modbus.
   Ese thread conoce los dispositivos conectados.
   Los dispositivos deben implementar:
   * usoBus(): debe mirar si necesita actualizar datos, y si procede usar el bus.
               devuelve el error. Si un dispositivo da error, sólo se reintentará cada 10s
   El sdm120CT debe tener punteros a las medidas
   Las medidas deben guardar antiguedad de la lectura
 */




// MODBUS 9600
modbus::modbus(uint16_t baudiosPar)
{

    if (definido)
    {
        chprintf((BaseSequentialStream *)&SD1,"Error: solo dbe haber un objeto MODBUS\n");
        return;
    }
    baudios = baudiosPar;
    for (uint8_t d=1;d<MAXDISPOSITIVOS;d++)
    {
        listDispositivosMB[d-1] = NULL;
        errorEnDispMB[d-1] = 0;
    }
    numDispositivosMB = 0;
    procesoModbus = NULL;
    modbusPtr = this;
    definido = 1;
};

uint32_t modbus::diBaudios(void)
{
    return baudios;
}


void modbus::deleteMB(void)
{
    for (uint8_t d=0;d<MAXDISPOSITIVOS;d++)
        listDispositivosMB[d] = NULL;
    numDispositivosMB = 0;
    definido = 0;
}

void modbus::addDisp(dispositivo *disp)
{
    if (numDispositivosMB>=MAXDISPOSITIVOS)
    {
        chprintf((BaseSequentialStream *)&SD1,"Error, demasiados dispositivos\n");
        return;
    }
    listDispositivosMB[numDispositivosMB] = disp;
    errorEnDispMB[numDispositivosMB] = 0;
    numDispositivosMB++;
}

void modbus::leeTodos(uint8_t incluyeErroneos)
{
    uint8_t error;
    /*
     *      static uint32_t  numMs[MAXDISPOSITIVOS];
            static uint32_t  numAccesos[MAXDISPOSITIVOS];
            static float tMedio[MAXDISPOSITIVOS];
     */
    systime_t tIni, tFin;
    for (uint8_t d=1;d<=numDispositivosMB;d++)
    {
        if (incluyeErroneos || errorEnDispMB[d-1]==0)
        {
            dispositivo *disp = listDispositivosMB[d-1];
            if (disp!=NULL)
            {
                tIni = chVTGetSystemTimeX();
                error = disp->usaBus(); // 0: no error, 1: error, 2: no se ha usado
                if (error==1 && errorEnDispMB[d-1]!=1)
                {
                    numErrores[d-1]++;
                    error = disp->usaBus(); // si es un error suelto, reintenta
                }
                if (error==0)
                {
                    tFin = chVTGetSystemTimeX();
                    numMs[d-1] += chTimeI2MS(chTimeDiffX(tIni, tFin));
                    numAccesos[d-1]++;
                    tMedio[d-1] = (float) (numMs[d-1]/numAccesos[d-1]);
                }
                if (error!=2 && error != errorEnDispMB[d-1])
                {
                    if (error)
                        chprintf((BaseSequentialStream *)&SD1,"Error en %s\n",disp->diNombre());
                    else
                        chprintf((BaseSequentialStream *)&SD1,"Recuperado eror de %s\n",disp->diNombre());
                }
                if (error!=2)
                    errorEnDispMB[d-1] = error;
            }
        }
    }
}

int16_t modbus::chReadStrRs485(uint8_t *buffer, uint16_t numBytesExpected, uint16_t *bytesReceived,  sysinterval_t timeout)
{
    size_t nb;
    palClearLine(LINE_TXRX);
    nb = sdReadTimeout(&SD2,buffer,numBytesExpected,timeout);
    *bytesReceived = nb;
    if (*bytesReceived != numBytesExpected) return 1;
    return 0;
}

int16_t modbus::chprintStrRs485(uint8_t *str, uint16_t lenStr, sysinterval_t timeout)
{
    eventmask_t evt;
    event_listener_t endEot_event;
    chEvtRegisterMaskWithFlags (chnGetEventSource(&SD2),&endEot_event, EVENT_MASK (0),CHN_TRANSMISSION_END);
    palSetLine(LINE_TXRX);
    chThdSleepMilliseconds(2);
    sdWrite(&SD2, str,lenStr);
    while (true)
    {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, timeout);
        if (evt==0) // Timeout
            break;
        if (evt & EVENT_MASK(0)) // Evento fin de transmision, limpio RX/TX
            break;
    }
    palClearLine(LINE_TXRX);
    chEvtUnregister(chnGetEventSource(&SD2),&endEot_event);
    if (evt==0)
        return 1;
    return 0;
}

int8_t modbus::init(void)
{
    SerialConfig configSD2;
    palSetLineMode(LINE_TX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_RX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palClearLine(LINE_TXRX);
    palSetLineMode(LINE_TXRX, PAL_MODE_OUTPUT_PUSHPULL);
    configSD2.speed = baudios;
    configSD2.cr1 = 0;
    configSD2.cr2 = USART_CR2_STOP1_BITS;// | USART_CR2_LINEN;;
    configSD2.cr3 = 0;
    sdStart(&SD2, &configSD2);
    if (definido && modbusPtr!=NULL && procesoModbus==NULL)
        procesoModbus = chThdCreateStatic(wamodbus, sizeof(wamodbus), NORMALPRIO, modbusThrd, NULL);
    else
        return 1;
    return 0;
}

void modbus::stop(void)
{
    if (procesoModbus!=NULL)
    {
        chThdTerminate(procesoModbus);
        chThdWait(procesoModbus);
        procesoModbus = NULL;
    }
    sdStop(&SD2);
}

modbus::~modbus()
{
    numDispositivosMB = 0;
    procesoModbus = NULL;
    modbusPtr = NULL;
    definido = 0;
}

const char *modbus::diTipo(void)
{
    return "modbus";
}

const char *modbus::diNombre(void)
{
    return "MODBUS";
}

void modbus::addDsMB(uint16_t ds)
{
    for (uint8_t d=1;d<=numDispositivosMB;d++)
    {
        dispositivo *disp = listDispositivosMB[d-1];
        if (disp!=NULL)
        {
            disp->addDs(ds);
        }
    }
}

void modbus::print(void)
{
    chprintf((BaseSequentialStream *)&SD1,"MODBUS %d baud, %d dispositivos\n",baudios, numDispositivosMB);
}

float medidaV;

void testMB(void)
{
    modbus *modbusObj = new modbus(9600);
    sdm120ct *medflexo = new sdm120ct("flexo",2);
    medflexo->attachMedida(&medidaV, "V", 10, "tension flexo");
    modbusObj->addDisp(medflexo);
    modbusObj->init();

}
