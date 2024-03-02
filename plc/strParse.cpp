#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "string.h"
#include "bloques.h"
#include "nextion.h"
#include <stdlib.h>


extern "C"
{
    void checkDivideString(void);
}

uint8_t tipDuracion(char *descTipDuracion)
{
  if (*descTipDuracion=='s')
    return 1;
  if (*descTipDuracion=='m')
    return 2;
  if (*descTipDuracion=='h')
    return 3;
  return 0;
}

/*
 * Divide string por espacios o por comas
 * Lo que haya entre [] se considera un solo grupo
 */
void divideString(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[])
{
    uint8_t comasEnCurso;
    //  sustituyo CR o LF por nulos
    for (uint8_t i = 0; i < strlen(buff); i++)
        if (buff[i] == '\n' || buff[i] == '\r')
            buff[i] = 0;
    *numPar = 0;
    comasEnCurso = 0;
    while (*buff != 0)
    {
        char car = *buff;
        if (car == ' ')
        {
            *(buff++) = 0; // eliminamos espacios en blanco
            continue;
        }
        if (car == ',' && comasEnCurso==0)
        {
            *(buff++) = 0; // eliminamos si hay una sola coma
            comasEnCurso++;
            continue;
        }
        if (car == '[')
        {
            par[*numPar] = buff++;
            (*numPar)++;
            while (*buff)
            {
                if (*buff == ']') // fin de bloque, ok
                {
                    buff++;
                    break;
                }
                if (*buff == 0) // final sin fin de bloque, mal
                {
                    nextion::enviaLog(tty,"Error: '[' sin cierre");
                    *numPar = 0;
                    return;
                }
                buff++;
            }
            comasEnCurso = 0;
            continue;
        }
        if (car == ',')
        {
            *buff = 0;
            par[*numPar] = buff;
            (*numPar)++;
            buff++;
            comasEnCurso = 0;
            continue;
        }
        // estamos en un parametro
        par[*numPar] = buff++;
        (*numPar)++;
        comasEnCurso = 0;
        while (*buff != ' ' && *buff != ',' && *buff != '[' && *buff != 0) // recorremos parametros
        {
            // si son comillas, salta hasta su final
            if (*buff == '"')
            {
                buff++;
                while (*buff)
                {
                    if (*buff == '"') // fin de bloque, ok
                        break;
                    if (*buff == 0) // final de linea sin fin de bloque, mal
                    {
                        nextion::enviaLog(tty,"Error: '\"' sin cierre");
                        *numPar = 0;
                        return;
                    }
                    buff++;
                }
            }
            buff++;
        }
    }
    if (comasEnCurso)
    {
        par[*numPar] = buff;
        (*numPar)++;
    }
}


void checkDivideString(void)
{
    char buff[60];
    uint8_t numPar;
    char *par[10];

    //    void divideString(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[])
        strncpy(buff,"23   44",sizeof(buff));
        divideString(NULL,buff,&numPar,par);
        strncpy(buff,"23,,   44",sizeof(buff));
        divideString(NULL,buff,&numPar,par);
        strncpy(buff,"23,,44,",sizeof(buff));
        divideString(NULL,buff,&numPar,par);
        strncpy(buff,"23,[],44,",sizeof(buff));
        divideString(NULL,buff,&numPar,par);
}
/*
 * Identifica parametros en bloques [a, b, c]
 * Devuelve 1 si no empieza por '['
 */
uint8_t divideBloque(BaseSequentialStream *tty, char *buff, uint8_t *numPar, char *par[])
{
    if (buff[0] != '[')
    {
        nextion::enviaLog(tty,"Error, bloque no empieza por '['");
        *numPar = 0;
        return 1;
    }
    buff++;
    //  sustituyo CR o LF por nulos
    for (uint8_t i = 0; i < strlen(buff); i++)
        if (buff[i] == '\n' || buff[i] == '\r')
            buff[i] = 0;
    *numPar = 0;
    char *token = strtok(buff, ", ]");
    while (token != NULL)
    {
        par[*numPar] = token;
        (*numPar)++;
        token = strtok(NULL, ", ]");
    }
    return 0;
}


uint8_t tienePaginaNextion(const char *nombre)
{
  char* pPosition = strchr(nombre, '.');
  if (pPosition==NULL)
    return 0;
  else
    return 1;
}

/*
 *   En una variable o parametro opcionalmente se puede poner datos para sacarlo por Nextion
 *   se necesitar� idNombreVar,idPageNextion,tipoNextion, picBase
 *   p.e.
 *   riegoActivo: no se publica nada en Nextion
 *   riegoActivo.riego.val: un objeto que hay que actualizar val, normalmente 0, 1 (o 2 como mucho) en pagina riego
 *   riegoActivo.riego.txt: se pone un texto descriptivo, p.e.
 *   riegoActivo.riego.pic.12: un objeto para actualizar el valor pic; la variable se sumara a picBase
 *   opcionalmente tiene nombreWWW al final encerrado entre "", p.e. riegoActivo.riego.pic.12"Riego funcionando"
 *   devuelve 0 si no hay error
 *
 */

