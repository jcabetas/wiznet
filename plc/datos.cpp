#include "ch.hpp"
#include "hal.h"
using namespace chibios_rt;
#include "chprintf.h"
#include "string.h"
#include "tipoVars.h"
#include "bloques.h"
#include "nextion.h"


//parametroU16Flash hayModbus("hayModbus", 0, 0, 1);
//parametroU16Flash baudModbus("baudModbus", 9600, 300, 15200);
//parametroU16Flash idModbusEnergia("idModbusEnergia", 0, 0, 32000);
//parametroU16Flash idModbusVariador("idModbusVariador", 0, 0, 32000);
//parametroU16Flash parametroFrecuenciaMB("parametroFrecuenciaMB", 0, 0, 32000);
//parametroU16Flash parametroPresionMB("parametroPresionMB", 58, 0, 32000);
//parametroU16Flash parametroEstadoMB("parametroEstadoMB", 23, 0, 32000);
//parametroStringFlash apn("apn","ORANGEWORLD");
//parametroU16Flash hayWifi("hayWifi", 0, 0, 1);
//parametroU16Flash avisaAbuso("avisaAbuso", 0, 0, 1);
//parametroU16Flash litrosPorPulso("litrosPorPulso", 100, 1, 1000);


//extern varNUMERO idModbusEnergia;
//extern varNUMERO idModbusVariador;
//extern varNUMERO parametroSpeedMB;
//extern varNUMERO parametroPresionMB;


//varNUMERO idIni(0,1,1,100,"id inicial");
//varNUMERO idFin(0,1,1,100,"id Final");

//varNOSI hayModbus(120,0,"Hay Modbus");//);
//varBAUD baudModbus(121,115200,"Baud Modbus");//, initMedidas);
//varNUMERO idModbusEnergia(1,1, 0, 128, "Id Sensor Ener");//, initMedidas);
//varNUMERO idModbusVariador(2,2, 0, 128, "Id Variador");//, initMedidas);
//varNUMERO parametroFrecuenciaMB(42,0, 0, 4000, "MB frec 0");
//varNUMERO parametroPresionMB(43,58, 0, 4000, "MB presion 58");
//varNUMERO parametroEstadoMB(43, 23, 0, 4000, "MB Estado 23");




//varSTR50 apn(3,"ORANGEWORLD","apn");


//varNOSI hayWifi(4,0,"Hay Wifi");

//varNOSI hayGPRS(5,0,"Hay GPRS");//,haCambiadoGPRS);
//varNOSI avisaAbuso(40,0,"Avisa abuso");
//varNUMERO litrosPorPulso(44,100,1,100,"Litros/pulso");



//varSTR50 telefAdm(21,"+34619262851","Tlf adm");
//varSTR50 telef1(22,"","Tlf Id 1");
//varSTR50 telef2(23,"","Tlf Id 2");
//varSTR50 telef3(24,"","Tlf Id 3");
//varSTR50 telef4(25,"","Tlf Id 4");
//varSTR50 telef5(26,"","Tlf Id 5");
//varSTR50 telef6(27,"","Tlf Id 6");
//varSTR50 telef7(28,"","Tlf Id 7");
//varSTR50 telef8(29,"","Tlf Id 8");
//
//varSTR50 nombreAdm(30,"Sr. Administrador","Nombre Adm");
//varSTR50 nombre1(31,"Alejandro","Nombre Id 1");
//varSTR50 nombre2(32,"Luis","Nombre Id 2");
//varSTR50 nombre3(33,"Marta","Nombre Id 3");
//varSTR50 nombre4(34,"Sr. Callen","Nombre Id 4");
//varSTR50 nombre5(35,"Maripaz","Nombre Id 5");
//varSTR50 nombre6(36,"Reserva6","Nombre Id 6");
//varSTR50 nombre7(37,"Reserva7","Nombre Id 7");
//varSTR50 nombre8(38,"Reserva8","Nombre Id 8");

//parametroU16Flash peticionAgua("Peticion agua",39,0,);

uint8_t montarSDValor()
{
//  varNOSI *peticionAgua= new varNOSI(39,0,"Peticion agua");
//  varNOSI *peticionAgua2= new varNOSI(39,0,"Peticion agua");
//  delete peticionAgua;
    return 1;
//    if (montarSD==NULL)
//        return 1;
//  return montarSD->valor();
}
