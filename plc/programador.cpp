#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "bloques.h"
#include "dispositivos.h"
#include "string.h"
#include <stdint.h>
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "nextion.h"
#include "sms.h"
#include "colas.h"
#include <ctype.h>

void enviaTxt(uint16_t idPage, uint16_t idVar, const char *valor);
void enviaPic(uint16_t idPage, uint16_t idVar, uint16_t valor);

extern struct queu_t colaMsgTxWWW;
extern event_source_t enviarWWWsource;

programador *programadores[MAXPROGRAMADORES];
uint8_t programador::numProgramadores=0;

/*
 * Opciones:
 * - PROGRAMADOR picEstatus         estadoSINOActivo  salidaBombaEstado      suspenderEstado   msgTxt          [MedlitRiego Medcaudal riego lxmin]
     PROGRAMADOR riego.base.picc.15 rOO               bombaOnR.riego.pic.0   pararRiego        logR.riego.txt

   Máquina de estados:
   - numProgOn indica si esta el riego enchufado o no
   - Estado
      0: no hay riego activo
      1..numZonas: regando esa zona


      Antes:
          PROGRAMADOR riegoProg.base.pic.24"Riego" riego.base.picc.15 bombaOnR.riego.pic.0 pararRiego logR.riego.txt
      Ahora:
      PROGRAMADOR riegoProg.base.pic.24"Riego" riego.base.picc.15 bombaOnR.riego.pic.0 pararRiego logR.riego.txt

 */
