/*
 * queuMed.c
 *
 *  Created on: 9/6/2018
 *      Author: jcabe
 */


#include "ch.hpp"
#include "hal.h"

#include "colas.h"
using namespace chibios_rt;


extern "C" {
    uint8_t putQueuC(struct queu_t *colaMed, void *ptrStructOrigen);
    uint8_t getQueuC(struct queu_t *colaMed, void *ptrStructDestino);
    void initColas(void);
}

void initColaMsgLcd(void);
void initColaMsgRx(void);
void initColaMsgTx(void);

uint8_t putQueuC(struct queu_t *colaMed, void *ptrStructOrigen)
{
    return putQueu(colaMed, ptrStructOrigen);
}
uint8_t getQueuC(struct queu_t *colaMed, void *ptrStructDestino)
{
    return getQueu(colaMed, ptrStructDestino);
}


uint8_t putQueu(struct queu_t *colaMed, void *ptrStructOrigen)
{
    chMtxLock(colaMed->mtxQUEU);
    if (colaMed->numItems>=colaMed->sizeQueu) // no cabe mas
    {
        chMtxUnlock(colaMed->mtxQUEU);
        return 1;
    }
    uint8_t posHead = colaMed->headQueu;
    if (++colaMed->headQueu>=colaMed->sizeQueu)
        colaMed->headQueu = 0;
    colaMed->numItems++;
    colaMed->ptrFuncionPonerEnCola(posHead, ptrStructOrigen);
    chMtxUnlock(colaMed->mtxQUEU);
    return 0;
}


uint8_t getQueu(struct queu_t *colaMed, void *ptrStructDestino)
{
    chMtxLock(colaMed->mtxQUEU);
    if (colaMed->numItems==0) // no hay nada que coger
    {
        chMtxUnlock(colaMed->mtxQUEU);
        return 0;
    }
    colaMed->ptrFuncionCogerDeCola(colaMed->tailQueu, ptrStructDestino);
    if (++colaMed->tailQueu>=colaMed->sizeQueu)
        colaMed->tailQueu = 0;
    colaMed->numItems--;
    chMtxUnlock(colaMed->mtxQUEU);
    return 1;
}

void clearQueu(struct queu_t *cola)
{
    chMtxLock(cola->mtxQUEU);
    if (cola->numItems==0) // no hay nada que limpiar
    {
        chMtxUnlock(cola->mtxQUEU);
        return;
    }
    cola->numItems = 0;
    cola->headQueu = 0;
    cola->tailQueu = 0;
    chMtxUnlock(cola->mtxQUEU);
    return;
}

void initQueu(struct queu_t *cola, mutex_t *mutexCola, uint8_t numElementos,
              void (*ptrFuncionPonerEnColaPar)(uint16_t posAGuardar, void *ptrStructOrigen),
              void (*ptrFuncionCogerDeColaPar)(uint16_t posALeer, void *ptrStructDestino))
{
    chMtxObjectInit(mutexCola);
    cola->numItems = 0;
    cola->headQueu = 0;
    cola->tailQueu = 0;
    cola->sizeQueu = numElementos;
    cola->ptrFuncionPonerEnCola = ptrFuncionPonerEnColaPar;
    cola->ptrFuncionCogerDeCola = ptrFuncionCogerDeColaPar;
    cola->mtxQUEU = mutexCola;
}

void initColas(void)
{
    initColaMsgLcd();
    initColaMsgRx();
    initColaMsgTx();
}
