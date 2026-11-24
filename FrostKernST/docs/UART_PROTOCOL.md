# FrostKern UART v1 — STM32 Master ↔ ESP32

Primera implementación del borrador UART del plan. Ambos extremos deben implementar este contrato. Este paquete contiene el lado STM32; no instala firmware en el ESP32 ni implementa Wi-Fi/backend.

## Enlace

- USART2, 115200 baudios, 8 bits, sin paridad, 1 stop, sin control de flujo.
- STM32 PA12 = TX hacia el ESP32; PA11 = RX desde el ESP32.
- En el esquema wba55_v1, WBA_TX llega a D7 del XIAO y WBA_RX llega a D6. Verificar las equivalencias GPIO del modelo de XIAO en su firmware; no usar números GPIO de otro modelo.
- PB7 / ESP32_EN activa el load switch TPS22918 del ESP32. BridgeTask lo pone en alto únicamente en Master. Esto requiere que exista la alimentación +5 V de la PCB.
- USART1 continúa reservado para los logs de ST. Los mensajes de log no se mezclan en USART2.
- Esta versión requiere CFG_LPM_LEVEL=0. No implementa recepción UART durante Stop ni el pin IRQ del ESP32.

## Trama

| Campo | Bytes | Definición |
|---|---:|---|
| SOF | 1 | 0xA5 |
| LEN | 1 | Longitud del PAYLOAD, de 0 a 64. No cuenta cabecera ni CRC. |
| TYPE | 1 | Tipo de mensaje. |
| SEQ | 1 | Secuencia de 0 a 255. |
| PAYLOAD | LEN | Según TYPE. |
| CRC16 | 2 | Menos significativo primero. |

CRC-16/CCITT-FALSE: polinomio 0x1021, inicial 0xFFFF, sin reflexión, XOR final 0. Se calcula sobre LEN, TYPE, SEQ y PAYLOAD; excluye SOF y el propio CRC. Vector de referencia: ASCII `123456789` → 0x29B1.

Tramas de prueba verificadas también con `binascii.crc_hqx` de Python:

```text
PING, SEQ=1:                 A5 00 02 01 DF BA
PERMIT_JOIN 180 s, SEQ=2:     A5 01 20 02 B4 8F F5
```

Los enteros de varios bytes se serializan en little-endian. La excepción es el IEEE: sus ocho bytes van del más significativo al menos significativo, igual que `AppDeviceId.ieee_address`. Las temperaturas son int16 con signo, en centésimas de °C. No se envían estructuras C crudas.

Una trama no debe tener pausas de 100 ms entre bytes. Tras ese intervalo el receptor descarta el fragmento. El parser valida longitud y CRC, ignora ruido y busca la siguiente trama. CRC no ofrece autenticación ni garantiza entrega.

Las publicaciones espontáneas usan el contador TX del Master. PONG, ACK y NACK repiten el SEQ de la solicitud. El SEQ de cabecera no es la secuencia de muestras del sensor.

## Tipos

| TYPE | Nombre | Dirección | PAYLOAD |
|---|---|---|---|
| 0x01 | HEARTBEAT | Master → ESP32 | 22 bytes, tabla siguiente. |
| 0x02 | PING | ESP32 → Master | Vacío. |
| 0x03 | PONG | Master → ESP32 | version:u8, network_started:u8, uptime_ms:u32. |
| 0x10 | TELEMETRY_TEMP | Master → ESP32 | 21 bytes, tabla siguiente. |
| 0x11 | DEVICE_JOINED | Master → ESP32 | version:u8, node_id:u8, IEEE:8 bytes. |
| 0x20 | PERMIT_JOIN | ESP32 → Master | seconds:u8, 0..254; 0 solicita cerrar el permiso. |
| 0x21 | SET_SETPOINT | ESP32 → Master | Reservado: siempre NACK/UNSUPPORTED en esta versión. |
| 0x7E | ACK | Master → ESP32 | request_type:u8, result_code:u8. |
| 0x7F | NACK | Master → ESP32 | request_type:u8, result_code:u8. |

HEARTBEAT se publica aproximadamente cada segundo. `network_started` indica que terminó la formación/unión inicial de Zigbee; no es una prueba de salud permanente del enlace.

DEVICE_JOINED se emite una vez por IEEE observado desde el arranque del Master, antes de su primera telemetría. Es un anuncio de registro local, no una detección continua de conexión. Los IDs 1/2 aún se asignan en RAM según los dispositivos observados y no persisten al reiniciar. La aplicación debe identificar la placa por IEEE.

## HEARTBEAT, LEN=22

