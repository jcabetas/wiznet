/*
 * vars.h
 *
 *  Created on: 06/03/2014
 *      Author: joaquin
 */

#ifndef VARS_H_
#define VARS_H_

#define VAR_NOSI     1
#define VAR_NOSIAUTO 2
#define VAR_BAUD     3
#define VAR_MODOPOZO 4
#define VARU8   5
#define VAR8    6
#define VARU16  7
#define VAR16   8
#define VAR32   9
#define VAR50  10

#define SECTORINICIALVARS 2
#define LONGNAME 32
#define LONGFAT 16

#define STARTFAT (3+LONGNAME)
#define MAXFAT (STARTFAT+LONGFAT-1)
#define STARTDAT (MAXFAT+1)

/*
 Formato flash:
  - Cada sector (16 paginas) tiene una sola variable. Se indexa con la "posicion"

 Datos PLC
  - Los primeros dos sectores guardan dos versiones PLC
  - Comienzan con una variable uint16_t con valor 12345
  - Otra variable uint16_t con longitud de datos
  - Los datos empiezan en posici�n 4 (0-base)

 Cada variable:
 - pos 0: Tipo de variable: 0/FF (no ocupada) 1 (No/Si)  2 (No/Si/Auto) 3 (Baud) 4 (ModoPozo)
                            5 (uint8) 6 (int8) 7 (uint16) 8 (int16) 9 (int32)
                           10 (str50)
 - pos 1: Checksum (XOR) del nombre uint16_t
 - pos 3..3+LONGNAME-1 (16): nombre de variable
 - pos pos STARTFAT (3+LONGNAME)..MAXFAT (STARTFAT+LONGFAT-1): posiciones ocupadas.
                   Cada bit (empezando por LSB) indica una posici�n usada
                   p.e. 0x00 0x00 0b11100000 indica posici�n 8+8+5 = 21
 - pos STARTDAT (STARTFAT+LONGFAT): datos. El �ltimo v�lido est� en MAXFAT+1+posicion*Size
                   p.e. si MAXFAT = 36 y size=32 ... posIni = 36+1+21*32 = 709 (0x205)
                   si estabamos en sector 8, hay que empezar a leer en 8*16*255 + 709 = 0x8245
                   = p�gina 0x82, posicion 0x45

 - A partir de ahi hay que borrar el sector entero y volver a grabar
*/


#define MAXVARU8    20
#define MAXVARS     30


//void actualizaCRCEeprom(void);
//void inicializarVars(void);
//uint8_t esCRCok(void);
//void leeConfig(void);
//void escribeVars(void);

int16_t escribeVar(uint16_t sectorParam, uint8_t tipoVar, const char *nombVar, uint8_t *pValor);
int16_t leeVariableU8(uint16_t *sectorParam, const char *nombParam, uint8_t valorMin, uint8_t valorMax, uint8_t valorDefault, uint8_t *valor);
int16_t escribeVarU8(uint16_t *sectorParam, const char *nombVar, uint8_t valor);
int16_t leeVariable8(uint16_t *sectorParam, const char *nombParam, int8_t valorMin, int8_t valorMax, int8_t *valorDefault, int8_t *valor);
int16_t escribeVar8(uint16_t *sectorParam, const char *nombVar, int8_t valor);
int16_t leeVariableU16(uint16_t *sectorParam, const char *nombParam, uint16_t valorMin, uint16_t valorMax, uint16_t valorDefault, uint16_t *valor);
int16_t escribeVarU16(uint16_t *sectorParam, const char *nombVar, uint16_t valor);
int16_t leeVariable32(uint16_t *sectorParam, const char *nombParam, int32_t valorMin, int32_t valorMax, int32_t valorDefault, int32_t *valor);
int16_t escribeVar32(uint16_t *sectorParam, const char *nombVar, int32_t valor);
int16_t leeStr50(uint16_t *sectorParam, const char *nombParam, const char *valorDefault, char *valor);
int16_t escribeStr50(uint16_t *sectorParam, const char *nombVar, char *valor);
uint16_t strChecksum(const char *nombVar);
void volcarFlash(BaseSequentialStream *tty);
uint16_t leePosEnFAT(uint16_t sectorParam, uint8_t *error, uint16_t *pageByteFat, uint8_t *pageAddressByteFat, uint8_t *byteFat);

#endif /* VARS_H_ */
