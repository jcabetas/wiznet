/*
 * medidas.h
 *
 *  Created on: 19 abr. 2021
 *      Author: joaquin
 */

#ifndef DISPOSITIVOS_DISPOSITIVOS_H_
#define DISPOSITIVOS_DISPOSITIVOS_H_

#include "calendarUTC.h"
#include "registros.h"


/*
- clase basica de interfaz
  cada interface conoce los dispositivos conectados
  cada dispositivo conoce las medidas o estados que puede leer/escribir
  pero seria interesante que cada interfaz conociera todas las medidas

  Interface (modbus1) -> dispositivo A (sdm120ct) -> medida (kW)
  El thread debería:
  - Mirar cual es la medida/estados/otros mas urgentes
  - Usar el dispositivo correspondiente para leerlo

  Eso lleva a:
  Interface (modbus1) -> medida (kW) -> dispositivo A (sdm120ct)

*/

#define MAXDISPOSITIVOS 5
#define MAXMEDIDAS      3
#define NOMBRELENGTH   15
#define NUMINPUTREGS   20
#define NUMHOLDINGREGS 30

class dispositivo
{
private:
    static dispositivo *listDispositivos[MAXDISPOSITIVOS];
    static uint8_t numDispositivos;
public:
    dispositivo();
    virtual ~dispositivo() = 0;
    static void deleteAll(void);
    virtual uint8_t usaBus(void) = 0;
    virtual char *diNombre(void) = 0;
    virtual const char *diTipo(void) = 0;
    virtual int8_t init(void) = 0;
    virtual void addDs(uint16_t ds) = 0;
};



class modbus
{
protected:
    SerialDriver *SD;
    uint16_t baudios;
    thread_t *procesoModbus;
    uint16_t usIntermsg;      // us entre mensajes
    uint16_t usMaxChar;       // us maximo de un byte
public:
    modbus(void);
    ~modbus();
    uint32_t diBaudios(void);

    void esperaSilencioModbus(void);
    int16_t envioStrModbus(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    bool readModbus(uint8_t *buffer, uint16_t sizeOfBuffer, uint16_t *bytesReceived, sysinterval_t timeout);
    bool errorMB(uint8_t idModbus, uint8_t *buffer, uint8_t exceptionCode);
    int16_t chReadStrRs485(uint8_t *buffer, uint16_t numBytesExpected, uint16_t *bytesReceived,  sysinterval_t timeout);
    int16_t chprintStrRs485(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    uint16_t CRC16(const uint8_t *nData, uint16_t wLength);

    void startMBSerial(void);
    int8_t init(void);
    void stop(void);
    void addDsMB(uint16_t ds);
    void print(void);
};

class modbusSlave: public modbus
{
protected:
    holdingRegister *listHoldingRegister[MAXHOLDINGREGISTERS];
    uint8_t numHoldingRegistros;
    inputRegister *listInputRegister[MAXINPUTREGISTERS];
    uint8_t numInputRegistros;
public:
    modbusSlave(SerialDriver *SDpar);
    ~modbusSlave();
    holdingRegister *addHoldingRegister(const char *nombrePar,const char *opcionesStr[3],
                                 const uint32_t int2ext[], uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault);
    inputRegister *addInputRegister(const char *nombrePar);
    uint8_t setHR(uint16_t numHR, uint16_t valor);
    uint8_t getHR(uint16_t numHR, uint32_t *valor);
    uint8_t getHRInterno(uint16_t numHR, uint16_t *valor);

    bool interpretoFunction03(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction04(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction06(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretoFunction16(uint8_t myId, uint8_t *buffer, uint16_t bytesReceived);
    bool interpretaMsg(uint8_t *buffer, uint16_t bytesReceived);
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
    modbusMaster(void);
    ~modbusMaster();
    void enviaMBfunc04(uint8_t dirMB, uint16_t addressReg, uint8_t numRegs, uint8_t bufferRx[], \
                               uint16_t sizeofbufferRx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error);
    void enviaMBfunc10(uint8_t dirMB, uint16_t startAddressReg, uint8_t numRegs, uint16_t RegsTx[],\
                                     uint16_t numRegsEnTx, uint16_t msDelayMax, uint16_t *msDelay, uint8_t *error);

    uint8_t usaBus(void);
    void addDisp(dispositivo *disp);
    void leeTodos(uint8_t incluyeErroneos);
    dispositivo *findDispositivo(uint16_t idNombre);

    char *diTipo(void);
    const char *diNombre(void);
    void startMBSerial(void);
    int8_t init(void);
    void stop(void);
    void addDsMB(uint16_t ds);
    void print(void);
};


class sdm120ct : public dispositivo
{
    // SDM120CT MedidorFlexo 2
protected:
    char nombre[15];
    uint16_t direccion;
    uint8_t  erroresSeguidos;
    uint8_t  numMedidas;
    float *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint16_t dsUpdateMaxMed[MAXMEDIDAS];
    uint16_t dsDesdeUpdate[MAXMEDIDAS];
    modbusMaster *modbusPtr;
    uint16_t msDelay; // tiempo real de respuesta
public:
    sdm120ct(modbusMaster *modbusPtr, const char *nombrePar, uint16_t dirPar);
    ~sdm120ct();
    uint8_t attachMedida(float *ptrMedPar, const char *tipoMedida, uint8_t dsUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, uint8_t *error);
    uint8_t usaBus(void);
    void leer(float *valor, uint16_t addressReg, uint8_t *error);
    uint16_t diDir(void);
    const char *diTipo(void);
    char *diNombre(void);
    int8_t init(void);
    void addDs(uint16_t ds);
};


class vacon : public dispositivo
{
    // SDM120CT MedidorFlexo 2
protected:
    modbusMaster *modbusConectado;
    char nombre[15];
    uint16_t direccion;
    uint8_t  erroresSeguidos;
    uint8_t  numMedidas;
    float *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint16_t dsUpdateMaxMed[MAXMEDIDAS];
    uint16_t dsDesdeUpdate[MAXMEDIDAS];
    uint16_t msDelay; // tiempo real de respuesta
public:
    vacon(modbusMaster *modbusPtr, const char *nombrePar);
    ~vacon();
    uint8_t attachMedida(float *ptrMedPar, const char *tipoMedida, uint8_t dsUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, uint8_t *error);
    uint8_t usaBus(void);
    void leer(uint16_t *valorInt, uint16_t addressReg, uint8_t *error);
    void leerTip(float *valor, uint8_t tipMedida, uint8_t *error);
    uint16_t diDir(void);
    const char *diTipo(void);
    char *diNombre(void);
    int8_t init(void);
    void addDs(uint16_t ds);
};

#endif /* DISPOSITIVOS_DISPOSITIVOS_H_ */
