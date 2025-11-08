/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#ifndef REGISTROS_H_
#define REGISTROS_H_

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#define MAXHOLDINGREGISTERS 40
#define MAXINPUTREGISTERS 40

#define HR_MODORADIO 2
#define HR_IDMODBUSVACON 9
#define HR_IDMODBUSLLAMADOR 22

class modbusSlave;

typedef bool(*functionCheckPtr)(uint16_t nuevoValorInterno);

class holdingRegister
{
protected:
    char nombre[25];
    uint16_t valInterno;
    uint16_t valInternoDefecto;
    uint16_t valMax;
    uint8_t posicion;
    functionCheckPtr funcionHayError;
    bool grabable;
    static mutex_t mtxUsandoW25q16;
    static modbusSlave *mbSlave;
    void graba(void);
public:
    holdingRegister(uint8_t pos, const char *nombre, bool grabable);
    uint8_t getPosicion(void);
    uint16_t getValorInterno(void);
    uint16_t getValorInternoDefecto(void);
    uint8_t  setValorInterno(uint16_t valor);
    uint8_t  setValorInternoSinGrabar(uint16_t valor);
    char *getNombre(void);
    virtual bool valorNuevoEsOk(uint16_t nuevoValor);
    static void setMBSlave(modbusSlave *mbSlavePar);
    bool esGrabable(void);
};


class holdingRegisterInt2Ext : public holdingRegister
{
private:
    uint32_t valExternoDefecto;
    uint16_t numOpciones;
    const uint32_t *valInt2Ext;
public:
    holdingRegisterInt2Ext(uint8_t pos, const char *nombre, const uint32_t int2ext[], uint16_t numOpciones, uint32_t opcExternoDefault, bool grabable);
    uint16_t getValorDefecto(void);
    uint32_t getValor(void);
    uint32_t getValor(uint16_t pos);
    uint16_t getNumOpciones(void);
    bool valorNuevoEsOk(uint16_t nuevoValor);
    uint8_t  setValor(uint32_t valor);
    uint8_t  validaValor(uint32_t valor);
};


class holdingRegisterOpciones: public holdingRegister
{
private:
    uint16_t valDefecto;
    uint16_t numOpciones;
    const char *descValInt[10];
public:
    holdingRegisterOpciones(uint8_t pos, const char *nombre,const char *opcionesStr[3], uint16_t numOpciones, uint16_t opcDefault, bool grabable);
    holdingRegisterOpciones(uint8_t pos, const char *nombre,const char *opcionesStr[3], uint16_t numOpciones, uint16_t opcDefault,
                            bool grabable, functionCheckPtr funcionHayError);
    uint16_t getValor(void);
    bool valorNuevoEsOk(uint16_t nuevoValor);
    uint8_t setValor(uint16_t);
    const char *getDescripcion(void);
    const char *getDescripcion(uint16_t valor);
    uint16_t getNumOpciones(void);
};


class holdingRegisterFloat: public holdingRegister
{
private:
    float valDefecto;
    float valMin;
    float valMax;
    float escala;
public:
    holdingRegisterFloat(uint8_t pos, const char *nombre, float valMinPar, float valMaxPar, float opcDefecto, float escala, bool grabable);
    holdingRegisterFloat(uint8_t pos, const char *nombre, float valMinPar, float valMaxPar, float opcDefecto,
                                               float escalaPar, bool grabablePar, functionCheckPtr funcionHayError);
    float getValorDefecto(void);
    float getValorMax(void);
    float getValorMin(void);
    bool valorNuevoEsOk(uint16_t nuevoValor);
    uint8_t setValor(float valor);
    float  getValor(void);
    float getEscala(void);
};

class holdingRegisterInt: public holdingRegister
{
private:
    uint16_t valDefecto;
    uint16_t valMin;
    uint16_t valMax;
public:
    holdingRegisterInt(uint8_t pos, const char *nombre, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabable);
    holdingRegisterInt(uint8_t pos, const char *nombre, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault,
                       bool grabable, functionCheckPtr funcionHayError);
    uint16_t getValorDefecto(void);
    uint16_t getValorMax(void);
    uint16_t getValorMin(void);
    bool valorNuevoEsOk(uint16_t nuevoValor);
    uint8_t  setValor(uint16_t valor);
    uint16_t getValor(void);
    uint16_t validaValor(uint16_t valor);
};

class holdingRegisterEscala : public holdingRegister
{
private:
    float valDefecto;
    float valMin;
    float valMax;
public:
    holdingRegisterEscala(uint8_t pos, const char *nombre, float opcMin, float opcMax, float opcDefault, bool grabable);
    float getValorDefecto(void);
    float getValorMax(void);
    float getValorMin(void);
    bool valorNuevoEsOk(uint16_t nuevoValor);
    uint8_t setValor(float valor);
    uint16_t validaValor(float valor);
};

class inputRegister
{
private:
    char nombre[20];
    uint16_t valInterno;
public:
    inputRegister(const char *nombre);
    uint16_t getValor(void);
    char *getNombre(void);
    void setValor(uint16_t valor); // devuelve codigo de error
};

void error(void);

#endif /* REGISTROS_H_ */
