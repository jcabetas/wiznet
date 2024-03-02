/*
 * varsGestion.c
 *
 *  Created on: 27/10/2019
 *      Author: jcabe
 */

#include "ch.hpp"
#include "hal.h"

using namespace chibios_rt;

#include <w25q16/w25q16.h>
#include "varsFlash.h"
#include "string.h"

#define tty2 (BaseSequentialStream *)&SD2

uint8_t longParam[] = {1,1,1,1,1,1,2,2,4,8,50};



struct var_t
{
    uint8_t  varTipo;
    char nombVar[LONGNAME];
};
uint8_t varValorU8[MAXVARU8], varDefU8[MAXVARU8], numVarsU8;
struct var_t variables[MAXVARS];

void addVar8(uint8_t idVar, uint8_t tipo, const char *nombreVar, uint8_t defValue)
{
    variables[idVar].varTipo = tipo;
    strncpy(variables[idVar].nombVar, nombreVar,sizeof(variables[idVar].nombVar));
    varDefU8[numVarsU8] = defValue;
    if (tipo==VAR_NOSI || tipo==VAR_NOSIAUTO || tipo==VAR_BAUD || tipo==VAR_MODOPOZO)
    {
 //       variables[idVar].numVarXX = numVarsU8;
        varValorU8[numVarsU8++] = defValue;
    }
}



// comprueba el nombre de la variable en la pagina "page"
// devuelve 1 si son iguales
int8_t nombreVarEnFlashOk(uint16_t sector, const char *nombreVar)
{
  uint8_t pageAddress, i, byte, sonDiferentes;
  uint16_t page;
  i = 0;
  sonDiferentes = 0;
  page = 16*sector;
  pageAddress = 3;
  W25Q16_initStreamRead(page, pageAddress);
  do
  {
      byte = W25Q16_streamRead(&page, &pageAddress);
      if (byte!=nombreVar[i])
      {
          sonDiferentes = 1;
          break;
      }
      if (byte==0)
      {
          break;
      }
      if (++i==(LONGNAME-1))
      {
          break;
      }
  } while (TRUE);
  W25Q16_closeStreamRead();
  if (!sonDiferentes)
    return 1;
  else
    return 0;
}




uint16_t strChecksum(const char *nombVar)
{
  uint16_t chkSumStr = 0;
  for (uint8_t i=0;i<strlen(nombVar);i++)
      chkSumStr += nombVar[i];
  return chkSumStr;
}

uint16_t strChecksumFlash(uint16_t sector)
{
  uint8_t pageAddress, byte, i = 0;
  uint16_t page, chkSumStr = 0;
  // lee nombre de la variable
  page = 16*sector;
  pageAddress = 3;
  W25Q16_initStreamRead(page, pageAddress);
  do
  {
    byte = W25Q16_streamRead(&page, &pageAddress);
    if (byte==0)
        break;
    chkSumStr += byte;
    if (++i==(LONGNAME-1))
        break;
  } while (TRUE);
  W25Q16_closeStreamRead();
  return chkSumStr;
}



/*
 * Devuelve 0 si no coincide, 1 si coincide, 2 si es pagina en blanco
 */
uint8_t coincidenciaDatosVar(uint16_t sectorParam, uint8_t tipoVar, const char *nombVar, uint16_t chkSumStr)
{
  uint16_t chkSumFlash;
  uint8_t tipoVarFlash;
  tipoVarFlash = W25Q16_read(16*sectorParam, 0);
  if (tipoVarFlash == 0xFF)
      return 0;
  chkSumFlash = W25Q16_read_u16(16*sectorParam, 1);
  if (tipoVarFlash!=tipoVar || chkSumFlash!=chkSumStr)
      return 0;
  if (!nombreVarEnFlashOk(sectorParam, nombVar)) // coincide nombres?
      return 0;
  return 1;
}

