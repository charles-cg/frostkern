# Frostkern firmware — FreeRTOS runtime design

Status: agreed design, v2 (2026-11-19). v1 was 2026-11-10. Schedule and session decisions: `roadmap.md`. `freertos-plan.html` still shows v1.

## 1. System overview

Target vehicle: gasoline (combustion-engine) car, not an EV (decided 2026-11-21). Each slave's loop is a **simulation of an engine coolant loop**, with the slave board placed as if in the engine bay. Consequences:
- Demo rig: a bench rig with a heater that is **not controllable** (always on when plugged in). Loop kept at **≤ 80 °C**; demo setpoint **60–70 °C**; slave sets `AbsMinCoolSetpointLimit` / `AbsMaxCoolSetpointLimit` to **30 °C / 95 °C** (replacing the cluster's default 16–32 °C).
- The YF-B7 is rated for water up to 80 °C by some sources and 100 °C by others; no manufacturer datasheet confirmed, hence the 80 °C loop limit.
- NTC divider: R19 = 22 kΩ from `NTC_BIAS` (PB15) to `NTC_DIV`, NTC from `NTC_DIV` to GND. NTC believed to be **50 kΩ / B3950** (YF-B7 built-in type), not yet verified: measure it at room temperature before B4 (≈50 kΩ at 25 °C). At 90 °C this gives ~0.58 V and ~12 ADC counts/°C (12-bit), which is enough. If it turns out to be 10 kΩ, change R19 to ~1.5–2.2 kΩ.
- A real engine bay (100–120 °C) exceeds alkaline AA ratings (~55–60 °C). Fine for the bench rig; a real install needs a different power source.
- With a combustion engine, 12 V rises to ~13.8–14.4 V while the alternator charges, which makes engine-running detection (open question 3) a simple voltage threshold.

| Board | Count | Zigbee role | Power | Sleeps? |
|---|---|---|---|---|
| Master | 1 | Coordinator (trust center) | Car 12 V battery via step-down module | Never |
| Slave | 2 | Sleepy End Device (SED) | 4×AA → TPS70933 LDO 3.3 V; TPS63070 buck-boost 5 V on demand | Yes (Stop 1 / Sleep) |

- All three boards are the same PCB (`wba55_v1`), STM32WBA55CGU6.
- Two CubeMX projects built with CMake in VS Code (not CubeIDE): `firmware/master` (Coordinator, FFD) and `firmware/slave` (Sleepy End Device, RFD), plus shared code in `firmware/common`. The Zigbee role is set in each `.ioc` (`ZGB_DEVICE_ROLE`, `ZGB_APPLICATION`, `ZGB_SLEEPY_MODE`, `CFG_LPM_LEVEL`) and drives generated code and linked libs, so one `.ioc` cannot produce both roles.
- Master: XIAO ESP32 add-on is a Wi-Fi bridge to `frostkernApp` over UART. ESP32 supply is switched by `ESP32_EN` (TPS22918). The ESP32 runs a SoftAP + WebSocket server; the phone connects to it directly (no router or internet in the car).
- Slave: 1 NTC, 1 YF-B7 flow sensor. Fan (PC fan) runs from an external supply, switched by a MOSFET driven by the slave. The pump is always on and not controlled by the MCU.
- Control is local to each slave (PID). The master only sends the setpoint and collects telemetry.

## 2. Pin map (from `wba55_v1.kicad_pcb`)

| Signal | Pin | Use | Notes |
|---|---|---|---|
| PWM1 | PA1 | Fan MOSFET | LPTIM2_CH2 (AF13) — only PWM pin with a low-power timer |
| PWM2 | PA2 | Pump MOSFET | Unused in v1 (pump always on); configure as analog |
| PWM3 | PA10 | Spare | TIM3_CH1 |
| YFB7_DIV | PA0 | Flow pulses | LPTIM1_IN1 (AF0); counts in Stop 1 |
| NTC_DIV | PA5 | NTC divider | ADC4_IN4 |
| NTC_BIAS | PB15 | Switched NTC bias | Drive high only while sampling |
| BAT_SENS | PA6 | Battery divider | ADC4_IN3 |
| YFB7_EN | PB6 | YF-B7 load switch | |
| BOOST_EN | PB5 | 5 V buck-boost enable | YF-B7 needs 5 V |
| ESP32_EN | PB7 | ESP32 load switch | Master only |
| WBA_TX / WBA_RX | PA12 / PA11 | UART to ESP32 | Master only; USART2 |
| IRQ | PA9 | ESP32 interrupt line | Master only |
| PAIR_BTN | PA7 | Button | Not fitted (footprint error). Pairing is done from the app; factory reset = chip erase over ST-LINK |
| LD1 | PB4 | Status LED | |
| SCL / SDA | PB2 / PB1 | I2C header | Unused in v1 |
| VCP1_TX / RX | PB12 / PA8 | Debug UART | USART1, through the STLINK-V3MINIE VCP |

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
| Slave | `ControlTask` | PID → fan duty (LPTIM2 PWM) | 32 | 1024 | New sample or config change |
| Slave | `SensorTask` | NTC bias + ADC → °C; LPTIM1 count → flow; battery ADC | 28 | 1024 | Every 2 s (Active) / 30 s (Idle) |
| Both | `ZbAppTask` | Network form/join, clusters, reporting, setpoint writes | 24 | 3072 | Stack callbacks, events, poll timer |
| Master | `BridgeTask` | Binary UART frames to/from ESP32; ESP32 power gating | 20 | 1536 | UART RX, events |
| Both | `SupervisorTask` | Battery state, faults, LED (watchdog check-ins after the demo) | 16 | 768 | Periodic |

## 5. Inter-task communication

| Object | Type | Writer → Reader | Content |
|---|---|---|---|
| `qSample` | Queue, length 1, `xQueueOverwrite` | Sensor → Control | Latest reading (temp, flow, battery, timestamp) |
| `gStatus` | Struct + mutex | Sensor, Control → ZbApp, Bridge, Supervisor | Latest measurements, fan duty, mode, faults |
| `qConfig` | Queue | ZbApp → Control | New setpoint |
| `evSystem` | Event group | Any → Any | `EV_FAULT`, `EV_PERMIT_JOIN` (master), `EV_REPORT_NOW`, `EV_MODE_CHANGE`; watchdog check-in bits after the demo |

ISRs use only `FromISR` calls, at NVIC priority ≥ 5.

## 6. Zigbee stack and interrupt rules

- Stack: Exegin Zigbee R23, precompiled libraries (`ZigBeeProR23_FFD.a` master, `ZigBeeProR23_RFD.a` slaves, `ZigBeeClusters.a`).
- Project bases: master = copy of `Zigbee_OnOff_Server_Coord_FreeRtos`. Slave = copy of `Zigbee_OnOff_Client_Router_FreeRtos` (FreeRTOS structure), switched in CubeMX to END_DEVICE / RFD / sleepy ON. There is no FreeRTOS SED example; take SED settings from `Zigbee_OnOff_Client_SED` / `Zigbee_PowerConfig_Server_SED`.
- Radio IRQ priorities from ST's `app_conf.h` (coordinator example): RADIO_INTR_PRIO_HIGH 5, RADIO_INTR_PRIO_LOW 7, RADIO_SW_LOW 14, RCC 1. The SED example sets RADIO_INTR_PRIO_LOW 5; check the slave's generated value. Our peripheral IRQs that call FreeRTOS: 6–13.
- Never call Zigbee APIs from ISRs or from tasks other than `ZbAppTask`.

## 7. Low-power strategy (slaves)

| | Active mode | Idle mode |
|---|---|---|
| Enter when | temp ≥ setpoint − `MARGIN` (e.g. 5 °C) | temp < setpoint − `MARGIN` − `HYST` |
| Sample / PID period | 2 s, PID runs | 30 s, sample only, fan off |
| Report to master | every 60 s, or Δtemp > 0.5 °C, or fault | every 5 min, or change, or fault |
| Parent poll | every 10 s | every 60 s |
| Sleep depth | Stop 1 if LPTIM2 PWM survives Stop 1, else Sleep while fan runs | Stop 1 |
| 5 V boost + YF-B7 | On continuously | Off, except a 2 s flow window every 5 min |
| NTC bias | Pulsed per sample | Pulsed per sample |

All numbers are `#define`s in one config header.

Real battery savings are a demo requirement. During development the slave runs with `CFG_LPM_LEVEL` 0 (debugger stays attached); Stop 1 is proven in a spike right after the SED joins and enabled for good at the end (see `roadmap.md`).

Hardware facts:
- Board erratum (`wba55_v1`): the 32.768 kHz LSE crystal does not start at ST's default drive (MediumLow). It starts at `RCC_LSEDRIVE_HIGH`, set in both `.ioc` files. Likely the 6 pF load caps don't match the crystal's CL; check the crystal datasheet for v2.
- WBA55 has Stop 0, Stop 1 and Standby only (no Stop 2).
- LPTIM1 pulse counting on PA0 works in Stop 1 (ST example `LPTIM_PulseCounter`, NUCLEO-WBA55CG).
- TIM1/2/3/16/17 stop in Stop mode.
- ADC4 can convert autonomously in Stop 1; GPDMA1 only runs in Stop 0.
- IWDG keeps counting in Stop; the supervisor must wake before it expires.

## 8. Zigbee data model (slave endpoint, standard clusters only)

| Cluster | ID | Key attributes | Notes |
|---|---|---|---|
| Temperature Measurement | 0x0402 | MeasuredValue (int16, 0.01 °C) | Shipped by ST |
| Thermostat | 0x0201 | OccupiedCoolingSetpoint (int16, 0.01 °C) | Shipped by ST (`ZbZclThermServerAlloc`). Master/app writes setpoint. ST's default limits are 16–32 °C; ours are 30–95 °C (set at B7) |
| Power Configuration | 0x0001 | BatteryVoltage (100 mV), BatteryPercentageRemaining | Shipped by ST |
| Frostkern Status (manufacturer-specific) | 0xFC00 | Flow (uint16, 0.01 L/min), FanDuty (uint8, %), Mode (Idle/Active), FaultFlags (bitmap) | Built by us with the generic ZCL API (`ZbZclClusterAlloc`, `ZbZclAttrAppendList`, `ZbZclClusterAttach`) |

- Flow Measurement (0x0404, 0.1 m³/h ≈ 1.7 L/min steps) and Fan Control (0x0202, FanMode enum only, no duty %) are not used; `0xFC00` replaces both. PID gains are compile-time constants.
- Pairing: app "Pair" (`PERMIT_JOIN` frame) or master boot → master permits join for 180 s. A slave that loses its parent rejoins with backoff (1 s doubling to a 60 s cap); after 5 failures it drops the network and does a full join, which needs permit-join open. Factory reset = chip erase over ST-LINK (no buttons fitted). Network data persisted in flash. Security: Zigbee 3.0 default TC link key.

## 9. Master ↔ ESP32 UART frames (frozen 2026-11-24)

USART2 (PA12 TX / PA11 RX), 115200 8N1.

### 9.1 Frame

`SOF | LEN | TYPE | SEQ | PAYLOAD[LEN] | CRC_L | CRC_H`

| Field | Size | Value |
|---|---|---|
| SOF | 1 | `0xA5` |
| LEN | 1 | payload length in bytes, 0–64 (header and CRC not counted) |
| TYPE | 1 | see 9.2 |
| SEQ | 1 | sender's counter, +1 per frame, wraps 255 → 0. Each side has its own |
| PAYLOAD | LEN | see 9.3 |
| CRC | 2 | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`, no reflection, no final XOR) over LEN, TYPE, SEQ, PAYLOAD (not SOF). Sent low byte first |

- All multi-byte fields are **little-endian** (native on both the STM32 and the ESP32). IEEE addresses too.
- Receiver: hunt for SOF, read LEN; LEN > 64 or a bad CRC → drop the frame and resynchronise on the next SOF. A bad frame gets a `NACK` with code `BAD_CRC` only if its SEQ could be read.
- Every ESP → WBA command gets exactly one `ACK` or `NACK` echoing its SEQ. WBA → ESP frames are not acknowledged (telemetry is periodic; a lost one is replaced by the next).

### 9.2 Types

| TYPE | Name | Direction |
|---|---|---|
| `0x01` | `TELEMETRY` | WBA → ESP |
| `0x02` | `DEVICE_JOINED` | WBA → ESP |
| `0x03` | `DEVICE_OFFLINE` | WBA → ESP |
| `0x10` | `SET_SETPOINT` | ESP → WBA |
| `0x11` | `PERMIT_JOIN` | ESP → WBA |
| `0x7E` | `ACK` | WBA → ESP |
| `0x7F` | `NACK` | WBA → ESP |

### 9.3 Payloads

Slaves are identified by their **IEEE address** (8 bytes, never changes; known boards in `roadmap.md` → Boards). `0xFFFFFFFFFFFFFFFF` means "all slaves" where allowed.

| Type | Payload | LEN |
|---|---|---|
| `TELEMETRY` | `ieee u64`, `tempCenti i16` (0.01 °C), `setpointCenti i16` (0.01 °C), `flowCenti u16` (0.01 L/min), `fanPct u8` (0–100), `batteryMv u16`, `mode u8` (0 idle, 1 active), `faults u16` (`FK_FAULT_*` bits) | 20 |
| `DEVICE_JOINED` | `ieee u64`, `shortAddr u16` | 10 |
| `DEVICE_OFFLINE` | `ieee u64` (no report for 3 × the reporting interval) | 8 |
| `SET_SETPOINT` | `ieee u64` (or all), `setpointCenti i16`, range 3000–9500 | 10 |
| `PERMIT_JOIN` | `seconds u8`, 1–254 | 1 |
| `ACK` | `ackedSeq u8` | 1 |
| `NACK` | `ackedSeq u8`, `code u8` | 2 |

NACK codes: `0x01 BAD_CRC`, `0x02 BAD_LEN`, `0x03 UNKNOWN_TYPE`, `0x04 OUT_OF_RANGE`, `0x05 UNKNOWN_SLAVE`, `0x06 BUSY` (Zigbee request could not be queued).

### 9.4 WebSocket JSON (ESP32 SoftAP ↔ app)

The ESP32 converts frames to JSON text messages in **human units** (it does the ÷100 and the IEEE formatting). IEEE is a 16-digit upper-case hex string without `0x`.

WBA → app:
```json
{"type":"telemetry","ieee":"0080E1FFFE32FC28","tempC":61.25,"setpointC":65.0,"flowLpm":3.4,"fanPct":55,"batteryMv":5800,"mode":"active","faults":["overtemp"]}
{"type":"deviceJoined","ieee":"0080E1FFFE32FC28","shortAddr":"D04B"}
{"type":"deviceOffline","ieee":"0080E1FFFE32FC28"}
{"type":"result","id":7,"ok":false,"error":"outOfRange"}
```
App → WBA (the app picks `id`; the ESP32 maps it to SEQ and answers with `result`):
```json
{"type":"setSetpoint","id":7,"ieee":"all","setpointC":65.0}
{"type":"permitJoin","id":8,"seconds":180}
```
- `mode`: `"idle"` / `"active"`. `faults`: list of `"overtemp"`, `"ntc"`, `"noFlow"`. `error`: camelCase of the NACK code name, or `"timeout"` if the ESP32 got no ACK/NACK within 1 s.
- Changing anything in §9 after 2026-11-24 needs the ESP32, the app and the master updated together.

## 10. Fail-safe (minimum for v1)

- NTC reading out of range → fault flag, fan 100 %, report immediately.
- Loop temperature ≥ 80 °C (`FK_OVERTEMP_C` in `fk_config.h`) → `OVERTEMP` fault flag, fan 100 %, report immediately; the app shows an alarm so the operator unplugs the heater (it is not MCU-controlled). Clears below the threshold minus a hysteresis.
- Slave loses its parent → keep running PID on the last setpoint, rejoin with backoff.
- No flow for > 5 s → fault flag + report (pump stays on; it is not MCU-controlled).
- IWDG watchdog policy: after the demo.

## 11. Open questions

1. Which PID implementation: own PI in `common/pid.c` (default) or CMSIS-DSP `arm_pid_f32`. Decide at brick B6.
2. Does LPTIM2 PWM on PA1 keep running in Stop 1? Test at B9; fallback is Sleep while the fan runs.
3. Engine-running detection (12 V level to ADC) for a future board revision.
4. Install codes instead of the default TC link key.
5. Facts still not checked against RM0493: LPTIM2 in Stop 1, ADC4 autonomy, Standby under FreeRTOS.
6. How to widen the Thermostat setpoint limits from ST's default 16–32 °C to 30–95 °C (attribute limits in `ZbZclThermServerAlloc` or our own range check). Decide at B7.

Resolved 2026-11-19: transport (SoftAP + WebSocket), YF-B7 power policy (§7), flow resolution and fan duty (`0xFC00`, §8), Thermostat shipped by ST, fail-safe minimum (§10), USART instances (§2).
