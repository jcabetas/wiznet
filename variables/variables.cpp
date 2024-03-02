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


void initSpiPinsCPP(void);


extern "C" {
    void leeVariablesC(void);
}

/*
 * Si la EEPROM es valida, los dos primeros bytes seran 0x7851
 *
 */


uint8_t leeVariables(void)
{
//  initSpiPinsCPP();
  uint16_t hayW25q16 = W25Q16_start();
  if (hayW25q16)
  {
      uint16_t claveEEprom = W25Q16_read_u16(0, 0);
      if (claveEEprom != 0x7851)
          reseteaEeprom();

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

  }
  else
  {
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
//  initSpiPinsCPP();
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

      W25Q16_write_u16(0, 0, 0x7851);
  }
  spiStop(&SPID1);
  leeVariables();
}

void leeVariablesC(void)
{
  leeVariables();
}


//static const SPIConfig spicfg = {
//    .circular         = false,
//    .slave            = false,
//    .data_cb          = NULL,
//    .error_cb         = NULL,
//    .ssport           = GPIOA,
//    .sspad            = GPIOA_W25Q16_CS,
//    .cr1              = SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA,
//    .cr2              = 0U
//};
//

//uint16_t initW25q16(void)
//{
//    // defino los pines
//    /*
//     * CS -   PA4
//     * SCK -  PA5    SPI1_SCK
//     * MISO - PA6    SPI1_MISO
//     * MOSI - PA7    SPI1_MOSI
//     */
//    palSetLine(LINE_W25Q16_CS);
//    palSetLineMode(LINE_W25Q16_CS, PAL_MODE_OUTPUT_PUSHPULL);
//    palClearLine(LINE_SPI1_SCK);
//    palClearLine(LINE_SPI1_MOSI);
//    palClearLine(LINE_SPI1_MISO);
//
//    palSetLineMode(LINE_SPI1_SCK,
//                     PAL_MODE_ALTERNATE(5) |
//                     PAL_STM32_OSPEED_HIGHEST);         /* SPI SCK.             */
//    palSetLineMode(LINE_SPI1_MISO,
//                     PAL_MODE_ALTERNATE(5) |
//                     PAL_STM32_OSPEED_HIGHEST);         /* MISO.                */
//    palSetLineMode(LINE_SPI1_MOSI,
//                     PAL_MODE_ALTERNATE(5) |
//                     PAL_STM32_OSPEED_HIGHEST);         /* MOSI.                */
//    spiStart(&SPID1, &spicfg);
//    return W25Q16_init();
//}


