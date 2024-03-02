/*
 * colas.h
 *
 *  Created on: 5 may. 2018
 *      Author: joaquin
 */

#ifndef COLAS_H_
#define COLAS_H_

#include "time.h"

struct queu_t {
    uint8_t numItems;
    uint8_t headQueu; // puntero al hueco
    uint8_t tailQueu; // puntero al primer elemento en cola
    uint8_t sizeQueu; // maximos elementos en cola
    void (*ptrFuncionPonerEnCola)(uint16_t posAGuardar, void *ptrStructOrigen);
    void (*ptrFuncionCogerDeCola)(uint16_t posALeer, void *ptrStructDestino);
    mutex_t *mtxQUEU;
};

struct cambiosPozo_t {
  time_t timet;
  uint8_t estLlam;
  uint8_t estActi;
  uint8_t estAbuso;
  uint8_t estErr;
  float m3Total;
  float kWhPunta;
  float kWhValle;
};

struct msgRx_t {
  time_t timet;
  uint8_t numBytes;
  int16_t rssi;
  uint8_t msg[30];
};

struct msgTx_t {
  uint8_t numBytes;
  uint8_t msg[30];
};

struct msgLcd_t {
    uint8_t fila;
    char msg[21];
};


uint8_t putQueu(struct queu_t *colaMed, void *ptrStructOrigen);
uint8_t getQueu(struct queu_t *colaMed, void *ptrStructDestino);
void clearQueu(struct queu_t *cola);


void initQueu(struct queu_t *cola, mutex_t *mutexCola, uint8_t numElementos,
              void (*ptrFuncionPonerEnColaPar)(uint16_t posAGuardar, void *ptrStructOrigen),
              void (*ptrFuncionCogerDeColaPar)(uint16_t posALeer, void *ptrStructDestino));

void ponerMsgLcdEnCola(uint16_t pos, void *ptrStructOrigen);
void cogerMsgLcdDeCola(uint16_t pos, void *ptrStructDestino);
void msgParaLCD(const char *msg,uint8_t fila);
void initColaMsgLcd(void);

void ponerLogEnCola(uint16_t pos, void *ptrStructOrigen);
uint8_t ponEnColaLCD(uint8_t fila, const char *msg);

#endif /* COLAS_H_ */
