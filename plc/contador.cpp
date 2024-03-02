/*
 * contadorAgua.cpp
 *
 *  Created on: 27 may. 2022
 *      Author: joaquin
 */


/*
 * contadorAgua.cpp
 *
 *  Created on: 16/2/2020
 *      Author: jcabe
 */


#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "bloques.h"
#include "stdlib.h"
#include "dispositivos.h"


event_source_t qePulsoAguaEvent;
static virtual_timer_t antiReboteAgua_vt;
static void qePulsoAgua_cb(void *arg);

// MEDIDA caudal"Caudal (lit/hora)" 0 0
// MEDIDA litRiego"Litros riego:" 0 0
// CONTADOR 1 litRiego caudal(lit/hora) minCaudal(lit/hora)

float contador::k = 1.0f;  // constante pulso=>cantidad
uint32_t contador::contadorImpulsos = 0;
medida   *contador::contadorCantidad = NULL;
medida   *contador::flujo = NULL; // cantidad/hora
uint32_t contador::maxMsEntrePulsos = 0; // a partir de este valor, ponemos cero como flujo
uint32_t contador::msEntrePulsos = 0;
systime_t contador::lastPulseTime = 0;
bool contador::recalcula = false;

/*
    float k;  // constante pulso=>cantidad
    medida   *contadorCantidad;
    medida   *flujo; // cantidad/hora
    uint32_t maxDsEntrePulsos; // a partir de este valor, ponemos cero como flujo
    uint32_t dsDesdeUltImpulso;
 */
contador::contador(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar != 5)
    {
        nextion::enviaLog(tty,"#parametros incorrecto CONTADOR");
        *hayError = 1;
        return; // error
    }
    // contador = #pulsos*k
    // caudal = k/tiempoSegundos*3600
    // maxTiempo = k*3600/minCaudal
    k = atof(pars[1]);
    contadorCantidad = medida::findMedida(pars[2]);
    flujo = medida::findMedida(pars[3]);
    if (contadorCantidad==NULL || flujo==NULL)
    {
        nextion::enviaLog(tty,"medidas no encontradas en CONTADOR");
        *hayError = 1;
        return; // error
    }
    float minFlujo= atof(pars[4]);
    maxMsEntrePulsos = (uint32_t) (k*3600000.0f/minFlujo);
    contadorImpulsos = 0;
    recalcula = false;
}

contador::~contador()
{
}

void contador::incrementaPulso(void)
{
    contadorImpulsos++;
    if (contadorImpulsos>1)
    {
        msEntrePulsos = chTimeI2MS(chVTTimeElapsedSinceX(lastPulseTime));
        recalcula = true;
        lastPulseTime = chVTGetSystemTimeX();
    }
}

static void activaPulsoAgua_cb(void) {
    uint8_t estadoPulso = palReadLine(LINE_PULSEXT);
    if (estadoPulso==0) // sigue pulso reciente (cuando hay corto, pasa a 0)
        contador::incrementaPulso();
    chSysLockFromISR();
    chEvtBroadcastI(&qePulsoAguaEvent);
    palEnableLineEventI(LINE_PULSEXT, PAL_EVENT_MODE_FALLING_EDGE);
    palSetLineCallbackI(LINE_PULSEXT, qePulsoAgua_cb, NULL);
    chSysUnlockFromISR();
}

static void qePulsoAgua_cb(void *arg) {
    (void)arg;
    // el pulso es falling, de duracion 65 ms
    uint8_t estadoPulso = palReadLine(LINE_PULSEXT);
    chSysLockFromISR();
    if (estadoPulso==0) // pulso reciente (cuando hay corto, pasa a 0)
    {
        // desactiva interrupciones 30 ms (no mucho) para evitar rebotes
        palDisableLineEventI(LINE_PULSEXT);
        chVTSetI(&antiReboteAgua_vt, OSAL_MS2I(30), (vtfunc_t) activaPulsoAgua_cb, NULL);
    }
    chSysUnlockFromISR();
}

const char *contador::diTipo(void)
{
    return "contador";
}

const char *contador::diNombre(void)
{
    return "contador";
}

