# FrostKern: DS18B20 del Master + dos PWM + reconexión

Actualización que integra el DS18B20 de Joaquín (`FrostKernST (1).zip`), la medición
nueva al reconectar ya probada, persistencia Zigbee y temperatura/caudal de dos
Slaves. No modifica credenciales, .project ni .cproject.

## Recorrido y canales

Retorno con calentador → bomba → llave → radiador del A/C → Target 1
(**0080E1FFFE32FC18**) → radiador principal → Target 2 → retorno.

El control usa FC18 por IEEE, aunque se registre como Slave 2. La lectura local
del DS18B20 se publica aparte como AIR MASTER y no gobierna el ventilador automático.

| Función | Pin del Master | Operación |
|---|---|---|
| ENA del A/C | PA1, TIM3_CH2, AF2 | Manual 0..100 % |
| ENB del radiador principal | PA10, TIM3_CH1, AF2 | Automático desde FC18 |
| DS18B20 DQ | PB1, SDA de CN6 | Un sensor 1-Wire de tres hilos |
| TX hacia RX de XIAO | PA12 → D7/GPIO12 | Telemetría |
| RX desde TX de XIAO | PA11 ← D6/GPIO11 | Órdenes |

Los dos PWM comparten frecuencia y tienen ciclos independientes. TIM2 queda
reservado al tiempo de microsegundos del DS18B20. LPTIM2 deja de asignar PA1.
El PWM se configura por LL en app_fan_pwm.c: no requiere habilitar HAL_TIM ni
agregar archivos de biblioteca al proyecto. El IOC reserva los pines como GPIO
propiedad de la aplicación; al regenerar no los reasignes a LPTIM2.

## Instalación

1. Respalda el proyecto fuera de su carpeta.
2. Combina el CONTENIDO de FrostKernST de este paquete con tu FrostKernST actual,
   reemplazando archivos. Evita FrostKernST/FrostKernST. Conserva .project/.cproject.
3. F5/Refresh en CubeIDE. Deben aparecer app_master_control.c, app_fan_policy.c,
   app_fan_pwm.c, ds18b20.c y ds18b20_decode.c en Core/Src y app_zigbee_refresh.c
   en STM32_WPAN/App.
4. Clean/Build de Master y Slave; programa Master/FrostKern.elf en Master y
   Slave/FrostKern.elf en cada Slave. SWD 1000 kHz; F8 si se detiene en main().
   Termina Debug antes de mover ST-LINK. Las pilas alimentan la PCB, no ST-LINK.
5. Actualiza el sketch Arduino con los tres archivos de ESP32/FrostKern_UART_Receiver.
   Conserva tu wifi_config.h privado. Programa XIAO_ESP32C5, Monitor Serie 115200,
   New Line. Esto permite probar sin Wi-Fi.

## Cableado, con las fuentes desconectadas

- Mueve ENA de PA2 a **PA1**. ENB va a **PA10**. No conectes PA2 a ENA.
- Retira los puentes ENA y ENB del L298N que mantendrían los enables en HIGH.
- Une GND de Master, XIAO y L298N. Usa fuente externa de 12 V para ventiladores;
  los GPIO y el boost de 5 V de la PCB no alimentan esos motores.
- Fija IN1=HIGH/IN2=LOW e IN3=HIGH/IN4=LOW para un sentido constante. No dejes
  esas entradas flotando. Confirma los puentes/regulador de tu módulo L298N
  antes de unir sus bornes de alimentación lógica.
- Se recomiendan pull-down externos de 10 kΩ en ENA/ENB para apagado durante reset.
- DS18B20: VDD=3.3 V, GND, DQ=PB1 con pull-up externo de aproximadamente 4.7 kΩ
  hacia 3.3 V. No usa alimentación parásita. PB1 no puede usarse simultáneamente
  como SDA I2C. Confirma el orden real de cables del sensor, no solo sus colores.
- En la prueba XIAO externa alimentada por USB: PA12→D7, **D6→PA11**, GND→GND.
  La segunda señal es nueva respecto a la recepción pasiva. No unas 5V/3V3 de
  ambas placas en ese montaje; no montes la XIAO con USB y VESP32 activos sin
  resolver la doble alimentación.

## Control

Al arrancar Master, ENA y ENB están apagados y AUTO deshabilitado. No se suponen
umbrales ni mínimo estable de los ventiladores. La configuración vive en RAM;
después de reiniciar Master hay que enviarla otra vez.

ENA aplica el porcentaje seleccionado si está habilitado. OFF aplica 0 % y conserva
en RAM el porcentaje que incluye la orden. Solicitar 0 % también apaga la salida.

