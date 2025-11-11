/*
 * externRegistros.h
 *
 *  Created on: 2 jul 2025
 *      Author: joaquin
 */

#ifndef MODBUS_EXTERNREGISTROS_H_
#define MODBUS_EXTERNREGISTROS_H_

extern holdingRegisterInt *modbusIdHR;           // Id modbus
extern holdingRegisterInt2Ext *modbusBaudHR;     // baudios modbus
extern holdingRegisterOpciones *modoRadioHR;       // {"llamador","pozo","registrador"}
extern holdingRegisterInt *sTimeOutLlamadoresHR;        // tiempo olvido en s
extern holdingRegisterInt *idLlamadorHR;     // id en modo llamador
extern holdingRegisterInt *sMaxEntreMsgsLlamadorHR; // s maximo entre mensajes en modo llamador
extern holdingRegisterInt *dsMinEntreMsgsLlamadorHR; // ds minimo entre mensajes en modo llamador
extern holdingRegisterInt *bloqueaAbusonesHR; // 0 no,1 bloqueo abusones
extern holdingRegisterInt *minutosAbusoHR;   //
extern holdingRegisterInt *sMaxEntreMsgsPozoHR;  // s maximo entre mensajes en modo pozo
extern holdingRegisterInt *dsMinEntreMsgsPozoHR; // ds minimo entre mensajes en modo pozo
extern holdingRegisterFloat *barMaxSensPresionHR;     // *10
extern holdingRegisterOpciones *usaSensorHR;     // control peticion agua modo llamador (0: modbus, 1: sensor)
extern holdingRegisterInt *pideAguaHR;           // peticion agua por modbus (modo llamador)


extern inputRegister *activosIR;        // estaciones activos
extern inputRegister *peticionesIR;
extern inputRegister *abusonesIR;
extern inputRegister *presBarIR;        // *100
extern inputRegister *bombaOnIR;
extern inputRegister *miLlamacionIR;


#endif /* MODBUS_EXTERNREGISTROS_H_ */
