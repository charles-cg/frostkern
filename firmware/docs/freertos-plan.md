# Frostkern firmware — FreeRTOS runtime design

Status: agreed design, v1 (2026-11-10). Human-readable version: `freertos-plan.html`.

## 1. System overview

| Board | Count | Zigbee role | Power | Sleeps? |
|---|---|---|---|---|
| Master | 1 | Coordinator (trust center) | Car 12 V battery via step-down module | Never |
| Slave | 2 | Sleepy End Device (SED) | 4×AA → TPS70933 LDO 3.3 V; TPS63070 buck-boost 5 V on demand | Yes (Stop 1 / Sleep) |

- All three boards are the same PCB (`wba55_v1`), STM32WBA55CGU6.
- One codebase, one CubeIDE project, two build configurations: `ROLE_MASTER`, `ROLE_SLAVE`. The role selects which application tasks are created and which Zigbee stack library (FFD for master, RFD for slaves) is linked.
- Master: XIAO ESP32 add-on is a Wi-Fi bridge to `frostkernApp` over UART. ESP32 supply is switched by `ESP32_EN` (TPS22918).
- Slave: 1 NTC, 1 YF-B7 flow sensor. Fan (PC fan) and pump run from an external supply, switched by MOSFETs driven by the slave. Pump is on/off.
- Control is local to each slave (PID). The master only sends the setpoint and collects telemetry.

## 2. Pin map (from `wba55_v1.kicad_pcb`)

| Signal | Pin | Use | Notes |
|---|---|---|---|
| PWM1 | PA1 | Fan MOSFET | LPTIM2_CH2 (AF13) — only PWM pin with a low-power timer |
| PWM2 | PA2 | Pump MOSFET | Plain GPIO on/off |
| PWM3 | PA10 | Spare | TIM3_CH1 |
| YFB7_DIV | PA0 | Flow pulses | LPTIM1_IN1 (AF0); counts in Stop 1 |
| NTC_DIV | PA5 | NTC divider | ADC4_IN4 |
| NTC_BIAS | PB15 | Switched NTC bias | Drive high only while sampling |
| BAT_SENS | PA6 | Battery divider | ADC4_IN3 |
| YFB7_EN | PB6 | YF-B7 load switch | |
| BOOST_EN | PB5 | 5 V buck-boost enable | YF-B7 needs 5 V |
| ESP32_EN | PB7 | ESP32 load switch | Master only |
| WBA_TX / WBA_RX | PA12 / PA11 | UART to ESP32 | Master only; confirm USART instance in CubeMX |
| IRQ | PA9 | ESP32 interrupt line | Master only |
| PAIR_BTN | PA7 | Button | Master: permit join. Slave: hold 5 s = leave + factory reset |
| LD1 | PB4 | Status LED | |
| SCL / SDA | PB2 / PB1 | I2C header | Unused in v1 |
| VCP1_TX / RX | PB12 / PA8 | Debug UART | |

## 3. FreeRTOS configuration (bare bones)

Taken from ST's `Zigbee_OnOff_*_FreeRtos` examples (STM32CubeWBA v1.10.0, X-CUBE-FREERTOS 1.3.1):