AUTO requiere T_OFF < T_ON < T_FULL y mínimo entre 1 y 100 %. Desde apagado,
arranca al alcanzar T_ON. Aumenta linealmente del mínimo al 100 % entre T_ON y
T_FULL. Al bajar, permanece al mínimo dentro de la banda T_OFF..T_ON y se apaga
al llegar a T_OFF. El mínimo estable se mide arrancando desde reposo en banco.

Si AUTO está habilitado y falta una muestra válida, no hay red o la temperatura
de FC18 tiene más de **90 s**, ENB aplica **100 %**, como acordaron. Al volver los
datos continúa la histéresis. Este límite del control no cambia los 180 s de la
telemetría ni los intervalos de radio. DS18B20 y Target 2 no sustituyen a FC18.
AUTO OFF apaga ENB. BENCH deshabilita AUTO y prueba ENB por 5 s; hay que rearmar AUTO.

El PWM manual se conserva si se pierde Wi-Fi. El automático continúa localmente
con los reportes Zigbee. No se controla bomba ni calentador. El porcentaje aplicado
es una orden eléctrica: no hay tacómetro ni confirmación de giro.

## Prueba de banco paso a paso

1. Primero deja motores desconectados y comprueba sensor, UART y señales PWM.
2. Enciende Master y Slave FC18. Además de TEMP/FLOW deben aparecer:

   ```text
   [AIR MASTER] T=2500 centi-C ...
   [FANS] AC enabled=0 seleccionado=0% aplicado=0% AUTO enabled=0 aplicado=0% ...
   ```

   2500 centi-C = 25.00 °C. Sin DS18B20 o con CRC incorrecto, AIR dice NO DISPONIBLE.
3. Envía estas líneas desde Monitor Serie, una por vez:

   ```text
   AC ON 40
   AC OFF 40
   AC ON 100
   AC ON 0
   ```

   Busca ACK y porcentajes correspondientes en [FANS]. PA1 debe pasar por 40 %, 0 %,
   100 %, 0 %, mientras PA10 permanece apagado. Usa osciloscopio/analizador lógico
   para duty/frecuencia; un multímetro solo aproxima tensión promedio.
4. `BENCH 40` aplica 40 % a PA10 durante 5 s; después vuelve a cero. `BENCH OFF`
   cancela antes. ENA mantiene su orden independiente.
5. Conecta los ventiladores y determina mínimo de arranque/giro estable y frecuencia.
   Repite arranque desde reposo, no solo reducción cuando ya gira. Revisa caída de
   tensión y calentamiento del L298N. `APP_FAN_PWM_HZ=1000` en app_fan_policy.h es
   un punto inicial de banco, **no una frecuencia validada** para VN4-012P. Cambia
   ese valor y recompila para probar otras frecuencias.
6. Con calibración medida, usa `AUTO T_OFF_cC T_ON_cC T_FULL_cC MINIMO_PORCIENTO`.
   Ejemplo solo de FORMATO, no ajuste recomendado: `AUTO 2500 3000 4000 40`
   representa OFF=25 °C, ON=30 °C, FULL=40 °C, mínimo=40 %. Sustituye los números.
7. Varía el NTC de FC18 y comprueba arranque, incremento, histéresis y apagado.
   El tiempo incluye los reportes Zigbee existentes: cambios pequeños pueden esperar
   el reporte de temperatura de 60 s; no es un control de alta velocidad.
8. Apaga FC18 con AUTO habilitado: al superar 90 s de edad debe aplicar 100 %.
   Vuelve a encenderlo y verifica muestra nueva y retorno al control. Conserva
   la prueba de reconexión, también con valores que no cambiaron.
9. Cambiar Target 2 o DS18B20 no debe gobernar ENB. Desconectar DS18B20 debe invalidar
   AIR sin detener TEMP/FLOW. `AUTO OFF` deshabilita y apaga ENB.

ACK confirma **encolado**, no motor en giro. Verifica [FANS] y la señal física.

## App, ESP32 y Firebase

Se conserva la estación de tu compañero en firmware/esp32/station y sus rutas
/slaves/{IEEE}/telemetry. Añade:

| Ruta Firebase | Contenido |
|---|---|
| /controller/telemetry | DS18B20, porcentajes pedidos/aplicados, estado AUTO y uartFresh |
| /controller/uartCommand | Última solicitud de la app, string JSON |
| /controller/commandResult | ID, queued/rejected/timeout/skipped_on_boot y código UART |

**Arduino sirve para banco local**. Para el recorrido Firebase completo usa la
estación ESP-IDF; no copies sus fuentes dentro de Arduino. Tu compañero compila
esa carpeta con su instalación existente: `idf.py set-target esp32c5`,
`idf.py menuconfig` (credenciales existentes), `idf.py build` y
`idf.py -p COM5 flash monitor`.

