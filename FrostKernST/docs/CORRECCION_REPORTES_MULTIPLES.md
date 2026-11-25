# Corrección: reporte de temperatura con atributos adicionales

## Causa confirmada por el log de la PCB

El Master recibió:

```text
[ZB-DIAG RX] src-mode=2 src-ep=17 dst-ep=17 attr=0x0000 type=0x29 len=6
[ZB-DIAG RX] dropped: attribute, type or length mismatch
[ZB-DIAG RX] src-mode=2 src-ep=17 dst-ep=17 attr=0xFFFE type=0x30 len=1
```

El valor SIGNED_16BIT de temperatura ocupa dos bytes. El parámetro de longitud del callback puede incluir bytes de los siguientes registros del reporte. En este caso, los seis bytes disponibles son compatibles con dos bytes de temperatura seguidos de cuatro bytes del registro AttributeReportingStatus: ID 0xFFFE (dos bytes), tipo 0x30 (un byte) y valor (un byte).

Nuestra condición `length != 2U` rechazaba el primer registro válido antes de interpretarlo. Es un error de validación del callback del Master. La radio entregó el reporte a la aplicación; el problema observado no es que el Slave no lo haya enviado.

El ejemplo de ST de cliente de temperatura valida que la longitud del valor del atributo no exceda la longitud disponible, en lugar de exigir que sean iguales. `ZbZclParseInteger` interpreta los bytes correspondientes al tipo recibido y no comprueba por sí mismo la longitud.

## Cambio

En `APP_ZIGBEE_TemperatureReport`:

- Se conserva la validación de punteros, endpoint, atributo de temperatura y tipo SIGNED_16BIT.
- Se rechazan valores truncados mediante `length < 2U`.
- Se aceptan buffers de dos o más bytes, leyendo únicamente el entero de 16 bits de temperatura.
- Los otros atributos, incluido 0xFFFE, se omiten con un mensaje informativo. No se interpretan como temperatura ni se encolan como medición.
- Se añade al arranque del Master la marca `[ZB-FIX] multi-attribute temperature reports enabled` cuando el diagnóstico está habilitado.

No se modifican SensorTask, ADC, fórmula NTC, rol Slave, intervalos de reporte ni datos UART. Esta corrección se ejecuta en la rama de compilación Master. El Slave de diagnóstico ya programado puede conservarse.

## Aplicar y probar

1. Conserva fuera del proyecto un respaldo de tu `app_zigbee_endpoint.c` actual.
2. Extrae el ZIP y copia el contenido de ACTUALIZAR dentro del `FrostKernST` activo. Combina carpetas y reemplaza el archivo indicado.
3. En STM32CubeIDE selecciona FrostKern y pulsa F5.
4. Activa Master, ejecuta Clean y Build y verifica que termine sin errores.
5. Conecta el ST-LINK a la PCB Master. Usa FrostKern Master, ejecutable `Master/FrostKern.elf`, Build Configuration Master y SWD a 1000 kHz.
6. Programa el Master; abre PuTTY de USART1 y pulsa Resume/F8 si el depurador queda detenido en main.
7. Comprueba la marca `[ZB-FIX] multi-attribute temperature reports enabled`.
8. Con Master ejecutándose y su ventana de admisión abierta, reinicia el Slave. Ambos deben conservar alimentación. No es necesario volver a programar el Slave.
9. Espera la unión y la aceptación de reportes. Mantén PuTTY en el Master.

Esperado, con números variables:

```text
[ZB-DIAG RX] src-mode=2 src-ep=17 dst-ep=17 attr=0x0000 type=0x29 len=6
[ZB-DIAG RX] queued: temperature=2358 valid=1
Temperature report from ...: 2358 centi-C
[ZB-DIAG RX] src-mode=2 src-ep=17 dst-ep=17 attr=0xFFFE type=0x30 len=1
[ZB-DIAG RX] ignored: non-temperature attribute=0xFFFE
```

2358 significa 23.58 °C. La recepción debe comprobarse en la PCB después de cargar el cambio. Si aparece RX queued pero no el mensaje de guardado, conserva cualquier mensaje de IPC/identificación que aparezca.

## Pruebas de regresión

Se reprodujo el rechazo con el callback anterior y un buffer de seis bytes compuesto por temperatura y AttributeReportingStatus. Con el callback corregido, la misma prueba pasa y el valor 2358 llega al IPC y al codificador UART. El registro 0xFFFE no modifica la muestra.

También se probaron temperatura negativa, valor desconocido 0x8000, reportes de un solo atributo y rechazo de buffers de cero/un byte, tipo erróneo y endpoints erróneos. Pasaron las pruebas existentes de ADC/IPC/atributo y recepción/IPC/UART, con ASan y UBSan en las pruebas de integración de PC.

Los bancos de prueba usan HAL/SDK/RTOS simulados. La compilación ARM con el SDK completo y la confirmación física tras corregir el firmware se realizan en el equipo de las PCB.

Desde FrostKernST, con Python y GCC de escritorio:

```bash
python tests/run_sensor_zigbee_tests.py
python tests/run_zigbee_diagnostics_tests.py
```

El archivo actualizado `tests/integration/test_master_reports_uart.c` queda incluido. El resto de los bancos de prueba ya forma parte del proyecto y del paquete de diagnóstico v1.

## Referencias

- SDK de ST: `zcl.h`, firma del callback `ZbZclClusterT.report` y `ZbZclParseInteger`.
- SDK de ST: `zcl.enum.h`, `ZCL_GLOBAL_ATTR_REPORTING_STATUS = 0xFFFE`.
- Ejemplo cliente de temperatura de ST: `APP_ZIGBEE_TempMeasServerReport`, validación `iAttrLen <= iDataInputLength`.
- [API oficial de ST: ZbZclParseInteger](https://wiki.st.com/stm32mcu/wiki/Connectivity:Zigbee_stack_API_overview).

Fecha: 24 de noviembre de 2026.
