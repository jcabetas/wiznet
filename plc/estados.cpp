#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "nextion.h"
#include "colas.h"

/*
 * estados
 *  - Si el nombre es del tipo "estadoBomba.main" quiere decir que esta definido en Nextion
 *    y cuando cambie valor lo tiene que enviar a Nextion (quizas solo si esta en esa pagina)
 * La representación en nextion tiene:
    static uint8_t estado[MAXSTATES];     // devuelve valor del estado correspondiente
    static uint8_t estadoOld[MAXSTATES];  // idem estado anterior iteración en tiempo
    static uint8_t definidoOut[MAXSTATES];// si es 1, ya hay alguien que define su valor
    static uint16_t idNombre[MAXSTATES];  // 0 si no tiene identificados a nombre, 1..nombres::numNombres
    static uint16_t idPage[MAXSTATES];    // 0 si no tiene pagina, 1..nombres::numNombres con nombre de la pagina
    static uint8_t  picBase[MAXSTATES];   // 0 si no esta definida, valor del fichero grafico base en nextion
    static uint8_t tipoNextion[MAXSTATES] // 0 si no esta definido, 1..4 o 99
 */

estados est;
uint8_t hayCambios;

uint16_t estados::numEstados = 0;
uint8_t estados::estado[MAXSTATES] = {0};
uint8_t estados::estadoOld[MAXSTATES] = {0};
uint8_t estados::definidoOut[MAXSTATES] = {0};
uint16_t estados::idNombre[MAXSTATES] = {0};
uint16_t estados::idNombreWWW[MAXSTATES] = {0};
uint16_t estados::idPage[MAXSTATES] = {0};
uint8_t estados::picBase[MAXSTATES] = {0};
uint8_t estados::tipoNextion[MAXSTATES] = {0};
mutex_t estados::MtxEstado[MAXSTATES];// = {0};

//uint8_t hallaNombreyDatosNextion(char *param, uint16_t *idName, uint16_t *idPage, uint8_t *tipoNextion, uint8_t *picBase);
extern struct queu_t colaMsgTxWWW;

void estados::init(void)
{
    numEstados = 0;
    for (uint16_t i=0;i<MAXSTATES;i++)
    {
        estado[i] = 0;
        estadoOld[i] = 0xFF;
        definidoOut[i] = 0;
        idNombre[i] = 0;
        idPage[i] = 0;
        picBase[i] = 0;
        tipoNextion[i] = 0;
        chMtxObjectInit(&MtxEstado[i]);
    }
};

uint8_t &estados::operator[](uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //printf("Numero estado ilegal en []\n");
    }
    return estado[numEstado - 1];
}

uint16_t estados::findIdEstado(uint16_t idNom)
{
    uint16_t idEstado;
    idEstado = 0;
    if (numEstados>0)
        for (uint16_t idEstadoN = 0; idEstadoN<numEstados; idEstadoN++)
            if (idNom == idNombre[idEstadoN])
            {
                idEstado = (uint16_t) idEstadoN+1;
                break;
            }
    return idEstado;
}

uint16_t estados::findIdEstado(char *nombre)
{
    uint16_t idNom = nombres::buscaNoCase(nombre);
    if (idNom == 0) return 0;
    return findIdEstado(idNom);
}


/*
 * Add nuevo estado (id = 1..estados::numEstados)
 * p.e. fOOA.base.picc.15
 */
