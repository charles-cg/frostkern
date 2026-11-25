# Implementacion y validacion de persistencia Zigbee

## Archivos de produccion

| Archivo | Cambio |
|---|---|
| Core/Inc/app_persist_store.h y Core/Src/app_persist_store.c | Journal Flash portatil, CRC32 de header/datos, commit final separado y registros anexados en tres paginas. |
| Core/Inc/app_persist_flash.h y Core/Src/app_persist_flash.c | Backend HAL STM32WBA55CG, programacion de 128 bits, alineacion, comprobacion de limites y geometria, verificacion de escritura y conservacion del estado de ICache. |
| Core/Inc/app_zb_persistence.h y Core/Src/app_zb_persistence.c | Notificaciones ZbPersistNotifyRegister, snapshots ZbPersistGet, restauracion ZbStartupPersist y registro IEEE del Master. |
| STM32_WPAN/App/app_zigbee.c | Inicializacion/restauracion antes de formar/unirse, espera de callback, reintentos de la red guardada, servicio periodico y recuperacion automatica. Se cancela el timer tambien al salir de la espera para cubrir callbacks inmediatos. |
| STM32_WPAN/App/app_zigbee_endpoint.c | Habilita persistencia, restaura registro/guarda al iniciar y muestra EPID/PAN. |
| STM32_WPAN/App/app_zigbee_endpoint.h | Retira la declaracion antigua sin implementacion APP_ZIGBEE_PersistenceStartup. |
| Core/Inc/app_zb_recovery.h y Core/Inc/app_network_config.h | Integra el parche de recuperacion acordado, compatible con la persistencia. |
| STM32WBA55CGUX_FLASH.ld | Reduce el area del programa a 1000 KiB y reserva los ultimos 24 KiB. |
| STM32WBA55CGUX_RAM.ld | Declara tambien los simbolos NVM sin alterar su distribucion previa del programa en RAM. Ese perfil no es el utilizado para las pruebas Master/Slave. |

`app_uart_bridge.c`, `sensor_task.c`, los parametros ADC/NTC, los pines, las
tramas UART y el arreglo de reportes con varios atributos se conservan del ZIP
aportado. Las configuraciones Master/Slave ya incluyen Core como directorio de
fuentes, por lo que el IDE incorpora los nuevos .c al refrescar y reconstruir.

## Memoria y formato

- MCU: STM32WBA55CG con 1 MiB de Flash y paginas de 8192 bytes.
- Programa: 0x08000000 hasta antes de 0x080FA000, 1000 KiB.
- NVM: 0x080FA000 hasta antes de 0x08100000, 24 KiB, paginas 125/126/127.
- Los simbolos del linker `__zb_nvm_start__` y `__zb_nvm_end__` deben coincidir
  con el backend. Si no coinciden, se registra un error y no se escribe Flash.
- No hay una seccion cargable de NVM en el ELF. Las paginas se escriben en
  ejecucion, no se inicializan como parte del programa.
- Un registro contiene header de 80 bytes, commit de 16 bytes y bloque ST opaco
  alineado a 16 bytes. Maximo snapshot: 8096 bytes. Si crece mas, se registra
  error, no se trunca ni se escribe fuera de la reserva.
- El header incluye version, generacion, longitud, CRC32, IEEE propio, rol,
  canal, identificador de formato SDK y los dos IEEE del registro Master.
- El bloque ST se guarda y se entrega a ZbStartupPersist SIN modificarlo.
- Se programa primero header/datos, se verifica y por ultimo se escribe el
  commit. Una escritura nueva no borra la pagina de la copia mas reciente.
  Si la pagina se llena, se rota a la siguiente. Al arrancar se busca la copia
  compatible mas reciente con CRC y commit validos.
- IEEE/rol/canal/formato se comprueban para evitar restaurar una imagen de otra
  placa o perfil. El formato de este firmware corresponde a WBA 1.10.0/R23.
- No se guardan muestras, validez, edades ni uptime del IPC. Los tiempos del
  arranque anterior no tienen significado como edad en el arranque nuevo.

Los datos no cambian la region de heap/stack del proyecto. El buffer temporal
de snapshot usa malloc y se libera al guardar; no se agrega un array permanente
de 8 KiB a la RAM. Si no hay memoria disponible se informa el error y se conserva
el snapshot anterior.

## Flujo de ejecucion

1. Tras crear pila/clusters y entrar a la tarea de red, se valida la reserva
   Flash, se busca snapshot y se registran notificaciones.
2. Sin imagen valida se mantiene el arranque normal ZbStartup.
3. Con imagen valida se usa ZbStartupPersist con el prototipo de SEIS argumentos
   de STM32CubeWBA 1.10.0, incluida la estructura ZbStartupT y su callback.
4. No se declara la red lista hasta que el callback confirma exito. Si falla,
   se reinicia el estado de la pila y se reintenta restaurar esa red, sin
   reemplazarla automaticamente por una red nueva.
