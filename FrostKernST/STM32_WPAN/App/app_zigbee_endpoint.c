/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_zigbee_endpoint.c
  * Description        : Zigbee Application to manage endpoints and these clusters.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <assert.h>
#include <stdint.h>

#include "app_common.h"
#include "app_conf.h"
#include "log_module.h"
#include "app_entry.h"
#include "app_zigbee.h"
#include "dbg_trace.h"
#include "ieee802154_enums.h"
#include "mcp_enums.h"

#include "stm32_lpm.h"
#include "stm32_rtos.h"
#include "stm32_timer.h"
#include "stm32_lpm_if.h"

#include "zigbee.h"
#include "zigbee.nwk.h"
#include "zigbee.security.h"

/* Private includes -----------------------------------------------------------*/
#include "zcl/zcl.h"
#include "zcl/general/zcl.onoff.h"

/* USER CODE BEGIN PI */
#include "app_network_config.h"
#include "app_ipc.h"
#include "zcl/general/zcl.temp.meas.h"
/* USER CODE END PI */

/* Private defines -----------------------------------------------------------*/
#define APP_ZIGBEE_CHANNEL                APP_NETWORK_CHANNEL
#define APP_ZIGBEE_CHANNEL_MASK           ( 1u << APP_ZIGBEE_CHANNEL )
#define APP_ZIGBEE_TX_POWER               ((int8_t) 0)    /* TX-Power is at 0 dBm by default. */

#define APP_ZIGBEE_ENDPOINT               APP_NETWORK_ENDPOINT
#define APP_ZIGBEE_PROFILE_ID             ZCL_PROFILE_HOME_AUTOMATION
#define APP_ZIGBEE_DEVICE_ID              ZCL_DEVICE_ONOFF_LIGHT
#define APP_ZIGBEE_GROUP_ADDRESS          0x0001u

#define APP_ZIGBEE_CLUSTER_ID             ZCL_CLUSTER_ONOFF
#define APP_ZIGBEE_CLUSTER_NAME           "OnOff Server"

/* USER CODE BEGIN PD */
/* Valores provisionales del ejemplo de ST. */
#define APP_ZB_TEMP_MIN_CENTI_C 100
#define APP_ZB_TEMP_MAX_CENTI_C 10000
#define APP_ZB_TEMP_TOLERANCE_CENTI_C 100U
/* USER CODE END PD */

// -- Redefine Clusters to better code read --
#define OnOffServer                       pstZbCluster[0]

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private constants ---------------------------------------------------------*/
/* USER CODE BEGIN PC */

/* USER CODE END PC */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

/* OnOff Server Callbacks */
static enum ZclStatusCodeT  APP_ZIGBEE_OnOffServerOffCallback               ( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg );
static enum ZclStatusCodeT  APP_ZIGBEE_OnOffServerOnCallback                ( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg );
static enum ZclStatusCodeT  APP_ZIGBEE_OnOffServerToggleCallback            ( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg );

static struct ZbZclOnOffServerCallbacksT stOnOffServerCallbacks =
{
  .off = APP_ZIGBEE_OnOffServerOffCallback,
  .on = APP_ZIGBEE_OnOffServerOnCallback,
  .toggle = APP_ZIGBEE_OnOffServerToggleCallback,
};

/* USER CODE BEGIN PFP */
#if APP_IS_MASTER

static void APP_ZIGBEE_TemperatureReport(
    struct ZbZclClusterT *cluster,
    struct ZbZclHeaderT *header,
    struct ZbApsdeDataIndT *data_ind,
    uint16_t attribute_id,
    enum ZclDataTypeT data_type,
    const uint8_t *payload,
    uint16_t length,
    bool *discard);

#endif
/* USER CODE END PFP */

/* Functions Definition ------------------------------------------------------*/

/**
 * @brief  Zigbee application initialization
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_ApplicationInit(void)
{
  LOG_INFO_APP( "ZIGBEE Application Init" );

  /* Initialization of the Zigbee stack */
  APP_ZIGBEE_Init();

  /* Configure Application Form/Join parameters : Startup, Persistence and Start with/without Form/Join */
  stZigbeeAppInfo.eStartupControl = ZbStartTypeForm;
  stZigbeeAppInfo.bPersistNotification = false;
  stZigbeeAppInfo.bNwkStartup = true;

  /* USER CODE BEGIN APP_ZIGBEE_ApplicationInit */
#if APP_IS_MASTER
  stZigbeeAppInfo.eStartupControl = ZbStartTypeForm;
