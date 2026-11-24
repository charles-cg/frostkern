# Integrar UART en FrostKernST

> Actualizacion: este proyecto ya integra SensorTask con el estado compartido. Consulta `INTEGRACION_ADC_ZIGBEE_UART.md` para los cambios actuales y la prueba de reportes.


El paquete agrega cuatro archivos de firmware. La integración requiere un include y una llamada en tu `app_freertos.c`. Conserva tu código Zigbee actual.

## 1. Copiar los archivos

1. Extrae el ZIP en una carpeta temporal.
2. Abre la carpeta `FrostKernST` extraída. Contiene `Core`, `docs`, `tests` y `tools`.
3. Copia esas cuatro carpetas.
4. Abre la carpeta del proyecto que estás usando en el IDE. Confírmala con clic derecho al proyecto → Properties → Resource → Location. En tu ubicación original era:
   `C:\Users\ximef\Desktop\5to semestre\Embebidos Avanzados\frostkern\FrostKernST`
5. Pega allí y combina carpetas. El destino correcto contiene directamente `Core`, `STM32_WPAN`, `.project` y `.cproject`.

Los archivos nuevos son:

```text
Core/Inc/app_uart_protocol.h
Core/Inc/app_uart_bridge.h
Core/Src/app_uart_protocol.c
Core/Src/app_uart_bridge.c
```

`docs`, `tests` y `tools` contienen documentación, pruebas de PC y una herramienta de diagnóstico. Las pruebas C están fuera de `Core`; no moverlas a las carpetas de fuentes del firmware. Las source entries del proyecto revisado solo compilan Core/Common/System/STM32_WPAN/Utilities/Middlewares/Drivers/RTOS2.

## 2. Agregar el include

En STM32CubeIDE, selecciona el proyecto y pulsa F5. Abre `Core/Src/app_freertos.c`.

Busca `/* USER CODE BEGIN Includes */` y agrega dentro de ese bloque:

```c
#include "app_uart_bridge.h"
```

Conserva los includes existentes, especialmente `app_ipc.h`.

## 3. Iniciar la tarea

Dentro de `MX_FREERTOS_Init()`, busca:

```c
/* USER CODE BEGIN RTOS_THREADS */
```

Agrega dentro de ese bloque, una sola vez:

```c
AppUartBridge_Start();
```

Conserva `AppIpc_Init();` en el bloque `USER CODE BEGIN Init`, al principio de `MX_FREERTOS_Init()`. Debe ejecutarse antes de `AppUartBridge_Start()`.

La función crea `BridgeTask` solo en Master. En Slave no crea tareas ni activa el ESP32. No hay que añadir funciones de callback a `main.c` ni modificar `USART2_IRQHandler()`; ya llama a `HAL_UART_IRQHandler(&huart2)`.

USART2 se inicializa dentro de BridgeTask mediante la función existente `MX_USART2_UART_Init()`. BridgeTask tiene prioridad 20, pila de 1536 bytes y una cola RX de 256 bytes. USART2 debe pertenecer únicamente a este módulo.

## 4. Confirmar requisitos existentes

En los archivos revisados ya estaban:

- `app_task_config.h`: APP_TASK_PRIO_BRIDGE=20, APP_TASK_STACK_BRIDGE=1536 y APP_STACK_DEPTH.
- `app_ipc.h/.c`: AppIpc_ReadSlaveStatus(), AppIpc_NetworkStarted(), AppIpc_RequestPermitJoin() y qZbCommand, incorporados durante los pasos previos.
- `app_conf.h`: CFG_LPM_LEVEL=0 para esta fase de prueba.
- `stm32wbaxx_hal_conf.h`: USE_HAL_UART_REGISTER_CALLBACKS=1.
- `FreeRTOSConfig.h`: configUSE_TASK_NOTIFICATIONS=1 e INCLUDE_vTaskDelete=1.
- USART2: PA12 TX, PA11 RX, 115200, 8N1. Su IRQ usa prioridad 8; max-syscall de FreeRTOS es 5.
- `main.h`: declara MX_USART2_UART_Init(), ESP32_EN_Pin y ESP32_EN_GPIO_Port.

