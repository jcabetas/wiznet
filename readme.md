# Mini pozo

## Sistema operativo
ChibiOS/RT port for ARM-Cortex-M4 STM32F411.

## Hardware:
-  STM32F411, datos en https://stm32-base.org/boards/STM32F411CEU6-WeAct-Black-Pill-V2.0.html
-  Placa Alim. "miniPozo"

## Errores de placa:
-  El rele esta conectado a PC14, que es salida de oscilador. Si no funciona, hay que usar oscilador interno
-  MOSI/MISO de flash esta compartida con RFM96. Se puede resolver arrancando y apagando cuando se usa
-  Secuencia de pines en salida SPI a LCD debe estar para SSD1306, pero no para LCD
-  Los pullup del LCD no deben estar instalados por defecto
-  HM10 tienen intercambiados TX6 y RX6
-  No estan conectadas las salidas de RF95