#else
  stZigbeeAppInfo.eStartupControl = ZbStartTypeJoin;
#endif
  LOG_INFO_APP( "FrostKern role: %s", APP_NETWORK_ROLE_NAME );

  /* USER CODE END APP_ZIGBEE_ApplicationInit */

  /* Initialize Zigbee stack layers */
  APP_ZIGBEE_StackLayersInit();
}

/**
 * @brief  Zigbee application start
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_ApplicationStart( void )
{
  /* USER CODE BEGIN APP_ZIGBEE_ApplicationStart */
	AppIpc_SetDeviceIeee(ZbExtendedAddress(stZigbeeAppInfo.pstZigbee));
	AppIpc_MarkNetworkStarted();
  LOG_INFO_APP( "FrostKern network ready: %s, short address 0x%04X",
                APP_NETWORK_ROLE_NAME, ZbShortAddress( stZigbeeAppInfo.pstZigbee ) );
#if APP_IS_MASTER && APP_NETWORK_OPEN_JOIN_ON_BOOT
  APP_ZIGBEE_PermitJoin( APP_NETWORK_PERMIT_JOIN_SECONDS );
  LOG_INFO_APP( "Bring-up join window: %u seconds", APP_NETWORK_PERMIT_JOIN_SECONDS );
#endif

  /* USER CODE END APP_ZIGBEE_ApplicationStart */

#if ( CFG_LPM_LEVEL != 0)
  /* Authorize LowPower now */
  UTIL_LPM_SetMaxMode( 1 << CFG_LPM_APP, UTIL_LPM_MAX_MODE );
#endif /* CFG_LPM_LEVEL */
}

/**
 * @brief  Configure Zigbee application endpoints
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_ConfigEndpoints(void)
{
  struct ZbApsmeAddEndpointReqT   stRequest;
  struct ZbApsmeAddEndpointConfT  stConfig;
  /* USER CODE BEGIN APP_ZIGBEE_ConfigEndpoints1 */

  /* USER CODE END APP_ZIGBEE_ConfigEndpoints1 */

  /* Add EndPoint */
  memset( &stRequest, 0, sizeof( stRequest ) );
  memset( &stConfig, 0, sizeof( stConfig ) );

  stRequest.profileId = APP_ZIGBEE_PROFILE_ID;
  stRequest.deviceId = APP_ZIGBEE_DEVICE_ID;
  stRequest.endpoint = APP_ZIGBEE_ENDPOINT;
  ZbZclAddEndpoint( stZigbeeAppInfo.pstZigbee, &stRequest, &stConfig );
  assert( stConfig.status == ZB_STATUS_SUCCESS );

  /* Add OnOff Server Cluster */
  stZigbeeAppInfo.OnOffServer = ZbZclOnOffServerAlloc( stZigbeeAppInfo.pstZigbee, APP_ZIGBEE_ENDPOINT, &stOnOffServerCallbacks, NULL );
  assert( stZigbeeAppInfo.OnOffServer != NULL );
  if ( ZbZclClusterEndpointRegister( stZigbeeAppInfo.OnOffServer ) == false )
  {
    LOG_ERROR_APP( "Error during OnOff Server Endpoint Register." );
  }

  /* USER CODE BEGIN APP_ZIGBEE_ConfigEndpoints2 */

  #if APP_IS_MASTER

      stZigbeeAppInfo.temperatureCluster =
          ZbZclTempMeasClientAlloc(
              stZigbeeAppInfo.pstZigbee,
              APP_ZIGBEE_ENDPOINT);

  #else

      stZigbeeAppInfo.temperatureCluster =
          ZbZclTempMeasServerAlloc(
              stZigbeeAppInfo.pstZigbee,
              APP_ZIGBEE_ENDPOINT,
              APP_ZB_TEMP_MIN_CENTI_C,
              APP_ZB_TEMP_MAX_CENTI_C,
              APP_ZB_TEMP_TOLERANCE_CENTI_C);

  #endif

      if (stZigbeeAppInfo.temperatureCluster == NULL)
      {
          Error_Handler();
          return;
      }

  #if APP_IS_SLAVE

      enum ZclStatusCodeT status =
          ZbZclAttrIntegerWrite(
              stZigbeeAppInfo.temperatureCluster,
              ZCL_TEMP_MEAS_ATTR_MEAS_VAL,
              (int16_t)ZCL_TEMP_MEAS_UNKNOWN);

      if (status != ZCL_STATUS_SUCCESS)
      {
          Error_Handler();
          return;
      }

  #endif

      if (!ZbZclClusterEndpointRegister(
              stZigbeeAppInfo.temperatureCluster))
      {
          Error_Handler();
          return;
      }