/*
 * Formato flashopcional:
 * - Cada sector (16 paginas) tiene una sola variable. Se indexa con la "posicion"
 *
 * Cada versi�n PLC
 * - Los primeros dos sectores guardan dos versiones PLC
 * - Comienzan con una variable uint16_t con valor 12345
 * - Otra variable uint16_t con longitud de datos
 * - Los datos empiezan en posici�n 4 (0-base)
 *
 * Cada sector siguiente es una variable:
 - pos 0: Tipo de variable: 0/FF (no ocupada) 1 (uint8_t)  2 (int8_) 3 (uint16_t) 4 (int16_t)
                                       5 (uint32_t) 6 (int32_t) 7 (str8) 8 (str16) 9 (str24)
                                      10 (str32)
 - pos 1: Checksum (XOR) del nombre uint16_t
 - pos 3..3+LONGNAME (16): nombre de variable
 - pos STARTFAT..MAXFAT: posiciones ocupadas. Cada bit (empezando por LSB) indica una posici�n usada
                   p.e. 0x00 0x00 0b11100000 indica posici�n 8+8+5 = 21
 - pos STARTDAT: datos. El �ltimo v�lido est� en MAXFAT+1+posicion*Size
                   p.e. si MAXFAT = 36 y size=32 ... posIni = 36+1+21*32 = 709 (0x205)
                   si estabamos en sector 8, hay que empezar a leer en 8*16*255 + 709 = 0x8245
                   = p�gina 0x82, posicion 0x45

 - A partir de ahi hay que borrar el sector entero y volver a grabar
*/

void initVarFlash(uint16_t sectorParam, uint8_t tipoVar, const char *nombVar, uint8_t *pValorDefault)
{
    uint16_t chkSum, i;
    uint16_t page = 16*sectorParam;
    uint8_t pageAddress;
    chkSum = strChecksum(nombVar);
    W25Q16_write(page, 0, tipoVar);       // tipoVar en pos 0
    W25Q16_write_u16(page, 1, chkSum); // checkSum nombre
    pageAddress = 3;
    W25Q16_initStreamWrite(page, pageAddress);
    for (i=0;i<strlen(nombVar) && i<LONGNAME-1;i++)
        W25Q16_streamWrite(&page, &pageAddress, nombVar[i]);
    W25Q16_streamWrite(&page, &pageAddress, 0);
    W25Q16_closeStreamWrite();  // La FAT se habr� quedadoen 0xFF
    // escribo valor de defecto
    pageAddress = STARTDAT;
    W25Q16_initStreamWrite(page, pageAddress);
    for (i=0;i<longParam[tipoVar];i++)
        W25Q16_streamWrite(&page, &pageAddress, pValorDefault[i]);
    W25Q16_closeStreamWrite();
}


// explora FAT buscando posicion de variable. Devuelve posicion y actualiza page y pageAdd de la posicion
// tambien devuelve el valor del byte de la FAT
uint16_t leePosEnFAT(uint16_t sectorParam, uint8_t *error, uint16_t *pageByteFat, uint8_t *pageAddressByteFat,
                     uint8_t *byteFat)
{
    uint16_t pos;
    uint8_t byte, i;
    *pageByteFat = 16*sectorParam;
    *pageAddressByteFat = STARTFAT;
    *error = 0;
    pos = 0;
    W25Q16_initStreamRead(*pageByteFat, *pageAddressByteFat);
    while (*pageAddressByteFat<=MAXFAT)
    {
        byte = W25Q16_streamRead(pageByteFat, pageAddressByteFat);
        *byteFat = byte;
        if (byte==0)
        {
            pos += 8;
            continue;
        }
        if (byte!=0)
        {
            for (i=0;i<8;i++)
            {
                if (byte & 1) // se ponen los ceros por la derecha
                {
                    W25Q16_closeStreamRead();
                    (*pageAddressByteFat)--;  // era el byte anterior el que tenia la FAT actual
                    return pos;
                }
                byte = (byte>>1);
                pos += 1;
            }
        }
    }
    // llegue al final de la FAT sin encontrar un 0
    W25Q16_closeStreamRead();
    *error = 1;
    return 0;
}



uint8_t encuentraVariable(uint16_t *sectorParam, uint8_t tipoVar, uint8_t *pValorDefault, const char *nombVar)
{
    uint16_t chkSumStr;
    uint8_t tipoVarFlash;
    *sectorParam = SECTORINICIALVARS;
    chkSumStr = strChecksum(nombVar);
    do
    {
        tipoVarFlash = W25Q16_read(16*(*sectorParam), 0);
        if (tipoVarFlash==0xFF)
        {
            // no la he encontrado, la creo e inicializo
            initVarFlash(*sectorParam, tipoVar, nombVar, pValorDefault);
            return 1;
        }
        if (coincidenciaDatosVar(*sectorParam, tipoVar, nombVar, chkSumStr)==1)
            return 1;
        (*sectorParam)++;
    } while (*sectorParam<512);
    return 0;
}

