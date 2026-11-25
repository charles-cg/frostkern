# FrostKern: temperatura, caudal y reconexion

Este paquete parte de tu FrostKernST con persistencia, no de los proyectos
alternativos de tu companero. Integra el caudal real del YF-B7 en el recorrido
Slave -> Zigbee -> Master -> UART -> XIAO, junto con la temperatura.

## Que carpeta corresponde a cada programa

| Carpeta | Programa | Destino |
|---|---|---|
| `FrostKernST` | STM32CubeIDE | Tus PCB Master y Slave |
| `ESP32/FrostKern_UART_Receiver` | Arduino IDE | XIAO ESP32-C5 |
| `ESP32/tests` | Pruebas en PC | No se copia al sketch ni al proyecto STM32 |

## Actualizar el STM32, paso por paso

1. Guarda un respaldo de TODO tu proyecto actual fuera de su carpeta.
2. Extrae este ZIP en otra carpeta. Entra a `FrostKernST` del ZIP extraido.
3. Copia TODO su contenido dentro de la carpeta de tu proyecto `FrostKernST`
   actual. Combina carpetas y reemplaza archivos. Conserva los archivos de
   proyecto y los `.ld` incluidos. Evita crear `FrostKernST/FrostKernST`.
   No copies la carpeta ESP32 dentro del proyecto de CubeIDE.
4. Abre el proyecto que ya tenias funcionando en STM32CubeIDE. Selecciona
   el proyecto y pulsa F5 / Refresh para que aparezca el archivo nuevo
   `STM32_WPAN/App/app_zigbee_flow.c`.
5. Selecciona Build Configurations -> Set Active -> Master. Haz Clean y Build.
   Repite con Slave. Usa el SDK WBA 1.10.0 y FreeRTOS que ya tienes instalados.
6. En Debug Configurations, revisa cada configuracion por separado:
   - FrostKern Master: `Master/FrostKern.elf`, Build Configuration `Master`.
   - FrostKern Slave: `Slave/FrostKern.elf`, Build Configuration `Slave`.
   - Debugger: SWD a 1000 kHz en ambas.
7. Alimenta las PCB con sus pilas. Programa la Master y la Slave con sus
   respectivos firmwares. Si Debug queda en main()/Suspended, pulsa F8 una
   vez para dejarlas ejecutar. No necesitan ST-LINK para funcionar despues.

Ambas placas necesitan esta actualizacion para transmitir caudal. El ZIP trae
fuentes; no incluye ELF nuevos ni sustituye la instalacion del SDK de ST.
No vuelvas a aplicar los parches antiguos sobre esta version.

## Actualizar la XIAO, paso por paso

1. Conserva el montaje externo que ya probaron: XIAO fuera del zocalo,
   alimentada por su USB, y Master alimentado con pilas.
2. Conecta Master PA12 (TX) -> XIAO D7 / GPIO12 (RX), y GND -> GND.
   Para este receptor no hace falta conectar D6/TX.
   No unas los pines 5 V ni 3V3 de los dos equipos en este montaje.
3. Abre `ESP32/FrostKern_UART_Receiver/FrostKern_UART_Receiver.ino` en Arduino IDE.
   Mantiene junto al sketch los dos archivos `app_uart_protocol.c/.h` y
   `wifi_config.example.h`; no copies solo el texto del .ino a un sketch vacio.
   El sketch usa el ejemplo sin Wi-Fi hasta que crees tu archivo local `wifi_config.h`.
4. Selecciona XIAO_ESP32C5, su puerto COM y USB CDC On Boot -> Enabled, como
   en la prueba que ya funciono. Carga el programa.
5. Abre el Monitor Serie a 115200 baudios. No abras ese mismo COM en PuTTY
   simultaneamente. Para logs STM32 usa el COM de su consola diagnostica.

No montes la XIAO en el zocalo con su USB normal conectado y el VESP32 de la PCB
activo: en esta placa la linea USB de 5 V queda unida a esa alimentacion.
El montaje de dos jumpers evita unir ambas fuentes y permite ver el monitor.

## Primera prueba de temperatura y caudal

1. Enciende Master y despues Slave. Espera que se unan/restauren su red.
2. En PuTTY del Master espera estas confirmaciones independientes:

   ```text
   Slave 1: temperature report configuration accepted
   Slave 1: flow report configuration accepted
   ```

   La configuracion inicial se agenda a los 20 s. Si hay timeout, reintenta;
   no tiene que confirmarse todo en el primer intento.
3. Con reportes recibidos, el Monitor Serie de la XIAO mostrara, por ejemplo:

   ```text
   [TEMP] Slave 1 IEEE=0x0080E1FFFE32FC18 T=24.29 C valid=1 stale=0 report_seen=1 ...
   [FLOW] Slave 1 IEEE=0x0080E1FFFE32FC18 Q=7500 mL/min (7.500 L/min) valid=1 stale=0 report_seen=1 ...
   ```

   Estos numeros son ejemplos, no lecturas simuladas que genere el firmware.
