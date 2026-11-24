# Reintentos de configuracion de reportes de temperatura

> Actualizacion: este proyecto ya integra SensorTask con el estado compartido. Consulta `INTEGRACION_ADC_ZIGBEE_UART.md` para los cambios actuales y la prueba de reportes.


Correccion preparada sobre el ZIP actualizado `rostKernST_UART_actual.zip.zip` revisado en esta conversacion. No sustituye el proyecto completo. Agrega dos fuentes de la logica de reintentos y actualiza los tres archivos Zigbee que la conectan con el SDK.

## Comportamiento

El Master mantiene un ciclo independiente por Slave, usando el nodeId local ya asociado al IEEE. La direccion corta se actualiza al recibir un nuevo aviso de union.

| Etapa | Accion |
|---|---|
| Aviso de union procesado | Programa la primera solicitud para dentro de 20 s. |
| Intento 1 | Envia Configure Reporting y espera confirmacion hasta 20 s. |
| Fallo del intento 1 | Programa el segundo intento 10 s despues de detectar el fallo. |
| Fallo del intento 2 | Programa el tercer intento 30 s despues de detectar el fallo. |
| Fallo del intento 3 | Detiene ese ciclo y registra el fallo. No reintenta indefinidamente. |
| Confirmacion valida | Detiene los reintentos para ese Slave. |
| Nuevo aviso de union tras finalizar el ciclo | Puede iniciar otro ciclo limitado. |

La pausa inicial de 20 s es un parametro de puesta en marcha; el ejemplo oficial Zigbee_TempMeas_Client_Coord de STM32CubeWBA v1.10.0 tambien espera 20 s antes de configurar reportes. No es una garantia de que toda union termine en ese plazo.

Se consideran fallos: error inmediato al enviar la solicitud, respuesta negativa o invalida y ausencia de una confirmacion valida dentro del plazo de la aplicacion. Cada intento tiene un ticket distinto: una respuesta tardia de un intento anterior no puede confirmar uno nuevo. Los avisos de union repetidos mientras el mismo destino ya esta pendiente no reinician continuamente el contador.

Estos son tres intentos de la aplicacion en total. El stack puede hacer sus propias retransmisiones de transporte. Los reintentos de unirse a la red (10/60/300 s) permanecen separados.

El codigo no bloquea ZbAppTask durante las pausas. En Master, su espera de comandos pasa de indefinida a un maximo de 1 s para revisar los plazos. UART y los otros comandos siguen atendidos. El callback del stack solo pone la respuesta en una cola privada sin esperar; las llamadas activas a Zigbee se hacen desde ZbAppTask.

La configuracion solicitada conserva los parametros actuales: minimo 10 s, maximo 60 s y cambio reportable 50 centesimas de grado. Los limites y la tolerancia del sensor no se modifican.

## Aplicar en el IDE

1. Guarda todo con Ctrl+Shift+S.
2. Extrae este ZIP en una carpeta temporal.
3. Abre su carpeta FrostKernST y copia Core, STM32_WPAN, docs y tests.
4. Pegalas dentro de tu carpeta de proyecto activa, la que ya contiene Core, STM32_WPAN y .cproject. Combina carpetas y reemplaza los tres archivos Zigbee cuando Windows lo solicite.
5. En el IDE, selecciona el proyecto y pulsa F5.
6. Compila Master y Slave.

Archivos nuevos de firmware:

```text
Core/Inc/app_zb_report_retry.h
Core/Src/app_zb_report_retry.c
```

Archivos actualizados:

```text
STM32_WPAN/App/app_zigbee.c
STM32_WPAN/App/app_zigbee.h
STM32_WPAN/App/app_zigbee_endpoint.c
```

La declaracion de APP_ZIGBEE_ConfigureTemperatureReports cambia para recibir nodeId y direccion corta; los tres archivos actualizados contienen ya la declaracion, definicion y llamada correspondientes. Copialos juntos.

El paquete conserva el callback que recibe temperaturas y la funcion que actualiza el atributo del Slave. No contiene app_freertos.c: conserva el ajuste al manejador de desbordamiento de pila si ya lo hiciste. No contiene .project, .cproject ni archivos UART.

## Mensajes esperados

```text
Slave 1: temperature report config scheduled in 20 s
Slave 1: report config attempt 1/3 to 0x1234
Slave 1: temperature report configuration accepted
```

Si el envio o la respuesta fallan, o vence el plazo, veras intentos 2/3 y 3/3. Si se agotan:

```text
Slave 1: report config stopped after 3 attempts
```

La direccion mostrada es un ejemplo. En tu placa sera la que asigne Zigbee.

Una aceptacion confirma la configuracion de reportes, no que el sensor ya tenga una temperatura valida. Sigue siendo necesaria la integracion de sensores/control que actualice gStatus.latest_sample.

## Comprobaciones realizadas

- Pruebas de la politica en C: primera espera, tres intentos limitados, respuestas positivas/negativas, dos Slaves, agotamiento por timeout, avisos duplicados, cambio de direccion, respuestas tardias y desbordamiento de contadores.
- Pruebas del adaptador con SDK/FreeRTOS simulados: respuesta en cola, rechazo, error de envio y recuperacion por timeout si se pierde la respuesta de la cola.
- Se usaron GCC con AddressSanitizer/UndefinedBehaviorSanitizer. Estas pruebas no sustituyen compilar los perfiles ARM en tu IDE ni probar las placas.

Para repetir la prueba de politica en un PC con GCC, desde FrostKernST:

```sh
gcc -std=c11 -Wall -Wextra -Werror -pedantic -ICore/Inc Core/Src/app_zb_report_retry.c tests/test_zb_report_retry.c -o /tmp/test_zb_report_retry
/tmp/test_zb_report_retry
```

## Alcance

El ciclo empieza cuando ZbAppTask procesa el aviso de alta ya encolado. Un aviso de alta que no pudo entrar en qZbCommand por cola llena sigue registrandose como error en APP_ZIGBEE_SetNewDevice; este parche no crea un sistema de descubrimiento periodico de dispositivos.

Tampoco implementa recuperacion general de red, persistencia de identidades ni diagnostico de sensores. Corrige el intento unico de configurar reportes. Un fallo permanente de endpoint, cluster o comunicacion puede seguir agotando los tres intentos y debe investigarse con el log.

Documentacion del SDK: https://wiki.st.com/stm32mcu/wiki/Connectivity:Zigbee_stack_API_overview
