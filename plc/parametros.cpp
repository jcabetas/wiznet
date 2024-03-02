#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "string.h"
#include "stdlib.h"
#include "parametros.h"
#include "bloques.h"
#include "varsFlash.h"
#include "nextion.h"

/*
 * Los parametros son variables de ajuste
 * Pueden ser:
 * - Variables fijas. Por ejemplo 55
 * - Variables ajustables, que deben tener persistencia en flash.
 *   Dependiendo del contexto, pueden ser de diversos tipos
 *   Su identificador es su tipo y el nombre, que incluye la pagina nextion.
 *   [riego.hora1 8 0 23]
 *   [llamacion.Timer_salida,5,1,60]
 * Adicionalmente, los estados son variables que nunca se ajustan, pero pueden
 */

uint16_t parametro::numParams = 0;
parametro *parametro::params[MAXPARAMETROS];

uint16_t parametroFlash::numParamsFlash = 0;
parametroFlash *parametroFlash::paramsFlash[MAXPARAMETROS];

void enviaVal(uint16_t idPage, uint16_t idVar, uint16_t valor);
void enviaTxt(uint16_t idPage, uint16_t idVar, const char *valor);
void enviaTxt(uint16_t idPage, uint16_t idVar, uint16_t valor);
void enviaPic(uint16_t idPage, uint16_t idVar, uint16_t valor);

parametro::parametro()
{
    params[numParams] = this;
    numParams++;
}

parametro::~parametro()
{
}

void parametro::deleteAll(void)
{
    for (int16_t nparam=0;nparam<parametro::numParams;nparam++)
        delete params[nparam];
    parametro::numParams = 0;
}

void parametroFlash::deleteAll(void)
{
    parametroFlash::numParamsFlash = 0;
}


void parametro::printAll(BaseSequentialStream *tty)
{
    if (parametro::numParams>0)
        chprintf(tty, "Hay %d parametros, empezando en %X y siguiente %X\r\n",parametro::numParams,params[0],params[1]);
    else
        chprintf(tty,"No hay parametros definidos\r\n");
    for (int16_t nparam=0;nparam<parametro::numParams;nparam++)
        params[nparam]->print(tty);
}

parametroFlash::parametroFlash()
{
    paramsFlash[numParamsFlash] = this;
    numParamsFlash++;
    idNextionPage =0;
    idNextionVar = 0;
    tipoNextion = 0;
    picBase = 0;
    sectorParam = 0;
}

void parametroFlash::enviaToNextion()
{
}



//void parametroFlash::enviaTodoANextion()
//{
//    for (uint16_t i=0;i<numParamsFlash;i++)
//        paramsFlash[i]->enviaToNextion();
//}
//
//void parametroFlash::enviaPaginaANextion(const char *nomPage)
//{
//    uint16_t idPage = nombres::busca(nomPage);
//    for (uint16_t i=0;i<numParamsFlash;i++)
//        if (paramsFlash[i]->idNextionPage==idPage)
//            paramsFlash[i]->enviaToNextion();
//}

parametroFlash *parametroFlash::findParametroFlash(const char *nomParametro)
{
    uint16_t chkSum = strChecksum(nomParametro);
    for (uint16_t i=0;i<numParamsFlash;i++)
    {
        if (paramsFlash[i]->chkSumNombre==chkSum && !strcasecmp(nomParametro,nombres::nomConId(paramsFlash[i]->idNextionVar)))
            return paramsFlash[i];
    }
    return NULL;
}

parametroFlash *parametroFlash::findParametroFlashNoCase(const char *nomParametro)
{
    for (uint16_t i=0;i<numParamsFlash;i++)
    {
        if (!strcasecmp(nomParametro,nombres::nomConId(paramsFlash[i]->idNextionVar)))
            return paramsFlash[i];
    }
    return NULL;
}

parametroFlash *parametroFlash::findAndSet(const char *nomParametro, const char *valor)
{
    parametroFlash *param;
    param = findParametroFlash(nomParametro);
    if (param==NULL) return NULL;
    param->set(valor);
    return param;
}

/*
 *     parametroU16Valor(uint16_t valor);
    uint16_t valor(void);
    void print(BaseSequentialStream *tty);
 */


parametroU16Valor::parametroU16Valor(uint16_t valor)
{
    valU16 = valor;
}

parametroU16Valor::~parametroU16Valor()
{
}


uint16_t parametroU16Valor::valor(void)
{
    return valU16;
}


void parametroU16Valor::set(uint16_t valor)
{
    valU16 = valor;
}

void parametroU16Valor::set(const char *valor)
{
    valU16 = atoi(valor);
}



void parametroU16Valor::print(BaseSequentialStream *tty)
{
    if (tty!=NULL)
        chprintf(tty,"Valor de U16Valor@%X:%d\r\n",this,valU16);
}

void parametroU16Valor::valorString(char *buff, uint16_t bufferSize)
{
    chsnprintf(buff,sizeof(bufferSize),"%d",valU16);
}


