# FrostKernST Avanzado: persistencia Zigbee Master y Slave

Este proyecto integra guardado y restauracion de la red Zigbee en la Flash del
STM32WBA55CG. Incluye tambien la recuperacion automatica acordada anteriormente.
La extension actual agrega caudal y reconexion en caliente; lee primero
`LEEME_TEMPERATURA_CAUDAL.md`. Conserva el codigo del sensor, la correccion de reportes multiples, la UART y
la secuencia PB5 -> espera 50 ms -> PB7 que ya probaron.

## Instalar y compilar

1. Guarda un respaldo de tu carpeta actual `FrostKernST` fuera del proyecto.
2. Extrae el ZIP. Copia TODO el contenido de su carpeta `FrostKernST` sobre la
   carpeta `FrostKernST` que ya compila en tu computadora. Combina las carpetas
   y reemplaza los archivos. **Incluye los archivos `.ld` de la raiz**:
   no basta con copiar Core y STM32_WPAN.
3. En STM32CubeIDE selecciona el proyecto y pulsa F5 (Refresh).
4. Selecciona Master, haz Clean y Build. Repite con Slave. Usa las mismas
   versiones y rutas de ST que ya tenias funcionando (WBA 1.10.0 y FreeRTOS).
5. Programa la PCB Master con `Master/FrostKern.elf` y la PCB Slave con
   `Slave/FrostKern.elf`. Los dos firmwares cambian. Los perfiles mantienen
   SWD a 1000 kHz; pulsa F8 si Debug se detiene en main().
6. Para esta version con caudal, carga el receptor actualizado de la carpeta
   `ESP32/FrostKern_UART_Receiver` del paquete. Para el monitor USB
   conserva la conexion externa ya probada: Master PA12 -> XIAO D7 y GND -> GND,
   XIAO fuera del zocalo alimentada por USB, Master alimentado con sus pilas.

El ZIP actualizado omite las carpetas de resultados generados Master, Slave y
Debug y las caches de Python. El IDE las recrea al compilar. No contiene un ELF
nuevo listo para programar: hay que generar ambos en el IDE. No se modificaron
los proyectos adicionales que venian dentro de `frostkern`.

No apliques despues los paquetes antiguos de recuperacion o diagnostico sobre
estos archivos: podrian reemplazar la integracion actual.

## Primer arranque, sin red guardada

1. Enciende el Master y despues el Slave, dentro de la ventana inicial de 180 s.
2. En PuTTY, en la consola diagnostica de cada STM32, aparecera:

   ```text
   [ZB-PERSIST] no valid saved network; normal form/join
   ```

3. Tras formar/unirse, debe aparecer en CADA placa:

   ```text
   [ZB-PERSIST] saved: ... bytes, generation=...
   [ZB-PERSIST] network identity: EPID ..., PAN 0x....
   ```

4. Espera la configuracion de reportes y que la XIAO vuelva a mostrar
   `valid=1 stale=0 report_seen=1`. Deja ambas placas encendidas unos 30 s mas
   para dar margen al guardado de cambios de claves, bindings y reportes.
   Conserva el EPID/PAN para comparar despues del reinicio. No cambies sus valores.

**Si no aparece un guardado satisfactorio, no se ha comprobado la persistencia.**
Conserva los errores de PuTTY: longitud de snapshot, memoria, geometria Flash,
registro de notificaciones o fallo de escritura no deben confundirse con exito.

## Prueba A: apagar y encender solo el Slave

1. Mantiene el Master encendido. Para diferenciar restauracion de una nueva
   alta, puedes esperar que termine la ventana inicial antes de esta prueba.
2. Apaga el Slave unos segundos y vuelve a encenderlo, sin programarlo otra vez.
3. En su PuTTY debes ver:

   ```text
   [ZB-PERSIST] saved network found: ...
   [ZB-PERSIST] restoring saved network
   [ZB-PERSIST] saved network restored successfully
   ```

4. Comprueba que su EPID/PAN coincide con el de antes y con el Master. Espera
   los reportes: XIAO debe volver a mostrar temperatura valida y una edad pequena.

Si el Master esta apagado, el Slave puede fallar al restaurar y reintentar.
La red guardada se conserva; no vuelve silenciosamente a una asociacion nueva.
Las pausas existentes son 10, 60 y 300 s, ademas del tiempo del propio intento.

## Prueba B: apagar y encender las dos placas

1. Confirma antes el guardado satisfactorio en ambas placas.
2. Apaga Master y Slave. Enciende el Master y despues el Slave.
3. Ambas deben indicar que encontraron y restauraron su red guardada.
4. Compara EPID/PAN antes y despues. El Master debe conservar su red y el
   registro de los IEEE de los Slaves.
5. En XIAO la temperatura puede mostrarse inicialmente como no disponible.
   Eso es correcto: no se restauran temperaturas viejas como si fueran nuevas.
   Espera un reporte real para volver a `valid=1 report_seen=1`.

Restaurar la red no obliga a conservar una direccion corta para siempre; la
identidad permanente de la placa sigue siendo su IEEE.

## Persistencia y recuperacion automatica

- Persistencia: conserva el bloque opaco de la pila ST y los IEEE del registro
  del Master a traves de un apagado. Usa las APIs de ST, no estructuras de la
  pila copiadas o modificadas manualmente.
- Recuperacion: si un Slave conocido deja de reportar durante mas de 180 s,
  solicita abrir la union por 180 s y la renueva cada 60 s mientras falten
  reportes. Solicita cerrarla cuando todos los conocidos vuelven a reportar.
- La reapertura es una ventana normal de admision, no una lista exclusiva de
  IEEE. Un cierre UART manual explicito pausa la recuperacion hasta una nueva
  apertura manual o un reinicio del Master.
- Restaurar un Slave con datos guardados y recuperar una placa que no conserva
  su red son mecanismos diferentes. Los logs permiten comprobar cual se uso.

## Borrar la red guardada deliberadamente

Para rehacer la red desde cero, limpia la persistencia de TODOS los participantes
que quieras volver a asociar. No basta con borrar solo el Master y dejar los
Slaves intentando recuperar la red anterior.

En `Core/Inc/app_zb_persistence.h` existe:

```c
#define APP_ZB_PERSIST_ERASE_ON_BOOT 0
```

Para una limpieza deliberada, cambia a 1, compila/programa la placa y deja que
arranque para ejecutar la limpieza. Despues vuelve a 0 y compila/programa otra
vez antes de probar la conservacion entre reinicios. Con 1 borraria la red en
cada arranque. La operacion elimina solo las tres paginas reservadas.

Un Full Chip Erase tambien elimina la red guardada. Para comprobar persistencia
usa apagado/encendido o reset, sin borrar Flash ni volver a programar entre cada
paso. La programacion por sectores del nuevo firmware puede conservar el area
NVM; no se debe asumir que cualquier herramienta o configuracion de borrado lo haga.

## Que se modifico y que se probo

Los detalles, la zona Flash y el alcance de validacion estan en
`docs/PERSISTENCIA_ZIGBEE.md`. Pasaron las pruebas en computadora y la compilacion
aislada Cortex-M33 de los modulos nuevos con headers reales de ST.
**Falta el build completo en tu STM32CubeIDE y la comprobacion fisica de RF/Flash.**
