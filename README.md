# FrostKern — descarga y compilación

Firmware para **STM32WBA55CGUx**, con FreeRTOS, adquisición del sensor YF-B7, comunicación Zigbee y puente UART en el Master.

Esta guía describe la preparación en **Windows** de la rama **`RTOS`**. El proyecto de STM32CubeIDE está en **`FrostKernST`** y aparece en el IDE con el nombre **`FrostKern`**.

> Los perfiles Master y Slave se compilaron en el equipo del proyecto. En una computadora nueva hay que instalar las mismas dependencias y adaptar las rutas antes de compilar. Las pruebas de compilación no sustituyen las pruebas de sensores y comunicación en las PCB.

## 1. Herramientas y versiones

| Componente | Versión de referencia | Uso |
|---|---|---|
| STM32CubeIDE, edición Eclipse | **2.2.0** | Importar, compilar y depurar el firmware. |
| STM32CubeMX | **6.18.1** | Administrar paquetes ST; es la versión registrada en el `.ioc`. No hace falta generar código para compilar. |
| STM32CubeWBA | **1.10.0** | HAL, CMSIS, utilidades y bibliotecas Zigbee de STM32WBA. |
| STMicroelectronics X-CUBE-FREERTOS | **1.3.1** | Kernel FreeRTOS, puerto Cortex-M33 y CMSIS-RTOS2. |
| GitHub Desktop o Git | — | Clonar y actualizar la rama `RTOS`. También se puede descargar el ZIP. |

Utiliza el compilador y las herramientas de construcción suministrados con STM32CubeIDE. No necesitas instalar otro GCC ARM para seguir esta guía. Python no es necesario para compilar el firmware; se utiliza en las herramientas y pruebas opcionales.

No se necesita conectar la PCB ni el ST-LINK para **compilar**.

