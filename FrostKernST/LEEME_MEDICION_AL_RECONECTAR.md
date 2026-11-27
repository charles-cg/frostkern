# FrostKern: medir y reportar al recuperar la conexion Zigbee

Actualizacion sobre la rama RTOS observada en el commit
`b9febbd6e88ecd26d624e55e796e11c6aa6cfee0`.

## Problema corregido

El Slave ya adquiria sensores aproximadamente cada segundo, pero recuperar la
red no obligaba a tomar una muestra posterior a esa recuperacion y enviarla.
El reporte de temperatura podia esperar al intervalo normal de hasta 60 s.
Ademas, al observar una nueva incorporacion del mismo IEEE, el Master seguia
teniendo marcada como valida la medicion anterior hasta recibir otra.

## Comportamiento nuevo

1. Al unirse, restaurar su conexion guardada o completar un rejoin, el Slave
   invalida los atributos ZCL anteriores y pide una adquisicion nueva.
2. SensorTask inicia una ventana COMPLETA de un segundo para contar pulsos de
   caudal, lee el ADC del NTC y publica ambos datos con una generacion interna.
3. Zigbee espera esa generacion exacta: no acepta una muestra capturada antes
   de la solicitud aunque se haya publicado despues.
4. Actualiza los atributos y pide un reporte forzado de temperatura y caudal,
   incluso si el valor coincide con el anterior. No espera el cambio minimo
   ni el intervalo periodico para ese envio de recuperacion.
5. Cada cluster espera su propio binding. Si el binding aparece tarde, toma
   otra muestra nueva antes de forzar el envio. No fuerza un valor vencido.
6. El Master, cuando observa que ese Slave se incorpora/reincorpora, marca
   sus mediciones como no disponibles hasta recibir los nuevos reportes.
   Conserva el registro IEEE y los datos del otro Slave.

Si falla la lectura nueva, se reporta medicion desconocida; no se reutiliza la
anterior como si fuera actual. Si falla la solicitud de envio, se reintenta.
El exito de la API indica que se solicito el reporte; confirma la recepcion
con los mensajes del Master y con las fechas nuevas en la XIAO.

La adquisicion necesita aproximadamente un segundo de ventana de caudal mas
el tiempo de ADC, tareas y radio. Con bindings guardados suele poder enviarse
tras esa adquisicion. En una primera asociacion tambien se espera que se
configure el binding; esa configuracion sigue agendada inicialmente a los 20 s.
No se promete recepcion instantanea ni un tiempo fijo ante fallos de radio.

## Instalar

Este ZIP es una actualizacion para tu proyecto existente, no un proyecto
independiente. Contiene solo los archivos nuevos o modificados.

1. Respalda tu proyecto actual fuera de su carpeta.
2. Copia la carpeta `FrostKernST` de este paquete sobre la carpeta del mismo
   nombre en la raiz de tu repositorio. Combina carpetas y reemplaza archivos.
   Si copias dentro de la carpeta del proyecto abierta en CubeIDE, pega el
   CONTENIDO de `FrostKernST`, evitando otra carpeta anidada del mismo nombre.
3. En CubeIDE pulsa F5/Refresh. Debe aparecer el nuevo archivo
   `STM32_WPAN/App/app_zigbee_refresh.c`.
4. Haz Clean y Build de Master y de Slave. Se actualizan los headers internos,
   por eso hay que reconstruir ambos perfiles completos.
5. Programa Master con `Master/FrostKern.elf` y CADA Slave con el mismo
   `Slave/FrostKern.elf`. Conserva SWD 1000 kHz y F8 si se detiene en main().
6. Conserva tu firmware actual de ESP32/XIAO y los cambios Firebase de tu
   companero. Este paquete no cambia ninguna de esas carpetas ni las claves.

La UART conserva los mismos tipos, longitudes y CRC. No cambia el formato de
los mensajes que consume ESP32. La generacion nueva pertenece solo al contrato
interno de muestras (`APP_DATA_CONTRACT_VERSION=2`); no se transmite por UART
ni se guarda como una medicion persistente.

## Prueba fisica recomendada

1. Mantiene Master y XIAO encendidos. Confirma T y Q validos y anota sus valores.
2. Apaga solo un Slave durante unos segundos.
3. Cambia la temperatura del sensor durante ese apagado, por ejemplo con la
   mano, o prepara un cambio de circulacion de agua para al encenderlo.
4. Enciende el Slave y espera que se una/restaure su red.
5. En su consola busca:

   ```text
   [ZB-FRESH SLAVE] new measurement requested, generation=...
   [ZB-FRESH SLAVE] temperature report requested from new measurement, generation=...
   [ZB-FRESH SLAVE] flow report requested from new measurement, generation=...
   ```

6. En Master, si recibe el aviso de reincorporacion, aparecera:

   ```text
   [ZB-FRESH MASTER] Slave 1 rejoined; waiting for new measurements
   ```

   Mientras espera puede emitir TEMP/FLOW no disponibles. Luego deben aparecer
   los nuevos `Temperature report from ...` y `Flow report from ...`.
7. En XIAO comprueba nuevas fechas `updated_at_ms`, edades pequenas y los
   valores actuales. El envio ocurre aunque los valores no hayan cambiado:
   repite una vez la prueba dejando el sensor en las mismas condiciones.
8. Con dos Slaves, confirma que la reincorporacion de uno no borre ni invalide
   los reportes del otro.

## Alcance y limites

- Esta correccion actua cuando el Slave completa una union/restauracion/rejoin.
  El Master invalida cache al observar esa reincorporacion. No agrega deteccion
  instantanea del instante en que se quitan las pilas: el timeout de datos
  vencidos de 180 s se conserva.
- Reiniciar solo el Master no siempre ocasiona un nuevo evento de enlace en
  un Slave que continuo encendido. En ese caso el Master arranca sin mediciones
  cacheadas y espera los reportes existentes; este parche no agrega un comando
  de consulta remota desde el Master.
- La recepcion de un nuevo reporte actualiza la edad en Master. El protocolo
  existente no transporta un timestamp del sensor ni una generacion para
  detectar paquetes antiguos retrasados dentro de la red; esa seria otra
  extension de protocolo. La correccion evita reutilizar el cache local previo
  al generar el reporte solicitado por la reconexion.
- Los intervalos periodicos, persistencia y soporte de dos Slaves se conservan.
  Solo se agrega muestreo/envio por el evento de recuperacion y sus reintentos.
- No se agrega el heartbeat del LED en este cambio.

## Validacion realizada

Se probaron en PC el SensorTask y el IPC reales, la generacion de muestra,
ventana completa de caudal, muestra anterior publicada tarde, solicitudes
superpuestas, binding tardio/independiente, valores iguales, fallo ADC, fallo
de escritura/envio y cola de avisos llena. En Master se ejercita el despacho
de reincorporacion real, invalidacion de un solo IEEE y las tramas UART reales.

Las regresiones existentes cubren temperatura/caudal, dos identidades,
persistencia con escrituras interrumpidas, recuperacion y CRC. Se compilan
los archivos afectados para Cortex-M33, ambos roles, con headers reales del SDK.
Las pruebas PC simulan HAL/radio. Falta el Clean/Build completo de tu CubeIDE
y la prueba fisica del conjunto con esta version.

```sh
python3 tests/run_all_tests.py
```
