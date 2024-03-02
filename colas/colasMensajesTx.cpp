/*
 * colasRegistrador.c
 *
 *  Created on: 6/10/2019
 *      Author: jcabe
 */
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

#define NUMMSGTXENCOLA  3

mutex_t MtxMsgTx;

struct queu_t colaMsgTx;
struct msgTx_t msgTx[NUMMSGTXENCOLA];

void ponerMsgTxEnCola(uint16_t pos, void *ptrStructOrigen)
{
    struct msgTx_t *ptrMsg = (struct msgTx_t *)ptrStructOrigen;
    msgTx[pos].numBytes = ptrMsg->numBytes;
    memcpy(msgTx[pos].msg, ptrMsg->msg,ptrMsg->numBytes);
}

void cogerMsgTxDeCola(uint16_t pos, void *ptrStructDestino)
{
    struct msgTx_t *ptrMsg = (struct msgTx_t *)ptrStructDestino;
    ptrMsg->numBytes = msgTx[pos].numBytes;
    memcpy(ptrMsg->msg, msgTx[pos].msg,msgTx[pos].numBytes);
}

void initColaMsgTx(void)
{
    initQueu(&colaMsgTx, &MtxMsgTx, NUMMSGTXENCOLA, ponerMsgTxEnCola, cogerMsgTxDeCola);
}