Se incluye adaptador reutilizable en ESP32/app_client/control_client.mjs y panel
de banco index.html, sin modificar la interfaz Expo de su app actual:

1. Desde la raíz sirve la carpeta, por ejemplo:
   `python -m http.server 8000 --directory ESP32/app_client`.
2. Abre http://localhost:8000. Introduce URL HTTPS de Firebase e **ID token del
   usuario autenticado** con permisos sobre /controller. Nunca metas el secreto
   administrativo del ESP32 en la app. No se abren reglas a acceso público.
3. Enciende la estación y espera /controller/telemetry. Pulsa Leer estado.
4. Envía intensidad manual y comprueba commandResult más porcentajes aplicados.
   Para AUTO usa los valores medidos en banco.

El panel no implementa un login ni reemplaza el diseño de su app móvil. Su sesión
existente debe proporcionar/renovar el ID token. La pantalla existente puede usar:

```js
import {acCommand, submitCommand, controllerState} from './control_client.mjs';
await submitCommand(firebaseUrl, userIdToken, acCommand(Date.now(), true, 40));
const actual = await controllerState(firebaseUrl, userIdToken);
// Mostrar actual.commandResult y actual.telemetry. Un PUT no confirma aplicación.
```

La estación consulta aproximadamente cada 2 s, más HTTPS y reintentos; UART corre
en otra tarea. Solo una solicitud pendiente por ruta: espera resultado antes de
enviar otra. Al arrancar ESP32 descarta una orden antigua retenida en Firebase;
manda otra con ID nuevo una vez conectado. Reiniciar Master no rearma su PWM.
Sin Wi-Fi aún puedes probar todo el control local desde el sketch Arduino.

## Extensiones UART v1

Se mantienen el formato, CRC y las tramas TEMP 0x10/FLOW 0x12. Números en little
endian; IEEE[8] en big endian. Actualiza XIAO para decodificar los tipos nuevos.

| Tipo | Dirección | Payload |
|---|---|---|
| 0x13 MASTER_AIR | Master→ESP | 12: versión, flags, int16 T_cC, uint32 updated, uint32 age |
| 0x14 FAN_STATUS | Master→ESP | 33 bytes: ver offsets debajo |
| 0x22 SET_AC_FAN | ESP→Master | 2: enabled 0/1, porcentaje 0..100 |
| 0x23 SET_AUTO_FAN | ESP→Master | 16: enabled, IEEE[8], int16 OFF, ON, FULL, uint8 mínimo |
| 0x24 FAN_BENCH | ESP→Master | 2: enabled 0/1, porcentaje ENB; lease de 5 s y desarma AUTO |

FAN_STATUS: [0] versión, [1] AC enabled, [2] seleccionado, [3] AC aplicado,
[4] AUTO enabled, [5] ENB aplicado, [6] razón, [7] banco, [8..15] IEEE,
[16..17] temperatura entrada, [18..21] edad entrada, [22..23] OFF, [24..25] ON,
[26..27] FULL, [28] mínimo, [29..32] frecuencia. Razón: 0 deshabilitado, 1 frío,
2 regulando, 3 sin entrada reciente (100 %), 4 banco. No interpretes temperatura
entrada cuando su edad sea UINT32_MAX.

Firebase transporta `"ID_DECIMAL:TIPO_HEX:PAYLOAD_HEX"`, por ejemplo
`"123:22:0128"` para AC ON 40. El adaptador genera estos strings y IDs nuevos.

## Validación y límites

Pruebas PC con sanitizadores: reconexión, persistencia, dos IEEE, control por
identidad con plazas invertidas, histéresis/rampa, pérdida de datos, órdenes
inválidas/cola llena, lease y rollover, CRC DS18B20 y valores negativos/extremos.
Se verifican tramas UART nuevas, receptor Arduino, adaptador app y transporte
estación con HTTP/UART simulados. Archivos STM32 afectados compilados Cortex-M33,
Master y Slave, con headers reales del SDK.

**Falta en su equipo**: Clean/Build y enlace completo CubeIDE, compilación Arduino
ESP32/ESP-IDF con sus paquetes, RF real con este merge, PWM físico, arranque/giro,
calibración, Wi-Fi y permisos Firebase. No se afirma validación física de este paquete.

```sh
python FrostKernST/tests/run_all_tests.py
bash ESP32/tests/run.sh
python firmware/esp32/station/tests/run.py
node ESP32/tests/test_control_client.mjs
```

Fuentes consultadas: STM32WBA DS14127 Rev10
https://www.st.com/resource/en/datasheet/stm32wba55cg.pdf ; DS18B20
https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf ;
L298 https://www.st.com/resource/en/datasheet/l298.pdf ; HTTP ESP-IDF
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/protocols/esp_http_client.html
