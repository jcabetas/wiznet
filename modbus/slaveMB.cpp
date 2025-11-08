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

#include "string.h"
#include "stdlib.h"
#include "tty.h"
#include "colas.h"
#include "modbus.h"
#include "registros.h"
#include "lcd.h"



extern "C" {
    uint8_t initModbusSlave(void);
}
/*
 * Hay dos interfaces Modbus:
 * - Modbus esclavo: recibe ajustes a grabar en holdingRegisters, y proporciona inputRegisters
 * - Modbus maestro: habla con dispositivos (vacon,..) y actualiza datos regularmente en inputRegisters del esclavo
 */

uint32_t modbusSpeed[10] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
const char *modoStr[10] = {"llamador","pozo","registrador"};


holdingRegisterInt *modbusIdHR;           // Id modbus
holdingRegisterInt2Ext *modbusBaudHR;     // baudios modbus
holdingRegisterOpciones *modoRadioHR;       // {"llamador","pozo","registrador"}
holdingRegisterInt *sTimeOutLlamadoresHR;        // tiempo olvido en s
holdingRegisterInt *idLlamadorHR;     // id en modo llamador
holdingRegisterInt *sMaxEntreMsgsLlamadorHR; // s maximo entre mensajes en modo llamador
holdingRegisterInt *dsMinEntreMsgsLlamadorHR; // ds minimo entre mensajes en modo llamador
holdingRegisterInt *bloqueaAbusonesHR; // 0 no,1 bloqueo abusones
holdingRegisterInt *minutosAbusoHR;   //
holdingRegisterInt *sMaxEntreMsgsPozoHR;  // s maximo entre mensajes en modo pozo
holdingRegisterInt *dsMinEntreMsgsPozoHR; // ds minimo entre mensajes en modo pozo
holdingRegisterFloat *barMaxSensPresionHR;     // *10
holdingRegisterInt *pideAguaHR;           // pide agua

void initHoldingRegistersControl(modbusSlave *modbusControl)
{
   // comunes
   modbusIdHR = modbusControl->addHoldingRegisterInt("IdMB", 1, 127, 2, true);
   modbusBaudHR = modbusControl->addHoldingRegisterInt2Ext("baud MB",modbusSpeed,10, 9600, true);
   modoRadioHR =  modbusControl->addHoldingRegisterOpciones("modo radio",modoStr,3, 0, true);
   sTimeOutLlamadoresHR =  modbusControl->addHoldingRegisterInt("Tiempo olvido (s)", 30, 36000, 120, true);  // timeout para que el satelite se considere desconectado
   idLlamadorHR = modbusControl->addHoldingRegisterInt("id Llamador", 1, 8, 6, true);                        // id satelite en modo llamador

   sMaxEntreMsgsLlamadorHR = modbusControl->addHoldingRegisterInt("T refresco (s)", 10, 36000, 60, true); // tiempo maximo llamadores sin enviar estado peticion
   dsMinEntreMsgsLlamadorHR = modbusControl->addHoldingRegisterInt("T intermsg (ds)", 5, 100, 30, true);  // tiempo minimo llamadores entre mensajes
   bloqueaAbusonesHR = modbusControl->addHoldingRegisterInt("Bloquea abuso", 0, 1, 1, true);
   minutosAbusoHR = modbusControl->addHoldingRegisterInt("Abuso minutos", 30, 1440, 720, true);
   sMaxEntreMsgsPozoHR = modbusControl->addHoldingRegisterInt("T refresco pozo (s)", 5, 180,5, true); // tiempo maximo pozo sin enviar estados
   dsMinEntreMsgsPozoHR = modbusControl->addHoldingRegisterInt("T interrmsg pozo (ds)", 5, 100, 20, true);    // tiempo minimo pozo entre estados
   barMaxSensPresionHR = modbusControl->addHoldingRegisterFloat("Bar max sensor*10", 1.0, 16.0, 8.0, 10.0f, true);
   pideAguaHR = modbusControl->addHoldingRegisterInt("Llamacion MB", 0, 1, 0, false);
}


inputRegister *activosIR;        // estaciones activos
inputRegister *peticionesIR;
inputRegister *abusonesIR;
inputRegister *presBarIR;        // *100


void initInputRegistersControl(modbusSlave *modbusControl)
{
   // comunes
   activosIR = modbusControl->addInputRegister("Activos");
   peticionesIR = modbusControl->addInputRegister("Peticiones");
   abusonesIR = modbusControl->addInputRegister("Abusones");
   presBarIR = modbusControl->addInputRegister("Presion");            // *100
}


thread_t *slaveMBThread = NULL;
modbusSlave *controlMB;

static THD_WORKING_AREA(wamodbus, 3000);
static THD_FUNCTION(modbusThrd, arg) {
    (void)arg;
    uint8_t contador = 0;
    uint8_t buffer[256];
    uint16_t bytesReceived;
    chRegSetThreadName("modbus");
    while (true) {
        controlMB->esperaSilencioModbus();
        bool hayMsg = controlMB->readModbus(buffer, sizeof(buffer),&bytesReceived, chTimeMS2I(5000));
        if (hayMsg)
        {
            controlMB->interpretaMsg(contador, buffer, bytesReceived);
            if (++contador > 9)
                contador = 0;
        }
        if (chThdShouldTerminateX())
        {
            chThdExit((msg_t) 1);
        }
    }
}



/*
 * Modbus para recibir ordenes
 */
uint8_t initModbusSlave(void)
{
    palSetLineMode(LINE_TX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_RX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OSPEED_HIGHEST);
    palClearLine(LINE_TXRX2);
    palSetLineMode(LINE_TXRX2, PAL_MODE_OUTPUT_PUSHPULL);
    controlMB = new modbusSlave(&SD2, LINE_TXRX2);
    holdingRegister::setMBSlave(controlMB);
    initHoldingRegistersControl(controlMB);
    initInputRegistersControl(controlMB);
    controlMB->leeHR();
    controlMB->startMBSerial(modbusBaudHR->getValor());
    if (slaveMBThread==NULL)
        slaveMBThread = chThdCreateStatic(wamodbus, sizeof(wamodbus), NORMALPRIO, modbusThrd, NULL);
    else
        return 1;
    return 0;
}
