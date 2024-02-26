/*
 * sms.h
 *
 *  Created on: 1/8/2017
 *      Author: joaquin
 */

#ifndef SMS_H_
#define SMS_H_

#define SECTORNUMTELEF 1
#define SECTORPIN 2
#include "time.h"

uint8_t initSIM800SMS(BaseChannel  *pSD, uint8_t verbose);
void mataSMS(void);
void initThreadSMS(BaseChannel  *pSD);
uint8_t modemOrden(BaseChannel  *pSD, const char *orden, uint8_t *buffer, uint8_t bufferSize, systime_t timeout);
uint8_t modemParametro(BaseChannel  *pSD, const char *orden, uint8_t *buffer, uint16_t bufferSize, systime_t timeout);
uint16_t modemParametros(BaseChannel  *pSD, const char *orden, const char *cadRespuesta, uint8_t *buffer, uint16_t bufferSize, systime_t timeout,
                         uint8_t *numParametros,uint8_t *parametros[]);
void chgetsNoEchoTimeOut(BaseChannel  *pSD,  uint8_t *buffer, uint16_t bufferSize, systime_t timeout, uint8_t *huboTimeout);
uint8_t chgetchTimeOut(BaseChannel  *pSD, systime_t timeout, uint8_t *huboTimeout);
void leoSMS(char *msg, uint16_t buffSize);
int8_t sendSMS(BaseChannel  *pSD, char *msg, char *numTelefono);
uint8_t ponHoraConGprs(BaseChannel  *pSD);
uint8_t horaSMStoTM(uint8_t *cadena, struct tm *fecha);
uint16_t lee2car(uint8_t *buffer, uint16_t posIni,uint16_t minValor, uint16_t maxValor, uint8_t *hayError);
void interpretaSMS(uint8_t *textoSMS);


class sms
{
    //SMS page tlfAdmin logTxt rssiTxt proveedorTxt smsReady
private:
    static char telefonoAdmin[50], pin[50];
    static char mensajeRecibido[200];
    static char msgRespuesta[200];
    static uint8_t estadoPuesto;
    static char telefonoRecibido[16];
    static char telefonoEnvio[16];
    static uint8_t bajoConsumo;
    static uint8_t durmiendo;
    static mutex_t MtxEspSim800SMS;
    static uint8_t horaSMStoTM(uint8_t *cadena, struct tm *fecha);
    static void procesaOrdenAsignacion(char *orden, char *puntSimbIgual);
    static void ponStatusHistorico(void);
    static void procesaStatusYAlgo(char *algo);
    static void procesaID(char *idNum);
    static void procesaStatus(void);
public:
    static BaseChannel  *pSD;
    static uint8_t callReady, smsReadyVal, estadoCREG, rssiGPRS, pinReady, hayError;
    static float vBat;
    static uint8_t bateriaBajaAvisada;
    static uint8_t porcBateriaSIM800L;
    static char proveedor[25];
    static time_t tiempoLastConexion;

    sms(const char *numTelefAdminDefault, const char *pinDefault, BaseChannel *puertoSD, uint8_t bajoConsumo);
    ~sms();

    // para implementar "bloque"
    static const char *diNombre(void);
    static int8_t init(void);
    static void calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    static void print(BaseSequentialStream *tty);
    static void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    static const char *diTipo(void);

    // especifico de SMS
    static void startSMS(void);
    static void mataSMS(void);
    static void leoSMS(char *bufferSendSMS, uint16_t buffSize);
    static uint8_t initSIM800SMS(void);
    static void initThreadSMS(void);
    static const char *diTelefonoAdmin(void);
    static const char *diMsgRespuesta(void);
    static uint8_t diSmsReady(void);
    static uint8_t ponHoraConGprs(void);
    static void interpretaSMS(uint8_t *textoSMS);
    static int8_t sendSMS(char *msg, char *numTelefono);
    static int8_t sendSMSAdmin(void);
    static int8_t sendSMS(void);
    static void avisaBateriaBaja(void);
    static void sleep(void);
    static void despierta(void);
    static void trataOrdenNextion(char *vars[], uint16_t numPars);
    static void leoSmsCMTI(void);
    static void borraMsgRespuesta(void);
    static void addMsgRespuesta(const char *texto);
    static void ponEstado(void);
    static void procesaOrden(char *orden, uint8_t *error);
};

#endif /* SMS_H_ */
