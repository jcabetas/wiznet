/*
 * variables.h
 *
 */

#ifndef VARS_H_
#define VARS_H_

#define     POSMODORADIO                2
#define     POSSOLVIDO                  4

#define     POSIDLLAMADOR               10
#define     POSDSMAXENTREMSGSLLAMADOR   12
#define     POSDSMINENTREMSGSLLAMADOR   14

#define     POSBLOQUEOABUSONES          20
#define     POSAVISAABUSO               22
#define     POSTIEMPOABUSOMINUTOS       24
#define     POSDSMAXENTREMSGSPOZO       26
#define     POSDSMINENTREMSGSPOZO       28

#define     POSIDVACON                  30


uint8_t leeVariables(void);
void escribeVariables(void);
uint16_t reseteaEeprom(void);
uint16_t initW25q16(void);

#endif /* VARS_H_ */