/*
 * Estructura flash 25q16:
   - Tiene 16Mbit = 2 Mbytes en 8.192 paginas de 255 bytes
   - Las paginas se borran en 512 sectores de 16 paginas
   - Los dos primeros sectores tienen cada uno una versi�n de datos PLC
   - Una variable ocupa 1 sector de 16 paginas
   - Las variables se identifican por numero de sector posParam

 * Cada versi�n PLC
  - Los primeros dos sectores guardan dos versiones PLC
  - Comienzan con una variable uint16_t con valor 12345
  - Otra variable uint16_t con longitud de datos
  - Los datos empiezan en posici�n 4 (0-base)

 * Cada variable:
  - pos 0: Tipo de variable: 0/FF (no ocupada) 1 (uint8_t)  2 (int8_) 3 (uint16_t) 4 (int16_t)
                                        5 (uint32_t) 6 (int32_t) 7 (str8) 8 (str16) 9 (str24)
                                       10 (str32)
  - pos 1: Checksum (XOR) del nombre uint16_t
  - pos 3..3+LONGNAME (16): nombre de variable
  - pos 20..MAXFAT: posiciones ocupadas. Cada bit (empezando por LSB) indica una posici�n usada
                    p.e. 0x00 0x00 0b11100000 indica posici�n 8+8+5 = 21
  - MAXFAT+1: datos. El �ltimo v�lido est� en MAXFAT+1+posicion*Size
                    p.e. si MAXFAT = 36 y size=32 ... posIni = 36+1+21*32 = 709 (0x205)
                    si estabamos en sector 8, hay que empezar a leer en 8*16*255 + 709 = 0x8245
                    = p�gina 0x82, posicion 0x45

  - A partir de ahi hay que borrar el sector entero y volver a grabar
 */

/*
 * Escribe una variable generica en flash
 * Si no indican posicion, hay que buscar y/o crear
 * - Primero explora si la variable existe:
 * - Halla hash del nombre
 * - Recorre variables buscando hash y tipo iguales
 *   Si la hay, verifica que el nombre coincide
 *   Si coincide, localiza ultimo valor escrito
 *   Si coinciden valores, vuelve sin hacer nada; en caso contrario escribe
 * Si indican posicion:
 * - Verifica que coincide tipo y nombre
 * - Si no coincide, resetea
 */
int16_t escribeVar(uint16_t sectorParam, uint8_t tipoVar, const char *nombVar, uint8_t *pValor)
{
    uint16_t pageByteFat, chkSumStr;
    uint8_t pageAddressByteFat, error, byteFat, *pValorBucle;
    uint8_t posInPage, i;
    // Indican posicion?
    if (sectorParam!=0)
    {
        chkSumStr = strChecksum(nombVar);
        if (!coincidenciaDatosVar(sectorParam, tipoVar, nombVar, chkSumStr)) // el sector propuesto no vale
        {
            if (!encuentraVariable(&sectorParam, tipoVar, pValor, nombVar)) // miro si esta en otro sector
                return 0;// error, he llegado al final de la flash y ni siquiera tengo sitiopara escribir nueva;
        }
    }
    else
        encuentraVariable(&sectorParam, tipoVar, pValor, nombVar);
    // aqui debe estar ya apuntando sectorParam a la variable (si es necesario, la habra creado)
    posInPage = leePosEnFAT(sectorParam, &error, &pageByteFat, &pageAddressByteFat, &byteFat);
    if (error) // seguramente por haber superado el maximo de escrituras
    {
        //void initVarFlash(uint16_t sectorParam, uint8_t tipoVar, const char *nombVar, uint8_t *pValorDefault)
        W25Q16_sectorErase(16*sectorParam);
        initVarFlash(sectorParam, tipoVar, nombVar, pValor);
        return 0;
//        W25Q16_sectorErase(16*sectorParam);
//        initVarFlash(sectorParam, tipoVar, nombVar, pValor);
//        posInPage = leePosEnFAT(sectorParam, &error, &pageByteFat, &pageAddressByteFat, &byteFat);
    }
    // actualizo valor de la FATe incremento posicion
    byteFat = (byteFat << 1);
    W25Q16_write(pageByteFat, pageAddressByteFat, byteFat);
    posInPage += 1;
    // escribo en siguiente posicion
    uint32_t dirNewPos = (sectorParam<<12) + STARTDAT + posInPage*longParam[tipoVar];
    uint16_t pageData = dirNewPos>>8;
    uint8_t  pageAddressData = dirNewPos & 0xFF;
    // escribimos valor (hay que confirmar si es little-indian)
    pValorBucle = pValor;
    for (i=0;i<longParam[tipoVar];i++)
    {
        W25Q16_write(pageData, pageAddressData, *pValorBucle);
        pValorBucle++;
        if (++pageAddressData==0)
            pageData++;
    }
    return 0;
}