4. Sin movimiento en el caudalimetro, es normal `Q=0 mL/min (0.000 L/min)`.
   Haz circular agua por el sensor con el montaje de laboratorio que ya usan;
   observa que cambie Q. Conserva la calibracion existente: 660 pulsos/L.
5. Cambia suavemente la temperatura del NTC y observa el valor T. No es
   necesario que los dos reportes lleguen exactamente al mismo tiempo.

La XIAO imprime el ultimo estado cada segundo. Las lecturas Zigbee nuevas
llegan con sus propios intervalos: temperatura min. 10 s, max. 60 s, cambio
0.50 C; caudal min. 2 s, max. 10 s, cambio 100 mL/min. El Slave actualiza los
atributos aproximadamente cada 2 s. El scheduling y la radio pueden agregar
demora. El valor `updated_at_ms` cambia cuando llega un reporte de ESA variable.

`valid` indica si el dato puede interpretarse; no certifica calibracion ni
conexion fisica del sensor. Un contador operativo con cero pulsos produce
caudal valido 0, incluso si no hay sensor conectado. No distingue cable
desconectado de ausencia de flujo. La alarma de bajo flujo requiere una
politica de control y no queda implementada solo por enviar Q.

## Si no aparece caudal

- `report_seen=0`: el Master aun no ha recibido un reporte de caudal.
- Confirma que programaste AMBAS placas y cargaste el receptor nuevo en XIAO.
- En Slave debe aparecer `FrostKern flow cluster 0xFC10...` y un diagnostico
  `[ZB-FLOW SLAVE] Q=... bindings=...`. Tras configurar el reporte debe haber
  un binding para este cluster; el de temperatura por si solo no basta.
- En Master un reporte real genera `Flow report from ...: ... mL/min`.
- El Master revisa flujos ausentes a partir del minuto inicial y cada 120 s;
  solicita de nuevo la configuracion necesaria. No borres la Flash de entrada.
- Temperatura y caudal tienen edad independiente. Que T siga llegando no
  convierte Q en reciente, ni al reves. Si pasan mas de 180 s sin un reporte
  de una variable, su trama pasa a `stale=1 valid=0`.

## Prueba de reconexion y persistencia

1. Comprueba primero que T y Q ya tienen `report_seen=1 valid=1`.
2. Deja el Slave encendido y apaga solo el Master. No pauses el MCU en Debug.
3. Al detectar la perdida del padre, el Slave muestra `[ZB-REJOIN] parent lost`
   e intenta reincorporarse a la misma red. La deteccion depende del polling
   de Zigbee; no ocurre necesariamente al instante de quitar las pilas.
4. Vuelve a encender el Master. Debe restaurar su EPID/PAN guardado. El Slave
   debe recuperar el enlace y reanudar reportes. Revisa fechas de T y Q.
5. Repite apagando y encendiendo ambas placas, sin reprogramarlas. Debe
   conservarse la identidad de red, pero las mediciones esperan reportes nuevos.

Los reintentos de rejoin se ejecutan en ZbAppTask, con pausas crecientes y
sin solicitudes simultaneas. Tras cinco fallos, o una confirmacion que no llega
en 65 s, se reinicia la pila del Slave y se usa el arranque con persistencia.
No se borra deliberadamente la red guardada para intentar resolver un fallo
temporal. Consulta tambien `FrostKernST/LEEME_PERSISTENCIA.md`.

## Wi-Fi opcional en Arduino

Para probar primero el recorrido completo de sensores, el Wi-Fi viene apagado.
Copia `wifi_config.example.h` como `wifi_config.h` y cambia `FROSTKERN_WIFI_ENABLED` a 1 y completa SSID y clave
si quieres probar la conexion a tu router/hotspot. Al conectar imprimira la IP.
El procesamiento UART sigue activo aunque no encuentre la red.

Esto integra la conexion Wi-Fi que aportaba el ejemplo de tu companero, adaptada
a Arduino. No publica todavia en Firebase ni en una aplicacion web: falta
acordar e implementar ese destino. No expone las claves en los logs.

## Comprobaciones realizadas y limites

- Pruebas en PC con sanitizadores: sensores/IPC, callbacks de temperatura y
  caudal, codificacion UART, decoder Python independiente y receptor Arduino.
- Casos: cero caudal valido, valor no disponible, dos Slaves, paquetes
  incompletos, registros ZCL multiples, CRC incorrecto, datos vencidos por
  variable, vuelta del reloj, reintentos y respuestas tardias.
- Persistencia: CRC, escrituras interrumpidas, rotacion, restauracion y
  ausencia de mediciones antiguas tras reiniciar.
- Compilacion de los archivos modificados para Cortex-M33, roles Master y
  Slave, con headers reales de HAL/CMSIS/FreeRTOS/Zigbee del SDK.
- Las pruebas de PC usan radio/HAL simulados. No se hizo una compilacion
  completa enlazada en tu CubeIDE ni una prueba fisica de esta version.
  Haz Clean/Build y las pruebas anteriores antes de darla por validada en PCB.

El receptor Arduino se comprobo con servicios serie/Wi-Fi de prueba en PC;
su compilacion para XIAO y carga fisica se realizan en tu Arduino IDE.
