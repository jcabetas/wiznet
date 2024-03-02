#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "string.h"
#include "stdlib.h"
#include "nextion.h"


/*
protected:
    programador *program;
    parametroString *nombOut;
    int16_t numOut;
    uint8_t estadoOld;
    parametroU16 *minutosA;
    parametroU16 *minutosB;
    uint16_t idPageTxt, idNombre;
};
 */

extern estados est;
extern programador *programadores[MAXPROGRAMADORES];

void enviaVal(uint16_t idPage, uint16_t idVar, uint16_t valor);

/*
 *  ZONA riego  Alibustres zona0pic.riego.pic.0 [z0Min.riego 5] [z0MinB.riego 0]
 *       program nomZona    estadoSalida         param tiempo     param tiempoB   posEscribirCaudal
 *
 *  Antes:
 *      ZONA riegoProg Chopos-Norte zona0nam.riego.txt zona0pic.riego.pic.0 [z0Min.riego 5] [z0MinB.riego 0]
 *  Ahora:
 *      ZONA riegoProg Chopos-Norte zona0pic.riego.pic.0 [z0Min.riego 5] [z0MinB.riego 0]
 *
 */
zona::zona(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=6)
    {
        nextion::enviaLog(tty,"#parametros ZONA");
        *hayError = 1;
        return; // error
    }
    idNombre = nombres::incorpora(pars[2]);
    numOut = estados::addEstado(tty, pars[3], 1, hayError);
    minutosA = parametro::addParametroU16FlashMinMax(tty, pars[4],0,60,hayError);
    minutosB = parametro::addParametroU16FlashMinMax(tty, pars[5],0,60,hayError);
    flujoMax = 0.0f;
    ultimaMedidaFlujo = 0.0f;
    bloquearPorFlujoMax = false;
    for (uint8_t numProg=0;numProg<programador::numProgramadores;numProg++)
    {
        if (!strcmp(programadores[numProg]->diNombre(),pars[1]))
        {
            program = programadores[numProg];
            program->asignaZona(this);
            return;
        }
    }
    nextion::enviaLog(tty,"#programador desconocido");
    *hayError = 1;
};

zona::~zona()
{
}

void zona::actualizaNextion(uint8_t idPage)
{
    if (idPage==0 || idPage==estados::diNextionPage(numOut))
        estados::actualizaNextion(numOut);
    minutosA->enviaToNextion();
    minutosB->enviaToNextion();
}


void zona::reconoceFlujoMax(void)
{
    flujoMax = 1.2f*ultimaMedidaFlujo;
    bloquearPorFlujoMax = false;
}

// devuelve 1 si el flujo maximo es excedido
uint8_t zona::setFlujoLeido(float flujo)
{
    if (flujoMax<0.01f)
    {
        flujoMax = 1.2f*flujo;
        ultimaMedidaFlujo = flujo;
    }
    else if (flujo > flujoMax)
    {
        bloquearPorFlujoMax = true;
    }
    // envio flujo a Nextion
    return bloquearPorFlujoMax;;
}

bool zona::flujoExcedido(void)
{
    return bloquearPorFlujoMax;
}

float zona::diUltimaMedidaFlujo(void)
{
    return ultimaMedidaFlujo;
}

const char *zona::diTipo(void)
{
    return "zona";
}

const char *zona::diNombre(void)
{
    return nombres::nomConId(idNombre);
}

int8_t zona::init(void)
{
    estadoOld = 0;
    estados::ponEstado(numOut, 0);
    return 0;
}


uint16_t zona::diTiempo(uint8_t esB)
{
    if (!esB)
        return minutosA->valor();
    else
        return minutosB->valor();
}

void zona::ponSalida(uint8_t estado)
{
    estados::ponEstado(numOut, estado);
    if (estado!=estadoOld)
    {
        estadoOld = estado;
        zona::actualizaNextion(0);
    }
}

uint8_t zona::diSalida(void)
{
    return estados::diEstado(numOut);
}

void zona::leeMinutos(char *minutos, char *AoB)
{
    if (!strcasecmp(AoB,"a"))
    {
        minutosA->set(atoi(minutos));
        minutosA->enviaToNextion();
    }
    else if (!strcasecmp(AoB,"b"))
    {
        minutosB->set(atoi(minutos));
        minutosB->enviaToNextion();
    }
}

void zona::addTime(uint16_t , uint8_t , uint8_t , uint8_t , uint8_t )
{

}

//void zona::trataOrdenNextion(char *vars[], uint16_t numVars)
//{
//    // @orden,zona0,setMinA,34
//     if (numVars!=2)
//         return;
//     if (!strcasecmp(vars[0],"setMinA"))
//     {
//         minutosA->set(atoi(vars[1]));
//         minutosA->enviaToNextion();
//     }
//     else  if (!strcasecmp(vars[0],"setMinB"))
//     {
//         minutosB->set(atoi(vars[1]));
//         minutosB->enviaToNextion();
//     }
//     else  if (!strcasecmp(vars[0],"setDescripcion"))
//     {
//         descripcion->set(vars[1]);
//         descripcion->enviaToNextion();
//     }
//}

void zona::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"[%s-%d] = ZONA (programador %s, zona %s, %d min A, %d min B)",estados::nombre(numOut),numOut,program->diNombre(),descripcion->valor(), minutosA->valor(), minutosB->valor());
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}
