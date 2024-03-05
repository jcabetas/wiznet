# Mini pozo

## Sistema operativo
ChibiOS/RT port for ARM-Cortex-M4 STM32F411.

## Hardware:
-  STM32F411, datos en https://stm32-base.org/boards/STM32F411CEU6-WeAct-Black-Pill-V2.0.html
-  Placa Alim. "miniPozo"

## Errores de placa V0:
-  El rele esta conectado a PC14, que es salida de oscilador. Si no funciona, hay que usar oscilador interno
-  MOSI/MISO de flash esta compartida con RFM96. Se puede resolver arrancando y apagando cuando se usa
-  Secuencia de pines en salida SPI a LCD debe estar para SSD1306, pero no para LCD
-  Los pullup del LCD no deben estar instalados por defecto
-  HM10 tienen intercambiados TX6 y RX6
-  No esta conectada la salida DIO0 de RF95 a la CPU
-  En modbus, el pin 6 es "A" y debería tener pull-up hacia 3,3V (ahora esta en pull-down), idem pin 7 "B"
-  El SN651768 se alimenta a 5V, no a 3,3V
-  Cambiandolo a SN65HVD7X no hace falta pullup ni pulldown (que reduce numero de dispositivos), ver [Articulo 1 Signetics](https://www.ti.com/lit/an/slyt514/slyt514.pdf?ts=1709631755825&ref_url=https%253A%252F%252Fwww.google.com%252F) y [Articulo 2](https://www.ti.com/lit/ds/symlink/sn65hvd78.pdf?ts=1709647071754&ref_url=https%253A%252F%252Fwww.google.com%252F)