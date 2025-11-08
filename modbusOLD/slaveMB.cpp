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

#include "variables.h"
#include "string.h"
#include "stdlib.h"
#include "tty.h"
#include "variables.h"
#include "colas.h"
#include "modbus.h"
#include "registros.h"

/*
 * Hay dos interfaces Modbus:
 * - Modbus esclavo: recibe ajustes a grabar en holdingRegisters, y proporciona inputRegisters
 * - Modbus maestro: habla con dispositivos (vacon,..) y actualiza datos regularmente en inputRegisters
 */

const uint32_t modbusSpeed[6] = {1200, 4800, 9600, 19200, 57600, 115200};
const char *modoRadioStr[3] = {"llamador","pozo","registrador"};
const char *controlPozoStr[3] = {"off","sensor","radio"};
const char *controlLlamadorStr[3] = {"off","sensor","modbus"};

holdingRegister *baudHRSlave;
holdingRegister *idMBSlave;
holdingRegister *modoRadioHR;
holdingRegister *contPozoHR;
holdingRegister *tOlvSatHR;
holdingRegister *tAbusoHR;
holdingRegister *tPingPozoHR;
holdingRegister *tPingPozoActHR;
holdingRegister *diPresionHR;
holdingRegister *vaconIdHR;
holdingRegister *bloqAbusoHR;
holdingRegister *idLlamHR;
holdingRegister *contLlamHR;
holdingRegister *tPingLlamHR;
holdingRegister *tPingLlamPideHR;
holdingRegister *tEntreMsgLlamHR;
holdingRegister *baudHRMaestroHR;
holdingRegister *idMBVaconHR;

void initHoldingRegistersControl(modbusSlave *modbusControl)
{
   // comunes
   baudHRSlave = modbusControl->addHoldingRegister("baudios esclavo",NULL,modbusSpeed,1,6,3);       //  0
   idMBSlave = modbusControl->addHoldingRegister("idMB esclavo",NULL,NULL,2,127,2);                 //  1
   modoRadioHR =  modbusControl->addHoldingRegister("modo radio",modoRadioStr,NULL, 1,3,2);         //  2
   // pozo
   contPozoHR = modbusControl->addHoldingRegister("control pozo",controlPozoStr,NULL, 1,3,3);       //  3
   tOlvSatHR = modbusControl->addHoldingRegister("t olvido satelite (s)",NULL,NULL, 30,7200,600);   //  4
   tAbusoHR = modbusControl->addHoldingRegister("t abuso (min)",NULL,NULL, 15,1440,600);            //  5
   tPingPozoHR = modbusControl->addHoldingRegister("t ping pozo (s)",NULL,NULL, 5,600,60);          //  6
   tPingPozoActHR = modbusControl->addHoldingRegister("t ping pozo pidiendo (s)",NULL,NULL, 5,600,20);//7
   diPresionHR = modbusControl->addHoldingRegister("reportar presion",NULL,NULL, 0,1,0);            //  8
   vaconIdHR = modbusControl->addHoldingRegister("modbus ID Vacon",NULL,NULL,1,127,1);              //  9
   bloqAbusoHR = modbusControl->addHoldingRegister("bloquea abusones",NULL,NULL,0,1,1);             // 10
   // llamador
   idLlamHR = modbusControl->addHoldingRegister("Id Llamador",NULL,NULL, 1,8,6);                    // 11
   contLlamHR = modbusControl->addHoldingRegister("control llamador",controlLlamadorStr,NULL, 1,3,3); // 12
   tPingLlamHR = modbusControl->addHoldingRegister("t ping llamador (s)",NULL,NULL, 5,600,120);      // 13
   tPingLlamPideHR = modbusControl->addHoldingRegister("t ping llamador pide (s)",NULL,NULL, 5,600,30); //14
   tEntreMsgLlamHR = modbusControl->addHoldingRegister("t min llam. entre msgs (s)",NULL,NULL, 1,60,5); // 15
   baudHRMaestroHR = modbusControl->addHoldingRegister("baudios maestro",NULL,modbusSpeed,1,6,3);     // 16
   idMBVaconHR = modbusControl->addHoldingRegister("id modbus vacon",NULL,NULL, 1,127,2);             // 17
}


inputRegister *estatBombaIR;
inputRegister *activosIR;
inputRegister *pidiendoIR;
inputRegister *abusonesIR;
inputRegister *tLastMsgSentIR;
inputRegister *sensIR;
inputRegister *numLlamacionesIR;

inputRegister *presVaconIR, *presSensorIR, *hzIR;
inputRegister *tPide1IR, *tPide2IR, *tPide3IR, *tPide4IR, *tPide5IR, *tPide6IR, *tPide7IR, *tPide8IR;
inputRegister *tLastMsg1IR, *tLastMsg2IR, *tLastMsg3IR, *tLastMsg4IR, *tLastMsg5IR, *tLastMsg6IR, *tLastMsg7IR, *tLastMsg8IR;
inputRegister *secBombaOnIR, *horBombaOnIR;
inputRegister *kWh1IR, *kWh2IR, *kWh3IR, *kWh4IR, *kWh5IR, *kWh6IR, *kWh7IR, *kWh8IR, *kWhOtrosIR;

