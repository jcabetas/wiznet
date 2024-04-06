/*
 * variables.cpp
 *
 *  Created on: 11 mar 2023
 *      Author: joaquin
 */

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include "w25q16.h"
#include "variables.h"
#include "string.h"
#include "chprintf.h"

uint16_t modoRadio;
uint16_t sOlvido;

uint16_t idLlamador;
uint16_t dsMaxEntreMsgsLlamador;
uint16_t dsMinEntreMsgsLlamador;

uint16_t bloqueoAbusones;
uint16_t avisaAbuso;
uint16_t tiempoAbuso;         // minutos
uint16_t dsMaxEntreMsgsPozo;
uint16_t dsMinEntreMsgsPozo;

uint16_t idVacon;


void initSpiPinsCPP(void);


extern "C" {
    void leeVariablesC(void);
}

/*
 * Si la EEPROM es valida, los dos primeros bytes seran 0x7851
 *
 */
BaseSequentialStream * ttyCOM = (BaseSequentialStream *)&SD1;


void imprimeVariables(void)
{
    chprintf(ttyCOM,"- Modo radio: %d,  T. Olvido: %d s\n",modoRadio, sOlvido);
    chprintf(ttyCOM,"- Id llamador: %d,  ds max entre msgs:%d s,  ds min:%d\n",idLlamador, dsMaxEntreMsgsLlamador, dsMinEntreMsgsLlamador);
    chprintf(ttyCOM,"- Bloqueo abusones: %d,  avisaAbuso:%d,  tiempo abuso :%d min, ds max entre msgs:%d s,  ds min:%d\n",
             bloqueoAbusones, avisaAbuso, tiempoAbuso, dsMaxEntreMsgsPozo, dsMinEntreMsgsPozo);
    chprintf(ttyCOM,"- IdModbus Vacon: %d\n",idVacon);
}


uint8_t leeVariables(void)
{
  chprintf(ttyCOM,"Leo variables\n");
//  initSpiPinsCPP();
  uint16_t hayW25q16 = W25Q16_start();
  if (hayW25q16)
  {
      chprintf(ttyCOM,"- Detectado flash\n");
      uint16_t claveEEprom = W25Q16_read_u16(0, 0);
      if (claveEEprom != 0x7851)
      {
          chprintf(ttyCOM,"- No esta inicializada... la reseteamos\n");
          reseteaEeprom();
      }

      modoRadio = W25Q16_read_u16(0, POSMODORADIO);
      sOlvido = W25Q16_read_u16(0, POSSOLVIDO);

      idLlamador = W25Q16_read_u16(0, POSIDLLAMADOR);
      dsMaxEntreMsgsLlamador = W25Q16_read_u16(0, POSDSMAXENTREMSGSLLAMADOR);
      dsMinEntreMsgsLlamador = W25Q16_read_u16(0, POSDSMINENTREMSGSLLAMADOR);

      bloqueoAbusones = W25Q16_read_u16(0, POSBLOQUEOABUSONES);
      avisaAbuso = W25Q16_read_u16(0, POSAVISAABUSO);
      tiempoAbuso = W25Q16_read_u16(0, POSTIEMPOABUSOMINUTOS);
      dsMaxEntreMsgsPozo = W25Q16_read_u16(0, POSDSMAXENTREMSGSPOZO);
      dsMinEntreMsgsPozo = W25Q16_read_u16(0, POSDSMINENTREMSGSPOZO);

      idVacon = W25Q16_read_u16(0, POSIDVACON);
  }
  else
  {
      chprintf(ttyCOM,"- No hay flash, uso valores de defecto\n");
      // valores de defecto por si no puede leer flash
      modoRadio = 1; // 1:llamador, 2:pozo
      sOlvido = 500;

      idLlamador = 6;
      dsMaxEntreMsgsLlamador = 600;
      dsMinEntreMsgsLlamador = 20;

      bloqueoAbusones = 1;
      avisaAbuso = 1;
      tiempoAbuso = 600;
      dsMaxEntreMsgsPozo = 600;
      dsMinEntreMsgsPozo = 20;

      idVacon = 1;
  }
  spiStop(&SPID1);
  return hayW25q16;
}

uint16_t reseteaEeprom(void)
{
    uint16_t error = 0;
    W25Q16_sectorErase(0);
    escribeVariables();
    return error;
}

void escribeVariables(void)
{
  if (W25Q16_start()) // hay w25q16 instalado?
  {
      W25Q16_sectorErase(0);

      W25Q16_write_u16(0, POSMODORADIO, modoRadio);
      W25Q16_write_u16(0, POSSOLVIDO, sOlvido);

      W25Q16_write_u16(0, POSIDLLAMADOR, idLlamador);
      W25Q16_write_u16(0, POSDSMAXENTREMSGSLLAMADOR, dsMaxEntreMsgsLlamador);
      W25Q16_write_u16(0, POSDSMINENTREMSGSLLAMADOR, dsMinEntreMsgsLlamador);

      W25Q16_write_u16(0, POSBLOQUEOABUSONES, bloqueoAbusones);
      W25Q16_write_u16(0, POSAVISAABUSO, avisaAbuso);
      W25Q16_write_u16(0, POSTIEMPOABUSOMINUTOS, tiempoAbuso);
      W25Q16_write_u16(0, POSDSMAXENTREMSGSPOZO, dsMaxEntreMsgsPozo);
      W25Q16_write_u16(0, POSDSMINENTREMSGSPOZO, dsMinEntreMsgsPozo);

      W25Q16_write_u16(0, POSIDVACON, idVacon);

      W25Q16_write_u16(0, 0, 0x7851);
  }
  spiStop(&SPID1);
  leeVariables();
}

void leeVariablesC(void)
{
  leeVariables();
  imprimeVariables();
}