parametroU16Flash::parametroU16Flash(const char *nombre, uint16_t valorIni, uint16_t valorMin, uint16_t valorMax)
{
    sectorParam = 0;
    valU16 = valorIni;
    valIni = valorIni;
    valMin = valorMin;
    valMax = valorMax;
    hallaNombreyDatosNextion(nombre, TIPONEXTIONSTR, &idNextionVar, &idVarWWW, &idNextionPage, &tipoNextion, &picBase);
    chkSumNombre = strChecksum(nombres::nomConId(idNextionVar));
    leeVariableU16(&sectorParam, nombres::nomConId(idNextionVar), valorMin, valorMax,  valorIni, &valU16);
    enviaToNextion();
}

parametroU16Flash::parametroU16Flash(const char *nombre, uint16_t idNxtPage, uint8_t tipoNxt, uint8_t picBas, int16_t valorIni, uint16_t valorMin, uint16_t valorMax)
{
    sectorParam = 0;
    valU16 = valorIni;
    valIni = valorIni;
    valMin = valorMin;
    valMax = valorMax;
    hallaNombreyDatosNextion(nombre, tipoNxt, &idNextionVar, &idVarWWW, &idNextionPage, &tipoNextion, &picBase);
    if (idNextionPage==0)
        idNextionPage = idNxtPage;
    if (picBase==0)
        picBase = picBas;
    chkSumNombre = strChecksum(nombres::nomConId(idNextionVar));
    leeVariableU16(&sectorParam, nombres::nomConId(idNextionVar), valorMin, valorMax,  valorIni, &valU16);
    enviaToNextion();
}

parametroU16Flash::~parametroU16Flash()
{
}

void parametroU16Flash::leeDeFlash()
{
    leeVariableU16(&sectorParam, nombres::nomConId(idNextionVar), valMin, valMax, valIni, &valU16);
}

void parametroU16Flash::set(const char *valor)
{
    valU16 = atoi(valor);
    escribeVarU16(&sectorParam, nombres::nomConId(idNextionVar), valU16);
}

void parametroU16Flash::set(uint16_t valor)
{
    valU16 = valor;
    escribeVarU16(&sectorParam, nombres::nomConId(idNextionVar), valU16);
}


uint16_t parametroU16Flash::valor(void)
{
    return valU16;
}

void parametroU16Flash::print(BaseSequentialStream *tty)
{
    if (tty!=NULL)
        chprintf(tty, "Valor de U16Flash@%X %s.%s: %d\r\n",this,nombres::nomConId(idNextionVar),nombres::nomConId(idNextionPage),valU16);
}


void parametroU16Flash::valorString(char *buff, uint16_t bufferSize)
{
    chsnprintf(buff,sizeof(bufferSize),"%d",valU16);
}


void parametroU16Flash::enviaToNextion(void)
{
    if (idNextionPage==0 || idNextionVar==0)
        return;
    switch (tipoNextion)
    {
    case TIPONEXTIONSILENCIO: break; // no enviar nada
    case TIPONEXTIONNUMERO: enviaVal(idNextionPage, idNextionVar, valU16);
            break;
    case TIPONEXTIONSTR: enviaTxt(idNextionPage, idNextionVar, valU16);
            break;
    case TIPONEXTIONGRAFICO: enviaPic(idNextionPage, idNextionVar, picBase+valU16);
            break;
    case TIPONEXTIONCROP: enviaPicc(idNextionPage, idNextionVar, picBase+valU16);
            break;
    };
}

void parametroU16Flash::recibeDeNextion(char *msgNextion)
{
  valU16 = atoi(msgNextion);
}


parametroU16 *parametro::addParametroU16(BaseSequentialStream *tty, char *parStr, uint8_t *error)
{
    uint8_t numPar;
    char *par[10];

    if (*parStr!='[')
        return new parametroU16Valor(atoi(parStr)); // debe ser solo un numero, poner como valor
    // es un bloque del tipo
    // [riego,0,0,60]
    if (divideBloque(tty, parStr, &numPar, par)!=0)
        return NULL;
    if (numPar != 4)
    {
        *error = 1;
        return new parametroU16Valor(0); // valor cuando hay error
    }
    return new parametroU16Flash(par[0], atoi(par[1]),atoi(par[2]),atoi(par[3]));
}

// parecido, pero con valores maximos y minimos explicitos en llamada
parametroU16Flash *parametro::addParametroU16FlashMinMax(BaseSequentialStream *tty, char *parStr, uint16_t minVal, uint16_t maxVal, uint8_t *error)
{
    uint8_t numPar;
    char *par[10];

    // es un bloque del tipo
    // [llamaPlc,0]
    if (divideBloque(tty, parStr, &numPar, par)!=0)
        return NULL;
    if (numPar != 2)
    {
        *error = 1;
        nextion::enviaLog(tty,"#datos en parametro!=2");
        return NULL; // hay error
    }
    return new parametroU16Flash(par[0], atoi(par[1]),minVal, maxVal);
}