Descargas oficiales:

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [STM32CubeMX](https://www.st.com/en/development-tools/stm32cubemx.html)
- [STM32CubeWBA](https://www.st.com/en/embedded-software/stm32cubewba.html)
- [X-CUBE-FREERTOS](https://www.st.com/en/embedded-software/x-cube-freertos.html)
- [GitHub Desktop](https://desktop.github.com/)

## 2. Descargar la rama correcta

Repositorio: [charles-cg/frostkern, rama RTOS](https://github.com/charles-cg/frostkern/tree/RTOS).

### Opción A: GitHub Desktop

1. Selecciona **File → Clone repository → URL**.
2. Introduce `https://github.com/charles-cg/frostkern.git`.
3. Elige una carpeta local, por ejemplo `C:\dev\frostkern`, y pulsa **Clone**.
4. En **Current branch**, selecciona **RTOS**.
5. Pulsa **Fetch origin** y, si aparece, **Pull origin**.
6. Usa **Repository → Show in Explorer** para abrir la carpeta descargada.

### Opción B: Git en una terminal

Ejecuta esto dentro de la carpeta donde quieras crear el repositorio:

```bash
git clone --branch RTOS --single-branch https://github.com/charles-cg/frostkern.git
cd frostkern
```

### Opción C: ZIP

1. Abre el enlace de la rama `RTOS`.
2. Comprueba que el selector de rama diga **RTOS**.
3. Pulsa **Code → Download ZIP**.
4. Extrae el ZIP por completo antes de abrir el proyecto.

En cualquiera de las opciones, localiza estos archivos:

- `FrostKernST/.project`
- `FrostKernST/.cproject`
- `FrostKernST/FrostKern.ioc`
- `FrostKernST/Core/Src/app_freertos.c`

**No importes `firmware`, `frostkernApp` ni una carpeta comprimida como proyecto de STM32CubeIDE.** Para esta guía se importa únicamente `FrostKernST`.

## 3. Instalar los paquetes ST

1. Instala y abre STM32CubeMX.
2. Ve a **Help → Manage embedded software packages**, o **Install/Remove** en la pantalla inicial.
3. Instala **STM32CubeWBA 1.10.0**.
4. Instala **STMicroelectronics X-CUBE-FREERTOS 1.3.1** en el administrador de paquetes.
5. Si utilizas descargas locales oficiales, usa **From Local…** para instalar el archivo `.zip` del paquete MCU o el `.pack` de FreeRTOS.
6. Acepta las licencias y espera a que termine la instalación.

Selecciona las versiones indicadas, aunque exista una más reciente. No basta con cambiar el nombre de una carpeta de otra versión.

### Localizar el repositorio de paquetes

La instalación habitual está en:

```text
C:\Users\TU_USUARIO\STM32Cube\Repository
```

Las dependencias que espera el proyecto son:

| Paquete | Carpeta esperada dentro de `Repository` |
|---|---|
| STM32CubeWBA | `STM32Cube_FW_WBA_V1.10.0` |
| FreeRTOS | `Packs\STMicroelectronics\X-CUBE-FREERTOS\1.3.1` |

Comprueba que existan, por ejemplo:

```text
STM32Cube_FW_WBA_V1.10.0\Utilities\trace\adv_trace\stm32_adv_trace.c
STM32Cube_FW_WBA_V1.10.0\Drivers\STM32WBAxx_HAL_Driver\Inc\stm32wbaxx_hal.h
STM32Cube_FW_WBA_V1.10.0\Middlewares\ST\STM32_WPAN\zigbee\stack\ZigBeeProR23_FFD.a
STM32Cube_FW_WBA_V1.10.0\Middlewares\ST\STM32_WPAN\zigbee\stack\ZigBeeProR23_RFD.a
Packs\STMicroelectronics\X-CUBE-FREERTOS\1.3.1\Middlewares\Third_Party\FreeRTOS\Source\include\FreeRTOS.h
```

Las rutas anteriores son relativas a la carpeta `Repository`. Si falta alguno de estos archivos, revisa la instalación antes de continuar. Evita una extracción con dos carpetas `STM32Cube_FW_WBA_V1.10.0` anidadas.

## 4. Adaptar las rutas a tu computadora

**Haz este paso antes de importar el proyecto. Cierra STM32CubeIDE si ya lo habías abierto.**

La configuración original contiene rutas de `C:/Users/ximef` y enlaces `PARENT-5-PROJECT_LOC`. Estos últimos dependen de la ubicación del proyecto: clonar en otra carpeta puede romperlos aunque el SDK esté instalado.

Los siguientes cambios afectan solamente las ubicaciones de las dependencias. Conserva los perfiles, sus identificadores, las macros y las bibliotecas existentes.

1. Guarda una copia de `.project` y `.cproject` fuera del repositorio, para poder recuperar la configuración original.
2. Abre los originales en un editor de texto como VS Code o Notepad++.
3. Identifica la ruta **real** de tu carpeta `Repository`.

En los ejemplos se usa `C:/Users/joaquin/STM32Cube/Repository`. **Sustitúyela por tu ruta real**; `joaquin` es solo un ejemplo. Escribe barras `/`, sin una barra adicional al final. Utiliza una ubicación sin caracteres especiales como `&`, porque estos archivos son XML.

### 4.1. Rutas de compilación: `.cproject`

En `FrostKernST/.cproject`, utiliza **Reemplazar todo**:

| Buscar exactamente | Reemplazar por, usando tu ruta real |
|---|---|
| `C:/Users/ximef/STM32Cube/Repository` | `C:/Users/joaquin/STM32Cube/Repository` |

Haz el reemplazo en el archivo completo, para actualizar las rutas de los distintos perfiles, los encabezados y las bibliotecas del enlazador. Guarda el archivo.

### 4.2. Archivos enlazados: `.project`

En `FrostKernST/.project`, realiza estos **tres reemplazos globales**, en este orden:

| Buscar exactamente | Reemplazar por |
|---|---|
| `PARENT-5-PROJECT_LOC/STM32Cube/Repository` | Tu ruta real, por ejemplo `C:/Users/joaquin/STM32Cube/Repository` |
| `<locationURI>` | `<location>` |
| `</locationURI>` | `</location>` |

Así cada enlace queda como una ruta local absoluta, independiente de cuántas carpetas haya entre el proyecto y tu usuario. Ejemplo:

```xml
<location>C:/Users/joaquin/STM32Cube/Repository/STM32Cube_FW_WBA_V1.10.0/Utilities/trace/adv_trace/stm32_adv_trace.c</location>
```

Guarda el archivo con su nombre original **`.project`**, sin añadir `.txt` y conservando la codificación UTF-8.

### 4.3. Comprobación antes de importar

- En `.cproject`, ya no deben quedar las rutas antiguas `C:/Users/ximef/STM32Cube/Repository`, salvo que esa sea realmente tu instalación.
- En `.project`, ya no debe aparecer `PARENT-5-PROJECT_LOC`.
- Las rutas de ambos archivos deben apuntar a los mismos paquetes instalados.
- Conserva los sufijos `STM32Cube_FW_WBA_V1.10.0` y `Packs/STMicroelectronics/X-CUBE-FREERTOS/1.3.1`.

Si tus dos paquetes están instalados bajo raíces distintas, adapta cada prefijo por separado a su ubicación real. No dejes uno de los paquetes apuntando a la instalación anterior.

## 5. Importar en STM32CubeIDE

1. Abre STM32CubeIDE.
2. Elige un workspace nuevo, por ejemplo `C:\STM32Workspaces\FrostKern`. El workspace guarda ajustes del IDE; el código puede permanecer en `C:\dev\frostkern`.
3. Selecciona **File → Import… → General → Existing Projects into Workspace → Next**.
4. En **Select root directory**, elige la carpeta **`FrostKernST`** que contiene `.project`.
5. Marca el proyecto **FrostKern**.
6. Deja desmarcado **Copy projects into workspace** para trabajar sobre la copia del repositorio.
7. Deja desmarcado **Close newly imported projects upon completion**.
8. Pulsa **Finish** y espera a que el IDE termine de importar e indexar.
9. Selecciona **FrostKern** en Project Explorer y presiona **F5**.

Algunas carpetas de ST aparecen como recursos enlazados. Sus fuentes se encuentran en los paquetes instalados; no es necesario copiarlas al repositorio.

**No necesitas abrir `FrostKern.ioc`, migrarlo ni pulsar Generate Code para importar o compilar.** Hay adaptaciones manuales del firmware; regenerar sin revisar los cambios puede sobrescribirlas.

## 6. Compilar Master y Slave

Los dos perfiles se generan desde el mismo proyecto:

| Perfil | Macro del rol | Biblioteca Zigbee | Ejecutable esperado |
|---|---|---|---|
| **Master** | `ROLE_MASTER=1` | `ZigBeeProR23_FFD.a` | `FrostKernST/Master/FrostKern.elf` |
| **Slave** | `ROLE_SLAVE=1` | `ZigBeeProR23_RFD.a` | `FrostKernST/Slave/FrostKern.elf` |

Conserva estas opciones ya configuradas. Para esta integración, utiliza **Master** y **Slave**; no selecciones **Debug** o **Release**.

### 6.1. Compilar Master

1. Haz clic derecho sobre **FrostKern** en Project Explorer.
2. Selecciona **Build Configurations → Set Active → Master**.
3. Si el IDE pregunta por reconstruir el índice, pulsa **Rebuild Index**.
4. Guarda los archivos con **File → Save All**.
5. Ve a **Project → Clean…**, selecciona únicamente **FrostKern** y limpia el proyecto. Si aparece **Start a build immediately**, puedes desmarcarlo para ejecutar el siguiente paso de forma explícita.
6. Haz clic derecho sobre **FrostKern → Build Project**.
7. En la consola, verifica que diga **Build of configuration Master** y termine con **Build Finished. 0 errors**. Revisa cualquier advertencia que aparezca.
8. Comprueba que se haya generado **`Master/FrostKern.elf`**.

### 6.2. Compilar Slave

1. Cambia a **Build Configurations → Set Active → Slave**.
2. Acepta **Rebuild Index** si se solicita.
3. Repite **Project → Clean…** y después **Build Project**.
4. Verifica **Build of configuration Slave**, **Build Finished. 0 errors** y el archivo **`Slave/FrostKern.elf`**.

En la primera compilación de cada perfil, o después de cambiar rutas, utiliza **Clean → Build**. Para cambios normales del código en el mismo perfil, suele bastar **Build Project**.

Si el equipo tiene poca memoria y la compilación paralela falla, reduce temporalmente los trabajos paralelos a 2–4 en **Properties → C/C++ Build → Behavior**, dentro de las opciones de compilación paralela.

## 7. Si aparece un error

Lee el **primer error concreto de la consola**, por encima de `make ... exit code 2`. El código 2 es el resumen del fallo, no su causa.

| Mensaje o situación | Qué comprobar |
|---|---|
| `No rule to make target ... stm32_adv_trace.c` u otro archivo ST | Que el archivo exista en el SDK y que su enlace en `.project` apunte a esa instalación. Revisa el paso 4.2; después refresca, limpia y compila. |
| `FreeRTOS.h: No such file or directory` o falta `cmsis_os2.c` | Instala X-CUBE-FREERTOS **1.3.1** y revisa tanto los includes de `.cproject` como los enlaces de `.project`. |
| `stm32wbaxx_hal.h: No such file or directory` | Comprueba STM32CubeWBA **1.10.0** y las rutas de includes en `.cproject`. |
| `cannot find -l:ZigBeeProR23_FFD.a` o `RFD.a` | Comprueba la biblioteca correspondiente dentro de `Middlewares/ST/STM32_WPAN/zigbee/stack` y sus rutas de búsqueda en `.cproject`. |
| Error que solicita definir un rol | Activa **Master** o **Slave**. Conserva solamente la macro del rol correspondiente en cada perfil. |
| `arm-none-eabi-gcc` o `make` no encontrados | Revisa la instalación de STM32CubeIDE y su selección de herramientas en **Properties → C/C++ Build → Settings → MCU/MPU Toolchain**. Utiliza las herramientas incluidas con el IDE. |
| `A project 'FrostKern' already exists in the workspace` | Usa un workspace nuevo para esta copia. Puedes comprobar la ubicación del proyecto existente en **Properties → Resource → Location**. |
| El editor marca símbolos en rojo, pero Build termina correctamente | Refresca con F5 y utiliza **Index → Rebuild** desde el menú contextual del proyecto. El índice del editor y el compilador son procesos distintos. |
| Aparecen errores nuevos después de generar código desde `.ioc` | Revisa el diff y recupera las adaptaciones que se hayan sobrescrito. Compilar la versión del repositorio no requiere regenerarla. |

Si la compilación aún utiliza una ruta antigua después de editar los archivos de configuración, cierra el IDE, verifica que modificaste el `.project` y `.cproject` de la copia importada, vuelve a abrir, refresca y ejecuta Clean → Build. Si los enlaces siguen desactualizados en el workspace, importa esa misma copia en un workspace nuevo.

## 8. Depuración: elegir el ELF correcto

Este paso es posterior a la compilación y requiere la PCB alimentada y el ST-LINK conectado correctamente.

Abre **Run → Debug Configurations… → STM32 C/C++ Application**. Selecciona cada configuración y comprueba:

| Configuración | Project | C/C++ Application | Build Configuration |
|---|---|---|---|
| FrostKern Master | `FrostKern` | `Master/FrostKern.elf` | `Master` |
| FrostKern Slave | `FrostKern` | `Slave/FrostKern.elf` | `Slave` |

Pulsa **Apply** al guardar los cambios. Evita cargar accidentalmente un ejecutable antiguo de `Debug`.

En **Debugger**, el proyecto utiliza **ST-LINK (ST-LINK GDB server)** y **SWD**. El ST-LINK-V3MINIE externo utiliza esta misma selección; no necesita una casilla especial que diga «externo». **Autostart local GDB server** indica que el servidor se ejecuta en tu computadora.

Si hay varios ST-LINK conectados, selecciona su número de serie mediante **ST-LINK S/N → Scan**. Las configuraciones guardadas también contienen una ruta antigua de log del servidor ST-LINK: si activas la escritura de ese log, cambia su destino a una carpeta local existente en las opciones del depurador.

## 9. Comprobaciones posteriores y documentación

Una compilación correcta confirma que el firmware puede construirse con esas dependencias. La adquisición real del ADC, la calibración del NTC, los reportes Zigbee y la conexión UART deben comprobarse en las placas.

El Master contiene el puente UART; el Slave contiene la tarea de adquisición. Consulta los documentos y herramientas del proyecto para las pruebas de funcionamiento:

- [Integración ADC, Zigbee y UART](FrostKernST/docs/INTEGRACION_ADC_ZIGBEE_UART.md)
- [Protocolo UART](FrostKernST/docs/UART_PROTOCOL.md)
- [Integración del puente UART](FrostKernST/docs/INTEGRAR_UART.md)
- [Reintentos de reportes Zigbee](FrostKernST/docs/CORREGIR_REINTENTOS_ZIGBEE.md)
- [Herramienta de diagnóstico UART](FrostKernST/tools/uart_probe.py)
- [Comprobación de reportes Zigbee](FrostKernST/tools/check_zigbee_reports.py)

Las pruebas de PC son opcionales y están documentadas en la guía de integración. Requieren Python y, para las pruebas C, un GCC de escritorio; el GCC ARM del firmware no sustituye ese compilador de PC. Utilizan HAL/SDK/RTOS simulados, por lo que no comprueban la radio ni la electrónica reales.

## 10. Compartir cambios sin archivos generados

Mantén el código, la documentación, los archivos de proyecto y las configuraciones `.launch`. Deja fuera de los commits:

- Carpetas de compilación: `FrostKernST/Debug`, `Master`, `Slave` y `Release`.
- Archivos generados: `.elf`, `.o`, `.map` y `.list`.
- El workspace del IDE y su carpeta `.metadata`.
- Paquetes SDK externos y copias de respaldo de la configuración.
- Copias anidadas del repositorio, como `FrostKernST/frostkern`.

Las rutas absolutas adaptadas en el paso 4 son ajustes locales: revisa `.project` y `.cproject` antes de compartir cambios para no sustituir accidentalmente las rutas de otro integrante por las tuyas. Cuando actualices el repositorio, comprueba si esos archivos cambiaron y vuelve a adaptar las rutas si es necesario; no los descartes sin revisar sus cambios funcionales.

## Referencias

- [Instalación y administración de paquetes en STM32CubeMX 6.18.1](https://dev.st.com/stm32cube-docs/stm32cubemx/6.18.1/en/docs/markup/CubeMX_UserManual/chapters/03_3_installing_and_running_stm32cubemx.html)
- [Formato `.project`: enlaces y rutas locales de Eclipse](https://help.eclipse.org/latest/topic/org.eclipse.platform.doc.isv/reference/misc/project_description_file.html)

Guía preparada para la estructura de la rama `RTOS` revisada en el commit [`adb9ed4`](https://github.com/charles-cg/frostkern/commit/adb9ed4b43c04ca08081cdf73dac7e57434d57bf), el 24 de noviembre de 2026.