- `heap_4`, `configTOTAL_HEAP_SIZE` 32768 (start value)
- `configTICK_RATE_HZ` 100
- `configUSE_TICKLESS_IDLE` 2 (ST's custom `vPortSuppressTicksAndSleep`)
- `configMAX_PRIORITIES` 56 (CMSIS-RTOS2 mapping)
- `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` 5, 4 priority bits
- ST's generated stack glue stays CMSIS-RTOS2 and is not edited. All application tasks use the native FreeRTOS API.
- Application tasks are created once at startup and never deleted.

Later additions (not in v1): stack overflow check (method 2), malloc-failed hook, `uxTaskGetStackHighWaterMark` logging.

## 4. Tasks

### 4.1 ST Zigbee stack tasks (generated, do not touch)

| Task | CMSIS priority | Native | Stack (B) |
|---|---|---|---|
| MacLayer | osPriorityRealtime3 | 51 | 2048 |
| Link Layer | osPriorityRealtime2 | 50 | 3072 |
| ZigbeeLayer | osPriorityRealtime1 | 49 | 3072 |
| RNG | osPriorityHigh | 40 | 512 |

ST's example app tasks (NwkFormJoin, Application) are replaced by `ZbAppTask`.

### 4.2 Application tasks

Priorities and stacks are first estimates.

| Role | Task | Job | Prio | Stack (B) | Runs when |
|---|---|---|---|---|---|
| Slave | `ControlTask` | PID → fan duty (LPTIM2 PWM); owns pump output | 32 | 1024 | New sample or config change |
| Slave | `SensorTask` | NTC bias + ADC → °C; LPTIM1 count → flow; battery ADC | 28 | 1024 | Every 2 s (Active) / 30 s (Idle) |
| Both | `ZbAppTask` | Network form/join, clusters, reporting, setpoint writes | 24 | 3072 | Stack callbacks, events, poll timer |
| Master | `BridgeTask` | Binary UART frames to/from ESP32; ESP32 power gating | 20 | 1536 | UART RX, events |
| Both | `SupervisorTask` | Watchdog check-ins, battery state, faults, LED, button | 16 | 768 | Periodic; policy TBD |

## 5. Inter-task communication

| Object | Type | Writer → Reader | Content |
|---|---|---|---|
| `qSample` | Queue, length 1, `xQueueOverwrite` | Sensor → Control | Latest reading (temp, flow, battery, timestamp) |
| `gStatus` | Struct + mutex | Sensor, Control → ZbApp, Bridge, Supervisor | Latest measurements, fan duty, mode, faults |
| `qConfig` | Queue | ZbApp → Control | New setpoint |
| `evSystem` | Event group | Any → Any | `EV_FAULT`, `EV_BUTTON`, `EV_REPORT_NOW`, `EV_MODE_CHANGE`, watchdog check-in bits |

ISRs use only `FromISR` calls, at NVIC priority ≥ 5.

## 6. Zigbee stack and interrupt rules

- Stack: Exegin Zigbee R23, precompiled libraries (`ZigBeeProR23_FFD.a` master, `ZigBeeProR23_RFD.a` slaves, `ZigBeeClusters.a`).
- Reference examples: `Zigbee_OnOff_Server_Coord_FreeRtos` (master), `Zigbee_OnOff_Client_Router_FreeRtos` (FreeRTOS structure). There is no FreeRTOS SED example; take SED settings from `Zigbee_TempMeas_Server_SED` / `Zigbee_PowerConfig_Server_SED`.
- Radio IRQ priorities from ST's `app_conf.h`: RADIO_INTR_PRIO_HIGH 5, RADIO_INTR_PRIO_LOW 7, RADIO_SW_LOW 14, RCC 1. Our peripheral IRQs that call FreeRTOS: 6–13.
- Never call Zigbee APIs from ISRs or from tasks other than `ZbAppTask`.

## 7. Low-power strategy (slaves)

| | Active mode | Idle mode |
|---|---|---|
| Enter when | temp ≥ setpoint − `MARGIN` (e.g. 5 °C) | temp < setpoint − `MARGIN` − `HYST` |
| Sample / PID period | 2 s, PID runs | 30 s, sample only, fan off |
| Report to master | every 60 s, or Δtemp > 0.5 °C, or fault | every 5 min, or change, or fault |
| Parent poll | every 10 s | every 60 s |
| Sleep depth | Stop 1 if LPTIM2 PWM survives Stop 1, else Sleep while fan runs | Stop 1 |

All numbers are `#define`s in one config header.

Hardware facts:
- WBA55 has Stop 0, Stop 1 and Standby only (no Stop 2).
- LPTIM1 pulse counting on PA0 works in Stop 1 (ST example `LPTIM_PulseCounter`, NUCLEO-WBA55CG).
- TIM1/2/3/16/17 stop in Stop mode.
- ADC4 can convert autonomously in Stop 1; GPDMA1 only runs in Stop 0.
- IWDG keeps counting in Stop; the supervisor must wake before it expires.

## 8. Zigbee data model (slave endpoint, standard clusters only)

| Cluster | ID | Key attributes | Notes |
|---|---|---|---|
| Temperature Measurement | 0x0402 | MeasuredValue (int16, 0.01 °C) | Shipped by ST |
| Flow Measurement | 0x0404 | MeasuredValue (uint16, 0.1 m³/h), Min, Max | Built by us with the generic ZCL API (`ZbZclClusterAlloc`, `ZbZclAttrAppendList`, `ZbZclClusterAttach`) |
| Fan Control | 0x0202 | FanMode (enum off/low/med/high/on/auto) | Shipped by ST |
| Thermostat | 0x0201 | OccupiedCoolingSetpoint (int16, 0.01 °C) | Master/app writes setpoint; confirm ST ships it |
| Power Configuration | 0x0001 | BatteryVoltage (100 mV), BatteryPercentageRemaining | Shipped by ST |

- No manufacturer-specific cluster. PID gains are compile-time constants.
- Pairing: app "Pair" or master button → master permits join for 180 s. An unpaired slave searches on its own with backoff (10 s → 1 min → 5 min cap). Slave button held 5 s → leave + factory reset. Network data persisted in flash. Security: Zigbee 3.0 default TC link key.

## 9. Master ↔ ESP32 UART frames (draft)

`SOF(0xA5) | LEN(1) | TYPE(1) | SEQ(1) | PAYLOAD(LEN) | CRC16-CCITT(2)`

| TYPE | Direction | Payload |
|---|---|---|
| `TELEMETRY` | WBA → ESP | slave id, temp, flow, FanMode, battery, mode, fault flags |
| `SET_SETPOINT` | ESP → WBA | slave id (or all), setpoint 0.01 °C |
| `PERMIT_JOIN` | ESP → WBA | duration s |
| `DEVICE_JOINED` | WBA → ESP | slave id, IEEE address |
| `ACK` / `NACK` | both | SEQ, error code |

The ESP32 converts frames to whatever it uses towards the app (MQTT TBD).

## 10. Open questions

1. Fail-safe policy: sensor fault, comms loss, watchdog policy, pump on flow fault (partner).
2. MQTT or another transport on the ESP32.
3. Which PID library.
4. Does LPTIM2 PWM on PA1 keep running in Stop 1? Test on hardware first.
5. Engine-running detection (12 V level to ADC) for a future board revision.
6. Install codes instead of the default TC link key.
7. YF-B7 needs the 5 V boost + load switch: keep it powered all the time, or only in Active mode / short windows in Idle?
8. Flow cluster resolution is 0.1 m³/h (≈1.7 L/min). Enough, or add a finer attribute?
9. Fan Control (0x0202) has no duty-% attribute. Is FanMode enough at the master/app?
10. Facts not checked against the datasheet / RM0493 (PDFs wouldn't load): LPTIM2 in Stop 1, ADC4 autonomy, Standby under FreeRTOS, whether ST ships the Thermostat cluster.
