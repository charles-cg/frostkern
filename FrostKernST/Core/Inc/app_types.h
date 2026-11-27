#ifndef FROSTKERN_APP_TYPES_H
#define FROSTKERN_APP_TYPES_H

#include <stdint.h>

/* Contrato de datos interno, version 1. Sin dependencias de HAL o FreeRTOS.
 * NO transmitir structs crudos por UART/Zigbee: padding, enums y orden de
 * bytes no constituyen un protocolo. Serializar cada campo explicitamente.
 */
#define APP_DATA_CONTRACT_VERSION 2U
#define APP_SLAVE_COUNT           2U
#define APP_NODE_MASTER           0U
#define APP_NODE_SLAVE_1          1U
#define APP_NODE_SLAVE_2          2U
#define APP_NODE_UNASSIGNED       254U
#define APP_NODE_ALL_SLAVES       255U /* Solo destino de comandos. */
#define APP_DUTY_MAX_PERMILLE      1000U
#define APP_BATTERY_PERCENT_UNKNOWN 255U

typedef enum {
    APP_DEVICE_ROLE_UNKNOWN = 0,
    APP_DEVICE_ROLE_MASTER  = 1,
    APP_DEVICE_ROLE_SLAVE   = 2
} AppDeviceRole;

typedef struct {
    AppDeviceRole role;
    uint8_t node_id; /* 0 master, 1/2 slaves, 254 aun sin asignar. */
    /* IEEE/EUI-64, byte mas significativo primero en este contrato.
     * Adaptar al orden utilizado por las APIs de ST en la capa Zigbee.
     * La direccion corta Zigbee NO se usa como identidad permanente.
     */
    uint8_t ieee_address[8];
} AppDeviceId;

typedef uint32_t AppSampleValidFlags;
#define APP_SAMPLE_VALID_TEMP     (UINT32_C(1) << 0)
#define APP_SAMPLE_VALID_FLOW     (UINT32_C(1) << 1)
#define APP_SAMPLE_VALID_BATTERY  (UINT32_C(1) << 2)

typedef struct {
    uint8_t source_node_id;    /* Slave 1/2; 254 antes de asignar su ID. */
    uint32_t sequence;         /* Aumenta por muestra; reinicia al arrancar. */
    /* Internal handshake only; not serialized over Zigbee/UART. A refresh
     * generation is acknowledged after taking a NEW complete flow window. */
    uint32_t refresh_generation;
    uint32_t timestamp_ms;     /* Tiempo local desde arranque, modulo 2^32. */
    AppSampleValidFlags valid; /* Un campo sin bit valido NO se interpreta. */
    int16_t temperature_centi_c; /* 2534 = 25.34 grados C. */
    uint32_t flow_ml_min;      /* 1500 = 1.5 L/min; no son unidades ZCL. */
    uint16_t battery_mv;       /* 5800 = 5.8 V de bateria, no del pin ADC. */
    uint8_t battery_percent;   /* 0..100; 255 si aun no hay estimacion. */
} AppSample;

/* Modo solicitado. ACTIVE/IDLE son estados calculados, no roles de red.
 * OFF solicita apagar ventilador/bomba, sujeto a la politica de seguridad
 * que acuerde el equipo. AUTO solicita control local automatico.
 */
typedef enum {
    APP_CONTROL_OFF  = 0,
    APP_CONTROL_AUTO = 1
} AppControlMode;

typedef struct {
    uint8_t target_node_id;       /* Slave 1/2 o APP_NODE_ALL_SLAVES. */
    uint32_t request_id;          /* Correlacion con confirmacion/rechazo. */
    int16_t setpoint_centi_c;     /* 4500 = 45.00 grados C. */
    AppControlMode mode;
} AppConfig;

typedef struct {
    uint16_t fan_duty_permille; /* 0 apagado, 500 mitad, 1000 maximo. */
    uint8_t pump_on;            /* Exactamente 0 o 1. */
} AppActuatorState;

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_OFF,
    APP_STATE_IDLE,
    APP_STATE_ACTIVE,
    APP_STATE_FAULT
} AppRuntimeState;

typedef uint32_t AppFaultFlags;
#define APP_FAULT_NONE             UINT32_C(0)
#define APP_FAULT_TEMPERATURE      (UINT32_C(1) << 0)
#define APP_FAULT_FLOW_SENSOR      (UINT32_C(1) << 1)
#define APP_FAULT_LOW_FLOW         (UINT32_C(1) << 2)
#define APP_FAULT_LOW_BATTERY      (UINT32_C(1) << 3)
#define APP_FAULT_LINK_LOST        (UINT32_C(1) << 4)
#define APP_FAULT_STALE_SAMPLE     (UINT32_C(1) << 5)
#define APP_FAULT_ACTUATOR         (UINT32_C(1) << 6)
#define APP_FAULT_INVALID_CONFIG   (UINT32_C(1) << 7)

/* Ultimo estado conocido de UN slave. En master mantener una entrada por
 * slave. Los valores iniciales cero NO representan mediciones validas.
 * applied_config representa lo que el slave confirmo que esta aplicando,
 * no simplemente el ultimo comando que el master intento enviar.
 */
typedef struct {
    AppDeviceId device;
    AppSample latest_sample;
    AppConfig applied_config;
    AppActuatorState actuators;
    AppRuntimeState state;
    AppFaultFlags faults;
    uint32_t updated_at_ms; /* Ultimo reporte de cualquier medicion. */
    /* Master: cada variable tiene su propia frescura. No se persisten muestras. */
    uint32_t temperature_report_at_ms;
    uint32_t flow_report_at_ms;
    AppSampleValidFlags reports_seen; /* Incluye reportes recibidos no validos. */
} AppStatus;

/* Comprobar rangos, valores enum y destino ANTES de aplicar un comando.
 * Estos tipos no deciden limites termicos ni acciones ante fallas.
 */

#endif /* FROSTKERN_APP_TYPES_H */