uint8_t hallaNombreyDatosNextion(const char *paramConst, uint8_t tipoNextDefault, uint16_t *idName, uint16_t *idNameWWW, uint16_t *idPage, uint8_t *tipoNextion, uint8_t *picBase)
{
    char buff[30];
    char param[70];
    char *ptrTok;
    strncpy(param, paramConst, sizeof(param));
    *idName = 0;
    *idNameWWW = 0;
    *idPage = 0;
    *tipoNextion = tipoNextDefault;
    *picBase = 0;
    if (strlen(param)<3)
        return 1; //  parametro muy corto
    // nombre
    // primero miro si tiene al final nombreWWW entre ""
    char* cPositionIni = strchr(param, '"');
    if (cPositionIni!=NULL)
    {
        *cPositionIni = 0; // sera el final del resto de parametros
        cPositionIni++;
        char* cPositionFin = strchr(cPositionIni, '"');
        if (cPositionFin==NULL) // no hay comillas de cierre!!
            return 2;
        *cPositionFin = 0;
        *idNameWWW = nombres::incorpora(cPositionIni);
    }
    ptrTok = strtok(param,".");
    if (ptrTok==NULL)
        return 1; // no deberia ocurrir, ya que he exigido algo de longitud
    *idName = nombres::incorpora(ptrTok);
    // pagina nextion
    ptrTok = strtok(NULL,".");
    if (ptrTok==NULL)
        return 0;
    *idPage = nombres::incorpora(ptrTok);
    // tipo de respuesta nextion, por defecto val si no se define
    *tipoNextion = tipoNextDefault;
    ptrTok = strtok(NULL,".");
    if (ptrTok==NULL)
        return 0; // solo tenia nombre
    // decodifico tipo de respuesta Nextion
    /*
     *  #define TIPONEXTIONSILENCIO 0
        #define TIPONEXTIONSTR 1
        #define TIPONEXTIONNUMERO 2
        #define TIPONEXTIONGRAFICO 3
        #define TIPONEXTIONCROP 4
     */
    if (!strcasecmp(ptrTok,"val"))
        *tipoNextion = TIPONEXTIONNUMERO;
    else if (!strcasecmp(ptrTok,"pic"))
        *tipoNextion = TIPONEXTIONGRAFICO;
    else if (!strcasecmp(ptrTok,"picc"))
        *tipoNextion = TIPONEXTIONCROP;
    else if (!strcasecmp(ptrTok,"txt"))
        *tipoNextion = TIPONEXTIONSTR;
    else
    {
        *idPage = 0;
        *tipoNextion = TIPONEXTIONSILENCIO;
        chsnprintf(buff,sizeof(buff),"nextion %s no valido",ptrTok);
        nextion::enviaLog(NULL,buff);
        return 2;
    }
    // picBase
    ptrTok = strtok(NULL,".");
    if (ptrTok==NULL && (*tipoNextion==TIPONEXTIONGRAFICO || *tipoNextion==TIPONEXTIONCROP))
    {
        *idPage = 0;
        *tipoNextion = TIPONEXTIONSILENCIO;
        nextion::enviaLog(NULL,"nextion pic/picc sin picBase");
        return 3;
    }
    else if (ptrTok!=NULL && *tipoNextion!=TIPONEXTIONGRAFICO && *tipoNextion!=TIPONEXTIONCROP)
    {
        nextion::enviaLog(NULL,"demasiados parametros var nextion");
        return 4;
    }
    else if (ptrTok!=NULL && (*tipoNextion==TIPONEXTIONGRAFICO || *tipoNextion==TIPONEXTIONCROP))
    {
        *picBase = atoi(ptrTok);
        return 0;
    }
    return 0;
}

// devuelve 0 si no hay error
//uint8_t hallaPageyNombreNextion(char *param, uint16_t *idPage, uint16_t *idName)
//{
//  *idPage = 0;
//  *idName = 0;
//  if (strlen(param)<3)
//      return 1; //  parametro muy corto
//  for (uint8_t i=0;i<strlen(param);i++)
//    if (param[i]=='.')
//    {
//      param[i] = 0;
//      if (strlen(param)<1)
//          return 2;//falta pagina nextion
//      if (strlen(&param[i+1])<1)
//          return 3; //falta nombre variable nextion
//      *idPage = nombres::incorpora(param);
//      *idName = nombres::incorpora(&param[i+1]);
//      if (*idPage==0 || *idName==0)
//          return 4;
//      return 0;
//    }
//  *idPage = 0;
//  *idName = nombres::incorpora(param);
//  return 5; //No hay separador pagina y nombre
//}



// devuelve 0 si no hay error, y en parametros NULL si no existe
void divideEnPageyVarNextion(char *param, char **page, char **var)
{
  *page = NULL;
  *var = NULL;
  for (uint8_t i=0;i<strlen(param);i++)
  {
      if (param[i]=='.')
      {
          param[i] = 0;
          if (strlen(param)>0)
              *page = param;
          if (strlen(&param[i+1])>0)
              *var = &param[i+1];
      }
  }
  // no he encontrado '.', no hay pagina en la variable
  if (strlen(param)>0)
      *var = param;
}


