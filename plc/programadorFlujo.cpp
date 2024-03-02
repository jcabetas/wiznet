#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"

#include "bloques.h"
#include "dispositivos.h"
#include "string.h"
#include "stdlib.h"
#include "nextion.h"


extern programador *programadores[MAXPROGRAMADORES];

/*
 *  PROGFLUJO riego medLitros medCaudal
 */
programador::progflujo(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    if (numPar!=4)
    {
        nextion::enviaLog(tty,"#parametros PROGFLUJO");
        *hayError = 1;
        return; // error
    }
    medida *medLitros = medida::findMedida(pars[2]);
    medida *medCaudal = medida::findMedida(pars[3]);
    for (uint8_t numProg=0;numProg<programador::numProgramadores;numProg++)
    {
        if (!strcmp(programadores[numProg]->diNombre(),pars[1]))
        {
            programador *program = programadores[numProg];
            program->asignaFlujo(medLitros, medCaudal);
            return;
        }
    }
    nextion::enviaLog(tty,"programador en PROGFLUJO desconocido");
    *hayError = 1;
};
