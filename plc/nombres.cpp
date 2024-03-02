/*
 * nombres.cpp
 *
 *  Created on: 4 abr. 2020
 *      Author: joaquin
 */


#include "string.h"
#include <stdint.h>
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "bloques.h"
#include "nextion.h"

/*
 * class nombres {
    public:
      static uint16_t incorpora(const char *nombre_p);
      static uint16_t busca(const char *nombre_p);
      static const char *nomConId(uint16_t id);
};

    #define STORESIZE   1000
    #define MAXSTATES     50
    #define MAXNOMBRES    50

 */

char nombres::nombStore[STORESIZE];
uint16_t nombres::nombStart[MAXNOMBRES];
uint16_t nombres::numNombres = 0;

void nombres::init(void)
{
    numNombres = 0;
    for (uint16_t n=0;n<MAXNOMBRES;n++)
        nombStart[n] = 0;
    for (uint16_t s=0;s<STORESIZE;s++)
        nombStore[s] = 0;
}

// devuelve posicion 1..numNombres, o 0 si no puede
uint16_t nombres::incorpora(const char *nombre_p)
{
    uint16_t posIniGrabacion;
    // compruebo si existe
    uint16_t posNomb = nombres::busca(nombre_p);
    if (posNomb>0)
        return posNomb;
    // hay que crearlo... cabe?
    if (numNombres>0)
    {
        uint16_t sitioUsado = nombStart[numNombres-1];
        uint16_t longNom = strlen(&nombStore[nombStart[numNombres-1]]) + 1;
        sitioUsado += longNom;
        if (sitioUsado+strlen(nombre_p)+1>=STORESIZE)
            return 0;
        posIniGrabacion = sitioUsado;
    }
    else
    {
        if (strlen(nombre_p)+1>=STORESIZE)
            return 0;
        posIniGrabacion = 0;
    }
    // a grabar!
    nombStart[numNombres] = posIniGrabacion;
    strcpy(&nombStore[nombStart[numNombres]],nombre_p);
    if (numNombres<MAXNOMBRES-1)
        numNombres++;
    else
        nextion::enviaLog(NULL,"Demasiados nombres");
    return numNombres;
};


// devuelve posicion 1..numNombres, o 0 si no puede
uint16_t nombres::busca(const char *nombre_p)
{
    for (uint16_t posNomb=0;posNomb<numNombres;posNomb++)
        if (!strcmp(&nombStore[nombStart[posNomb]],nombre_p))
            return posNomb+1;
    return 0;
};


// devuelve posicion 1..numNombres, o 0 si no puede
uint16_t nombres::buscaNoCase(const char *nombre_p)
{
    for (uint16_t posNomb=0;posNomb<numNombres;posNomb++)
        if (!strcasecmp(&nombStore[nombStart[posNomb]],nombre_p))
            return posNomb+1;
    return 0;
};

const char *nombres::nomConId(uint16_t id)
{
    if (id==0 || id>numNombres)
        return "Error: id erroneo";
    return &nombStore[nombStart[id-1]];
};
