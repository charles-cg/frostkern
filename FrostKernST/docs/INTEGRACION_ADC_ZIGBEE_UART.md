# Integracion del SensorTask de Joaquin con Zigbee y UART

Base usada: `FrostKernST_UART_Actual.zip`, la version actual del proyecto que ya contiene UART, reintentos de reportes y el manejador de desbordamiento de pila corregido. Sensor incorporado desde `frostkern-sensor-yfb7.zip`.

## Resultado de la revision

El codigo de adquisicion de Joaquin es compatible con los pines, configuracion y contrato de datos de la base actual. Los dos ZIP tienen identicos `main.c`, `main.h`, `stm32wbaxx_hal_msp.c`, `stm32wbaxx_it.c`, `app_types.h`, `app_task_config.h`, `FreeRTOSConfig.h` y `stm32wbaxx_hal_conf.h`.

La incompatibilidad era de integracion: SensorTask llamaba a AppIpc_PublishSample(), que solo escribia en qSample, mientras que el servidor Zigbee leia gStatus.latest_sample. No habia una tarea de control que copiara esa muestra al estado. Por eso el ADC podia funcionar y mostrar temperaturas en su log sin que Zigbee tuviera una muestra valida para enviar.

Ahora el recorrido es:

1. SensorTask lee ADC4 y el contador de flujo.
2. AppIpc_PublishSample() guarda la ultima muestra en gStatus, protegida por gStatusMutex, y la publica tambien en qSample.
3. La tarea Zigbee del Slave consulta ese estado aproximadamente cada 2 s y actualiza el atributo de temperatura.
4. El stack genera los reportes segun la configuracion aceptada: minimo 10 s, maximo 60 s, cambio de 0.50 C.
5. El callback del Master valida el reporte y lo pone en qZbCommand. ZbAppTask identifica el IEEE y guarda el dato en gSlaveStatus.
6. BridgeTask publica la fotografia de ese registro por USART2 cada segundo, manteniendo las banderas de validez.

## Que conserve de Joaquin

`Core/Src/sensor_task.c` y `Core/Inc/sensor_task.h` se incorporaron sin modificar su contenido.

- ADC4 de 12 bits, PA5/IN4 como rank 1 para NTC y PA6/IN3 como rank 2.
- LowPowerAutoWait activado desde SensorTask y calibracion inicial del ADC.
- Lectura por polling; se consume tambien la conversion de bateria para completar la secuencia.
- NTC_BIAS en PB15, con espera de 20 ms antes de leer y apagado posterior.
- Modelo NTC: resistencia fija 22 kohm, R25 de 50 kohm y beta de 3950 K.
- Formula, redondeo a centesimas de grado y validacion de extremos ADC.
- LPTIM1 para flujo, factor 11 Hz/(L/min), contador de 16 bits y calculo usando tiempo real transcurrido.
- Secuencia de BOOST_EN/YFB7_EN, prioridades y pila de SensorTask.

Esto conserva el algoritmo que el usuario indica que ya funciona en la placa. Las pruebas de PC no sustituyen verificar fisicamente la precision/calibracion del sensor y divisor reales.

El uso de AutoWait con polling es coherente con RM0493 y UM3140 de ST: la conversion espera a que se trate el dato anterior, y HAL_ADC_PollForConversion no limpia esos flags antes de HAL_ADC_GetValue cuando AutoWait esta activo.

Referencias:
- https://www.st.com/resource/en/reference_manual/rm0493-multiprotocol-wireless-bluetooth-le-and-ieee802154-stm32wba5xxx-armbased-32bit-mcus-stmicroelectronics.pdf
- https://www.st.com/resource/en/user_manual/um3140-description-of-stm32wba-hal-and-lowlayer-drivers-stmicroelectronics.pdf

## Que cambie

| Archivo | Cambio |
|---|---|
| Core/Src/app_freertos.c | Conserve AppIpc_Init, AppUartBridge_Start y el hook corregido; agregue SensorTask_Start solo en Slave. |
| Core/Src/app_ipc.c | AppIpc_PublishSample actualiza gStatus.latest_sample y updated_at_ms con mutex, y conserva qSample para Control. Agregue include explicito de task.h. |
| Core/Inc/app_ipc.h | Documente el nuevo comportamiento y la propiedad de latest_sample. No cambie la firma de la funcion. |
| Core/Inc/app_temperature_report.h | Nuevo criterio compartido para validez/frescura del dato a publicar. |
| Core/Src/app_temperature_report.c | Valida bandera TEMP, edad de muestra y rango anunciado antes de entregar el valor ZCL. |
| STM32_WPAN/App/app_zigbee_endpoint.c | Consume ese criterio antes de escribir el atributo; una muestra invalida, antigua o fuera del rango se publica como desconocida. |
| STM32_WPAN/App/app_zigbee.c | El log de cada temperatura recibida identifica su IEEE; separa explicitamente una lectura desconocida de una temperatura numerica. |
| tools/check_zigbee_reports.py | Herramienta nueva para verificar actualizaciones por IEEE en una captura o en USART2. |
| tests | Pruebas de integracion con HAL/SDK simulados y pruebas del comprobador de capturas. |