// por ultimo especificando la pagina y tipo de variable
// primer parametro del tipo [IFija 16], segundo:idPaginaNextion, tercero: tipoVariable
//    hallaNombreyDatosNextion(nombre, TIPONEXTIONSTR, &idNextionVar, &idVarWWW, &idNextionPage, &tipoNextion, &picBase);
/*
 *     case TIPONEXTIONNUMERO: enviaVal(idNextionPage, idNextionVar, valU16);
            break;
    case TIPONEXTIONSTR: enviaTxt(idNextionPage, idNextionVar, valU16);
            break;
 */
parametroU16Flash *parametro::addParametroU16FlashMinMax(BaseSequentialStream *tty, char *parStr, uint16_t idPageNxt, uint8_t tipoNxt, uint8_t picBas,
                                                              uint16_t minVal, uint16_t maxVal, uint8_t *error)
{
    uint8_t numPar;
    char *par[10];
    // parStr es un bloque del tipo
    // [llamaPlc,0]
    if (divideBloque(tty, parStr, &numPar, par)!=0)
        return NULL;
    if (numPar != 2)
    {
        *error = 1;
        nextion::enviaLog(tty,"#datos en parametro!=2");
        return NULL; // hay error
    }
    return new parametroU16Flash(par[0], idPageNxt, tipoNxt, picBas, atoi(par[1]),minVal, maxVal);
}


parametroStringValor::parametroStringValor(const char *ptrStrValor)
{
    idVal = nombres::incorpora(ptrStrValor);
}

parametroStringValor::~parametroStringValor()
{
}

void parametroString::set(const char *)
{
 // por defecto no hace nada
}

void parametroString::enviaToNextion(void)
{

}


const char *parametroStringValor::valor(void)
{
    return nombres::nomConId(idVal);
}



const char *parametroStringValor::id(void)  // si es string fijo, el id coincide con el nombre
{
    return nombres::nomConId(idVal);
}

void parametroStringValor::print(BaseSequentialStream *tty)
{
    if (tty!=NULL)
        chprintf(tty, "Valor de StringValor@%X:%s\n\r",this,valor());
}

void parametroStringValor::valorString(char *buff, uint16_t bufferSize)
{
    chsnprintf(buff,sizeof(bufferSize),"%s",valor());
}





parametroStringFlash::parametroStringFlash(const char *nombreConst, const char *valorIni)
{
    char nombre[50];
    strncpy(nombre,nombreConst,sizeof(nombre));
    sectorParam = 0;
    hallaNombreyDatosNextion(nombre, TIPONEXTIONSTR, &idNextionVar, &idVarWWW, &idNextionPage, &tipoNextion, &picBase);
    chkSumNombre = strChecksum(nombres::nomConId(idNextionVar));
    strncpy(valorStr, nombres::nomConId(idNextionVar), MAXLENSTR);
    idValIni = nombres::incorpora(valorIni);
    leeStr50(&sectorParam, nombres::nomConId(idNextionVar), nombres::nomConId(idValIni), valorStr);
}

parametroStringFlash::~parametroStringFlash()
{
}


void parametroStringFlash::leeDeFlash()
{
    leeStr50(&sectorParam, nombres::nomConId(idNextionVar), nombres::nomConId(idValIni), valorStr);
}



void parametroStringFlash::set(const char *valor)
{
    strncpy(valorStr,valor,sizeof(valorStr));
    escribeStr50(&sectorParam, nombres::nomConId(idNextionVar), valorStr);
}

const char *parametroStringFlash::valor(void)
{
    return valorStr;
}

const char *parametroStringFlash::id(void)
{
    return nombres::nomConId(idNextionVar);
}

void parametroStringFlash::print(BaseSequentialStream *tty)
{
    if (tty!=NULL)
        chprintf(tty, "Valor de StringFlash@%X %s.%s: %s\n\r",this,nombres::nomConId(idNextionPage),nombres::nomConId(idNextionVar),valorStr,valorStr);
}

void parametroStringFlash::valorString(char *buff, uint16_t bufferSize)
{
    chsnprintf(buff,sizeof(bufferSize),"%s",valorStr);
}

void parametroStringFlash::enviaToNextion(void)
{
    enviaTxt(idNextionPage,idNextionVar,valorStr);
}

void parametroStringFlash::recibeDeNextion(char *msgNextion)
{
  strncpy(valorStr,msgNextion,sizeof(valorStr));
}





parametroString *parametro::addParametroString(BaseSequentialStream *tty, char *parStr, uint8_t *hayError)
{
    uint8_t numPar;
    char *par[10];

    // si no empieza por '[' es un numero
    if (parStr[0]!='[')
        return new parametroStringValor(parStr); // podria verificar que son numeros
    // es un bloque del tipo
    // [nombreZona,Rocalla]
    divideBloque(tty, parStr, &numPar, par);
    if (numPar != 2)
    {
        *hayError = 1;
        return new parametroStringValor("Error en parametro"); // valor cuando hay error
    }
    return new parametroStringFlash(par[0], par[1]);
}
