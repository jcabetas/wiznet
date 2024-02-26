/*
 * gestor.cpp
 *
 *  Created on: 5 jul 2023
 *      Author: joaquin
 */

#include "ch.h"
#include "hal.h"
#include "varsFlash.h"
#include "colas.h"
#include "sms.h"
#include "jaula.h"

/*
* Procedimiento
* - Thread GSM:
*   * Estados: "No listo", "listo"
*   * Inicializa GSM
*   * Queda a la escucha de SMS, lo decodifica y responde a las ordenes ("STATUS", "TELEFONO","ACTIVA" y "DESACTIVA")
*   * También espera ordenes de transmision de estados por SMS
* - Thread VL053:
*   * Estados: "No inicializado", "Esperando boton", "Espero estabilidad", "Esperando gato", "Detecto gato"
*   * Inicializa VL053
*   * Cuando se apriete el boton 2s y reciba su evento:
*     - Mido alturas, que tienen que ser estables por 5 segundos
*     - Si son estables, lanza evento "VL53listo" y se mete en bucle monitorización. Si no, lanza evento "VL53inestable"
*   * Si recibe evento "desactiva" y no esta en "No inicializado" se pone en "Esperando boton"
*   * En modo "Esperando gato", si detecta disminucion de 5 cm de altura durante 2s, lanza evento "DetectadoGato"
* - Thread main
*   * Inicializa otros threads
*   * Espera eventos y los organiza
* - Thread LCD
*   * Manejo por colas
*/

extern event_source_t updateLCD_source;
extern event_source_t enviarSMS_source;

void cierraPuerta(void);
void abrePuerta(void);

char nomEstado[5][20] = {"Iniciando","Espero activacion","Midiendo altura","Espero gato","Gato en jaula"};
char nomEstadoSSD1306[5][20] = {"Iniciando","Pulsa!!    ","Midiendo...","Espero gato","Cerrado!!  "};

enum estado_t {esperoThreads=1, esperoBoton, esperoEstabilidad, esperoGato, gatoEnJaula};
enum estado_t estado;

extern char nomEstadoVL53[7][20];// = {"Iniciando","Espero activar","Mido altura","Espero gato","Hay gato"};
enum estadoVL53_t {vl53iniciando=1, vl53esperoActivacion, vl53midoAltura, vl53esperoGato, vl53hayGato};
extern enum estadoVL53_t estadoVL53;
extern event_source_t cambioEnVL53_source;

event_source_t activate_source, desactivate_source;
thread_t *procesoGestor = NULL;


extern "C"
{
    void initGestor(void);
}

void ponEstadoLCD(void)
{
    if (estado > 5)
        return;
#ifdef LCD
    ponEnColaLCD(3,nomEstado[estado-1]);
#endif
#ifdef SSD1306
    ponEnColaLCD(2,nomEstadoSSD1306[estado-1]);
#endif

}

uint8_t trataCambios(uint8_t botonPulsado)
{
    enum estado_t estadoOld = estado;
    uint8_t smsOk = sms::diSmsReady();
    if (estado>1 && (estadoVL53==vl53iniciando || !smsOk))
    {
        estado = esperoThreads;
        return 0;
    }
    switch (estado)
    {
        case esperoThreads:
            if (smsOk && estadoVL53==vl53esperoActivacion)
            {
                estado = esperoBoton;
            }
            break;
        case esperoBoton:
            if (botonPulsado)
            {
                chEvtBroadcast(&activate_source);
                estado = esperoEstabilidad;
            }
            break;
        case esperoEstabilidad:
            if (estadoVL53 == vl53esperoGato)
                estado = esperoGato;
            break;
        case esperoGato:
            if (botonPulsado) // desactivate
            {
                chEvtBroadcast(&desactivate_source);
                estado = esperoBoton;
                break;
            }
            if (estadoVL53 == vl53hayGato)
            {
                estado = gatoEnJaula;
                cierraPuerta();
                abrePuerta();  // para bloquear la puerta
                // envia mensaje
                sms::borraMsgRespuesta();
                sms::ponEstado();
                chEvtBroadcast(&enviarSMS_source);
            }
            break;
        case gatoEnJaula:
            break;
    }
    if (estadoOld!=estado)
        ponEstadoLCD();
    return (estadoOld!=estado);
}


/*
 * sensores thread.
 * gestiona interrupciones de los sensores
 */
static THD_WORKING_AREA(waGestor, 512);
static THD_FUNCTION(threadGestor, p) {
    (void)p;
    eventmask_t evt;
    uint16_t dsPulsado, dsObsoleto, botonPulsado;
    uint8_t hayCambios;
    event_listener_t cambioEnVL53Listener,activateListener, desactivateListener;
    chRegSetThreadName("gestor");
    dsPulsado = 0;
    dsObsoleto = 0;
    palSetLineMode(LINE_SENSOR, PAL_MODE_INPUT);
    chEvtRegisterMask(&cambioEnVL53_source, &cambioEnVL53Listener, EVENT_MASK(0));
    chEvtRegisterMask(&activate_source, &activateListener, EVENT_MASK(1));
    chEvtRegisterMask(&desactivate_source, &desactivateListener, EVENT_MASK(2));
    while(!chThdShouldTerminateX()) {
        evt = chEvtWaitAnyTimeout(ALL_EVENTS, TIME_MS2I(100));
        if (chThdShouldTerminateX())
        {
            chEvtUnregister(&cambioEnVL53_source, &cambioEnVL53Listener);
            procesoGestor = NULL;
            chThdExit((msg_t) 1);
        }
        if (evt==0) // 100 ms timeout
        {
            botonPulsado = 0;
            if (palReadPad(GPIOA, GPIOA_KEY) && palReadLine(LINE_SENSOR)) // se ha pulsado alguno? si es asi, incrementa dsPulsado
                dsPulsado = 0;
            else
            {
                if (++dsPulsado==20) // al llegar a 20, notifica boton
                botonPulsado = 1;
            }
            if (botonPulsado) // se ha apretado el boton
            {
                hayCambios = trataCambios(botonPulsado);
                if (hayCambios)
                    ponEstadoLCD();
            }
            dsObsoleto += 100;
            if (dsObsoleto>5000)
            {
                ponEstadoLCD();
                dsObsoleto = 0;
            }
            continue;
        }
        if (evt & EVENT_MASK(0)) // cambios en jaula
        {
            trataCambios(0);
            ponEstadoLCD();
        }
        if (evt & EVENT_MASK(1)) // orden de activacion
        {
            if (estado == esperoBoton)
            {
                estado = esperoEstabilidad;
                ponEstadoLCD();
            }
        }
        if (evt & EVENT_MASK(2)) // orden de desactivacion
        {
            if (estado == esperoEstabilidad || estado == esperoGato)
            {
                estado = esperoBoton;
                ponEstadoLCD();
            }
        }
    }
}


void initGestor(void)
{
    estado = esperoThreads;
    if (estadoVL53==vl53esperoActivacion && sms::diSmsReady())
        estado = esperoBoton;
    ponEstadoLCD();
    if (!procesoGestor)
        procesoGestor = chThdCreateStatic(waGestor, sizeof(waGestor), NORMALPRIO, threadGestor, NULL);
}
