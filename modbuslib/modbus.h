/*
 * medidas.h
 *
 *  Created on: 19 abr. 2021
 *      Author: joaquin
 */

#ifndef MODBUS_H_
#define MODBUS_H_

#include "registros.h"
#include "dispositivos.h"

class modbus
{
protected:
    SerialDriver *SD;
    ioline_t rxtxLine;
    uint16_t baudios;
    thread_t *procesoModbus;
    uint16_t usIntermsg;      // us entre mensajes
    uint16_t usMaxChar;       // us maximo de un byte
    mutex_t mtxUsandoW25q16;
    holdingRegister *listHoldingRegister[MAXHOLDINGREGISTERS];
    uint8_t numHoldingRegistros;
    inputRegister *listInputRegister[MAXINPUTREGISTERS];
    uint8_t numInputRegistros;
public:
    modbus(void);
    ~modbus();
    uint32_t diBaudios(void);
    holdingRegisterOpciones *addHoldingRegisterOpciones(const char *nombrePar,const char *opcionesStr[],  uint16_t numOpcionesPar,
                                                        uint16_t opcDefault , bool grabablePar);
    holdingRegisterOpciones *addHoldingRegisterOpciones(const char *nombrePar,const char *opcionesStr[],  uint16_t numOpcionesPar,
                                                        uint16_t opcDefault , bool grabablePar, functionCheckPtr funcionHayError);
    holdingRegisterInt2Ext *addHoldingRegisterInt2Ext(const char *nombrePar, const uint32_t int2ext[],
                                                       uint16_t numOpciones, uint32_t opcExternoDefault, bool grabablePar);
    holdingRegisterInt *addHoldingRegisterInt(const char *nombrePar, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabablePar);
    holdingRegisterInt *addHoldingRegisterInt(const char *nombrePar, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabablePar, functionCheckPtr funcionHayError);
    holdingRegisterFloat *addHoldingRegisterFloat(const char *nombrePar, float valMinPar, float valMaxPar, float opcDefecto, float escalaPar, bool grabablePar);
    holdingRegisterFloat *addHoldingRegisterFloat(const char *nombrePar, float valMinPar, float valMaxPar, float opcDefecto, float escalaPar, bool grabablePar, functionCheckPtr funcionHayError);

    inputRegister *addInputRegister(const char *nombrePar);
    uint8_t setHR(uint16_t numHR, uint16_t valor);
    uint8_t getHR(uint16_t numHR, uint32_t *valor);
    uint8_t getHRInterno(uint16_t numHR, uint16_t *valor);
    uint8_t escribeHR(bool usaValorDefecto);
    uint8_t grabaHR(holdingRegister *holdReg);
    uint8_t leeHR(void);
    holdingRegister *getHoldingRegister(uint8_t numHR);
    uint8_t getNumHR(void);
    inputRegister *getInputRegister(uint8_t numIR);
    uint8_t getNumIR(void);

    void esperaSilencioModbus(void);
    int16_t envioStrModbus(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    bool readModbus(uint8_t *buffer, uint16_t sizeOfBuffer, uint16_t *bytesReceived, sysinterval_t timeout);
    bool errorMB(uint8_t idModbus, uint8_t *buffer, uint8_t exceptionCode);
    int16_t chReadStrRs485(uint8_t *buffer, uint16_t numBytesExpected, uint16_t *bytesReceived,  sysinterval_t timeout);
    int16_t chprintStrRs485(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    uint16_t CRC16(const uint8_t *nData, uint16_t wLength);

    void startMBSerial(uint32_t baudios);
    int8_t init(void);
    void stop(void);
    void addDsMB(uint16_t ds);
    void print(void);
};

class modbusSlave: public modbus
{
public:
    modbusSlave(SerialDriver *SDpar, ioline_t rxtxLinePar);
    ~modbusSlave();

    bool interpretoFunction03(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction04(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction06(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction16(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretaMsg(uint8_t contador, uint8_t *buffer, uint16_t bytesReceived);
};

class modbusMaster: public modbus
{
protected:
    dispositivo *listDispositivosMB[MAXDISPOSITIVOS];
    uint8_t  errorEnDispMB[MAXDISPOSITIVOS];
    uint32_t  numErrores[MAXDISPOSITIVOS];
    uint32_t  numMs[MAXDISPOSITIVOS];
    uint32_t  numAccesos[MAXDISPOSITIVOS];
    float tMedio[MAXDISPOSITIVOS];
    uint16_t numDispositivosMB;
public:
    modbusMaster(SerialDriver *SDpar, ioline_t rxtxLinePar);
    ~modbusMaster();
    void enviaMBfunc(uint8_t func, uint8_t dirMB, uint16_t addressReg, uint8_t numRegs, uint8_t bufferRx[], \
                               uint16_t sizeofbufferRx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error);
    void enviaMBfunc10(uint8_t dirMB, uint16_t startAddressReg, uint8_t numRegs, uint16_t RegsTx[],\
                                     uint16_t numRegsEnTx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error);
    uint16_t decodeU16(uint8_t *bufferDatos, uint8_t posIni, uint8_t numRegsEnBuffer, uint8_t *error);
    uint8_t addDispositivo(dispositivo *disp);
    uint8_t usaBus(void);
    uint8_t addDisp(dispositivo *disp);
    uint8_t diNumDisp(void);
    uint8_t lee(uint8_t numDisp, uint16_t msDelay);
    void leeTodos(uint8_t incluyeErroneos);
    dispositivo *findDispositivo(uint16_t idNombre);

    char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void stop(void);
    void addDsMB(uint16_t ds);
    void print(void);
};


#endif /* MODBUS_H_ */
