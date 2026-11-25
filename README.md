# FrostKern — firmware STM32 y receptor XIAO

Codigo para dos roles de STM32WBA55: un Master/coordinador Zigbee y hasta dos
Slaves configurables. La validacion fisica reportada se realizo con un Master
y un Slave. La XIAO ESP32-C5 recibe las mediciones del Master mediante UART.

## Implementado

- Lectura NTC y pulsos del caudalimetro YF-B7 en Slave.
- Reportes Zigbee de temperatura (centi-C) y caudal (mL/min).
- Bridge UART binario con CRC y receptor Arduino para temperatura/caudal.
- Estado valido/vencido y edad independientes para cada variable.
- Persistencia de red en Flash, registro IEEE y recuperacion de conexion.
- Conexion Wi-Fi opcional en XIAO. La publicacion a Firebase/backend queda pendiente.

Las pruebas en PCB confirmaron cambios de temperatura/caudal, recepcion hasta
la XIAO, vencimiento de datos al apagar Slave y recuperacion tras reiniciar
Slave o Master. La reconexion ante una perdida prolongada del padre requiere
su prueba especifica; un reinicio corto del Master no confirma ese mecanismo.

## Carpetas actuales

| Carpeta | Uso |
|---|---|
| `FrostKernST` | Proyecto STM32CubeIDE: perfiles Master y Slave |
| `ESP32/FrostKern_UART_Receiver` | Sketch Arduino de la XIAO ESP32-C5 |
| `ESP32/tests` | Pruebas en PC del receptor |
| `firmware/docs` | Documentacion de diseno previa; consultar el protocolo implementado indicado abajo |
| `frostkernApp` | Aplicacion web del equipo |

## Descargar y compilar STM32

1. Clona este repositorio y selecciona la rama `RTOS`. Evita importar el
   proyecto directamente desde una vista de ZIP sin extraer.
2. Instala STM32CubeIDE, STM32Cube_FW_WBA_V1.10.0 y el paquete X-CUBE-FREERTOS
   correspondiente al proyecto (rutas actuales de 1.3.1).
3. Importa `FrostKernST` como proyecto existente. El nombre interno es `FrostKern`.
4. Revisa los recursos enlazados, las rutas de includes/librerias y las
   variables del proyecto: la configuracion heredada apunta al SDK instalado
   en la computadora de desarrollo. Ajusta esas ubicaciones si tu instalacion
   es distinta; las dependencias no estan embebidas en este repositorio.
5. Refresh/F5, Clean y Build del perfil Master; repite con Slave. El enlace
   usa FFD para Master y RFD para Slave.
6. Programa cada PCB con su ELF: `Master/FrostKern.elf` o `Slave/FrostKern.elf`.
   Usa SWD a 1000 kHz; pulsa F8 si Debug se detiene en main().

Las carpetas Master/Slave/Debug se generan al compilar. Conservar los `.ld`,
`.project`, `.cproject`, `.settings` y `.ioc` versionados. El linker reserva
24 KiB finales para persistencia; borrar toda la Flash tambien borra esa red.

## Cargar el receptor XIAO

1. Instala el soporte ESP32 en Arduino IDE y selecciona XIAO_ESP32C5.
   El entorno del equipo usa Arduino IDE 2.3.11 y core Espressif 3.3.12.
2. Abre `ESP32/FrostKern_UART_Receiver/FrostKern_UART_Receiver.ino`, manteniendo
   los archivos `.c/.h` que lo acompanan. Activa USB CDC On Boot.
3. Sube el sketch y abre el Monitor Serie a 115200.
4. Para el montaje externo de prueba: Master PA12/TX -> XIAO D7/GPIO12/RX y
   GND -> GND. XIAO fuera del zocalo alimentada por USB; Master/Slave con pilas.
   Deja 5 V/3V3 sin unir entre equipos. No unas USB normal de XIAO y la fuente
   VESP32 de la PCB a la vez en el montaje con el modulo insertado.

El sketch compila por defecto usando `wifi_config.example.h`, con Wi-Fi apagado.
Para configurar tu red, copia ese archivo como `wifi_config.h`, pon
`FROSTKERN_WIFI_ENABLED` a 1 y completa SSID/clave localmente. Git ignora ese
archivo local. Nunca pongas claves reales en el ejemplo versionado.

## Protocolo y pruebas

El contrato implementado esta en [UART_PROTOCOL.md](FrostKernST/docs/UART_PROTOCOL.md).
Es UART v1 con temperatura TYPE 0x10 y caudal TYPE 0x12; algunos documentos
de diseno anteriores de `firmware/docs` describen un contrato diferente.
No transmitas structs C crudos ni mezcles ambos contratos.

Instrucciones de placa: [LEEME_TEMPERATURA_CAUDAL.md](FrostKernST/LEEME_TEMPERATURA_CAUDAL.md).
Persistencia: [LEEME_PERSISTENCIA.md](FrostKernST/LEEME_PERSISTENCIA.md).

Con GCC/G++ y Python instalados, desde `FrostKernST`:

```sh
python3 tests/run_all_tests.py
```

Para el receptor, desde `ESP32` en un entorno con Bash/GCC/G++:

```sh
bash tests/run.sh
```

Estas pruebas de PC simulan HAL/SDK/radio. No reemplazan Clean/Build en
CubeIDE, compilacion en Arduino IDE ni validacion del conjunto en PCB.