int16_t leeVariable(uint16_t *sectorParam, uint8_t tipoVar, const char *nombVar, uint8_t *pValorDefault, uint8_t *pValor)
{
    uint8_t posInPage, pageAddressByteFat, byteFat, error, *pValorBucle, i;
    uint16_t pageByteFat, chkSumStr;
    if (*sectorParam!=0)
    {
        chkSumStr = strChecksum(nombVar);
        if (!coincidenciaDatosVar(*sectorParam, tipoVar, nombVar, chkSumStr)) // el sector propuesto no vale
        {
            if (!encuentraVariable(sectorParam, tipoVar, pValorDefault, nombVar)) // miro si esta en otro
                return 0;// error, he llegado al final de la flash y ni siquiera tengo sitiopara escribir nueva;
        }
    }
    else
      if (!encuentraVariable(sectorParam, tipoVar, pValorDefault, nombVar))
          return 0;// error, he llegado al final de la flash y ni siquiera tengo sitiopara escribir nueva;
    posInPage = leePosEnFAT(*sectorParam, &error, &pageByteFat, &pageAddressByteFat, &byteFat);
    // busco direccion en siguiente posicion
    uint32_t dirNewPos = ((*sectorParam)<<12) + STARTDAT + posInPage*longParam[tipoVar];
    uint16_t pageData = dirNewPos>>8;
    uint8_t  pageAddressData = dirNewPos & 0xFF;
    // lee valor
    pValorBucle = pValor;
    for (i=0;i<longParam[tipoVar];i++)
    {
        *pValorBucle = W25Q16_read(pageData, pageAddressData);
        if (++pageAddressData==0)
            pageData++;
        pValorBucle++;
    }
    return 0;
}

int16_t leeVariableU8(uint16_t *sectorParam, const char *nombParam, uint8_t valorMin, uint8_t valorMax,  uint8_t valorDefault, uint8_t *valor)
{
    int16_t error;
    uint8_t valorLeido;
    error = leeVariable(sectorParam, VARU8, nombParam, &valorDefault, &valorLeido);
    if (error)
        return error;
    *valor = valorLeido;
    if (*valor<valorMin)
    {
        *valor = valorMin;
        return -4;
    }
    if (*valor>valorMax)
    {
        *valor = valorMax;
        return -5;
    }
    return 0;
}

int16_t escribeVarU8(uint16_t *sectorParam, const char *nombVar, uint8_t valor)
{
  int16_t err;
  uint8_t valorInt;
  // comprobamos si es valor repetido, para evitar reescribir
  err = leeVariableU8(sectorParam, nombVar, 0, 0xFF, valor, &valorInt);
  if (err==0 && valorInt==valor)
      return 0;
  valorInt = valor;
  err = escribeVar(*sectorParam, VARU8, nombVar, &valorInt);
  return err;
}

int16_t leeVariable8(uint16_t *sectorParam, const char *nombParam, int8_t valorMin, int8_t valorMax, int8_t valorDefault, int8_t *valor)
{
    int16_t error;
    int8_t valorLeido;
    error = leeVariable(sectorParam, VAR8, nombParam, (uint8_t *)&valorDefault, (uint8_t *)&valorLeido);
    if (error)
        return error;
    *valor = valorLeido;
    if (*valor<valorMin)
    {
        *valor = valorMin;
        return -4;
    }
    if (*valor>valorMax)
    {
        *valor = valorMax;
        return -5;
    }
    return 0;
}


int16_t escribeVar8(uint16_t *sectorParam, const char *nombVar, int8_t valor)
{
  int16_t err;
  int8_t valorInt;
  // comprobamos si es valor repetido, para evitar reescribir
  err = leeVariable8(sectorParam, nombVar, -128, 127, &valor, &valorInt);
  if (err==0 && valorInt==valor)
      return 0;
  valorInt = valor;
  err = escribeVar(*sectorParam, VAR8, nombVar, (uint8_t *) &valorInt);
  return err;
}

