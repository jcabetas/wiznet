#ifndef PLC_PARAMETROS_H_
#define PLC_PARAMETROS_H_

#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include <stdint.h>

#define MAXPARAMETROS 80
#define MAXLENSTR  50
class parametroU16;
class parametroString;
class parametroU16Flash;

uint8_t hallaPageyNombreNextion(char *param, uint16_t *idPage, uint16_t *idName);

/*

parametro (para llevar cuenta de objetos y borrarlos con funcion virtual)

parametroFlash (mutable, se persiste en flash), es virtual porque necesita definir escribir y leer a flash
                            lleva la cuenta para poder leer/escribir masivamente a flash y escribir a Nextion)

parametroU16 (tiene funcion virtual para leer y escribir variable U32), es virtual porque puede implantar o no
   |
   |__________parametroU16Valor: parametro
   |__________parametroU16flash: parametro, parametroFlash

parametroString
*/

// esta clase es para llevar la cuenta
// de objetos parametros, y luego borrarlos
class parametro
{
private:
    static uint16_t numParams;
    static parametro *params[MAXPARAMETROS];
public:
    parametro();
    virtual ~parametro() = 0;
    static parametroU16 *addParametroU16(BaseSequentialStream *tty, char *parStr, uint8_t *hayError);
    static parametroString *addParametroString(BaseSequentialStream *tty, char *parStr, uint8_t *hayError);
    static parametroU16Flash *addParametroU16FlashMinMax(BaseSequentialStream *tty, char *parStr, uint16_t minVal, uint16_t maxVal, uint8_t *error);
    static parametroU16Flash *addParametroU16FlashMinMax(BaseSequentialStream *tty, char *parStr, uint16_t idPageNxt, uint8_t tipoNxt, uint8_t picBas,
                                                                  uint16_t minVal, uint16_t maxVal, uint8_t *error);
    static void deleteAll(void);
    static void printAll(BaseSequentialStream *tty);
    virtual void print(BaseSequentialStream *tty) = 0;
};

class parametroFlash
{
protected:
    static uint16_t numParamsFlash;
    static parametroFlash *paramsFlash[MAXPARAMETROS];
    uint16_t sectorParam;
public:
    uint16_t idNextionPage, idNextionVar, idVarWWW;
    uint8_t tipoNextion, picBase;
    uint16_t chkSumNombre;
    parametroFlash();
    static void deleteAll(void);
    static parametroFlash *findAndSet(const char *nombreParametro, const char *valor);
    static parametroFlash *findParametroFlash(const char *nombreParametro);
    static parametroFlash *findParametroFlashNoCase(const char *nombreParametro);
//    static void enviaTodoANextion();
//    static void enviaPaginaANextion(const char *nomPage);
    virtual void set(const char *valor) = 0;
    virtual void leeDeFlash() = 0;
    virtual void enviaToNextion(void) = 0;
    virtual void recibeDeNextion(char *msgNextion) = 0;
    virtual void valorString(char *buffer, uint16_t bufferSize) = 0;
};

class parametroU16
{
public:
    virtual uint16_t valor(void) = 0;
    virtual void set(uint16_t valor)=0;
    virtual void set(const char *valor)=0;
};

class parametroU16Valor: public parametro, public parametroU16
{
private:
    uint16_t valU16;
public:
    ~parametroU16Valor();
    parametroU16Valor(uint16_t valor);
    void set(uint16_t valor);
    void set(const char *valor);
    uint16_t valor(void);
    void print(BaseSequentialStream *tty);
    void valorString(char *buffer, uint16_t bufferSize);
};

class parametroU16Flash : public parametro, public parametroFlash, public parametroU16
{
private:
    uint16_t valU16;
    uint16_t valMin;
    uint16_t valMax;
    uint16_t valIni;
public:
    ~parametroU16Flash();
    parametroU16Flash(const char *nombre, uint16_t valorIni, uint16_t valorMin, uint16_t valorMax);
    parametroU16Flash(const char *nombre, uint16_t idNxtPage, uint8_t tipoNxt, uint8_t picBas, int16_t valorIni, uint16_t valorMin, uint16_t valorMax);
    void leeDeFlash();
    void set(const char *valor);
    void set(uint16_t valor);
    void enviaToNextion(void);
    void recibeDeNextion(char *msgNextion);
    uint16_t valor(void);
    void print(BaseSequentialStream *tty);
    void valorString(char *buffer, uint16_t bufferSize);
};


class parametroString
{
public:
    virtual const char *valor(void) = 0;
    virtual const char *id(void) = 0;
    virtual void set(const char *valor);
    virtual void enviaToNextion(void);
    virtual void valorString(char *buffer, uint16_t bufferSize) = 0;
};

class parametroStringValor : public parametro, public parametroString
{
private:
    uint16_t idVal;
public:
    ~parametroStringValor();
    parametroStringValor(const char *nombreVar);
    const char *valor(void);
    const char *id(void);
    void print(BaseSequentialStream *tty);
    void valorString(char *buffer, uint16_t bufferSize);
};

class parametroStringFlash : public parametroFlash, public parametroString
{
private:
    char valorStr[MAXLENSTR];
    uint16_t idValIni;
public:
    ~parametroStringFlash();
    parametroStringFlash(const char *nombre, const char *valorIni);
    void leeDeFlash();
    void set(const char *valor);
    const char *valor(void);
    const char *id(void);
    void print(BaseSequentialStream *tty);
    void valorString(char *buffer, uint16_t bufferSize);
    void enviaToNextion(void);
    void recibeDeNextion(char *msgNextion);
//    chprintf((BaseSequentialStream *)&SD1,"%s.%s.val=%d%c%c%c",pageName,varName,valor,0xFF,0xFF,0xFF);
};


#endif /* PLC_PARAMETROS_H_ */
