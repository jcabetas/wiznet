#ifndef PLC_BLOQUES_H_
#define PLC_BLOQUES_H_

#include <stdint.h>
#include "parametros.h"
#include "nextion.h"

#define STORESIZE 2000
#define MAXSTATES 200
#define MAXBLOQUES 140
#define MAXNOMBRES 400
#define MAXPARAMETROS 80
#define MAXPROGRAMADORES 2
#define MAXPLACASI2C 2
#define MAXZONAS 16
#define MAXSTARTS 4
#define MODOREGISTRADOR 1
#define MODOLLAMADOR    2
#define MODOPOZO        3
#define BUFFWWW 1000



uint8_t divideBloque(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[]);
void divideString(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[]);
uint8_t tipDuracion(BaseSequentialStream *tty, char *descTipDuracion);
uint8_t tienePaginaNextion(const char *nombre);
class sms;

class nombres
{
private:
    static char nombStore[STORESIZE];
    static uint16_t nombStart[MAXNOMBRES];
    static uint16_t numNombres;

public:
    static void init(void);
    static uint16_t incorpora(const char *nombre_p);
    static uint16_t busca(const char *nombre_p);
    static uint16_t buscaNoCase(const char *nombre_p);
    static const char *nomConId(uint16_t id);
};

class estados
{
private:
    static uint8_t estado[MAXSTATES];
    static uint8_t estadoOld[MAXSTATES];
    static uint8_t definidoOut[MAXSTATES];
    static uint16_t idNombre[MAXSTATES];
    static uint16_t idNombreWWW[MAXSTATES];
    static uint16_t idPage[MAXSTATES];
    static uint8_t  picBase[MAXSTATES];
    static uint8_t tipoNextion[MAXSTATES];
    static mutex_t MtxEstado[MAXSTATES];
public:
    uint8_t &operator[](uint16_t);
    static uint16_t numEstados;
    static void init(void);
    static uint16_t addEstado(BaseSequentialStream *tty, char *nombre, uint8_t esOut, uint8_t *hayError);
    static uint16_t findIdEstado(char *nombre);
    static uint16_t findIdEstado(uint16_t idNombre);
    static void ponEstado(uint16_t numEstado, uint8_t valor);
    static uint8_t estaDefinido(uint16_t numEstado);
    static uint8_t diEstado(uint16_t numEstado);
    static uint8_t diNextionPage(uint16_t numEstado);
    static uint8_t estadosInitOk(BaseSequentialStream *tty);
    static void printCabecera(BaseSequentialStream *tty);
    static void print(BaseSequentialStream *tty,uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    static void printAll(BaseSequentialStream *tty);
    static void printSize(BaseSequentialStream *tty);
    static const char *nombre(uint16_t numEstado);
    static uint16_t diIdNombre(uint16_t numEstado);
    static uint16_t diIdNombreWWW(uint16_t numEstado);
    static void ponEstadoEnWWW(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError);
    static void ponNombreWWW(uint16_t numEstado, uint16_t idNombreWWW);
    static void actualizaNextion(uint16_t numEstado);
    static void initWWW(BaseSequentialStream *SDPort, uint16_t numEstado, uint8_t *heVolcado);
    static void cambiosEstadoWWW(uint16_t numEstado);
};



class bloque
{
private:
    static uint16_t numBloques;
    static bloque *logicHistory[MAXBLOQUES];
protected:
    uint8_t hayCambiosWWW;
public:
    bloque();
    virtual ~bloque() = 0;
    virtual int8_t init(void) = 0;
    virtual void stop(void);
    virtual uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    virtual void print(BaseSequentialStream *tty) = 0;
    virtual void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds) = 0;
    virtual const char *diTipo(void) = 0;
    virtual const char *diNombre(void) = 0;
    virtual void trataOrdenNextion(char *vars[], uint16_t numVars);
    virtual void printStatus(char *buffer, uint16_t lenBuffer);
    virtual void statusSMS(sms *smsPtr);
    virtual void initWWW(BaseSequentialStream *SDPort, uint8_t *heVolcado);
    static void initWWW(BaseSequentialStream *SDPort);
    static void procesaStatusBloque(char *nomBloque, sms *smsPtr);
    static bloque *findBloque(char *nombreBloque);
    static void ordenNextionBlq(char *vars[],uint16_t numPars);
    static void vaciaBloques(void);
    static int8_t initBloques(void);
    static void stopBloques(void);
    static void addTimeBloques(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    static int8_t actualizaBloques(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    static void printBloques(BaseSequentialStream *tty);
    static void deleteAll(void);
    static uint16_t numero(void);
};


class www : public bloque
{
protected:
    static uint8_t definido;
    static uint8_t plcActivo;
    static www *ptrWWW;
public:
    www(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~www();
    static uint8_t estaDefinido(void);
    static uint8_t estaActivo(void);
    int8_t init(void);
    void stop(void);
    void initWWWThread(void);
    void mataWWWCom(void);
    static void trataRxWWW(char *bufferReceivedWWW);
    static void estadoWWW(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    const char *diTipo(void);
    const char *diNombre(void);
    static void initWWW(void);
};


class fecha : public bloque
{
protected:
    uint16_t idPage, idYear, idMes, idDia, idHora, idMin;
    static uint8_t actualizacionPendiente;
public:
    fecha(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~fecha();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void trataOrdenNextion(char *vars[], uint16_t numPars);
    void actualizaNextion(uint8_t idPagNextion);
    void statusSMS(sms *smsPtr);
    static void actualizaCuandoPuedas(void);
};



class grupo : public bloque
{
protected:
    uint16_t idGrupo;
    uint8_t numComponentes;
    uint16_t idNombres[10];

public:
    grupo(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~grupo();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};


class hora : public bloque
{
protected:
    uint16_t idPage, idHora, idMin;

public:
    hora(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~hora();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void trataOrdenNextion(char *vars[], uint16_t numPars);
    void actualizaNextion(uint8_t idPagNextion);
};

class add : public bloque
{
protected:
    uint16_t numOut;
    uint16_t numInputs[4];
    uint8_t oldStateNextion;
public:
    add(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~add();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class NOT : public bloque
{
protected:
    uint16_t numOut;
    uint16_t numInput;

public:
    NOT(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~NOT();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};



class esdenoche : public bloque
{
protected:
    uint16_t numOut;
    uint16_t minUTMAmanecer,minUTMAnochecer;
    uint32_t segundosUTMCambioInv2Ver, segundosUTMCambioVer2Inv;
    float latitudRad, longitudRad;
    void hallaMinAmanecer(void);
    void calculaHorasSolares(void);
    uint8_t CheckLeap(uint16_t uint16_t_Year);
    uint32_t secsUTM;
    struct tm fechaLocal;
    Mutex MtxSecs;
public:
    esdenoche(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~esdenoche();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class LED : public bloque
{
protected:
    uint16_t numInput;
public:
    LED(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~LED();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class pulsador : public bloque
{
protected:
    uint16_t numOutput;
public:
    pulsador(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~pulsador();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class medida;
class contador : public bloque
{
protected:
    static float k;  // constante pulso=>cantidad
    static uint32_t contadorImpulsos;
    static medida   *contadorCantidad;
    static medida   *flujo; // cantidad/hora
    static uint32_t maxMsEntrePulsos; // a partir de este valor, ponemos cero como flujo
    static systime_t lastPulseTime;
    static uint32_t msEntrePulsos;
    static bool recalcula;
public:
    contador(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~contador();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    static void incrementaPulso(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class rele : public bloque
{
protected:
    uint16_t numInput;
public:
    rele(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~rele();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class timer : public bloque
{
protected:
    int16_t numOut;
    int16_t numInput;
    uint16_t cuenta;
    parametroU16 *tiempo;
    uint8_t tipoCuenta;
    uint8_t horaIni, minIni, secIni, dsIni;

public:
    timer(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~timer();//static void operator delete(void* ptr);
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class pulso : public bloque
{
protected:
    int16_t numOut;
    int16_t numInput;
    uint8_t inputOld;
    uint16_t cuenta;
    parametroU16 *tiempo;
    uint8_t tipoCuenta;
    uint8_t horaIni, minIni, secIni, dsIni;

public:
    pulso(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~pulso();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class timerNoRedisp : public bloque
{
protected:
    int16_t numOut;
    int16_t numInput;
    uint16_t cuenta;
    parametroU16 * tiempo;
    uint8_t tipoCuenta;
    uint8_t horaIni, minIni, secIni, dsIni;

public:
    timerNoRedisp(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~timerNoRedisp();//static void operator delete(void* ptr);
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class delayon : public bloque
{
protected:
    int16_t numOut;
    int16_t numInput;
    uint16_t cuenta;
    parametroU16 *tiempo;
    uint8_t contando;
    uint8_t tipoCuenta;
    uint8_t horaIni, minIni, secIni, dsIni;

public:
    delayon(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~delayon();//static void operator delete(void* ptr);
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class OR : public bloque
{
protected:
    int16_t numOut;
    int16_t numInputs[4];

public:
    OR(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~OR();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class flipflop : public bloque
{
protected:
    int16_t numOut;
    int16_t numInputSet;
    int16_t numInputReset;

public:
    flipflop(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~flipflop();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};

class inputTest : public bloque
{
protected:
    uint16_t numOut;
    uint16_t cuentaDs;
    uint8_t segIni, segFin;

public:
    inputTest(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~inputTest();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};

class programador;

class zona : public bloque
{
protected:
    programador *program;
    parametroString *descripcion;
    int16_t idNombre, numOut;
    uint8_t estadoOld;
    parametroU16Flash *minutosA;
    parametroU16Flash *minutosB;
    float flujoMax;
    float ultimaMedidaFlujo;
    bool bloquearPorFlujoMax;
    uint16_t idPageTxt;
public:
    zona(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~zona();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void actualizaNextion(uint8_t idPage);
    void leeMinutos(char *minutos, char *AoB);
    uint8_t setFlujoLeido(float flujo);
    void ponBloqueoPorFlujoMax(bool bloquear);
    bool flujoExcedido(void);
    float diUltimaMedidaFlujo(void);
    void reconoceFlujoMax(void);
    //void trataOrdenNextion(char *vars[], uint16_t numVars);
    uint16_t diTiempo(uint8_t esB);
    void ponSalida(uint8_t estado);
    uint8_t diSalida(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t msInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};

class start : public bloque
{
protected:
    programador *program;
    uint16_t numNombre;
    parametroU16Flash *esB;
    parametroU16Flash *DOW;
    parametroU16Flash *hora;
    parametroU16Flash *min;
public:
    start(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~start();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    static uint8_t decodeDOW(const char *DOW);
    static void encodeDOW(uint8_t dowByte, char *DOW, uint8_t sizeDOW);
    void leeDOW(const char *DOW);
    void leeESP(const char *esEsp);
    void leeH(const char *hor);
    void leeM(const char *mn);
    void actualizaNextion(uint8_t idPage);
    void envia2plc(BaseSequentialStream *SDPort);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t msInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
};

class programador : public bloque
{
protected:
    zona *zonas[MAXZONAS];
    start *starts[MAXSTARTS];
    uint8_t picBase;
    uint16_t numNombre;
    uint8_t estado; // 0 o zonaActiva
    uint8_t cicloArrancado;
    uint8_t soloUnCiclo;
    uint16_t numProgOn;
    uint16_t numOutBomba;
    uint16_t numSuspendeConteo;
    uint16_t dsQueFaltan;
    uint16_t idPageTxt, idNombreTxt, idNombreTxtWWW;
    uint8_t estadoOld, estadoSuspendeOld, cambiadoDs, activoOld, cambiadoStarts;
    uint16_t idPageSt, idNombreSt, idNombreStWWW; // para poner estado en Nextion
    uint8_t  picBaseSt, tipoNextionSt;
    uint16_t segZonaActiva;
    medida *medLitros;
    medida *medCaudal;
    uint16_t idPageZonas;
    uint16_t idNombFlujos;
    float contLitrosInicioZona;
public:
    programador(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~programador();
    static uint8_t numProgramadores;
    uint8_t numZonas;
    uint8_t numStarts;
    void asignaZona(zona *zon);
    uint8_t asignaStart(BaseSequentialStream *tty, start *strt);
    const char *diTipo(void);
    const char *diNombre(void);
    void arranca(uint8_t esB);
    void arrancaZona(uint8_t numZona, uint8_t esB);
    void actualizaZonas(void);
    uint8_t programaActivado(void);
    void alertaFlujoExcedido(void);
    void actualizaFlujoZonaEnNextion(uint8_t numZona);
    void stop(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void printStatus(char *buffer, uint16_t longBuffer);
    void statusSMS(sms *smsPtr);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void actualizaNextion(uint8_t idPage);
    void trataOrdenNextion(char *vars[], uint16_t numPars);
    void initWWW(BaseSequentialStream *SDPort, uint8_t *heVolcado);
    void cambiosEstadoWWW(void);
    void cambiaBombaWWW(void);
    void cambiaEsperaWWW(void);
    void cambiosTiemposWWW(uint8_t zona, char codAB, uint8_t tiempo);
    void cambiosFlujomaxWWW(uint8_t zona, uint16_t tiempo);
    void cambiosDowWWW(uint8_t numArranque, char *dowStr);
    void cambiosHStartWWW(uint8_t numArranque, uint8_t hora);
    void cambiosMStartWWW(uint8_t numArranque, uint8_t min);
    void cambiosESBStartWWW(uint8_t numArranque, uint8_t esB);
};



class placaI2C;

class pinI2C: public bloque
{
protected:
    uint16_t numEstado;
    uint8_t valor, valorOld;
    uint8_t numPin;
    placaI2C *placaMadre;
    friend class placaI2C;
public:
    pinI2C(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~pinI2C();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void actualizaNextion(uint8_t idPage);
    void trataOrdenNextion(char *vars[], uint16_t numPars);
    void statusSMS(sms *smsPtr);
};



/*
typedef struct  {
    uint8_t dirI2C;
    uint8_t status;
    int numErrores;
    uint8_t esOutput;
    uint16_t valor;
    uint16_t codError;
    } placa;
 */
class placaI2C: public bloque
{
protected:
    parametroU16 *dirI2C;
    uint8_t esOutput;
    uint8_t numEstado;
    uint16_t valor, valorOld;
    uint16_t numEstadoPlaca; // indica si esta ok (cuando esta a 1)
    pinI2C *pines[16];
    friend class pinI2C;
    static placaI2C *placasI2C[MAXPLACASI2C];
public:
    placaI2C(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~placaI2C();
    static uint8_t numPlacas;
    static placaI2C *diPlaca(uint8_t numPlaca);
    static void leeEscribeAll(uint8_t checkIO);
    void actualizaPlacaInp(void);
    mutex_t MtxI2CValor;
    void asignaPin(pinI2C *pin);
    const char *diTipo(void);
    static void pararI2C(void);
    static void startI2C(void);
    static const char *diNombre(uint8_t numPlaca);
    const char *diNombre(void);
    int8_t init(void);
    static uint8_t configIO(void);
    void stop(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void actualizaNextion(uint8_t idPage);
    void trataOrdenNextion(char *vars[], uint16_t numPars);
    void statusSMS(sms *smsPtr);
};





class sino : public bloque
{
protected:
    int16_t numOut;
    parametroU16Flash *valor;
    uint8_t estadoWWW;
public:
    sino(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~sino();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint16_t diTiempo(uint8_t esB);
    void ponSalida(uint8_t estado);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t msInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void trataOrdenNextion(char **Vars, uint16_t numVars);
    void printStatus(char *buffer, uint8_t longBuffer);
    void statusSMS(sms *smsPtr);
    void initWWW(BaseSequentialStream *SDPort, uint8_t *heVolcado);
    void cambiosAjusteWWW(void);
};

class sinoauto : public bloque
{
protected:
    int16_t numOut, numInput;
    parametroU16Flash *ajuste;
    uint8_t estadoOld;
    uint8_t estadoAjusteWWW, estadoWWW;
public:
    sinoauto(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~sinoauto();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    uint8_t calcula(uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    uint16_t diTiempo(uint8_t esB);
    void ponSalida(uint8_t estado);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t msInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void trataOrdenNextion(char **Vars, uint16_t numVars);
    void actualizaNextion(void);
    void printStatus(char *buffer, uint8_t longBuffer);
    void statusSMS(sms *smsPtr);
    void initWWW(BaseSequentialStream *SDPort, uint8_t *heVolcado);
    void cambiosEstadoWWW(void);
    void cambiosAjusteWWW(void);
};


class estadoEnNextion: public bloque
{
protected:
    uint16_t idPage, idNombre, idNombreWWW, numEstado;
    uint8_t picBase, tipoNextion, estadoOld;
public:
    estadoEnNextion(BaseSequentialStream *tty,uint8_t numPar, char *pars[], uint8_t *hayError);
    ~estadoEnNextion();
    const char *diTipo(void);
    const char *diNombre(void);
    int8_t init(void);
    void print(BaseSequentialStream *tty);
    void addTime(uint16_t dsInc, uint8_t hora, uint8_t min, uint8_t seg, uint8_t ds);
    void statusSMS(sms *smsPtr);
};



#endif /* PLC_BLOQUES_H_ */