uint16_t estados::addEstado(BaseSequentialStream *tty, char *nombre, uint8_t esOut, uint8_t *hayError)
{
    char buffer[80];
    uint8_t tipoNxtProv, picBaseNxtProv;
    uint16_t idNombreNxtProv, idPageNxtProv, idEstado, idNombWWW;

    uint8_t error = hallaNombreyDatosNextion(nombre, TIPONEXTIONALL, &idNombreNxtProv, &idNombWWW, &idPageNxtProv, &tipoNxtProv, &picBaseNxtProv);
    if (error)
        *hayError = 1;

    // existia este estado (fijarse en el nombre) ?
    if (idNombreNxtProv > 0)
    {
        // ahora busca el estado que tenga el nombre con ese id
        idEstado = findIdEstado(idNombreNxtProv);
        if (idEstado>0)
        {
            // tiene datos nextion?
            uint8_t errorUpdatingNextion = 0;
            if (idPageNxtProv!=0)
            {
                if (idPage[idEstado-1] && idPage[idEstado-1]!=idPageNxtProv)
                    errorUpdatingNextion = 1;
                else
                    idPage[idEstado-1] = idPageNxtProv;
            }
            if (picBaseNxtProv!=0)
            {
                if (picBase[idEstado-1] && picBase[idEstado-1]!=picBaseNxtProv)
                    errorUpdatingNextion = 1;
                else
                    picBase[idEstado-1] = picBaseNxtProv;
            }
            if (tipoNxtProv!=99)
            {
                if (tipoNextion[idEstado-1]!=99 && tipoNextion[idEstado-1]!=tipoNxtProv)
                    errorUpdatingNextion = 1;
                else
                    tipoNextion[idEstado-1] = tipoNxtProv;
            }
            if (idNombWWW!=0)
                idNombreWWW[idEstado-1] = idNombWWW;
            if (errorUpdatingNextion)
            {
                chsnprintf(buffer,sizeof(buffer),"Estado %s con datos nextion incoherentes", nombres::nomConId(idNombreNxtProv));
                if (tty!=NULL)
                    chprintf(tty,buffer);
                else
                    nextion::enviaLog(tty,buffer);
                *hayError = 1;
            }
            if (esOut)
            {
                if (definidoOut[idEstado-1])
                {
                    chsnprintf(buffer,sizeof(buffer),"Salida %s se define varias veces", nombres::nomConId(idNombreNxtProv));
                    if (tty!=NULL)
                        chprintf(tty,buffer);
                    else
                        nextion::enviaLog(tty,buffer);
                    *hayError = 1;
                    return 0;
                }
                else
                {
                    definidoOut[idEstado-1] = esOut;
                }
            }
            return idEstado;
        }
    }
    estado[numEstados] = 0;
    estadoOld[numEstados] = 0xFF;
    definidoOut[numEstados] = esOut;
    idNombre[numEstados] = idNombreNxtProv;
    idNombreWWW[numEstados] = idNombWWW;
    idPage[numEstados] = idPageNxtProv;
    picBase[numEstados] = picBaseNxtProv;
    tipoNextion[numEstados] = tipoNxtProv;
//
//
//    estado[numEstados] = 0;
//    idNom2idEstado[idNombre] = numEstados;
//    definidoOut[numEstados] = esOut;
//    campoNxt[numEstados] = campoCN;
//    if (idPagina>0)
//    {
//        idNextionPage[numEstados] = idPagina;
//        idNextionTipo[numEstados] = tipoNextion;
//        idNextionPicBase[numEstados] = picBase;
//    }
    numEstados++;
    return numEstados;
}


/*
*  para poner un nuevo estado
*  si existe, verificar que no hay dos que sean output
*  los datos nextion solo se cogen del que define la salida
*/
//uint8_t estados::addEstadoConCampoNxt(BaseSequentialStream *tty, campoNextion *campoCN, uint8_t esOut, uint8_t *hayError)
//{
//    char buffer[80];
//    uint16_t idNombre = campoCN->diNombreNxt();
//    if (idNombre > 0)
//    {
//        // ahora busca el estado que tenga el nombre con ese id
//        uint16_t idEstado = idNom2idEstado[idNombre];
//        if (idEstado>0)
//        {
//            campoNxt[idEstado] = campoCN;
//            if (esOut)
//            {
//                if (definidoOut[idEstado]) //-1 OJO
//                {
//                    chsnprintf(buffer,sizeof(buffer),"Salida %s se define varias veces", nombres::nomConId(idNombre));
//                    if (tty!=NULL)
//                        chprintf(tty,buffer);
//                    else
//                        nextion::enviaLog(tty,buffer);
//                    *hayError = 1;
//                    return 0;
//                }
//                else
//                {
//                    definidoOut[idEstado-1] = esOut;
////                    if (idPagina>0)
////                    {
////                        idNextionPage[idEstado-1] = idPagina;
////                        idNextionTipo[idEstado-1] = tipoNextion;
////                        idNextionPicBase[idEstado-1] = picBase;
////                    }
//                }
//            }
//            return idEstado;
//        }
//    }
//    estado[numEstados] = 0;
////    idNom[numEstados] = idNombre;
//    idNom2idEstado[idNombre] = numEstados;
//    definidoOut[numEstados] = esOut;
//    campoNxt[numEstados] = campoCN;
////    if (idPagina>0)
////    {
////        idNextionPage[numEstados] = idPagina;
////        idNextionTipo[numEstados] = tipoNextion;
////        idNextionPicBase[numEstados] = picBase;
////    }
//    numEstados++;
//    return numEstados;
//}




//uint8_t estados::addEstado(BaseSequentialStream *tty, char *nombre, uint8_t esOut, uint8_t *hayError)
//{
//    campoNextion *campoCN = campoNextion::addCampoNextion(nombre, TIPONEXTIONALL, hayError);
//    return  estados::addEstadoConCampoNxt(tty, campoCN, esOut, hayError);
//}

// indice 1..numEstados
uint8_t estados::diEstado(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //chprintf(tty,"Numero de estado ilegal %d en diEstado\n\r",numEstado);
        return 0;
    }
    chMtxLock(&MtxEstado[numEstado - 1]);
    uint8_t est = estado[numEstado - 1];
    chMtxUnlock(&MtxEstado[numEstado - 1]);
    return est;
}

uint8_t estados::estaDefinido(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //chprintf(tty,"Numero de estado ilegal %d en diEstado\n\r",numEstado);
        return 0;
    }
    definidoOut[numEstado-1] = 1;
    return 1;
}

uint8_t estados::diNextionPage(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        return 0;
    }
    return idPage[numEstado - 1];
}