int8_t contador::init(void)
{
    palSetLineMode(LINE_PULSEXT, PAL_MODE_INPUT);         /* PE0 */
    chVTObjectInit(&antiReboteAgua_vt);
    chEvtObjectInit(&qePulsoAguaEvent);
    /* Enabling events on falling edges of pin PE0 */
    palEnableLineEvent(LINE_PULSEXT, PAL_EVENT_MODE_FALLING_EDGE);
    palSetLineCallback(LINE_PULSEXT, qePulsoAgua_cb, NULL);
    float cero = 0.0f;
    contadorImpulsos = 0;
    lastPulseTime = 0;
    recalcula = false;
    contadorCantidad->set(&cero, 1);
    flujo->set(&cero, 1);
    return 0;
}

uint8_t contador::calcula(uint8_t , uint8_t , uint8_t , uint8_t ds)
{
    float valor;
    if (recalcula)
    {
        float conteo = k*contadorImpulsos;
        contadorCantidad->set(&conteo,0);
        if (contadorImpulsos>1)
        {
            if (msEntrePulsos<maxMsEntrePulsos)
                valor = k*3600000.0f/msEntrePulsos;
            else
                valor = 0.0f;
            flujo->set(&valor,0);
        }
        recalcula = false;
    }
    if (ds==4) // refresco contador
        contadorCantidad->setValidez(1);
    return 0;
}

//uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds
void contador::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{

}

void contador::print(BaseSequentialStream *)
{
}

void contador::statusSMS(sms *)
{
}


//extern mutex_t MtxMedidas;
//
//extern parametroU16Flash litrosPorPulso;
//
//void apuntaAgua(float incM3);
//
//
//static void activaPulsoAgua_cb(void *arg) {
//    (void)arg;
//    uint8_t estadoPulso = palReadLine(LINE_PULSEXT);
//    if (estadoPulso==0) // sigue pulso reciente (cuando hay corto, pasa a 0)
//        apuntaAgua(litrosPorPulso.valor()/1000.0f);
//    chSysLockFromISR();
//    chEvtBroadcastI(&qePulsoAguaEvent);
//    palEnableLineEventI(LINE_PULSEXT, PAL_EVENT_MODE_FALLING_EDGE);
//    palSetLineCallbackI(LINE_PULSEXT, qePulsoAgua_cb, NULL);
//    chSysUnlockFromISR();
//}
//
//static void qePulsoAgua_cb(void *arg) {
//    (void)arg;
//    // el pulso es falling, de duracion 65 ms
//    uint8_t estadoPulso = palReadLine(LINE_PULSEXT);
//    chSysLockFromISR();
//    if (estadoPulso==0) // pulso reciente (cuando hay corto, pasa a 0)
//    {
//        // desactiva interrupciones 30 ms (no mucho) para evitar rebotes
//        palDisableLineEventI(LINE_PULSEXT);
//        chVTSetI(&antiReboteAgua_vt, OSAL_MS2I(30), activaPulsoAgua_cb, NULL);
//    }
//    chSysUnlockFromISR();
//}
//
//
//void apuntaAgua(float incM3)
//{
//    struct datosPozoGuardados *datos = (struct datosPozoGuardados *) BKPSRAM_BASE;
//    // Litros total
//    datos->m3Total += incM3;
//    // ahora a las estaciones
//    if (estadoLlamaciones==0)
//        return;
//    for (uint8_t est=0;est<8;est++)
//    {
//        uint8_t estLlama = (estadoLlamaciones>>est) & 1;
//        if (estLlama==1)
//        {
//            datos->datosId[est].m3Total += incM3;
//        }
//    }
//}
//
//
//void initContAgua(void)
//{
//    palSetLineMode(LINE_PULSEXT, PAL_MODE_INPUT);         /* PE0 */
//    chVTObjectInit(&antiReboteAgua_vt);
//    chEvtObjectInit(&qePulsoAguaEvent);
//    /* Enabling events on falling edges of pin PE0 */
//    palEnableLineEvent(LINE_PULSEXT, PAL_EVENT_MODE_FALLING_EDGE);
//    palSetLineCallback(LINE_PULSEXT, qePulsoAgua_cb, NULL);
//}





