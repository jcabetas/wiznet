/*
 * modbus.cpp
 *
 *  Created on: 17 abr. 2021
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;

#include "registros.h"
#include "string.h"
#include "lcd.h"

#include "modbus.h"

void error(const char *)
{
    //ponEnColaLCD(3, msg);
    while (true) {
        chThdSleepMilliseconds(500);
    }
}


//class holdingRegister
//{
//protected:
//    char nombre[25];
//    uint16_t valInterno;
//    uint8_t posicion;
//    bool grabable;
//    static mutex_t mtxUsandoW25q16;
//    void graba(void);
//public:
//    holdingRegister(uint8_t pos, const char *nombre, bool grabable);
//    uint16_t getValorInterno(void);
//    uint8_t  setValorInterno(uint16_t valor);
//    bool esGrabable(void);
//};

modbusSlave *holdingRegister::mbSlave = NULL;

holdingRegister::holdingRegister(uint8_t pos, const char *nombrePar, bool grabablePar)
{
    strncpy(nombre, nombrePar, sizeof(nombre));
    valInterno = 0; //¿aqui?
    grabable = grabablePar;
    posicion = pos;
    funcionHayError = NULL;
}

uint8_t holdingRegister::getPosicion(void)
{
    return posicion;
}

bool holdingRegister::esGrabable(void)
{
    return grabable;
}

bool holdingRegister::valorNuevoEsOk(uint16_t )
{
    return true;
}

uint8_t holdingRegister::setValorInterno(uint16_t valor)
{
    if (!valorNuevoEsOk(valor))
        return 1;
    if (funcionHayError!=NULL && funcionHayError(valor))
        return 1;
    valInterno = valor;
    if (mbSlave)
        mbSlave->grabaHR(this);
    return 0;
}

uint8_t holdingRegister::setValorInternoSinGrabar(uint16_t valor)
{
    if (valorNuevoEsOk(valor))
        valInterno = valor;
    else
        valInterno = getValorInternoDefecto();
    return 0;
}

uint16_t holdingRegister::getValorInterno(void)
{
    return valInterno;
}

uint16_t holdingRegister::getValorInternoDefecto(void)
{
    return valInternoDefecto;
}

char *holdingRegister::getNombre(void)
{
    return nombre;
}

void holdingRegister::setMBSlave(modbusSlave *mbSlavePar)
{
    mbSlave = mbSlavePar;
}

//class holdingRegisterInt2Ext : public holdingRegister
//{
//private:
//    uint32_t valExternoDefecto;
//    uint16_t numOpciones;
//    const uint32_t *valInt2Ext;
//    const char *descValInt[10];
//public:
//    holdingRegisterInt2Ext(uint8_t pos, const char *nombre, const uint32_t int2ext[], uint32_t opcExternoDefault, bool grabable);
//    uint16_t getValorDefecto(void);
//    uint32_t getValor(void);
//    uint8_t  setValor(uint32_t valor);
//    uint8_t  validaValor(uint32_t valor);
//};
holdingRegisterInt2Ext::holdingRegisterInt2Ext(uint8_t pos, const char *nombrePar, const uint32_t int2ext[], uint16_t numOpcionesPar,
                                               uint32_t opcExternoDefault,bool grabablePar) : holdingRegister(pos, nombrePar, grabablePar)
{
    uint16_t i;
    valExternoDefecto = opcExternoDefault;
    numOpciones = numOpcionesPar;
    if (int2ext == NULL)
        error("falta int2ext");
    valInt2Ext = int2ext;
    for (i=1;i<=numOpciones;i++)
        if (valInt2Ext[i-1]==opcExternoDefault)
        {
            valInterno = i-1;
            break;
        }
    if (i > numOpciones)
        error("HR: valor invalido");
    valInternoDefecto = valInterno;
}

uint8_t holdingRegisterInt2Ext::setValor(uint32_t valor)
{
    for (uint16_t i=1;i<=numOpciones;i++)
        if (valInt2Ext[i-1]==valor)
        {
            setValorInterno(i-1);//            valInterno = i-1;
            return 0;
        }
    error("HR: valor invalido");
    return 1;
}

uint32_t holdingRegisterInt2Ext::getValor(void)
{
    return valInt2Ext[valInterno];
}

uint32_t holdingRegisterInt2Ext::getValor(uint16_t pos)
{
    return valInt2Ext[pos];
}

uint16_t holdingRegisterInt2Ext::getNumOpciones(void)
{
    return numOpciones;
}

bool holdingRegisterInt2Ext::valorNuevoEsOk(uint16_t nuevoValor)
{
    if (nuevoValor<numOpciones)
        return true;
    else
        return false;
}

//class holdingRegisterOpciones: public holdingRegister
//{
//private:
//    uint32_t valDefecto;
//    uint16_t numOpciones;
//    const char *descValInt[10];
//public:
//    holdingRegisterOpciones(uint8_t pos, const char *nombre,const char *opcionesStr[3], uint16_t numOpciones, uint16_t opcDefault, bool grabable);
//    uint16_t getValor(void);
//    uint8_t setValor(uint16_t);
//    char *getDescripcion(void);
//    char *getDescripcion(uint16_t valor);
//    uint16_t getNumOpciones(void);
//};


holdingRegisterOpciones::holdingRegisterOpciones(uint8_t pos, const char *nombrePar,const char *opcionesStr[],  uint16_t numOpcionesPar,
                                            uint16_t opcDefault , bool grabablePar) : holdingRegister(pos, nombrePar, grabablePar)
{
    numOpciones = numOpcionesPar;
    if (opcDefault>=numOpciones)
        error("Opcion no valida");

    valInterno = opcDefault;
    valInternoDefecto = opcDefault;
    valDefecto = opcDefault;
    if (opcionesStr==NULL)
    error("Falta opcStr");
    for (uint8_t i=1;i<=numOpciones;i++)
        descValInt[i-1] = opcionesStr[i-1];
    funcionHayError = NULL;
}

holdingRegisterOpciones::holdingRegisterOpciones(uint8_t pos, const char *nombrePar,const char *opcionesStr[],  uint16_t numOpcionesPar,
                                            uint16_t opcDefault , bool grabablePar, functionCheckPtr funcionPar) : holdingRegister(pos, nombrePar, grabablePar)
{
    numOpciones = numOpcionesPar;
    if (opcDefault>=numOpciones)
        error("Opcion no valida");

    valInterno = opcDefault;
    valInternoDefecto = opcDefault;
    valDefecto = opcDefault;
    if (opcionesStr==NULL)
    error("Falta opcStr");
    for (uint8_t i=1;i<=numOpciones;i++)
        descValInt[i-1] = opcionesStr[i-1];
    funcionHayError = funcionPar;
}

uint16_t holdingRegisterOpciones::getValor(void)
{
    return valInterno;
}

bool holdingRegisterOpciones::valorNuevoEsOk(uint16_t nuevoValor)
{
    if (nuevoValor<numOpciones)
        return true;
    else
        return false;
}

uint8_t holdingRegisterOpciones::setValor(uint16_t valor)
{
    if (valor>=numOpciones)
        error("Opcion no valida");
    setValorInterno(valor); //    valInterno = valor;
    return 0;
}

const char *holdingRegisterOpciones::getDescripcion(void)
{
    return descValInt[valInterno];
}

const char *holdingRegisterOpciones::getDescripcion(uint16_t numOpc)
{
    if (numOpc>=numOpciones)
        error("Opcion no valida");
    return descValInt[numOpc];
}

uint16_t holdingRegisterOpciones::getNumOpciones(void)
{
    return numOpciones;
}

//class holdingRegisterFloat: public holdingRegister
//{
//private:
//    float valDefecto;
//    float valMin;
//    float valMax;
//    float escala;
//public:
//    holdingRegisterInt2Ext(uint8_t pos, const char *nombre, float opcDefecto, float valMinPar, float valMaxPar, float escala, bool grabable);
//    float getValorDefecto(void);
//    float getValorMax(void);
//    float getValorMin(void);
//    uint8_t setValor(float valor);
//    float  getValor(void);
//};
holdingRegisterFloat::holdingRegisterFloat(uint8_t pos, const char *nombrePar, float valMinPar,
                                           float valMaxPar, float opcDefecto, float escalaPar, bool grabablePar) : holdingRegister(pos, nombrePar, grabablePar)
{
    valDefecto = opcDefecto;
    escala = escalaPar;
    valInterno = (uint16_t) (opcDefecto*escala);
    valInternoDefecto = valInterno;
    valMin = valMinPar;
    valMax = valMaxPar;
    funcionHayError = NULL;
}

holdingRegisterFloat::holdingRegisterFloat(uint8_t pos, const char *nombrePar, float valMinPar, float valMaxPar, float opcDefecto,
                                           float escalaPar, bool grabablePar, functionCheckPtr funcionPar) : holdingRegister(pos, nombrePar, grabablePar)
{
    valDefecto = opcDefecto;
    escala = escalaPar;
    valInterno = (uint16_t) (opcDefecto*escala);
    valInternoDefecto = valInterno;
    valMin = valMinPar;
    valMax = valMaxPar;
    funcionHayError = funcionPar;
}

float holdingRegisterFloat::getValorDefecto(void)
{
    return valDefecto;
}

float holdingRegisterFloat::getValorMax(void)
{
    return valMax;
}

float holdingRegisterFloat::getValorMin(void)
{
    return valMin;
}

float holdingRegisterFloat::getValor(void)
{
    return (float) (valInterno/escala);
}

bool holdingRegisterFloat::valorNuevoEsOk(uint16_t nuevoValor)
{
    float newValor = nuevoValor/escala;
    if (newValor>valMax || newValor<valMin)
        return false;
    else
        return true;
}

uint8_t holdingRegisterFloat::setValor(float valor)
{
    setValorInterno((uint16_t) (valor*escala));
    return 0;
}

float holdingRegisterFloat::getEscala(void)
{
    return escala;
}

//class holdingRegisterInt: public holdingRegister
//{
//private:
//    uint16_t valDefecto;
//    uint16_t valMin;
//    uint16_t valMax;
//public:
//    holdingRegisterInt(uint8_t pos, const char *nombre, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabable);
//    uint16_t getValorDefecto(void);
//    uint16_t getValorMax(void);
//    uint16_t getValorMin(void);
//    uint8_t  setValor(uint16_t valor);
//    uint16_t validaValor(uint16_t valor);
//};
holdingRegisterInt::holdingRegisterInt(uint8_t pos, const char *nombrePar, uint16_t opcMin, uint16_t opcMax
                                       , uint16_t opcDefault, bool grabablePar) : holdingRegister(pos, nombrePar, grabablePar)
{
    valDefecto = opcDefault;
    valInterno = opcDefault;
    valInternoDefecto = valInterno;
    valMin = opcMin;
    valMax = opcMax;
    funcionHayError = NULL;
}


holdingRegisterInt::holdingRegisterInt(uint8_t pos, const char *nombrePar, uint16_t opcMin, uint16_t opcMax
                                       , uint16_t opcDefault, bool grabablePar, functionCheckPtr funcionPar) : holdingRegister(pos, nombrePar, grabablePar)
{
    valDefecto = opcDefault;
    valInterno = opcDefault;
    valInternoDefecto = valInterno;
    valMin = opcMin;
    valMax = opcMax;
    funcionHayError = funcionPar;
}

uint16_t holdingRegisterInt::getValorDefecto(void)
{
    return valDefecto;
}

uint16_t holdingRegisterInt::getValorMax(void)
{
    return valMax;
}

uint16_t holdingRegisterInt::getValorMin(void)
{
    return valMin;
}

uint16_t holdingRegisterInt::getValor(void)
{
    return valInterno;
}

bool holdingRegisterInt::valorNuevoEsOk(uint16_t nuevoValor)
{
    if (nuevoValor>=valMin && nuevoValor<=valMax)
        return true;
    else
        return false;
}

uint8_t holdingRegisterInt::setValor(uint16_t valor)
{
    if (valor<valMin || valor>valMax)
        error("valor erroneo");
    setValorInterno(valor);
    return 0;
}







holdingRegisterInt2Ext *modbus::addHoldingRegisterInt2Ext(const char *nombrePar, const uint32_t int2extPar[], uint16_t numOpcionesPar,
                                                      uint32_t opcExternoDefault,bool grabablePar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterInt2Ext *nuevoHR = new holdingRegisterInt2Ext(numHoldingRegistros, nombrePar, int2extPar, numOpcionesPar, opcExternoDefault, grabablePar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

holdingRegisterInt *modbus::addHoldingRegisterInt(const char *nombrePar, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabablePar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterInt *nuevoHR = new holdingRegisterInt(numHoldingRegistros, nombrePar, opcMin, opcMax, opcDefault, grabablePar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

holdingRegisterInt *modbus::addHoldingRegisterInt(const char *nombrePar, uint16_t opcMin, uint16_t opcMax, uint16_t opcDefault, bool grabablePar, functionCheckPtr funcionPar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterInt *nuevoHR = new holdingRegisterInt(numHoldingRegistros, nombrePar, opcMin, opcMax, opcDefault, grabablePar, funcionPar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

holdingRegisterOpciones *modbus::addHoldingRegisterOpciones(const char *nombrePar,const char *opcionesStr[]
                                                           , uint16_t numOpcionesPar, uint16_t opcDefault , bool grabablePar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterOpciones *nuevoHR = new holdingRegisterOpciones(numHoldingRegistros, nombrePar, opcionesStr, numOpcionesPar, opcDefault, grabablePar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

holdingRegisterOpciones *modbus::addHoldingRegisterOpciones(const char *nombrePar,const char *opcionesStr[]
                                                           , uint16_t numOpcionesPar, uint16_t opcDefault , bool grabablePar, functionCheckPtr funcionPar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterOpciones *nuevoHR = new holdingRegisterOpciones(numHoldingRegistros, nombrePar, opcionesStr, numOpcionesPar, opcDefault, grabablePar, funcionPar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}


holdingRegisterFloat *modbus::addHoldingRegisterFloat(const char *nombrePar, float valMinPar, float valMaxPar, float opcDefecto, float escalaPar, bool grabablePar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterFloat *nuevoHR = new holdingRegisterFloat(numHoldingRegistros, nombrePar, valMinPar, valMaxPar, opcDefecto, escalaPar, grabablePar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

holdingRegisterFloat *modbus::addHoldingRegisterFloat(const char *nombrePar, float valMinPar, float valMaxPar, float opcDefecto, float escalaPar, bool grabablePar, functionCheckPtr funcionPar)
{
    if (numHoldingRegistros>=MAXHOLDINGREGISTERS)
        return NULL;
    holdingRegisterFloat *nuevoHR = new holdingRegisterFloat(numHoldingRegistros, nombrePar, valMinPar, valMaxPar, opcDefecto, escalaPar, grabablePar, funcionPar);
    listHoldingRegister[numHoldingRegistros] = nuevoHR;
    numHoldingRegistros++;
    return nuevoHR;
}