#if APP_IS_MASTER
    stZigbeeAppInfo.temperatureCluster->report =
        APP_ZIGBEE_TemperatureReport;
#endif

  /* USER CODE END APP_ZIGBEE_ConfigEndpoints2 */
}

/**
 * @brief  Set Group Addressing mode (if used)
 * @param  None
 * @retval 'true' if Group Address used else 'false'.
 */
bool APP_ZIGBEE_ConfigGroupAddr( void )
{
#if APP_IS_SLAVE
  return false;
#else
  struct ZbApsmeAddGroupReqT  stRequest;
  struct ZbApsmeAddGroupConfT stConfig;

  memset( &stRequest, 0, sizeof( stRequest ) );

  stRequest.endpt = APP_ZIGBEE_ENDPOINT;
  stRequest.groupAddr = APP_ZIGBEE_GROUP_ADDRESS;
  ZbApsmeAddGroupReq( stZigbeeAppInfo.pstZigbee, &stRequest, &stConfig );

  return true;
#endif
}

/**
 * @brief  Return the Startup Configuration
 * @param  pstConfig  Configuration structure to fill
 * @retval None
 */
void APP_ZIGBEE_GetStartupConfig( struct ZbStartupT * pstConfig )
{
  /* Attempt to join a zigbee network */
  ZbStartupConfigGetProDefaults( pstConfig );

  /* Using the default HA preconfigured Link Key */
  memcpy( pstConfig->security.preconfiguredLinkKey, sec_key_ha, ZB_SEC_KEYSIZE );

  /* Setting up additional startup configuration parameters */
  pstConfig->startupControl = stZigbeeAppInfo.eStartupControl;
  pstConfig->channelList.count = 1;
  pstConfig->channelList.list[0].page = 0;
  pstConfig->channelList.list[0].channelMask = APP_ZIGBEE_CHANNEL_MASK;

  /* Set the TX-Power */
  if ( APP_ZIGBEE_SetTxPower( APP_ZIGBEE_TX_POWER ) == false )
  {
    LOG_ERROR_APP( "Switching to %d dB failed.", APP_ZIGBEE_TX_POWER );
    return;
  }

  /* USER CODE BEGIN APP_ZIGBEE_GetStartupConfig */
#if APP_IS_SLAVE
  /* Mismo patron que Zigbee_TempMeas_Server_SED v1.10.0; ademas
   * anunciamos alimentacion por bateria. RX apagada cuando esta inactivo.
   * Esto configura el rol de radio, no habilita Stop 1 de la CPU.
   */
  pstConfig->capability &= (uint8_t) ~( MCP_ASSOC_CAP_RXONIDLE |
      MCP_ASSOC_CAP_DEV_TYPE | MCP_ASSOC_CAP_ALT_COORD | MCP_ASSOC_CAP_PWR_SRC );
  pstConfig->endDeviceTimeout = APP_NETWORK_SED_TIMEOUT_CODE;
#else
  pstConfig->capability |= MCP_ASSOC_CAP_RXONIDLE |
      MCP_ASSOC_CAP_DEV_TYPE | MCP_ASSOC_CAP_PWR_SRC;
#endif

  /* USER CODE END APP_ZIGBEE_GetStartupConfig */
}

/**
 * @brief  Manage a New Device on Network (called only if Coord or Router).
 * @param  iShortAddress      Short Address of new Device
 * @param  dlExtendedAddress  Extended Address of new Device
 * @param  cCapability        Capability of new Device
 * @retval Group Address
 */
void APP_ZIGBEE_SetNewDevice( uint16_t iShortAddress, uint64_t dlExtendedAddress, uint8_t cCapability )
{
  LOG_INFO_APP( "New Device (%d) on Network : with Extended ( " LOG_DISPLAY64() " ) and Short ( 0x%04X ) Address.", cCapability, LOG_NUMBER64( dlExtendedAddress ), iShortAddress );

  /* USER CODE BEGIN APP_ZIGBEE_SetNewDevice */
#if APP_IS_MASTER
    if (!AppIpc_RequestTemperatureReports(
            iShortAddress,
            dlExtendedAddress))
    {
        LOG_ERROR_APP(
            "Could not queue temperature report configuration");
    }
#endif
  /* USER CODE END APP_ZIGBEE_SetNewDevice */
}

