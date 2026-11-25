# Diagnóstico de reportes Zigbee en las PCB

## Qué sabemos y qué falta

La prueba física confirmó la formación de red, el anuncio del Slave y una respuesta satisfactoria a Configure Reporting. El ADC del Slave produjo alrededor de 23.5–23.8 °C. No se observó todavía el mensaje de recepción de temperatura en el Master.

La aceptación de Configure Reporting confirma esa transacción. No demuestra por sí sola que un reporte posterior haya llegado al callback de aplicación del Master.

Esta versión añade observación. **No es todavía una corrección confirmada del fallo.** No contiene temperaturas simuladas ni modifica la fórmula del sensor, los valores adquiridos, los roles, los intervalos de reporte, la política de reintentos o las rutas del SDK.

Base: `app_zigbee_endpoint.c` de la rama RTOS revisada en `adb9ed4`. Si editaste después ese mismo archivo, compara antes de reemplazarlo. Conserva un respaldo de tu versión fuera del proyecto.

## Instalar

1. Extrae el ZIP.
2. Copia **el contenido de ACTUALIZAR** dentro del `FrostKernST` que tienes importado en STM32CubeIDE. Acepta combinar carpetas y reemplazar `STM32_WPAN/App/app_zigbee_endpoint.c`.
3. Selecciona FrostKern en el IDE y presiona F5.
4. Activa Master, ejecuta Clean y Build. Comprueba que su lanzamiento use `Master/FrostKern.elf` y Build Configuration Master. Programa únicamente la PCB Master y pulsa Resume/F8 si queda detenida.
5. Activa Slave, ejecuta Clean y Build. Comprueba `Slave/FrostKern.elf` y Build Configuration Slave. Programa la PCB Slave y pulsa Resume/F8 si queda detenida.
6. Con los dos firmwares ya grabados y sin sesiones de depuración activas, arranca primero Master y luego Slave dentro de la ventana de 180 segundos. Las dos placas necesitan su alimentación. No mantengas una placa pausada al comprobar radio.
7. Mantén PuTTY en USART1 del Master: 115200, 8N1, sin control de flujo. Guarda desde el arranque hasta unos 120 segundos después de la aceptación. Busca las líneas `[ZB-DIAG]` y `[ZB-DIAG RX]`.

En el arranque de ambas compilaciones debe verse:

```text
[ZB-DIAG] report diagnostics enabled
```

Si no aparece esa marca, no está ejecutándose esta versión o se perdió el comienzo del log. No interpretes la ausencia de otros marcadores hasta comprobar qué versión ejecuta cada placa.

## Qué añade en el Master

Al entrar al callback de reportes de temperatura:

```text
[ZB-DIAG RX] src-mode=... src-ep=17 dst-ep=17 attr=0x0000 type=0x29 len=2
[ZB-DIAG RX] queued: temperature=2361 valid=1
```

Los valores son ejemplos. Después debe aparecer el mensaje existente `Temperature report from ...: ... centi-C`.

Cada salida anticipada del callback también identifica la causa: datos nulos, endpoints distintos, atributo/tipo/longitud incorrectos, error de interpretación o modo de dirección no soportado. La comprobación de cola llena conserva su mensaje previo.

**La marca RX significa que el stack entregó un reporte al callback de este cluster.** Su ausencia no demuestra por sí sola que no haya habido ninguna trama de radio: podría no haberse enviado el reporte, no haber llegado, o no haber sido despachado a este callback.

## Qué añade en el Slave

La actualización existente del atributo emite un resumen al entrar por primera vez y aproximadamente cada 10 segundos mientras la tarea Zigbee continúe ejecutándolo:

```text
[ZB-DIAG SLAVE] sample=2361 zcl=2361 quality=1 age=... write=0x00 read=0x00 reportable=0x00 bindings=...
```

