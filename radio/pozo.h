/*
 * pozo.h
 *
 *  Created on: 12/12/2019
 *      Author: jcabe
 */

#ifndef POZO_H_
#define POZO_H_

#include "colas.h"

#define NUMSATELITES                8
#define MSG_STATUSPOZO              1
#define MSG_STATUSLLAMACIONLOCAL    2
#define MSG_ERROR                   3
#define MSG_CLEARERROR              4
#define MAXERRORESAVISO             5

#define SEEDHISTORICO    5674

void checkRx(void);
void int2str(uint8_t estadoLlamaciones, char binStr[]);
void registraCambio(char *binStrActi, char *binStrLlam);
void initColaPozo(void);
time_t GetTimeUnixSec(void);
int32_t bloqueoAbusonesValor(void);
int32_t sOlvidoValor(void);
int32_t tiempoAbusoValor(void);
uint8_t modoPozoValor(void);
int32_t idPozoValor(void);
int32_t dsMaxEntreMsgsPozoValor(void);
int32_t dsMinEntreMsgsPozoValor(void);
int32_t dsMaxEntreMsgsLlamadorValor(void);
int32_t dsMinEntreMsgsLlamadorValor(void);

uint8_t estadoPeticionBomba(void);
uint8_t haCambiadoEstados(void);
void reseteaEstadosOld(void);
void enviaStatusLlamacion(void);
void enviaStatusPozo(void);
void verSiSiguenConectados(void);
uint8_t estadoPeticionBomba(void);
void reseteaVariables(void);
void arrancaPozoLCD(void);
void ponEnColaRegistrador(void);
uint8_t montarSDValor(void);
void reseteaHistoriaPozo(void);
void registraCambiosPeticion(uint8_t estLlamaciones, uint8_t estLlamacionesOld);
void actualizoErrorDesdeLlamador(uint8_t numEstacion, uint8_t numError, uint8_t *msgError);
int8_t actualizoErrorDesdePozo(uint8_t numEstacion);
uint8_t limpiaError(uint8_t numEstacion, uint8_t numError);
uint8_t buscoSlot(uint8_t numError, uint8_t numEstMenos1, uint8_t *slot);

void trataRxRegistradoryLlamador(struct msgRx_t *msgRx);
void trataRxPozo(struct msgRx_t *msgRx);
void trataObsoletoPozo(void);
void trataObsoletoLlamador(void);
void actualizaRele(void);
void onCambioParametrosPozo(void);
void enviaClearErrorPozo(uint8_t numError, uint8_t numEstProblematica);


void menu(void);

struct datosIdGuardados
{
    uint32_t numPeticiones;
    uint32_t segundosPeticion;
    float m3Total;
    float kWhTotal;
    float kWhPunta;
    float kWhValle;
};

struct datosPozoGuardados
{
    uint16_t seedInicial; // debe ser 9181 cuando esta iniciado
    time_t fechaInicio;
    float m3Total;
    float kWhTotal;
    float kWhPunta;
    float kWhValle;
    struct datosIdGuardados datosId[8];
};

#endif /* POZO_H_ */