Los archivos UART y la logica de reintentos de reportes permanecen iguales a la base actual. Tampoco cambie las configuraciones de pines, clocks, perfiles Master/Slave, librerias FFD/RFD ni las rutas del SDK.

### Propiedad del estado compartido

SensorTask es el unico productor de muestras: actualiza latest_sample y qSample mediante AppIpc_PublishSample. Control consumira qSample y podra actualizar actuadores, estado, fallos y configuracion aplicada con el mismo mutex; no debe sobrescribir latest_sample con una muestra antigua que saco previamente de la cola.

AppIpc_ReadLocalStatus no consume qSample. Asi Zigbee no le quita mediciones a Control. La muestra que envia el sensor se copia; la funcion no modifica el argumento del llamador. El source_node_id de la copia se normaliza con el ID local. El Slave sigue sin un ID logico asignado por red (254); el Master usa el IEEE para asociarlo a su registro 1/2.

### Frescura de la muestra

El SensorTask original mide aproximadamente cada 1.02 s. Para esta fase se considera antigua una muestra con mas de 5000 ms, comparando HAL_GetTick con sample.timestamp_ms, que tambien procede de HAL_GetTick.

Ese limite esta en APP_TEMPERATURE_SAMPLE_MAX_AGE_MS. Si mas adelante se cambia el muestreo a 30 s por ahorro de energia, tambien debe ajustarse ese criterio. No se modifica en secreto el periodo original del sensor.

La invalidacion local se detecta en la siguiente consulta de ZbAppTask (aproximadamente cada 2 s). Su llegada al Master depende de los intervalos y del transporte Zigbee. No equivale a una alarma instantanea ni implementa control de actuadores.

### Rango anunciado

Se conservan los parametros que ya tenia tu endpoint: minimo 1 C, maximo 100 C y tolerancia declarada 1 C, aun provisionales. La nueva validacion evita anunciar como valida una temperatura que contradiga ese rango. El log ADC puede mostrar un valor fuera de ese rango mientras Zigbee lo marca como desconocido. No se alteraron los parametros del modelo NTC para forzarlo a entrar.

## Como aplicar este paquete

El ZIP contiene una copia del proyecto de fuentes integrado, sin Debug/Master/Slave/Release ni la carpeta duplicada frostkern. Las librerias externas de ST siguen requiriendo los paquetes instalados y las rutas actuales del proyecto.

1. Guarda todo en el IDE con Ctrl+Shift+S.
2. Extrae el ZIP en una carpeta temporal y abre su carpeta FrostKernST.
3. Copia su contenido dentro de la carpeta FrostKernST activa que ya estas usando, combinando carpetas y reemplazando los archivos correspondientes.
4. Selecciona el proyecto en el IDE y pulsa F5. Acepta recargar archivos si lo solicita.
5. Limpia y compila Master; despues limpia y compila Slave. Los nuevos archivos sensor_task.c y app_temperature_report.c deben aparecer entre las fuentes del perfil.
6. Se sigue trabajando con Master/Slave. Los perfiles antiguos Debug/Release no definen rol.

No hace falta regenerar el .ioc. La base ya enlaza -lm; no anadi otra configuracion para logf/lroundf. SensorTask solo arranca en Slave; BridgeTask solo arranca en Master.

## Comprobar reportes en las placas

El comprobador observa las tramas UART v1 que salen del Master. En el firmware actual, updated_at_ms de gSlaveStatus cambia cuando se guarda un reporte de temperatura. Repetir la misma fotografia por UART no cuenta como otro reporte Zigbee.

1. Carga el ELF Master en la placa Master y el ELF Slave en cada Slave.
2. Comprueba en el log del Master la programacion y aceptacion de reportes. Hay una pausa inicial de 20 s; si falla, se reintenta segun el modulo existente.
3. En Slave, revisa el log NTC y el mensaje de calidad: 0=sin dato valido, 1=valido, 2=antiguo, 3=fuera del rango anunciado.
4. Accede a los bytes RAW de USART2 mediante el ESP32 que implemente esta UART o una pasarela/adaptador apropiado. USART1 y los logs USB del ESP32 son canales distintos.
5. En tu PC instala pyserial si vas a usar un puerto serie:

