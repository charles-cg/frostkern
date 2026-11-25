# Cambios respecto a FrostKernST con persistencia

## Firmware STM32

- `app_zigbee_flow.c` agrega un cluster privado 0xFC10, atributo 0x0000,
  unsigned32 en mL/min. No altera el cluster Temperature Measurement 0x0402.
- Servidor de caudal en Slave y cliente en Master; configuracion de reportes
  con reintentos independientes de los de temperatura, 2..10 s y delta 100 mL/min.
- `app_ipc.c/.h` guarda valor, validez, presencia y fecha de cada variable
  por separado, para ambos IEEE; cola Zigbee ampliada a 16 mensajes.
- `app_uart_bridge.c` agrega TYPE 0x12, LEN 23, sin cambiar la trama 0x10.
  El receptor antiguo sigue pudiendo leer temperatura; hay que actualizarlo
  para decodificar el nuevo mensaje de caudal.
- `app_zigbee.c` detecta PARENT_LINK_FAIL y ATTEMPT_REJOIN; publica el evento
  y solicita rejoin desde ZbAppTask, con backoff, tickets y una sola solicitud
  en vuelo. Cinco fallos o 65 s sin confirmacion -> reset de pila/restauracion.
- El Master reconfigura periodicamente los streams ausentes, tambien despues
  de restaurar una red que todavia no tenia binding de caudal.
- Nuevas pruebas incluyen fragmentos de produccion, IPC real, encoder UART
  y decoders Python/Arduino; SDK/HAL/radio se simulan en las pruebas PC.

## Receptor XIAO

- Muestra `[TEMP]` y `[FLOW]`, identidad IEEE, estado y edad independientes.
- Formatea caudal tanto en mL/min como en L/min; cero es una lectura valida.
- Wi-Fi opcional para Arduino, con reconexion y sin esperar en un bucle que
  detenga UART. Viene desactivado; se configura en `wifi_config.h`.
- No implementa publicacion a Firebase/backend ni comandos hacia STM32.

## Codigo conservado

Se compararon byte por byte y se conservaron la adquisicion ADC/NTC y LPTIM
(`sensor_task.c`), pines e inicializacion (`main.h`/`main.c`), codec CRC,
modulos de persistencia, linker Flash, `.cproject` y `.project`.
Se mantiene la secuencia de habilitacion PB5/PB7 de la ESP32 y la de
PB5/PB6/PB15 de los sensores. La prueba fisica corresponde a la nueva version
completa; conservar codigo no equivale a haber probado el conjunto en placa.

El patch adjunto muestra cambios de produccion, incluidos archivos nuevos.
La instalacion recomendada es copiar los fuentes completos siguiendo el LEEME.