5. Al completar el arranque se restaura el registro IEEE del Master y se pide
   guardar el estado actual. El timeout de espera se cancela tambien cuando el
   callback ya se completo antes de entrar a la espera.
6. El callback de persistencia solo marca cambios. La tarea Zigbee obtiene y
   escribe el snapshot, con un margen minimo de 5 s entre intentos periodicos.
   La primera captura al completar el arranque es inmediata. Se conservan los
   tiempos de notificacion/coalescencia predeterminados de la pila ST.
7. Un snapshot identico y el mismo registro no provocan una escritura nueva.
   Actualizar una temperatura UART no marca por si mismo el snapshot como sucio.
8. El registro IEEE cambiado tambien provoca una captura aunque aun no haya
   otra notificacion de ST. Un mutex ocupado pospone la captura, evitando guardar
   un registro parcialmente vacio.

La region contiene parametros y claves de red administrados por la pila ST;
no debe copiarse de una PCB a otra. Cambiar de canal/rol o del formato ABI de la
pila puede hacer incompatible una imagen; para actualizar la red deliberadamente
se debe limpiar la persistencia de todos los participantes correspondientes.

## Validacion realizada

Pruebas host con GCC, AddressSanitizer y UndefinedBehaviorSanitizer:

- CRC32 contra el vector 123456789 = 0xCBF43926.
- Fallo/interrupcion en cada operacion de la primera escritura, de un registro
  anexado y de una rotacion con snapshot del tamano maximo. El modelo simula una
  programacion parcial del quadword. Se conserva una copia anterior valida.
- Corrupcion de datos, limites, longitud malformada con CRC de header valido,
  identidad/rol/canal/ABI distintos y recuperacion de la copia anterior.
- Doscientas actualizaciones pequenas con reutilizacion de pagina y rotacion;
  una imagen identica no programa ni borra Flash.
- Modulo de persistencia con IPC real en Master y Slave: primera captura,
  notificacion diferida, retorno a red guardada, restauracion del registro,
  temperatura desconocida al reiniciar y reintento despues de fallo de captura.
- Funcion real de arranque extraida del proyecto: arranque frio, callback de
  restauracion, fallo temporal y reintento sin llamar a form/join nuevo.
- Funcion real de espera: callback inmediato y asincrono no dejan un timer que
  invalide posteriormente un arranque correcto.
- Recuperacion: umbral de 180 s, renovacion, dos Slaves, primer reporte,
  comandos manuales, mutex, sensor invalido y rollover del reloj.
- Regresiones ADC/caudal -> IPC -> atributo ZCL y callback Master con reporte
  multiple -> IPC -> UART -> decodificador Python.

Comandos, desde FrostKernST, en una computadora con Python 3 y GCC:

```text
python3 tests/run_persistence_tests.py
python3 tests/run_zb_recovery_tests.py
python3 tests/run_zigbee_diagnostics_tests.py
python3 tests/run_sensor_zigbee_tests.py
```

Se compilaron adicionalmente los tres modulos nuevos para Cortex-M33 con Arm GNU
13.3.Rel1. El backend se compilo con HAL/CMSIS reales de STM32CubeWBA v1.10.0;
el modulo Zigbee para Master y Slave se compilo con headers reales de la pila
de esa version y adaptadores RTOS/log para la comprobacion aislada. Se enlazo un
ELF ARM de prueba de distribucion de memoria con el linker real y una huella
equivalente a la RAM previa: los simbolos NVM quedaron en 0x080FA000/0x08100000,
sin segmento cargable que ocupe la reserva. Esto NO es un enlace del firmware
completo ni sustituye el build en STM32CubeIDE.

No se probo aqui RF, Flash fisica, cortes reales de alimentacion, ECC, autonomia
ni desgaste real. El modelo de fallo no es una garantia frente a cualquier
brownout. La prueba de PCB y la verificacion de los logs siguen siendo necesarias.

## Referencias de ST utilizadas

- Ejemplo de persistencia WBA55 en STM32CubeWBA v1.10.0:
  https://github.com/STMicroelectronics/STM32CubeWBA/tree/v1.10.0/Projects/NUCLEO-WBA55CG/Applications/Zigbee/Zigbee_PressMeas_Client_Coord_Persist
- API de persistencia:
  https://wiki.st.com/stm32mcu/wiki/Connectivity:Zigbee_stack_API_overview
- SDK STM32_WPAN fijado por WBA v1.10.0, commit
  `7e764835982f0a8b94fdd6b2b056d6e0afc4d1ba`, headers zigbee.h/zigbee.startup.h.
- Driver HAL fijado por WBA v1.10.0, commit
  `c36d9aabc6068051ed27a3a683a6b4f35f04d815`.
- AN5492, datos persistentes y memoria no volatil:
  https://www.st.com/resource/en/application_note/an5492-persistent-data-management-zigbee-and-nonvolatile-memory-in-stm32wb-series-stmicroelectronics.pdf