/**
 * @brief  Print application information to the console
 * @param  None
 * @retval None
 */
void APP_ZIGBEE_PrintApplicationInfo(void)
{
  LOG_INFO_APP( "**********************************************************" );
  LOG_INFO_APP( "Network config : %s", APP_NETWORK_ROLE_NAME );

  /* USER CODE BEGIN APP_ZIGBEE_PrintApplicationInfo1 */

  /* USER CODE END APP_ZIGBEE_PrintApplicationInfo1 */
  LOG_INFO_APP( "Channel used: %d.", APP_ZIGBEE_CHANNEL );

  APP_ZIGBEE_PrintGenericInfo();

  LOG_INFO_APP( "Clusters allocated are:" );
  LOG_INFO_APP( "  %s on Endpoint %d.", APP_ZIGBEE_CLUSTER_NAME, APP_ZIGBEE_ENDPOINT );

  /* USER CODE BEGIN APP_ZIGBEE_PrintApplicationInfo2 */

  /* USER CODE END APP_ZIGBEE_PrintApplicationInfo2 */

  LOG_INFO_APP( "**********************************************************" );
}

/**
 * @brief  OnOff Server 'Off' command Callback
 */
static enum ZclStatusCodeT APP_ZIGBEE_OnOffServerOffCallback( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg )
{
  enum ZclStatusCodeT   eStatus = ZCL_STATUS_SUCCESS;
  /* USER CODE BEGIN APP_ZIGBEE_OnOffServerOffCallback */

  /* USER CODE END APP_ZIGBEE_OnOffServerOffCallback */
  return eStatus;
}

/**
 * @brief  OnOff Server 'On' command Callback
 */
static enum ZclStatusCodeT APP_ZIGBEE_OnOffServerOnCallback( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg )
{
  enum ZclStatusCodeT   eStatus = ZCL_STATUS_SUCCESS;
  /* USER CODE BEGIN APP_ZIGBEE_OnOffServerOnCallback */

  /* USER CODE END APP_ZIGBEE_OnOffServerOnCallback */
  return eStatus;
}

/**
 * @brief  OnOff Server 'Toggle' command Callback
 */
static enum ZclStatusCodeT APP_ZIGBEE_OnOffServerToggleCallback( struct ZbZclClusterT * pstCluster, struct ZbZclAddrInfoT * pstSrcInfo, void * arg )
{
  enum ZclStatusCodeT   eStatus = ZCL_STATUS_SUCCESS;
  /* USER CODE BEGIN APP_ZIGBEE_OnOffServerToggleCallback */

  /* USER CODE END APP_ZIGBEE_OnOffServerToggleCallback */
  return eStatus;
}

/* USER CODE BEGIN FD_LOCAL_FUNCTIONS */
#if APP_IS_MASTER

static void APP_ZIGBEE_TemperatureReport(
    struct ZbZclClusterT *cluster,
    struct ZbZclHeaderT *header,
    struct ZbApsdeDataIndT *data_ind,
    uint16_t attribute_id,
    enum ZclDataTypeT data_type,
    const uint8_t *payload,
    uint16_t length,
    bool *discard)
{
    UNUSED(cluster);
    UNUSED(header);
    UNUSED(discard);

    if ((data_ind == NULL) ||
        (payload == NULL) ||
        (qZbCommand == NULL))
    {
        return;
    }

    if ((data_ind->dst.endpoint != APP_ZIGBEE_ENDPOINT) ||
        (data_ind->src.endpoint != APP_ZIGBEE_ENDPOINT))
    {
        return;
    }

    if ((attribute_id != ZCL_TEMP_MEAS_ATTR_MEAS_VAL) ||
        (data_type != ZCL_DATATYPE_SIGNED_16BIT) ||
        (length != 2U))
    {
        return;
    }

    enum ZclStatusCodeT status;

    int16_t temperature =
        (int16_t)ZbZclParseInteger(
            data_type,
            payload,
            &status);

    if (status != ZCL_STATUS_SUCCESS)
    {
        return;
    }

    AppZbCommand command = {0};

    command.type = APP_ZB_COMMAND_TEMP_RECEIVED;
    command.short_address = 0xFFFFU;

    command.temperature_centi_c = temperature;

    command.temperature_valid =
        (temperature != (int16_t)ZCL_TEMP_MEAS_UNKNOWN);

    if (data_ind->src.mode == ZB_APSDE_ADDRMODE_EXT)
    {
        command.ieee_address = data_ind->src.extAddr;
    }
    else if (
        data_ind->src.mode == ZB_APSDE_ADDRMODE_SHORT)
    {
        command.short_address = data_ind->src.nwkAddr;
    }
    else
    {
        return;
    }

    if (xQueueSend(qZbCommand, &command, 0) != pdPASS)
    {
        LOG_ERROR_APP(
            "Temperature report dropped: command queue full");
    }
}