programador::programador(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
{
    uint8_t tipoNextion, picBase;
    if (numPar!=6 && numPar!=10)
    {
        nextion::enviaLog(tty,"#param PROGRAMADOR");
        *hayError = 1;
        return; // error
    }
    if (numProgramadores>=MAXPROGRAMADORES)
    {
        nextion::enviaLog(tty,"Error: Demasiados programadores");
        *hayError = 1;
        return;
    }
    // nextionStatus
    uint8_t error = hallaNombreyDatosNextion(pars[1], TIPONEXTIONSILENCIO, &idNombreSt, &idNombreStWWW, &idPageSt, &tipoNextionSt, &picBaseSt);
    if (error)
        *hayError = 1;
    numNombre = idNombreSt;
    if (numNombre==0)
        return;
    // selector
    numProgOn = estados::addEstado(tty, pars[2], 0, hayError);
    activoOld = 0; // estados::diEstado(numProgOn);
    // bombaOut
    if (strlen(pars[3])>0)
        numOutBomba = estados::addEstado(tty, pars[3],1,hayError);
    else
        numOutBomba = 0;
    // suspender bombeo
    if (strlen(pars[4])>0)
        numSuspendeConteo = estados::addEstado(tty, pars[4],0, hayError);
    else
        numSuspendeConteo = 0;
    // datos del nextionTxt
    if (numPar>=6 && strlen(pars[5])>0)
    {
        hallaNombreyDatosNextion(pars[5], TIPONEXTIONSILENCIO, &idNombreTxt, &idNombreTxtWWW, &idPageTxt, &tipoNextion, &picBase);
        if (tipoNextion!=1)
        {
            nextion::enviaLog(tty,"prog. pageTxt tipo no txt");
            *hayError = 1;
        }
    }
    else
        idPageTxt = 0;
    medLitros = NULL;
    medCaudal = NULL;
    if (numPar==10)
    {
        medLitros = medida::findMedida(pars[6]);
        medCaudal = medida::findMedida(pars[7]);
        if (medLitros==NULL || medCaudal==NULL)
        {
            nextion::enviaLog(tty,"Medidas caudal no encontradas en programador");
            *hayError = 1;
        }
        idPageZonas = nombres::incorpora(pars[8]);
        idNombFlujos = nombres::incorpora(pars[9]);
    }
    programadores[numProgramadores++] = this;
    numZonas = 0;
    for (uint8_t z=0;z<MAXZONAS;z++)
        zonas[z] = 0;
    numStarts = 0;
    for (uint8_t s=0;s<MAXSTARTS;s++)
        starts[s] = 0;
};

programador::~programador()
{
    programador::numProgramadores = 0;
    for (uint8_t z=0;z<MAXZONAS;z++)
        zonas[z] = 0;
    for (uint8_t s=0;s<MAXSTARTS;s++)
        starts[s] = 0;
    numProgOn = 0;
    estado = 0;
    picBase = 0;
    numNombre = 0;
    cicloArrancado = 0;
    soloUnCiclo = 0;
    numOutBomba = 0;
    numSuspendeConteo = 0;
    dsQueFaltan = 0;
    idPageTxt = 0;
    idNombreTxt = 0;
}

const char *programador::diTipo(void)
{
    return "programador";
}

const char *programador::diNombre(void)
{
    return nombres::nomConId(numNombre);
}

int8_t programador::init(void)
{
    estado = 0;
    if (numOutBomba>0)
        estados::ponEstado(numOutBomba, 0);
    soloUnCiclo = 0;
    programador::actualizaZonas();
    dsQueFaltan = 0;
    estadoOld = 99;
    cambiadoStarts = 99;
    estadoSuspendeOld = 99;
    cambiadoDs = 1;
    activoOld = 99;
    programador::actualizaNextion(0);
    return 0;
}


void programador::arranca(uint8_t esB)
{
    if (numZonas==0)
        return;
    estado = 1;
    // busca zona con algo de tiempo
    do // busca zonas con tiempo>0
    {
        if (zonas[estado-1]->diTiempo(esB)>0)
        {
            dsQueFaltan = 600*zonas[estado-1]->diTiempo(esB);
            zonas[estado-1]->ponSalida(1);
            programador::actualizaZonas();
            soloUnCiclo = 0;
            cicloArrancado = esB;
            if (medLitros!=NULL)
            {
                contLitrosInicioZona = medLitros->diValor();
                segZonaActiva = zonas[0]->diTiempo(esB);
            }
            if (numOutBomba>0)
            {
                estados::ponEstado(numOutBomba, 1);
                cambiaBombaWWW();
            }
            programador::actualizaNextion(0);
            break;
        }
    } while (++estado<=numZonas);
    if (estado>numZonas) // no hay mas zonas
    {
        // hemos cubierto todas las zonas sin encontrar nada
        estado = 0;
        if (numOutBomba>0)
        {
            estados::ponEstado(numOutBomba, 0);
            cambiaBombaWWW();
        }
    }
}

// zona 1..numZonas
void programador::arrancaZona(uint8_t numZona, uint8_t esB)
{
    if (numZona==0 || numZona>numZonas)
        return;
    estado = numZona;
    programador::actualizaZonas();
    cicloArrancado = esB;
    soloUnCiclo = 1;
    contLitrosInicioZona = medLitros->diValor();
    dsQueFaltan = 600*zonas[numZona-1]->diTiempo(esB);
    segZonaActiva = 60*zonas[numZona-1]->diTiempo(esB);
    if (numOutBomba>0 && !estados::diEstado(numOutBomba))
    {
        estados::ponEstado(numOutBomba, 1);
        cambiaBombaWWW();
    }
    programador::actualizaNextion(0);
    return;
}

void programador::actualizaZonas(void)
{
    uint8_t estaSuspendido = 0;
    if (numSuspendeConteo>0 && estados::diEstado(numSuspendeConteo))
        estaSuspendido = 1;
    for (uint8_t zona=1;zona<=numZonas;zona++)
        zonas[zona-1]->ponSalida(estado==zona && !estaSuspendido);
}

void programador::stop(void)
{
    if (estado>0)
    {
        estado = 0;
        if (numOutBomba>0)
        {
            estados::ponEstado(numOutBomba, 0);
            cambiaBombaWWW();
        }
        programador::actualizaZonas();
        programador::actualizaNextion(0);
    }
}

void programador::asignaZona(zona *zon)
{
    if (numZonas>=MAXZONAS)
        return;
    zonas[numZonas] = zon;
    numZonas++;
}

///*
// *  PROGFLUJO riego medLitros medCaudal
// */
//void programador::asignaFlujo(BaseSequentialStream *tty, uint8_t numPar, char *pars[], uint8_t *hayError)
//{
//    if (numPar!=4)
//    {
//        nextion::enviaLog(tty,"#parametros PROGFLUJO");
//        *hayError = 1;
//        return; // error
//    }
//    for (uint8_t numProg=0;numProg<programador::numProgramadores;numProg++)
//    {
//        if (!strcmp(programadores[numProg]->diNombre(),pars[1]))
//        {
//            programador *program = programadores[numProg];
//            medida *medLitrosNew = medida::findMedida(pars[2]);
//            if (medLitrosNew==NULL)
//            {
//                nextion::enviaLog(tty,"med. litros no encontrado en PROGFLUJO");
//                *hayError = 1;
//                return; // error
//            }
//            medida *medCaudalNew = medida::findMedida(pars[3]);
//            if (medCaudalNew==NULL)
//            {
//                nextion::enviaLog(tty,"med. caudal no encontrado en PROGFLUJO");
//                *hayError = 1;
//                return; // error
//            }
//            program->medLitros = medLitrosNew;
//            program->medCaudal = medCaudalNew;
//            return;
//        }
//    }
//    nextion::enviaLog(tty,"programador en PROGFLUJO desconocido");
//    *hayError = 1;
//};



uint8_t programador::asignaStart(BaseSequentialStream *tty, start *strt)
{
    if (numStarts>=MAXSTARTS)
    {
        nextion::enviaLog(tty,"demasiados Start");
        return 1;
    }
    starts[numStarts] = strt;
    numStarts++;
    return 0;
}

uint8_t programador::programaActivado(void)
{
    return estados::diEstado(numProgOn);
}

uint8_t programador::calcula(uint8_t , uint8_t , uint8_t , uint8_t ) // devuelve 1 si ha cambiado
{
    if (estado==0)
        return 0;
    if (numSuspendeConteo>0 && estados::diEstado(numSuspendeConteo)==1) // esta paralizado el conteo?
        zonas[estado-1]->ponSalida(0); // desactiva zona en marcha
    else
        zonas[estado-1]->ponSalida(1); // pon zona en marcha
    return 0;
}

void programador::addTime(uint16_t dsInc, uint8_t , uint8_t , uint8_t seg, uint8_t ds)
{
    if (estado==0) // toca actualizar temporizador cada minuto
    {
        if (estadoOld != estado || cambiadoDs || (numSuspendeConteo>0 && estadoSuspendeOld != estados::diEstado(numSuspendeConteo))
                || activoOld != estados::diEstado(numProgOn) || cambiadoStarts
                || (seg==0 && ds==0))
            programador::actualizaNextion(0);
        return;
    }
    if (numSuspendeConteo==0 || estados::diEstado(numSuspendeConteo)==0) // no esta paralizado el conteo?
    {
        if (dsInc <= dsQueFaltan)
            dsQueFaltan -= dsInc;
        else
            dsQueFaltan = 0;
        if (dsQueFaltan%10 == 0)
            cambiadoDs = 1;
    }
    else
        if (zonas[estado-1]->diSalida()==1)
        {
            zonas[estado-1]->ponSalida(0); // desactiva zona en marcha
        }
    if (dsQueFaltan==0) // hemos alcanzado el contador, pasa a siguiente zona
    {
        zonas[estado-1]->ponSalida(0); // desactiva zona en marcha
        // calcula flujo, si es que procede
        if (medLitros!=NULL && segZonaActiva>10)
        {
            // envia datos de flujo a la zona correspondiente
            float litros = medLitros->diValor() - contLitrosInicioZona;
            float flujoReal = 3600.0*litros/((float) segZonaActiva);
            uint8_t excedeMax = zonas[estado-1]->setFlujoLeido(flujoReal);
            if (excedeMax)
            {
                char buffer[60];
                snprintf(buffer,sizeof(buffer),"Flujo elevado en %s, la bloqueo",zonas[estado-1]->diNombre());
                nextion::enviaLog(NULL,buffer);
            }
            actualizaFlujoZonaEnNextion(estado);
        }
        if (estado>=numZonas || soloUnCiclo) // hemos cubierto todas las zonas
        {
            estado = 0;
            if (numOutBomba>0)
            {
                estados::ponEstado(numOutBomba, 0);
                cambiaBombaWWW();
            }
        }
        else
        {
            while (++estado<=numZonas) // busca zonas con tiempo>0
            {
                if (zonas[estado-1]->diTiempo(cicloArrancado)>0)
                {
                    dsQueFaltan = 600*zonas[estado-1]->diTiempo(cicloArrancado);
                    zonas[estado-1]->ponSalida(1);
                    break;
                }
            }
            if (estado>numZonas) // no hay mas zonas
            {
                // hemos cubierto todas las zonas
                estado = 0;
                if (numOutBomba>0)
                {
                    estados::ponEstado(numOutBomba, 0);
                    cambiaBombaWWW();
                }
            }
        }
    }
    programador::actualizaNextion(0);
}

void programador::trataOrdenNextion(char *vars[], uint16_t numPars)
{
    // Orden desde Nextion: @orden,riego,start,a-b o bien @orden,riego,zona,1-n,a-b o @orden,riego,stop
    if (numPars>=1 && !strcasecmp(vars[0],"start"))
    {
        if (numPars==1 || !strcasecmp(vars[1],"a"))
            programador::arranca(0);
        else if (!strcasecmp(vars[1],"b"))
            programador::arranca(1);
    }
    if (!strcasecmp(vars[0],"stop") && numPars==1)
    {
        programador::stop();
    }
    if (!strcasecmp(vars[0],"zona") && numPars>=2)
    { // zona 0 [a] (arrancar zona)
        if (!strcasecmp(vars[2],"a"))
            programador::arrancaZona(atoi(vars[1]),0);
        else if (!strcasecmp(vars[2],"b"))
            programador::arrancaZona(atoi(vars[1]),1);
        cambiosEstadoWWW();
    }
    //@orden,riego,setMinZ0,a,15
    if (!strncasecmp(vars[0],"setMinZ",7) && strlen(vars[0])>=8 && numPars==3)
    {
        uint8_t numZona = atoi(&vars[0][7]);
        if ((numZona+1)<=numZonas && zonas[numZona]!=NULL)
        {
            zonas[numZona]->leeMinutos(vars[2], vars[1]); // se actualiza automaticamente en nextion
            cambiosTiemposWWW(numZona,toupper(vars[1][0]),atoi(vars[2]));
        }
    }
    //@orden,riegoProg,nohayfugas
    if (!strncasecmp(vars[0],"nohayfugas",11) && strlen(vars[0])==12 && numPars==2)
    {
        for (uint8_t numZ=1;numZ<numZonas;numZ++)
            zonas[numZ-1]->reconoceFlujoMax();
        // ToDo libera en nextion
    }
    //@orden,riego,setH0,13
    if (!strncasecmp(vars[0],"setH",4) && strlen(vars[0])>=5 && numPars==2)
    {
        uint8_t numStart = atoi(&vars[0][4]);
        if ((numStart+1)<=numStarts && starts[numStart]!=NULL)
        {
            starts[numStart]->leeH(vars[1]);
            cambiosHStartWWW(numStart,atoi(vars[1]));
            cambiadoStarts = 1;
        }
    }
    //@orden,riego,setM0,58
    if (!strncasecmp(vars[0],"setM",4) && strlen(vars[0])>=5 && numPars==2)
    {
        uint8_t numStart = atoi(&vars[0][4]);
        if ((numStart+1)<=numStarts && starts[numStart]!=NULL)
        {
            starts[numStart]->leeM(vars[1]);
            cambiosMStartWWW(numStart,atoi(vars[1]));
            cambiadoStarts = 1;
        }
    }
    //@orden,riego,setDOWX,LMD
    if (!strncasecmp(vars[0],"setDOW",6) && strlen(vars[0])==7 && numPars>=1)
    {
        uint8_t numStart = vars[0][6]-'0';
        if (vars[0][6]>='0' && (numStart+1)<=numStarts && starts[numStart]!=NULL)
        {
            if (numPars==2)
                starts[numStart]->leeDOW(vars[1]);
            else if (numPars==1)
                starts[numStart]->leeDOW("");
            cambiadoStarts = 1;
            cambiosDowWWW(numStart, vars[1]);
        }
    }
    //@orden,riego,setEsp0,1
    if (!strncasecmp(vars[0],"setESP",6) && strlen(vars[0])==7 && numPars==2)
    {
        uint8_t numStart = vars[0][6]-'0';
        if (vars[0][6]>='0' && (numStart+1)<=numStarts && starts[numStart]!=NULL)
        {
            starts[numStart]->leeESP(vars[1]);
            cambiadoStarts = 1;
            cambiosESBStartWWW(numStart,atoi(vars[1]));
        }
    }
    //@orden,riego,toggle (conecta/desconecta)
    if (!strncasecmp(vars[0],"toggle",6) && numPars==1)
    {
        if (estados::diEstado(numProgOn)==0)
        {
            estados::ponEstado(numProgOn,1);
        }
        else
        {
            estados::ponEstado(numProgOn,0);
            programador::stop();
        }
    }
    programador::actualizaNextion(0);
}

void programador::actualizaFlujoZonaEnNextion(uint8_t numZona)
{
    char varName[20];
    if (idPageZonas==0 || medLitros==NULL)
        return;
    // void enviaTxt(uint16_t idPage, const char *varName, uint16_t valor)
    // formato = lxh0..z-1
    chsnprintf(varName,sizeof(varName),"%s%d",nombres::nomConId(idNombFlujos),numZona-1);
    enviaTxt(idPageZonas,varName,(uint16_t) zonas[numZona-1]->diUltimaMedidaFlujo());
    if (zonas[numZona-1]->flujoExcedido())
        ponColorEnTexto(idPageZonas,varName,63488); // rojo
}

void programador::actualizaNextion(uint8_t idPagNextion)
{
    char buffer[100];
    uint8_t picModo;
    // hay que actualizar?
    if (idPageTxt && (idPagNextion==0 || idPageTxt==idPagNextion)
        && (activoOld!=estados::diEstado(numProgOn) || estado!=estadoOld || cambiadoDs ||
       (numSuspendeConteo!=0 && estados::diEstado(numSuspendeConteo)!=estadoSuspendeOld)))
    {
        printStatus(buffer, sizeof(buffer));
        enviaTxt(idPageTxt, idNombreTxt, buffer);
    }
    if (numSuspendeConteo!=0 && estados::diEstado(numSuspendeConteo)!=estadoSuspendeOld)
    {
        cambiaEsperaWWW();
    }
    if (idPageSt && (estado!=estadoOld || (numSuspendeConteo!=0 && estados::diEstado(numSuspendeConteo)!=estadoSuspendeOld)
            || activoOld!=estados::diEstado(numProgOn)))
    {
        // *   donde: riego.riegoOn debe ser 4 pic (inactivo, activo, suspendido, regando), parecido a sino
        picModo = 0;
        if (estado==0)
        {
            if (estados::diEstado(numProgOn)==1)
                picModo = 1;
        }
        else
        {
            if (numSuspendeConteo && estados::diEstado(numSuspendeConteo)==1)
                picModo = 2;
            else
                picModo = 3;
        }
        enviaValPic(idPageSt,idNombreSt, tipoNextionSt, picBaseSt,picModo);
    }
    chThdSleepMilliseconds(100);
    if (estado!=estadoOld)
    {
        for (uint8_t z=1;z<=numZonas;z++)
            zonas[z-1]->actualizaNextion(0);
        cambiosEstadoWWW();
    }
    if (cambiadoStarts)
    {
        for (uint8_t s=1;s<=numStarts;s++)
            starts[s-1]->actualizaNextion(0);
    }
    activoOld = estados::diEstado(numProgOn);
//    estadoSuspendeOld = estados::diEstado(numSuspendeConteo);
    estadoOld =estado;
    cambiadoDs = 0;
    if (numSuspendeConteo!=0)
        estadoSuspendeOld = estados::diEstado(numSuspendeConteo);
    cambiadoStarts = 0;
}

/*
 * - numProgOn indica si esta el riego enchufado o no
   - Estado
      0: no hay riego activo
      1..numZonas: regando esa zona
 */
void programador::printStatus(char *buffer, uint16_t longBuffer)
{
    const char *estadoProg[2]={"Programa parado","Programa activo"};
    // pon primero si esta conectado el riego
    chsnprintf(buffer,longBuffer,"%s",estadoProg[estados::diEstado(numProgOn)]);
    // y ahora si esta regando
    if (estado>0)
    {
        uint16_t posBuff = strlen(buffer);
        if (estados::diEstado(numSuspendeConteo)==0)
            chsnprintf(&buffer[posBuff],longBuffer-posBuff,", en %s, falta %d s",zonas[estado-1]->diNombre(),dsQueFaltan/10);
        else
            chsnprintf(&buffer[posBuff],longBuffer-posBuff,", en %s (suspendido), falta %d s",zonas[estado-1]->diNombre(),dsQueFaltan/10);
    }
}

void programador::print(BaseSequentialStream *tty)
{
    char buffer[80];
    if (numSuspendeConteo==0)
        chsnprintf(buffer,sizeof(buffer),"PROGRAMADOR %s bomba:%s-%d",nombres::nomConId(numNombre),estados::nombre(numOutBomba),numOutBomba);
    else
        chsnprintf(buffer,sizeof(buffer),"PROGRAMADOR %s bomba:%s-%d suspendeConteo:%s-%d",nombres::nomConId(numNombre),estados::nombre(numOutBomba),numOutBomba,estados::nombre(numSuspendeConteo),numSuspendeConteo);
    if (tty!=NULL)
        nextion::enviaLog(tty,buffer);
}

void programador::statusSMS(sms *smsPtr)
{
    char buffer[100];
    printStatus(buffer, sizeof(buffer));
    smsPtr->addMsgRespuesta(buffer);
}

void programador::initWWW(BaseSequentialStream *SDPort, uint8_t *hayDatosVolcados)
{
  /*
   {"tipo": 'PROG',"id":'riego',"nombre":'Riego',"estado": 0,"bomba": 0,"espera": 0,
   "zonas":[{"nombre":'Arizonicas', "Ta":5, "Tb":1},{"nombre":'Chopos', "Ta":10, "Tb":0}]}
   */
    if (idNombreStWWW==0)
        return;
    // si entras, es que voy a volcar
    if (*hayDatosVolcados)
        chprintf(SDPort,",");
    chprintf(SDPort, "{\"tipo\":\"PROG\",\"id\":\"%s\",\"nombre\":\"%s\",\"estado\":%d",
               nombres::nomConId(numNombre),nombres::nomConId(idNombreStWWW),estado);
    // pongo bomba y suspende, si existen
    if (numOutBomba>0)
        chprintf(SDPort, ",\"bomba\":%d",estados::diEstado(numOutBomba));
    if (numSuspendeConteo>0)
        chprintf(SDPort, ",\"espera\":%d",estados::diEstado(numSuspendeConteo));
    // sigo con las zonas
    if (numZonas>0)
    {
        chprintf(SDPort, ",\"zonas\":[");
        // "zonas":[{"nombre":'Arizonicas', "Ta":5, "Tb":1},
        //          {"nombre":'Cesped chopos', "Ta":10, "Tb":0}}
        uint8_t primeraZona=1;
        for (uint8_t z=1;z<=numZonas;z++)
        {
            if (!primeraZona)
                chprintf(SDPort,",");
            chprintf(SDPort,"{\"nombre\":\"%s\",\"Ta\":%d,\"Tb\":%d}",
                       zonas[z-1]->diNombre(),zonas[z-1]->diTiempo(0),zonas[z-1]->diTiempo(1));
            primeraZona = 0;
        }
        chprintf(SDPort,"]");
    }
    if (numStarts>0)
    {
        chprintf(SDPort, ",\"starts\":[");
//        // "starts": [
//        // {"DOW":"LXV", "Hora":6, "Min":30, "esB":0},
//        // {"DOW":"MJS", "Hora":19, "Min":0, "esB":0}]
//
        uint8_t primerStart=1;
        for (uint8_t s=1;s<=numStarts;s++)
        {
            if (!primerStart)
                chprintf(SDPort,",");
            starts[s-1]->envia2plc(SDPort);
            primerStart = 0;
        }
        chprintf(SDPort,"]");
    }
    chprintf(SDPort,"}");
    *hayDatosVolcados = 1;
}

//{"id":"riego","accion":"zona","zona":2},
//{"id":"riego","accion":"setMinZ","tipo":"tRiego","zona":1,"codAB":"A","tiempo":"110"},
//{"id":"riego","accion":"bomba","salida":1},
//{"id":"riego","accion":"espera","salida":1}
//{"id":"riego","accion":"flujoMax","zona":"1","flujo":"250"}

void programador::cambiaBombaWWW(void)
{
    //"id":"riego","accion":"bomba","salida":1}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWPROGBOMBA;
    chsnprintf(message.valor, sizeof(message.valor), "%d",estados::diEstado(numOutBomba));
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiaEsperaWWW(void)
{
    //"id":"riego","accion":"espera","salida":1}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWPROGESPERA;
    chsnprintf(message.valor, sizeof(message.valor), "%d",estados::diEstado(numSuspendeConteo));
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosEstadoWWW(void)
{
    //{"tipo":"PROG","accion":"zona","id":"riego","zona":3}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWZONAACTIVA;
    chsnprintf(message.valor, sizeof(message.valor), "%d",estado);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosTiemposWWW(uint8_t zona, char codAB, uint8_t tiempo)
{
    //{"tipo":"PROG","accion":"setMinZ","id":"riego","zona":1,"codAB":'A',"tiempo":6}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWTZONA;
    //"ZZ,A,TTTT"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%c,%d",zona+1,codAB,tiempo);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosFlujomaxWWW(uint8_t zona, uint16_t flujo)
{
    //{"id":"riego","accion":"flujoMax","zona":"1","flujo":"250"}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWTFLUJOMAX;
    //"ZZ,TTTT"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%d",zona+1,flujo);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosDowWWW(uint8_t numArranque, char *dowStr)
{
    //{"tipo":"PROG","accion":"setDOW","id":"riego","numArranque":1,"DOW":"LXV"}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWDOW;
    //"numArr,DOW"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%s",numArranque,dowStr);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosHStartWWW(uint8_t numArranque, uint8_t hora)
{
    //{"tipo":"PROG","id":"riegoProg","accion":"setH0","tiempo":"9"}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWHSTART;
    //"numArr,hora"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%d",numArranque,hora);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosMStartWWW(uint8_t numArranque, uint8_t min)
{
    //{"tipo":"PROG","id":"riegoProg","accion":"setM0","tiempo":"30"}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWMSTART;
    //"numArr,min"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%d",numArranque,min);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}

void programador::cambiosESBStartWWW(uint8_t numArranque, uint8_t esB)
{
    //{"tipo":"PROG","id":"riegoProg","accion":"setEsp0","esB":1}
    struct msgTxWWW_t message;
    if (!idNombreStWWW)
        return;
    message.idNombVariable = idNombreSt;
    message.accion = TXWWWESBSTART;
    //"numArr,esB"
    chsnprintf(message.valor, sizeof(message.valor), "%d,%d",numArranque,esB);
    putQueu(&colaMsgTxWWW, &message);
    chEvtBroadcast(&enviarWWWsource);
}
