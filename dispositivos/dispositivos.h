/*
 * medidas.h
 *
 *  Created on: 19 abr. 2021
 *      Author: joaquin
 */

#ifndef DISPOSITIVOS_H_
#define DISPOSITIVOS_H_

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
#define MAXMEDIDAS      8
#define NOMBRELENGTH   15

class modbusMaster;

typedef void(*functionPtr)(float valor, bool esValido);

class medida
{
private:
    float valor;
    bool esValida;
    functionPtr funcion;
public:
    medida(functionPtr funcionPar);
    float getValor(void);
    float getValor(bool *esValidaPar);
    bool  getValidez(void);
    void setValor(float valorPar);
    void setValidez(bool validezPar);
};

class dispositivo
{
private:

public:
    modbusMaster *modbusPtr;
    dispositivo(void);
    virtual ~dispositivo() = 0;
    virtual uint8_t usaBus(void) = 0;
    virtual char *diNombre(void) = 0;
    virtual const char *diTipo(void) = 0;
    virtual int8_t init(void) = 0;
    virtual void addms(uint16_t ms) = 0;
};

class sdm120ct : public dispositivo
{
    // SDM120CT MedidorFlexo 2
protected:
    char nombre[15];
    uint8_t direccion;
    uint8_t  erroresSeguidos;
    uint8_t  numMedidas;
    medida *ptrMed[MAXMEDIDAS];
    uint16_t tipoMed[MAXMEDIDAS];
    char descrMed[MAXMEDIDAS][NOMBRELENGTH];
    uint16_t msUpdateMaxMed[MAXMEDIDAS];
    uint16_t msDesdeUpdate[MAXMEDIDAS];
    uint16_t msDelay; // tiempo real de respuesta
public:
    sdm120ct(modbusMaster *modbusPtr, const char *nombrePar, uint8_t dirPar);
    ~sdm120ct();
    uint8_t attachMedida(medida *ptrMedPar, const char *tipoMedida, uint16_t msUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, uint8_t *error);
    uint8_t usaBus(void);
    void leer(float *valor, uint16_t addressReg, uint8_t *error);
    uint16_t diDir(void);
    const char *diTipo(void);
    char *diNombre(void);
    int8_t init(void);
    void addms(uint16_t ms);
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
    uint16_t msUpdateMaxMed[MAXMEDIDAS];
    uint16_t msDesdeUpdate[MAXMEDIDAS];
    uint16_t msDelay; // tiempo real de respuesta
public:
    vacon(modbusMaster *modbusPtr, const char *nombrePar);
    ~vacon();
    uint8_t attachMedida(float *ptrMedPar, const char *tipoMedida, uint16_t msUpdatePar, const char *descrPar);
    void changeID(uint8_t oldId, uint8_t newId, uint8_t *error);
    uint8_t usaBus(void);
    void leer(uint16_t *valorInt, uint16_t addressReg, uint8_t *error);
    void leerTip(float *valor, uint8_t tipMedida, uint8_t *error);
    uint16_t diDir(void);
    const char *diTipo(void);
    char *diNombre(void);
    int8_t init(void);
    void addms(uint16_t ms);
};

#endif /* DISPOSITIVOS_H_ */
