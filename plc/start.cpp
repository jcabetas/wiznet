#include "bloques.h"
#include "string.h"
#include <stdint.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "nextion.h"
#include "calendar.h"

/*
class start: public bloque {
    protected:
        programador *program;
        uint16_t numInput;
        uint8_t esB;
        uint8_t DOW;
        uint8_t hora;
        uint8_t min;
    public:
        start(uint8_t numPar, char *pars[]);  // lee desde string
        const char *diTipo(void);
        const char *diNombre(void);
        int8_t init(void);
        int8_t calcula(void);
        void   print(void);
        int8_t addTime(uint16_t ms);
};
 */

extern estados est;
extern programador *programadores[MAXPROGRAMADORES];
extern mutex_t MtxNextionTx;

//void enviaVal(uint16_t idPage, uint16_t idVar, uint16_t valor);



/*
 * START riego manyanas [riego.hora0 8] [riego.min0 0] [riego.DOW LXJ] [ESP 0]
 */
start::start(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t numParDOW, dowValor;
    char *parDOW[10];
    if (numPar!=7)
    {
        nextion::enviaLog(tty, "#parametros START");
        *hayError = 1;
        return; // error
    }
    program=NULL;
    for (uint8_t numProg=0;numProg<programador::numProgramadores;numProg++)
    {
        if (!strcmp(programadores[numProg]->diNombre(),pars[1]))
        {
            program = programadores[numProg];
            if (program->asignaStart(tty, this))
                *hayError = 1;
            break;
        }
    }
    if (program==NULL)
    {
        nextion::enviaLog(tty, "#prog. desconocido en START");
        *hayError = 1;
        return; // error
    }
    numNombre = nombres::incorpora(pars[2]);
    if (numNombre==0)
        return;
    hora = parametro::addParametroU16FlashMinMax(tty, pars[3],0,23,hayError);
    min = parametro::addParametroU16FlashMinMax(tty, pars[4],0,59,hayError);
    // decodifico [DOW LXD]
    if (divideBloque(tty, pars[5], &numParDOW, parDOW)==0 && numParDOW==2)
        dowValor = start::decodeDOW(parDOW[1]);
    else
        dowValor = 0x7F;
    DOW = new parametroU16Flash(parDOW[0], dowValor,0, 0x7F);
    esB = parametro::addParametroU16FlashMinMax(tty, pars[6],0,1,hayError);
}

start::~start()
{
}

const char *start::diTipo(void)
{
	return "start";
}

const char *start::diNombre(void)
{
	return nombres::nomConId(numNombre);
}

int8_t start::init(void)
{
    return 0;
}


// convierte un string del tipo "MJD" a un byte donde el bit 6 es L y el 0 es D
uint8_t start::decodeDOW(const char *DOW)
{
    char dias[8]="DSVJXML";
    uint8_t dow = 0;
    for (uint8_t i=0;i<strlen(DOW);i++)
    {
        char dia=DOW[i];
        for (uint8_t diaSem=0;diaSem<strlen(dias);diaSem++)
        {
            if (dia==dias[diaSem])
            {
                dow += (1<<diaSem);
                break;
            }
        }
    }
    return dow;
}

// convierte un byte (donde el bit 6 es L y el 0 es D) a un string del tipo "MJD"
void start::encodeDOW(uint8_t dowByte, char *DOW, uint8_t sizeDOW)
{
    char dias[8]="DSVJXML";
    uint8_t pos=0;
    if (sizeDOW<8) return;
    DOW[pos] = 0;
    for (int8_t i=6;i>=0;i--)
    {
        if (dowByte & (1<<i))
            DOW[pos++] = dias[i];
    }
    DOW[pos] = 0;
    return;
}


void start::actualizaNextion(uint8_t idPage)
{
    char dias[8]="DSVJXML";
    // START riego manyanas [arranque.h0 8] [arranque.m0 0] [arranque.DOW0 LMXJVSD] [arranque.esp0]
    /*
     *     parametroU16 *esB;
    parametroU16 *DOW;
    parametroU16 *hora;
    parametroU16 *min;
     */
    if (DOW->idNextionPage>0 && (idPage==0 || DOW->idNextionPage==idPage))
    {
        uint8_t dow = DOW->valor(); // bit 6 es L y el 0 es D
        chMtxLock(&MtxNextionTx);
        for (uint8_t i=0;i<7;i++)
        {
            uint8_t valor = (dow>>i)&1;
            chprintf((BaseSequentialStream *)&SD1,"%s.%s%c.val=%d%c%c%c",nombres::nomConId(DOW->idNextionPage),nombres::nomConId(DOW->idNextionVar),dias[i],valor,0xFF,0xFF,0xFF);
        }
        chMtxUnlock(&MtxNextionTx);
    }
    if (esB->idNextionPage>0 && (idPage==0 || esB->idNextionPage==idPage))
    {
       // enviaValPic(esB->idNextionPage, esB->idNextionVar, esB->tipoNextion, esB->picBase, esB->valor());
        chMtxLock(&MtxNextionTx);
        chprintf((BaseSequentialStream *)&SD1,"%s.%s.val=%d%c%c%c",nombres::nomConId(esB->idNextionPage),nombres::nomConId(esB->idNextionVar),esB->valor(),0xFF,0xFF,0xFF);
        chMtxUnlock(&MtxNextionTx);
    }
    hora->enviaToNextion();
    min->enviaToNextion();
}


void start::leeDOW(const char *parDOW)
{
    DOW->set(start::decodeDOW(parDOW));
}

void start::leeESP(const char *esEsp)
{
    if (esB!=NULL)
        esB->set(esEsp);
}

void start::leeH(const char *hor)
{
    hora->set(hor);
    hora->enviaToNextion();
}

void start::leeM(const char *mn)
{
    min->set(mn);
    min->enviaToNextion();
}


void start::addTime(uint16_t , uint8_t horaP, uint8_t minP, uint8_t segP, uint8_t dsP)
{
    if (hora->valor()==horaP && min->valor()==minP && segP==0 && dsP==0)
    {
        if (program->programaActivado()==1)
        {
            uint8_t dowStart = DOW->valor(); // LMXJVSD
            uint8_t dow = calendar::getDOW();// {DLMXJVS} (si empezara en Lunes seria mas facil)
            uint8_t arranca = 0;
            if (dow==0)
            {
                if (dowStart & 1)
                    arranca = 1;
            }
            else
            {
                uint8_t dowMoved = (1<<(7-dow));
                if (dowMoved & dowStart)
                    arranca = 1;
            }
            if (arranca)
                program->arranca(esB->valor());
        }
    }
}

void start::envia2plc(BaseSequentialStream *SDPort)
{
    char bufferDOW[8];
    encodeDOW(DOW->valor(),bufferDOW,sizeof(bufferDOW));
//    chprintf(SDPort,"{\"DOW\":\"LXV\", \"Hora\":6, \"Min\":30, \"esB\":0}");
    chprintf(SDPort,"{\"DOW\":\"%s\",\"Hora\":%d,\"Min\":%d,\"esB\":%d}",
             bufferDOW, hora->valor(), min->valor(),esB->valor());
}


void start::print(BaseSequentialStream *tty)
{
    char buffer[80];
    chsnprintf(buffer,sizeof(buffer),"START (%s, %s, ciclo especial:%d, %d:%d)",program->diNombre(), nombres::nomConId(numNombre), esB->valor(), hora->valor(), min->valor());
    if (tty!=NULL)
        nextion::enviaLog(tty, buffer);
}