```text
python -m pip install pyserial
```

6. Ejecuta una captura de cuatro minutos. Ajusta COM7 al puerto real. Conecta los Slaves al iniciar la prueba para que puedan unirse dentro de la ventana de altas.

```text
python tools/check_zigbee_reports.py --port COM7 --seconds 240 --expected-slaves 2 --min-updates 2 --save-jsonl captura_zigbee.jsonl
```

Para una primera prueba con un solo Slave cambia expected-slaves a 1. Si necesitas solicitar una nueva ventana de altas desde la herramienta, agrega --permit-join 180; su ACK confirma encolado, no aceptacion de reportes.

El primer valor observado de cada IEEE es la base. Para PASS, el comprobador exige al menos dos actualizaciones posteriores del timestamp de reporte para cada uno de los Slaves requeridos, temperatura valida, ausencia de stale y telemetria UART reciente al terminar. Muestra los intervalos de las actualizaciones; no impone una cota exacta de 60 s porque esta observando el procesamiento en Master, no el instante de transmision en el aire.

Si deseas aislar la recepcion de reportes aunque el sensor entregue desconocido, usa --allow-invalid-temp. Ese PASS solo demuestra actualizaciones de reportes observadas; no certifica una medicion valida.

Una captura JSONL ya guardada, incluida la salida de uart_probe.py monitor, puede analizarse sin abrir un puerto:

```text
python tools/check_zigbee_reports.py --jsonl captura_zigbee.jsonl --expected-slaves 2 --min-updates 2
```

Codigos de salida: 0=criterios cumplidos, 1=criterios incumplidos, 2=error de uso/archivo/puerto. Si no llegan datos, comprueba primero heartbeat y PING/PONG con uart_probe.py. Si se actualiza la fecha de reporte pero la temperatura sigue invalida, el tramo de recepcion esta activo y corresponde revisar sensor/estado/rango.

La herramienta usa solo el IEEE como identidad permanente; los IDs locales pueden cambiar al reiniciar el Master. Detecta reinicios del reloj y no mezcla las cuentas de dos arranques. No modifica consignas ni actuadores.

## Pruebas realizadas en PC

Ejecutadas sobre esta version integrada con GCC, validacion estricta y AddressSanitizer/UndefinedBehaviorSanitizer:

- Se ejecuto sensor_task.c original usando un ADC y un contador HAL simulados.
- ADC raw 2844, con el modelo original, produjo 2499 centesimas (24.99 C). Es una prueba matematica con dato sintetico, no una medicion fisica ni una calibracion.
- Se comprobo el consumo de los dos ranks, AutoWait, conversiones fallidas, extremos ADC, apagado del bias y rollover del contador de flujo.
- Se ejecuto el IPC real para comprobar muestra compartida, cola de longitud 1, mutex, ID local y conservacion de actuadores/configuracion.
- Se ejecuto la funcion real de actualizacion ZCL con SDK simulado para validar frescura, desconocidos y limites.
- Se ejecutaron el callback y la rama de despacho reales del Master, con SDK simulado, y el IPC real: separacion de dos IEEE, resolucion de direccion corta, rechazo de datos mal formados y desconocidos.
- La muestra paso al codificador UART real, y un decodificador Python independiente comprobo temperatura e IEEE.
- Se probaron repeticion de snapshots, dos Slaves, lectura invalida/antigua, reinicios, rollover, perdida de frescura UART y capturas UTF-16 del comprobador.

Para repetir las pruebas de integracion en un PC con GCC, desde FrostKernST:

```text
python tests/run_sensor_zigbee_tests.py
python -m unittest discover -s tests -p test_report_checker.py -v
```

En Linux puedes activar los sanitizadores con FROSTKERN_SANITIZE=1 al ejecutar el primer comando. No se realizo una compilacion ARM nueva ni una prueba de radio real desde esta sesion. Compilar ambos perfiles en el IDE y ejecutar la captura en las placas son pasos pendientes.

## Lo que sigue pendiente

- Calibracion/precision fisica de NTC y confirmacion del rango/tolerancia anunciados.
- Control de actuadores, consignas Zigbee, supervision general y persistencia de identidades.
- El flujo adquirido se conserva localmente en qSample/gStatus; este paquete no agrega el cluster de flujo ni su envio por UART.
- El segundo rank ADC de bateria se consume pero su valor se descarta como en el codigo original: no se anuncia una bateria medida.
- Firmware del ESP32 compatible con el contrato UART y su conexion hacia Wi-Fi/app.

La integracion deja disponible el recorrido de temperatura para probarlo; no declara completado todo el sistema de control.