// indice 1..numEstados
void estados::ponEstado(uint16_t numEstado, uint8_t valor)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
        return;
    if (estado[numEstado - 1] != valor)
    {
        chMtxLock(&MtxEstado[numEstado - 1]);
        estado[numEstado - 1] = valor;
        chMtxUnlock(&MtxEstado[numEstado - 1]);
        hayCambios = 1;
        estados::actualizaNextion(numEstado);
        if (idNombreWWW[numEstado-1]>0)
            cambiosEstadoWWW(numEstado);

    }
}

uint8_t estados::estadosInitOk(BaseSequentialStream *tty)
{
    char buffer[80];
    uint8_t esOk = 1;
    for (uint16_t est = 1; est <= estados::numEstados; est++)
    {
        if (!estados::definidoOut[est-1])
        {
            chsnprintf(buffer,sizeof(buffer),"Estado %s (#%d) sin definir", estados::nombre(est),est);
            nextion::enviaLog(tty,buffer);
            esOk = 0;
        }
    }
    return esOk;
}


uint16_t estados::diIdNombre(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //chprintf(tty,"Numero de estado ilegal %d en diEstado\n\r",numEstado);
        return 0;
    }
    return idNombre[numEstado - 1];
}

uint16_t estados::diIdNombreWWW(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //chprintf(tty,"Numero de estado ilegal %d en diEstado\n\r",numEstado);
        return 0;
    }
    return idNombreWWW[numEstado - 1];
}

void estados::ponNombreWWW(uint16_t numEstado, uint16_t idNombWWW)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
        return;
    idNombreWWW[numEstado-1] = idNombWWW;
}

const char *estados::nombre(uint16_t numEstado)
{
    if (numEstado == 0 || numEstado > estados::numEstados)
    {
        //chprintf(tty,"Numero estado %d ilegal en nombre\n\r",numEstado);
        return "-";
    }
    //return nombres::nomConId(idNom[numEstado - 1]);
    return nombres::nomConId(idNombre[numEstado - 1]);
};

void estados::printCabecera(BaseSequentialStream *tty)
{
    if (tty==NULL)
        return;
    chprintf(tty,"          ");
    for (uint16_t est = 1; est <= estados::numEstados; est++)
    {
        //uint16_t id = estados::idNom[est];
        chprintf(tty," %10s", estados::nombre(est));
    }
    chprintf(tty,"\n\r");
}

//  estados::print(ds,hora,min,seg,ds);
void estados::print(BaseSequentialStream *tty,uint8_t hora, uint8_t min, uint8_t seg, uint8_t )
{
    if (tty==NULL)
        return;
    chprintf(tty,"%2d:%02d:%02d", hora, min, seg);
    for (uint16_t est = 1; est <= estados::numEstados; est++)
        chprintf(tty," %10d", diEstado(est));
    chprintf(tty,"\n\r");
}

void estados::printAll(BaseSequentialStream *tty)
{
    uint8_t numEnLinea = 0;
    if (tty==NULL)
        return;
    for (uint16_t est = 1; est <= estados::numEstados; est++)
    {
        chprintf(tty,"%d %s:%2d    ", est, nombre(est), diEstado(est));
        if (++numEnLinea==3)
        {
            numEnLinea=0;
            chprintf(tty,"\r\n");
        }
    }
    chprintf(tty,"\r\n");
}

void estados::printSize(BaseSequentialStream *tty)
{
    if (tty!=NULL)
        chprintf(tty,"Hay %d estados\r\n",estados::numEstados);
}


void estados::actualizaNextion(uint16_t numEstado)
{
    chMtxLock(&MtxEstado[numEstado - 1]);
    uint8_t est = estado[numEstado - 1];
    chMtxUnlock(&MtxEstado[numEstado - 1]);
    if (estadoOld[numEstado-1] != estado[numEstado-1] && idPage[numEstado-1]>0)
    {
        enviaValPic(idPage[numEstado-1],idNombre[numEstado - 1], tipoNextion[numEstado - 1],picBase[numEstado - 1], est);
        estadoOld[numEstado-1] = est;
    }
}

// Llamada en Init: vuelca a JSON
void estados::initWWW(BaseSequentialStream *SDPort, uint16_t numEstado, uint8_t *hayDatosVolcados)
{
    /*
    { tipo: 'DOT', id: 40, nombre: 'Farola', estado: 1 }
     */
    if (numEstado == 0 || numEstado > estados::numEstados)
        return;
    uint16_t idWWW = idNombreWWW[numEstado-1];
    if (idWWW!=0)
    {
        if (*hayDatosVolcados)
            chprintf(SDPort,",");
        chprintf(SDPort, "{\"tipo\":\"DOT\",\"id\":\"%s\",\"nombre\":\"%s\",\"salida\":%d}",
                   estados::nombre(numEstado),nombres::nomConId(idWWW),diEstado(numEstado));
        *hayDatosVolcados = 1;
    }
}

void estados::cambiosEstadoWWW(uint16_t numEstado)
{
    struct msgTxWWW_t message;
    message.idNombVariable = diIdNombre(numEstado);
    message.accion = TXWWWESTADOVAR;
    chsnprintf(message.valor, sizeof(message.valor), "%d",diEstado(numEstado));
    putQueu(&colaMsgTxWWW, &message);
}
