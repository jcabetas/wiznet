/*
 * medidas.h
 *
 *  Created on: 19 abr. 2021
 *      Author: joaquin
 */

#ifndef DISPOSITIVOS_DISPOSITIVOS_H_
#define DISPOSITIVOS_DISPOSITIVOS_H_

#include "calendarUTC.h"


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
    static uint16_t baudios;
    static uint8_t definido;
    static modbus *modbusPtr;
    static thread_t *procesoModbus;
    static dispositivo *listDispositivosMB[MAXDISPOSITIVOS];
    static uint8_t  errorEnDispMB[MAXDISPOSITIVOS];
    static uint32_t  numErrores[MAXDISPOSITIVOS];
    static uint32_t  numMs[MAXDISPOSITIVOS];
    static uint32_t  numAccesos[MAXDISPOSITIVOS];
    static float tMedio[MAXDISPOSITIVOS];
    static uint16_t numDispositivosMB;
public:
    modbus(uint16_t baudiosPar);
    ~modbus();
    static void deleteMB(void);
    static uint8_t usaBus(void);
    static uint32_t diBaudios(void);

    static void addDisp(dispositivo *disp);
    static void leeTodos(uint8_t incluyeErroneos);
    static dispositivo *findDispositivo(uint16_t idNombre);
    static int16_t chReadStrRs485(uint8_t *buffer, uint16_t numBytesExpected, uint16_t *bytesReceived,  sysinterval_t timeout);
    static int16_t chprintStrRs485(uint8_t *str, uint16_t lenStr, sysinterval_t timeout);
    static uint16_t CRC16(const uint8_t *nData, uint16_t wLength);
    const char *diTipo(void);
    const char *diNombre(void);
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
public:
    sdm120ct(const char *nombrePar, uint16_t dirPar);
    ~sdm120ct();
    uint8_t attachMedida(float *ptrMedPar, const char *tipoMedida, uint8_t dsUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, int16_t *error);
    uint8_t usaBus(void);
    void leer(float *valor, uint16_t addressReg, int16_t *error);
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
    char nombre[15];
    uint16_t direccion;
    uint8_t  erroresSeguidos;
    uint8_t  numMedidas;
    float *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint16_t dsUpdateMaxMed[MAXMEDIDAS];
    uint16_t dsDesdeUpdate[MAXMEDIDAS];
public:
    vacon(const char *nombrePar);
    ~vacon();
    uint8_t attachMedida(float *ptrMedPar, const char *tipoMedida, uint8_t dsUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, int16_t *error);
    uint8_t usaBus(void);
    void leer(uint16_t *valorInt, uint16_t addressReg, int16_t *error);
    void leerTip(float *valor, uint8_t tipMedida, int16_t *error);
    uint16_t diDir(void);
    const char *diTipo(void);
    char *diNombre(void);
    int8_t init(void);
    void addDs(uint16_t ds);
};

#endif /* DISPOSITIVOS_DISPOSITIVOS_H_ */