void initInputRegisters(modbusSlave *modbusControl)
{
   // comunes
   estatBombaIR = modbusControl->addInputRegister("Estado bomba");
   activosIR = modbusControl->addInputRegister("activos");
   pidiendoIR = modbusControl->addInputRegister("pidiendo");
   abusonesIR = modbusControl->addInputRegister("abusones");
   tLastMsgSentIR = modbusControl->addInputRegister("t lastmsg (s)");     // para enviar refresco de estado
   sensIR = modbusControl->addInputRegister("sensOnOff");
   numLlamacionesIR = modbusControl->addInputRegister("numLlamaciones");
   // pozo
   presVaconIR = modbusControl->addInputRegister("presion vacon (bar*100)");
   presSensorIR = modbusControl->addInputRegister("presion sensPres (bar*100)");
   hzIR = modbusControl->addInputRegister("Hz (*100)");
   tPide1IR = modbusControl->addInputRegister("t pidiendo1 (min)"); // para detectar abusos
   tPide2IR = modbusControl->addInputRegister("t pidiendo2 (min)");
   tPide3IR = modbusControl->addInputRegister("t pidiendo3 (min)");
   tPide4IR = modbusControl->addInputRegister("t pidiendo4 (min)");
   tPide5IR = modbusControl->addInputRegister("t pidiendo5 (min)");
   tPide6IR = modbusControl->addInputRegister("t pidiendo6 (min)");
   tPide7IR = modbusControl->addInputRegister("t pidiendo7 (min)");
   tPide8IR = modbusControl->addInputRegister("t pidiendo8 (min)");
   tLastMsg1IR = modbusControl->addInputRegister("t lastmsg1 (s)");    // para detectar activos
   tLastMsg2IR = modbusControl->addInputRegister("t lastmsg2 (s)");
   tLastMsg3IR = modbusControl->addInputRegister("t lastmsg3 (s)");
   tLastMsg4IR = modbusControl->addInputRegister("t lastmsg4 (s)");
   tLastMsg5IR = modbusControl->addInputRegister("t lastmsg5 (s)");
   tLastMsg6IR = modbusControl->addInputRegister("t lastmsg6 (s)");
   tLastMsg7IR = modbusControl->addInputRegister("t lastmsg7 (s)");
   tLastMsg8IR = modbusControl->addInputRegister("t lastmsg8 (s)");
   secBombaOnIR = modbusControl->addInputRegister("t bomba on (horas)");          // contador de funcionamiento
   horBombaOnIR = modbusControl->addInputRegister("t bomba on (sec)");        // guardar cada dia
   kWh1IR = modbusControl->addInputRegister("kWh1 (*100)");      // acumulado, guardar cada dia por si hay apagon
   kWh2IR = modbusControl->addInputRegister("kWh2 (*100)");
   kWh3IR = modbusControl->addInputRegister("kWh3 (*100)");
   kWh4IR = modbusControl->addInputRegister("kWh4 (*100)");
   kWh5IR = modbusControl->addInputRegister("kWh5 (*100)");
   kWh6IR = modbusControl->addInputRegister("kWh6 (*100)");
   kWh7IR = modbusControl->addInputRegister("kWh7 (*100)");
   kWh8IR = modbusControl->addInputRegister("kWh8 (*100)");
   kWhOtrosIR = modbusControl->addInputRegister("kWhVarios (*100)"); // cuando hay varios pidiendo
   // llamador
}


thread_t *slaveMBThread = NULL;
modbusSlave *controlMB;

static THD_WORKING_AREA(wamodbus, 3000);
static THD_FUNCTION(modbusThrd, arg) {
    (void)arg;
    uint8_t buffer[256];
    uint16_t bytesReceived;
    chRegSetThreadName("modbus");
    while (true) {
        uint8_t modoRadio = (uint8_t) modoRadioHR->getValor();
        // Modo llamador
        if (modoRadio==0) // es llamador, debo escuchar ordenes
        {

            controlMB->esperaSilencioModbus();
            bool hayMsg = controlMB->readModbus(buffer, sizeof(buffer),&bytesReceived, chTimeMS2I(10000));
            if (hayMsg)
            {
//                if (modoDebug==1 || modoDebug==2)
//                    modbus::debugMsg((BaseSequentialStream *)&SD1, buffer, bytesReceived);
//                if (modoDebug==0 || modoDebug==2)
                controlMB->interpretaMsg(buffer, bytesReceived);
            }
        }
        else if (modoRadio==1) // es pozo, debo interrogar a Vacon
        {
            // lee Presion
            // lee energia
        }
        //palSetLine(LINE_C13_LED);
        //chThdSleepMilliseconds(20);
        if (chThdShouldTerminateX())
        {
            chThdExit((msg_t) 1);
        }
    }
}




uint8_t initModbusVacon(void)
{
    palSetLineMode(LINE_TX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palSetLineMode(LINE_RX2,PAL_MODE_ALTERNATE(7) | PAL_STM32_OTYPE_OPENDRAIN | PAL_STM32_OSPEED_HIGHEST);
    palClearLine(LINE_TXRX);
    palSetLineMode(LINE_TXRX, PAL_MODE_OUTPUT_PUSHPULL);
    controlMB = new modbusSlave(&SD6);

    //startMBSerial();
    /*
     * Holding registers (se guardan en flash):
       Modbus ID
       Baudios
       Timeout recepcion (s)
       numDimmersInstalados
     */
    if (slaveMBThread==NULL)
        slaveMBThread = chThdCreateStatic(wamodbus, sizeof(wamodbus), NORMALPRIO, modbusThrd, NULL);
    else
        return 1;
    return 0;
}
