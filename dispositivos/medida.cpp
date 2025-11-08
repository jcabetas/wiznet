/*
 * medida.cpp
 *
 *  Created on: 27 jun 2025
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "dispositivos.h"

/*
 * class medida
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
 */

medida::medida(functionPtr funcionPar)
{
    valor = 0.0f;
    esValida = false;
    funcion = funcionPar;
}

float medida::getValor(void)
{
    return valor;
}

float medida::getValor(bool *esValidaPar)
{
    *esValidaPar = esValida;
    return valor;
}

bool  medida::getValidez(void)
{
    return esValida;
}

void medida::setValor(float valorPar)
{
    valor = valorPar;
    esValida = true;
    if (funcion != NULL)
        funcion(valor, esValida);
}

void medida::setValidez(bool validezPar)
{
    esValida = validezPar;
    if (funcion != NULL)
        funcion(valor, esValida);
}