#endif

void APP_ZIGBEE_UpdateTemperature(void)
{
#if APP_IS_SLAVE
    AppStatus localStatus;

    if (stZigbeeAppInfo.temperatureCluster == NULL)
    {
        return;
    }

    if (!AppIpc_ReadLocalStatus(&localStatus))
    {
        return;
    }

    int16_t temperature = (int16_t)ZCL_TEMP_MEAS_UNKNOWN;

    if ((localStatus.latest_sample.valid &
         APP_SAMPLE_VALID_TEMP) != 0U)
    {
        temperature =
            localStatus.latest_sample.temperature_centi_c;
    }

    enum ZclStatusCodeT result =
        ZbZclAttrIntegerWrite(
            stZigbeeAppInfo.temperatureCluster,
            ZCL_TEMP_MEAS_ATTR_MEAS_VAL,
            temperature);

    if (result != ZCL_STATUS_SUCCESS)
    {
        LOG_ERROR_APP(
            "Temperature attribute update failed: 0x%02X",
            (unsigned int)result);
    }
#endif
}


#if APP_IS_MASTER

static void APP_ZIGBEE_TemperatureReportConfigCallback(
    struct ZbZclCommandRspT *response,
    void *argument)
{
    UNUSED(argument);

    if (response == NULL)
    {
        return;
    }

    if ((response->aps_status != ZB_STATUS_SUCCESS) ||
        (response->status != ZCL_STATUS_SUCCESS))
    {
        LOG_ERROR_APP(
            "Temperature report config failed: APS=%u, ZCL=%u",
            (unsigned int)response->aps_status,
            (unsigned int)response->status);
        return;
    }

    if ((response->hdr.cmdId !=
         ZCL_COMMAND_CONFIG_REPORTING_RESPONSE) ||
        (response->payload == NULL) ||
        (response->length == 0U))
    {
        LOG_ERROR_APP(
            "Invalid temperature report config response");
        return;
    }

    /*
     * Para nuestro unico atributo, una respuesta de exito
     * contiene un solo byte con ZCL_STATUS_SUCCESS.
     */
    if ((response->length != 1U) ||
        (response->payload[0] != ZCL_STATUS_SUCCESS))
    {
        LOG_ERROR_APP(
            "Temperature report config rejected: status=%u",
            (unsigned int)response->payload[0]);
        return;
    }

    LOG_INFO_APP(
        "Temperature report configuration accepted");
}

#endif

void APP_ZIGBEE_ConfigureTemperatureReports(
    uint16_t shortAddress)
{
#if APP_IS_MASTER

    if ((stZigbeeAppInfo.temperatureCluster == NULL) ||
        (shortAddress >= 0xFFF8U))
    {
        LOG_ERROR_APP(
            "Invalid temperature report config destination");
        return;
    }

    struct ZbZclAttrReportConfigT config = {0};

    config.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
    config.dst.endpoint = APP_ZIGBEE_ENDPOINT;
    config.dst.nwkAddr = shortAddress;

    config.num_records = 1U;

    config.record_list[0].direction =
        ZCL_REPORT_DIRECTION_NORMAL;

    config.record_list[0].attr_id =
        ZCL_TEMP_MEAS_ATTR_MEAS_VAL;

    config.record_list[0].attr_type =
        ZCL_DATATYPE_SIGNED_16BIT;

    config.record_list[0].min = 10U;
    config.record_list[0].max = 60U;
    config.record_list[0].change = 50.0;

    enum ZclStatusCodeT result =
        ZbZclAttrReportConfigReq(
            stZigbeeAppInfo.temperatureCluster,
            &config,
            APP_ZIGBEE_TemperatureReportConfigCallback,
            NULL);

    if (result != ZCL_STATUS_SUCCESS)
    {
        LOG_ERROR_APP(
            "Could not send temperature report config: %u",
            (unsigned int)result);
        return;
    }

    LOG_INFO_APP(
        "Temperature report config requested for 0x%04X",
        (unsigned int)shortAddress);

#else

    (void)shortAddress;

#endif
}
/* USER CODE END FD_LOCAL_FUNCTIONS */
