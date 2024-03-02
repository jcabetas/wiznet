/*
 * colasRegistrador.c
 *
 *  Created on: 6/10/2019
 *      Author: jcabe
 */
#include "hal.h"
#include "ch.h"
#include "colas.h"
#include "string.h"

#define NUMMSGRXENCOLA  3

mutex_t MtxMsgRx;

struct queu_t colaMsgRx;
struct msgRx_t msgRx[NUMMSGRXENCOLA];

void ponerMsgRxEnCola(uint16_t pos, void *ptrStructOrigen)
{
    struct msgRx_t *ptrMsg = (struct msgRx_t *)ptrStructOrigen;
    msgRx[pos].timet = ptrMsg->timet;
    msgRx[pos].numBytes = ptrMsg->numBytes;
    msgRx[pos].rssi = ptrMsg->rssi;
    memcpy(msgRx[pos].msg, ptrMsg->msg,ptrMsg->numBytes);
}

void cogerMsgRxDeCola(uint16_t pos, void *ptrStructDestino)
{
    struct msgRx_t *ptrMsg = (struct msgRx_t *)ptrStructDestino;
    ptrMsg->timet = msgRx[pos].timet;
    ptrMsg->numBytes = msgRx[pos].numBytes;
    ptrMsg->rssi = msgRx[pos].rssi;
    memcpy(ptrMsg->msg, msgRx[pos].msg,msgRx[pos].numBytes);
}

void initColaMsgRx(void)
{
    initQueu(&colaMsgRx, &MtxMsgRx, NUMMSGRXENCOLA, ponerMsgRxEnCola, cogerMsgRxDeCola);
}