| Offset PAYLOAD | Tipo | Campo |
|---:|---|---|
| 0 | u8 | version = 1 |
| 1 | u8 | network_started, 0/1 |
| 2 | u32 | uptime_ms del Master |
| 6 | u32 | rx_dropped, bytes descartados por cola RX llena |
| 10 | u32 | uart_errors, callbacks de error HAL |
| 14 | u32 | crc_errors, candidatos de trama rechazados por CRC |
| 18 | u32 | tx_errors, intentos TX fallidos/agotados |

Los contadores reinician con el Master. Tras un error RX/overflow se limpia la recepción parcial; el ESP32 puede necesitar reenviar su solicitud. No hay retransmisión automática de comandos desde el STM32.

## TELEMETRY_TEMP, LEN=21

Se publica aproximadamente cada segundo para los registros con IEEE conocido. Repetir el dato por UART no significa que se haya recibido una nueva lectura Zigbee.

| Offset PAYLOAD | Tipo | Campo |
|---:|---|---|
| 0 | u8 | version = 1 |
| 1 | u8 | node_id local, 1/2 |
| 2 | 8 bytes | IEEE, MSB primero |
| 10 | u8 | flags |
| 11 | i16 | temperature_centi_c; interpretar solo si flags bit 0 está activo |
| 13 | u32 | updated_at_ms del registro en el Master |
| 17 | u32 | age_ms desde esa actualización; 0xFFFFFFFF si no hubo reporte |

Flags: bit 0 TEMP_VALID, bit 1 TEMP_STALE, bit 2 REPORT_SEEN. Los demás son cero.

Con el código de IPC actual, `updated_at_ms` cambia al guardar cada reporte de temperatura. Una edad mayor a 180000 ms marca STALE y desactiva VALID. Una lectura desconocida también desactiva VALID. Si no es válida, los bytes del campo de temperatura se envían como cero de relleno: **no representan una lectura de 0 °C**.

La edad se calcula con resta uint32, válida a través del desbordamiento del contador mientras no transcurra un ciclo completo (~49.7 días) sin actualizar. Si se añaden otras fuentes que modifiquen el mismo `updated_at_ms`, habrá que mantener un timestamp separado para temperatura antes de usar ese campo como edad de la lectura.

Esta versión no inventa flujo, batería, estado de actuadores ni consignas confirmadas. Se añadirán cuando sus rutas Zigbee estén implementadas.

## ACK/NACK

| Código | Significado |
|---:|---|
| 0 | ACK: PERMIT_JOIN fue puesto en la cola de la tarea Zigbee. No confirma ejecución por radio. |
| 1 | NACK: longitud incorrecta. |
| 2 | NACK: valor fuera de rango. |
| 3 | NACK: red inicial no disponible o cola no creada. |
| 4 | NACK: no se pudo encolar el comando. |
| 5 | NACK: comando no soportado. |

El Master no responde a ACK/NACK para evitar bucles. Tramas con CRC inválido se descartan sin respuesta, porque TYPE/SEQ podrían estar dañados.

## Prueba independiente de Zigbee

Sin Slaves, debe aparecer HEARTBEAT y PING debe obtener PONG. Así puede probarse primero UART. Para ver temperatura válida por UART se requiere un Slave que realmente envíe reportes y cuyo dato válido llegue a `gSlaveStatus`.

El script `tools/uart_probe.py` genera tramas y decodifica capturas sin instalar paquetes:

```text
python tools/uart_probe.py ping --seq 1
python tools/uart_probe.py permit-join --seconds 180 --seq 2
python tools/uart_probe.py decode "PEGA_AQUI_LA_TRAMA_HEXADECIMAL"
```

`monitor` requiere `pyserial` y un puerto serie que transporte los bytes RAW de USART2, mediante un adaptador UART apropiado o un ESP32 programado como pasarela transparente. Un puerto USB que solo muestra logs del ESP32 o USART1 no sirve como acceso RAW a USART2.

```text
python -m pip install pyserial
python tools/uart_probe.py monitor --port COM7 --seconds 20
```

No conectar dos emisores TX al mismo RX durante la prueba. Usar una sola ruta hacia USART2.

## Referencias de implementación

- Borrador UART de `freertos-plan.html` del proyecto.
- HAL UART de ST, commit usado por STM32CubeWBA v1.10.0: https://github.com/STMicroelectronics/stm32wbaxx-hal-driver/tree/c36d9aabc6068051ed27a3a683a6b4f35f04d815
- USART2 y su IRQ ya existen en el proyecto. Se registran callbacks por handle cuando USE_HAL_UART_REGISTER_CALLBACKS=1; no se reemplazan callbacks globales de USART1.