| Campo | Interpretación |
|---|---|
| `sample` | Temperatura de la muestra compartida, en centésimas de °C. |
| `zcl` | Valor leído de vuelta del atributo del cluster. Solo interpretable si `read=0x00`. `-32768` es desconocido. |
| `quality` | 0: sin dato válido; 1: válido; 2: antiguo; 3: fuera del rango configurado. |
| `age` | Edad local de la muestra en ms. El límite de esta versión es 5000 ms. |
| `write` | Resultado de escribir el atributo; `0x00` significa éxito. |
| `read` | Resultado de leerlo de vuelta; `0x00` significa éxito. |
| `reportable` | Resultado de leer con la comprobación de atributo reportable habilitada; `0x00` significa éxito. |
| `bindings` | Número de entradas APS locales cuyo origen coincide con endpoint 17 y cluster de temperatura 0x0402. No identifica por sí solo el destino ni reemplaza inspeccionar una configuración dedicada del SDK. |

Si no hay cluster o no puede leerse el estado compartido, lo señala con otro mensaje `[ZB-DIAG SLAVE]`. Si no aparece ningún resumen tras la unión, necesitamos comprobar que la tarea llegue a `APP_ZIGBEE_UpdateTemperature()`.

También se adjunta el callback documentado del SDK para avisos de envío:

```text
[ZB-DIAG TX] report-confirm callback (check Master RX)
```

Este callback no entrega en su firma un estado de recepción remota. **Su aparición no acredita que el Master haya recibido el reporte.** Se utiliza para correlacionar actividad del stack con el log RX del Master.

## Orden para investigar

1. Comprueba versión y unión: marca de diagnóstico, rol correcto, `Association accepted` y `FrostKern network ready` en Slave.
2. Comprueba que el resumen del Slave muestre muestra reciente, `quality=1`, escritura y lectura satisfactorias y `sample` coincidente con `zcl`.
3. Guarda el resultado `reportable`, el número de bindings y cualquier marca TX.
4. Correlaciona con el Master. Si entra RX pero lo descarta, conserva la causa y los campos recibidos. Si entra RX y se encola pero no aparece el mensaje de guardado, revisa los mensajes de IPC/identificación.

Con un solo ST-LINK puedes capturar en dos ejecuciones controladas. Guarda primero el log del Master. Para obtener el del Slave, termina la sesión de depuración si sigue activa, cambia el ST-LINK/UART al Slave y repite el arranque Master → Slave sin volver a programar. Las direcciones cortas pueden cambiar; utiliza los IEEE para identificar cada placa.

No alteres la temperatura ni fuerces envíos para obtener un resultado aparentemente válido: necesitamos observar el recorrido del dato real.

## Desactivar o retirar

En `app_zigbee_endpoint.c`, cambia el valor predeterminado de `APP_ZIGBEE_REPORT_DIAGNOSTICS` de 1 a 0 y recompila/programa los dos perfiles para retirar estas trazas. También puedes recuperar el archivo original de tu respaldo. Revisa diferencias si hubo cambios posteriores.

## Validación realizada

- Pruebas de PC de la función real de actualización, con SDK simulado: lectura de vuelta, consultas de diagnóstico, límite de frecuencia del log, errores de escritura, snapshot no disponible, muestra antigua y conservación de los datos de entrada.
- Callback real del Master con diagnóstico habilitado: filtros, cola, IPC y codificación UART, usando SDK/RTOS simulados.
- Pruebas existentes de integración ADC → IPC → atributo y callback → IPC → UART.

Se compilaron los bancos de prueba con GCC C11 y `-Wall -Wextra -Werror -pedantic`. **No se ejecutó una compilación ARM con el SDK completo ni una prueba RF en esta computadora.** La compilación de Master/Slave y las observaciones en las PCB siguen siendo necesarias.

Desde `FrostKernST`, con Python y GCC de escritorio:

```bash
python tests/run_sensor_zigbee_tests.py
python tests/run_zigbee_diagnostics_tests.py
```

## Referencias del SDK

- `zcl.h`: `ZbZclAttrIntegerRead`, `ZbZclAttrRead`, `ZbZclClusterReportConfirmCallbackAttach`.
- `zigbee.aps.h`: `ZbApsBindSrcCount`.
- [API Zigbee oficial de ST](https://wiki.st.com/stm32mcu/wiki/Connectivity:Zigbee_stack_API_overview).

Fecha: 24 de noviembre de 2026.