int16_t leeVariableU16(uint16_t *sectorParam, const char *nombParam, uint16_t valorMin, uint16_t valorMax, uint16_t valorDefault, uint16_t *valor)
{
    int16_t error;
    uint16_t valorLeido;
    error = leeVariable(sectorParam, VARU16, nombParam, (uint8_t *)&valorDefault, (uint8_t *)&valorLeido);
    if (error)
        return error;
    *valor = valorLeido;
    if (*valor<valorMin)
    {
        *valor = valorMin;
        return -4;
    }
    if (*valor>valorMax)
    {
        *valor = valorMax;
        return -5;
    }
    return 0;
}

int16_t escribeVarU16(uint16_t *sectorParam, const char *nombVar, uint16_t valor)
{
  int16_t err;
  uint16_t valorInt;
  // comprobamos si es valor repetido, para evitar reescribir
  err = leeVariableU16(sectorParam, nombVar, 0, 0xFFFF, valor, &valorInt);
  if (err==0 && valorInt==valor)
      return 0;
  valorInt = valor;
  err = escribeVar(*sectorParam, VARU16, nombVar, (uint8_t *)&valorInt);
  return err;
}


int16_t leeVariable32(uint16_t *sectorParam, const char *nombParam, int32_t valorMin, int32_t valorMax, int32_t valorDefault, int32_t *valor)
{
    int16_t error;
    int32_t valorLeido;
    error = leeVariable(sectorParam, VAR32, nombParam, (uint8_t *)&valorDefault, (uint8_t *)&valorLeido);
    if (error)
        return error;
    *valor = valorLeido;
    if (*valor<valorMin)
    {
        *valor = valorMin;
        return -4;
    }
    if (*valor>valorMax)
    {
        *valor = valorMax;
        return -5;
    }
    return 0;
}

int16_t escribeVar32(uint16_t *sectorParam, const char *nombVar, int32_t valor)
{
  int16_t err;
  int32_t valorInt;
  // comprobamos si es valor repetido, para evitar reescribir
  err = leeVariable32(sectorParam, nombVar, 0, 0xFFFF, valor, &valorInt);
  if (err==0 && valorInt==valor)
      return 0;
  valorInt = valor;
  err = escribeVar(*sectorParam, VAR32, nombVar, (uint8_t *)&valorInt);
  return err;
}

int16_t escribeStr50(uint16_t *sectorParam, const char *nombVar, char *valor)
{
  int16_t err;
  char buff[50];
  // comprobamos si es valor repetido, para evitar reescribir
  err = leeStr50(sectorParam, nombVar, valor, buff);
  if (err==0 && !strncmp(valor,buff,sizeof(buff)))
      return 0;
//  strncpy(valor,buff,sizeof(buff));
  err = escribeVar(*sectorParam, VAR50, nombVar, (uint8_t *)valor);
  return err;
}

int16_t leeStr50(uint16_t *sectorParam, const char *nombParam, const char *valorDefault, char *valor)
{
    int16_t error;
    error = leeVariable(sectorParam, VAR50, nombParam, (uint8_t *)valorDefault, (uint8_t *)valor);
    if (error)
        return error;
    return 0;
}

//int16_t escribeStr50_C(uint16_t *sectorParam, const char *nombVar, char *valor)
//{
//    return escribeStr50(sectorParam, nombVar, valor);
//}
//
//int16_t leeStr50_C(uint16_t *sectorParam, const char *nombParam, const char *valorDefault, char *valor)
//{
//    return leeStr50(sectorParam, nombParam, valorDefault, valor);
//
//}
/*
 * Maximum speed SPI configuration (27MHz -BR0==Fpclk/4-, CPHA=0, CPOL=0, MSb first).
  * Low speed SPI configuration (421.875kHz, CPHA=0, CPOL=0, MSb first).
 */
//static const SPIConfig spicfg = {
//  false,
//  NULL,
//  GPIOA,
//  GPIOA_W25Q16_CS,
//  SPI_CR1_BR_0 | SPI_CR1_BR_1 | SPI_CR1_CPOL | SPI_CR1_CPHA,
//  0
//};//



