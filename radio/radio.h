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

#define MODOLLAMADOR    1
#define MODOPOZO        2
#define MODOREGISTRADOR 3

#define NUMSATELITES                8
#define MSG_STATUSPOZO              1
#define MSG_STATUSLLAMACIONLOCAL    2
#define MSG_ERROR                   3
#define MSG_CLEARERROR              4
#define MAXERRORESAVISO             5


extern "C" {
    void arrancaRadioC(void);
}


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


typedef enum
{
    Llamador = 0,
    Pozo,
    Registrador
} ModoRadio;

class radio
{
protected:
    ModoRadio modo;  // modoRadio
    virtual void trataObsoleto(void) = 0;
    uint16_t bombaPozoOn;                                             // segun mensajes del pozo
    uint16_t numEstadoComOk, numEstadoBomba;
    static uint8_t estadoLlamaciones, estadoActivos, estadoAbusones;  // segun pozo
    static uint8_t estadoAbusonesOld;
    static time_t timeInicioPeticion[NUMSATELITES];
    static time_t timeUltConexion[NUMSATELITES];
    uint8_t numErrorAviso[MAXERRORESAVISO];
    uint8_t idEstacionAviso[MAXERRORESAVISO];
    uint8_t mensajeAviso[MAXERRORESAVISO][21];
    time_t timeInicioAvisoError[MAXERRORESAVISO];
    uint8_t cntTx, cntRx;
    static radio *radioPtr;
public:
//    static void apuntaEnergia(float incEner);
//    static void trataRxRf95Radio(eventmask_t evt);
//    virtual void trataRxRf95(eventmask_t evt) = 0;
    virtual void trataRx(struct msgRx_t *msgRx) = 0;
    virtual uint8_t init(void) = 0;
    static void obsoleto(void);

    static void reseteaHistoriaPozo(void);
    static void reseteaVariables(void);
    virtual void reseteaVariablesEspecificas(void) = 0;
    static void arrancaRadio(void);
    static void paraRadio(void);
    static uint8_t radioDefinida(void);
    uint8_t getCntTx(void);
    uint8_t getCntRx(void);

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
    uint16_t bombaPozoSolicitada;                 // segun la peticion que le he hecho al pozo
    uint16_t estadoDeseado;
    uint8_t  estadoComms;
    uint16_t dsAleatorioMinEntreMsgsLlamador;
    uint16_t dsAleatorioMaxEntreMsgsLlamador;
    struct fechaHora dateTimeEnvioAnterior;
    struct fechaHora dateTimeRxPozoAnterior;
    void trataObsoleto(void);
public:
    llamador(void);
    ~llamador();
    static llamador *llamadorPtr;
    void reseteaVariablesEspecificas(void);
    // para implementar "bloque"
    const char *diNombre(void);
    uint8_t init(void);
    void stop(void);
    void print(BaseSequentialStream *tty);
    void update(uint8_t estadoDeseado);
    const char *diTipo(void);
    uint8_t quitarAbuso(uint8_t numEstacion);
    void onCambioParametrosPozo(void);
    uint8_t limpiaError(uint8_t numEstacion, uint8_t numError);
    void enviaStatusLlamacion(void);
    uint8_t gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg);


    void trataRx(struct msgRx_t *msgRx);
};



class pozo: public radio
{
protected:
    struct fechaHora dateTimeEnvioAnterior;
    uint16_t dsAleatorioMinEntreMsgsPozo;
    uint16_t dsAleatorioMaxEntreMsgsPozo;
public:
    pozo(void);
    ~pozo();
    void reseteaVariablesEspecificas(void);
    // para implementar "bloque"
    const char *diNombre(void);
    uint8_t init(void);
    void stop(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    const char *diTipo(void);
    void reseteaVariables(void);
    uint8_t quitarAbuso(uint8_t numEstacion);
    void onCambioParametrosPozo(void);
    uint8_t buscoSlot(uint8_t numError, uint8_t numEstacion, uint8_t *slot);
    void actualizoErrorDesdeLlamador(uint8_t numEstacion, uint8_t numError, uint8_t *msgError);
    int8_t actualizoErrorDesdePozo(uint8_t numEstacion);

    void registraCambiosPeticion(uint8_t estLlamaciones, uint8_t estLlamacionesOld);

    // pozo:
    void enviaStatusPozo(void);
    void enviaErrorPozo(uint8_t slot);
    void enviaClearErrorPozo(uint8_t numError, uint8_t numEstProblematica);
    void reseteaEstadosOld(void);
    uint8_t haCambiadoEstados(void);
    uint8_t estadoPeticionBomba(void);
    uint8_t gestionaPeticionPozo(uint8_t estacionMsg, uint8_t petBombaMsg);
    void trataObsoleto(void);
    void trataRxPozo(struct msgRx_t *msgRx);
    // llamador
    void trataObsoletoLlamador(void);
    uint8_t gestionaEstadoPozo(uint8_t petBombaMsg, uint8_t estadoLlamacionesMsg, uint8_t estadoActivosMsg);
    void trataRxRegistradoryLlamador(struct msgRx_t *msgRx);

    //void trataRxRf95(eventmask_t evt);
    void trataRx(struct msgRx_t *msgRx);
    void trataOrdenSMS(char **Vars, uint16_t numVars);
};



#endif /* RADIO_RADIO_H_ */