El módulo genera un error explícito de compilación si se cambia CFG_LPM_LEVEL o se deshabilita el registro de callbacks. No regenerar el proyecto desde el `.ioc` para integrar estos archivos.

## 5. Compilar los dos perfiles

1. Guarda todo.
2. Activa Master y ejecuta Build Project. Deben compilar app_uart_protocol.c y app_uart_bridge.c.
3. Activa Slave y ejecuta Build Project. Este perfil no arranca BridgeTask.
4. Si las fuentes nuevas no aparecen, pulsa F5 en el proyecto y verifica que no estén excluidas de Master/Slave. Luego limpia el perfil afectado y compila de nuevo.
5. Comparte el primer error real si falla. No reemplazar tu `.cproject` por el de otra computadora para resolverlo.

No se ejecutó una compilación ARM de tu proyecto actualizado desde aquí: las compilaciones Master y Slave en tu IDE siguen siendo necesarias.

## 6. Subir junto con Zigbee a GitHub

Si el proyecto del IDE sigue en la carpeta original y el clon de GitHub está en otra ubicación, copia de nuevo las fuentes actualizadas al clon, como en la subida anterior. Incluye Core, STM32_WPAN, docs, tests y tools; así se incluyen también los cambios Zigbee hechos desde el último push.

En GitHub Desktop:

1. Confirma repositorio `frostkern`, rama `RTOS`.
2. Revisa que los cambios incluyan los cuatro archivos UART, el cambio en app_freertos.c y los cambios Zigbee/IPC recientes.
3. Excluye salidas Debug, Master, Slave, Release y el workspace `.metadata`.
4. Summary sugerido: `Integra puente UART del Master y reportes de temperatura Zigbee`.
5. Description sugerida: `Agrega protocolo UART con CRC16, heartbeat, ping/pong, telemetría de temperatura y solicitud de permit join. Validación física pendiente. SET_SETPOINT aún no soportado.`
6. Commit to RTOS y después Push origin.

El ZIP por sí solo no se sube a GitHub: deben subirse sus archivos integrados en el repositorio.

## 7. Qué probar en las placas

1. Carga el perfil Master en la placa elegida como Master.
2. Confirma HEARTBEAT por USART2. Esto no requiere Slaves ni sensores.
3. Envía PING y verifica PONG con el mismo SEQ. Ambos extremos deben implementar este formato binario. El firmware del ESP32 todavía debe implementarlo o servir como pasarela de bytes.
4. Cuando arranque la red Zigbee, envía PERMIT_JOIN y comprueba ACK de encolado.
5. Arranca un Slave: observa DEVICE_JOINED y telemetría de su IEEE. Antes de tener medición válida debe tener TEMP_VALID=0.
6. Con el trabajo de sensores/IPC integrado, compara la temperatura del Slave, gSlaveStatus y la trama UART.
7. Si dejan de llegar reportes, tras más de 180 segundos la telemetría debe marcar TEMP_STALE y quitar TEMP_VALID. UART repite el último registro cada segundo; no fabrica muestras nuevas.

Pendientes de Zigbee: pruebas reales (incluida la validación de los reintentos de configuración de reportes), asociación IEEE/ID persistente, recuperación de conexión y los demás clusters/comandos. La UART no declara completados esos pendientes.

## Pruebas incluidas para PC

Se ejecutaron el parser/CRC y el bridge con HAL/FreeRTOS simulados, usando GCC y sanitizadores de memoria/comportamiento indefinido. Se comprobó también que app_uart_bridge.c compila en ROLE_SLAVE como función vacía. Estas pruebas no equivalen a ejecutar el STM32.

Para repetir en Linux con GCC, desde FrostKernST y con los headers actuales del proyecto presentes:

```sh
gcc -std=c11 -Wall -Wextra -Werror -pedantic -ICore/Inc Core/Src/app_uart_protocol.c tests/test_uart_protocol.c -o /tmp/test_protocol
/tmp/test_protocol
gcc -std=c11 -Wall -Wextra -Werror -pedantic -DROLE_MASTER=1 -Itests/fakes -ICore/Inc Core/Src/app_uart_protocol.c tests/test_uart_bridge.c -o /tmp/test_bridge
/tmp/test_bridge
```
