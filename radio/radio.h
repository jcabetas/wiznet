/*
 * radio.h
 *
 *  Created on: 20 sept. 2020
 *      Author: jcabe
 */

#ifndef RADIO_RADIO_H_
#define RADIO_RADIO_H_


#include <stdint.h>
#include "calendarUTC.h"

#define SEEDHISTORICO    5674

#define MODOREGISTRADOR 1
#define MODOLLAMADOR    2
#define MODOPOZO        3

#define NUMSATELITES                8
#define MSG_STATUSPOZO              1
#define MSG_STATUSLLAMACIONLOCAL    2
#define MSG_ERROR                   3
#define MSG_CLEARERROR              4
#define MAXERRORESAVISO             5

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

class radio
{
protected:
    uint16_t modoRadio;
    uint16_t bombaPozoOn;                        // segun mensajes del pozo
    uint16_t bombaPozoSolicitada;                 // segun la peticion que le he hecho al pozo
    uint16_t numEstadoComOk, numEstadoBomba;
    static uint8_t estadoLlamaciones, estadoActivos, estadoAbusones;  // seg�n pozo
    static uint8_t estadoAbusonesOld;
    static time_t timeInicioPeticion[NUMSATELITES];
    static time_t timeUltConexion[NUMSATELITES];
    uint8_t numErrorAviso[MAXERRORESAVISO];
    uint8_t idEstacionAviso[MAXERRORESAVISO];
    uint8_t mensajeAviso[MAXERRORESAVISO][21];
    time_t timeInicioAvisoError[MAXERRORESAVISO];
    uint8_t cnt;
    static float totEner;
    static radio *radioPtr;
public:
    static void apuntaEnergia(float incEner);
    static void trataRxRf95Radio(eventmask_t evt);
    virtual void trataRxRf95(eventmask_t evt) = 0;
    virtual void trataRx(struct msgRx_t *msgRx) = 0;
    static void reseteaHistoriaPozo(void);
    static void reseteaVariables(void);
    virtual void reseteaVariablesEspecificas(void) = 0;
    static void arrancaRadio(void);
    static void paraRadio(void);
    static uint8_t radioDefinida(void);
    // pozoComun:
    void registraCambiosPeticion(uint8_t estLlamaciones, uint8_t estLlamacionesOld);
    static void ponEnColaRegistrador(void);
    uint8_t buscoSlot(uint8_t numError, uint8_t numEstacion, uint8_t *slot);
    void actualizoErrorDesdeLlamador(uint8_t numEstacion, uint8_t numError, uint8_t *msgError);
    uint8_t limpiaError(uint8_t numEstacion, uint8_t numError);
};


class llamador : public radio
{
protected:
    uint16_t estadoDeseado;
    uint8_t  estadoComms;
    uint16_t dsAleatorioMinEntreMsgsLlamador;
    uint16_t dsAleatorioMaxEntreMsgsLlamador;
    struct fechaHora dateTimeEnvioAnterior;
    struct fechaHora dateTimeRxPozoAnterior;
//    uint16_t idLlamador;
//    uint16_t dsMaxEntreMsgsLlamador;
//    uint16_t dsMinEntreMsgsLlamador;
//    uint16_t sOlvido;
public:
    llamador(void);
    ~llamador();
    void reseteaVariablesEspecificas(void);
    // para implementar "bloque"
    const char *diNombre(void);
    int8_t init(void);
    void stop(void);
    void print(BaseSequentialStream *tty);
    void update(uint8_t estadoDeseado);
    const char *diTipo(void);
    // objeto "radio" propiamente dicho
    // pozoComun:
    uint8_t quitarAbuso(uint8_t numEstacion);
    void onCambioParametrosPozo(void);
    uint8_t limpiaError(uint8_t numEstacion, uint8_t numError);
    //void apuntaEnergia(float incEner);


    // llamador
    void trataObsoletoLlamador(void);
    void enviaStatusLlamacion(void);
    uint8_t gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg);


    void trataRxRf95(eventmask_t evt);
    void trataRx(struct msgRx_t *msgRx);
    void trataOrdenSMS(char **Vars, uint16_t numVars);
};



class pozo: public radio
{
protected:
    uint16_t numOutput;
    int32_t dsAleatorioMinEntreMsgsPozo;
    int32_t dsAleatorioMaxEntreMsgsPozo;
    struct fechaHora dateTimeEnvioAnterior;
    uint16_t *dsMinEntreMsgsPozo;
    uint16_t *dsMaxEntreMsgsPozo;
    uint16_t *bloqueoAbusones;
    uint16_t *avisaAbuso;
    uint16_t *tiempoAbuso;         // minutos
    uint16_t *sOlvido;
public:
    pozo(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~pozo();
    void reseteaVariablesEspecificas(void);
    // para implementar "bloque"
    const char *diNombre(void);
    int8_t init(void);
    void stop(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
//    void trataOrdenNextion(char **Vars, uint16_t numVars);
    const char *diTipo(void);
    // objeto "radio" propiemante dicho
    void reseteaVariables(void);
    uint8_t quitarAbuso(uint8_t numEstacion);
    void onCambioParametrosPozo(void);
    uint8_t buscoSlot(uint8_t numError, uint8_t numEstacion, uint8_t *slot);
    void actualizoErrorDesdeLlamador(uint8_t numEstacion, uint8_t numError, uint8_t *msgError);
    int8_t actualizoErrorDesdePozo(uint8_t numEstacion);


    //void reseteaHistoriaPozo(void);
    //void apuntaEnergia(float incEner);
    void registraCambiosPeticion(uint8_t estLlamaciones, uint8_t estLlamacionesOld);

    // pozo:
    void enviaStatusPozo(void);
    void enviaErrorPozo(uint8_t slot);
    void enviaClearErrorPozo(uint8_t numError, uint8_t numEstProblematica);
    void reseteaEstadosOld(void);
    uint8_t haCambiadoEstados(void);
    uint8_t estadoPeticionBomba(void);
    uint8_t gestionaPeticionPozo(uint8_t estacionMsg, uint8_t petBombaMsg);
    void trataObsoletoPozo(void);
    void trataRxPozo(struct msgRx_t *msgRx);
    // llamador
    void trataObsoletoLlamador(void);
    void enviaStatusLlamacion(void);
    uint8_t gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg);
    void trataRxRegistradoryLlamador(struct msgRx_t *msgRx);

    void trataRxRf95(eventmask_t evt);
    void trataRx(struct msgRx_t *msgRx);
    void trataOrdenSMS(char **Vars, uint16_t numVars);
};



#endif /* RADIO_RADIO_H_ */
