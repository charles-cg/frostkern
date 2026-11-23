cmake_minimum_required(VERSION 3.22)
# Enable CMake support for ASM and C languages
enable_language(C ASM)
# STM32CubeMX generated symbols (macros)
set(MX_Defines_Syms 
	USE_FULL_LL_DRIVER 
	PHY_40nm_3_00_a 
	MAC 
	MAC_LAYER 
	DISABLE_RFTS_EXT_EVNT_HNDLR=1 
	USE_HAL_DRIVER 
	STM32WBA55xx
    $<$<CONFIG:Debug>:DEBUG>
)

# STM32CubeMX generated include paths
set(MX_Include_Dirs
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Log
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Debug_GPIO
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/CRC_Ctrl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/porting
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/src/shrd_utils/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/clusters/se
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zgp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/stack
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/stack/port
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Inc/Legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Log
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/SerialCmdInterpreter
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/tim_serv
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/config/ieee_15_4_basic
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Device/ST/STM32WBAxx/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/_40nm_reg_files
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/inc/ot_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/mac_802_15_4/core/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl/general
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl/key
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl/se
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl/security
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/include/zcl/zd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/stack/port/mac
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/stack/port/stm32wba
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/CMSIS/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/CMSIS/RTOS2/Include/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Board
)

# STM32CubeMX generated application sources
set(MX_Application_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/app_zigbee_endpoint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/app_zigbee.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/app_zigbee_persistence_nvm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/App/app_zigbee_utility.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/power_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/linklayer_plat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/ll_sys_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/mac_sys_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/zigbee_plat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../STM32_WPAN/Target/zigbee_sys_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Interfaces/stm32_lpm_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/CRC_Ctrl/crc_ctrl_conf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/Debug_GPIO/app_debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower/user_low_power_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../System/Config/LowPower/peripheral_init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/app_freertos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/app_entry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wbaxx_it.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/stm32wbaxx_hal_msp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/sysmem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/syscalls.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../startup_stm32wba55xx.s
)

# STM32 HAL/LL Drivers
set(STM32_Drivers_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Src/system_stm32wbaxx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_pka.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rcc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_flash_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_dma_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_pwr_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_exti.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_crc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_icache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_ramcfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rng_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_rtc_ex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_tim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_ll_dma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Drivers/STM32WBAxx_HAL_Driver/Src/stm32wbaxx_hal_uart_ex.c
)

# Drivers Midllewares


set(Utilities_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/trace/adv_trace/stm32_adv_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_systime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_sscanf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/misc/stm32_tiny_vsnprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/tim_serv/stm32_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Utilities/lpm/tiny_lpm/stm32_lpm.c
)
set(Board_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Board/stm32wbaxx_nucleo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Board/app_bsp.c
)
set(RTOS2_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/croutine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/tasks.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/timers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
)
set(STM32_WPAN_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_cs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_dp_slp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_intf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_sys/src/ll_sys_startup.c
)
set(Common_Src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Startup/stm32wbaxx_ResetHandler_GCC.s
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/SerialCmdInterpreter/serial_cmd_interpreter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug/RTDebug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/RTDebug/RTDebug_dtb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/Log/log_module.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_cmac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_ccm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/BasicAES/baes_ecb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/app_sys.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/crc_ctrl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/otp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/scm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/stm_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/stm_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Modules/wpan_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/adv_trace_usart_if.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_aes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_otp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/hw_rng.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Projects/Common/WPAN/Interfaces/timer_if.c
)

# Link directories setup
set(MX_LINK_DIRS

    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/link_layer/ll_cmd_lib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/mac_802_15_4/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/stack
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/ST/STM32_WPAN/zigbee/clusters
)
# Project static libraries
set(MX_LINK_LIBS 
 "-Wl,--start-group" 
	:LinkLayer_802_15_4_V3_0.a 
	:wba_mac_lib.a 
	:ZigBeeProR23_FFD.a 
	:ZigBeeClusters.a
 "-Wl,--end-group"
    STM32_Drivers
    ${TOOLCHAIN_LINK_LIBRARIES}
    Utilities	Board	RTOS2	STM32_WPAN	Common	
)
# Interface library for includes and symbols
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE ${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE ${MX_Defines_Syms})

# Create STM32_Drivers static library
add_library(STM32_Drivers OBJECT)
target_sources(STM32_Drivers PRIVATE ${STM32_Drivers_Src})
target_link_libraries(STM32_Drivers PUBLIC stm32cubemx)


# Create Utilities static library
add_library(Utilities OBJECT)
target_sources(Utilities PRIVATE ${Utilities_Src})
target_link_libraries(Utilities PUBLIC stm32cubemx)

# Create Board static library
add_library(Board OBJECT)
target_sources(Board PRIVATE ${Board_Src})
target_link_libraries(Board PUBLIC stm32cubemx)

# Create RTOS2 static library
add_library(RTOS2 OBJECT)
target_sources(RTOS2 PRIVATE ${RTOS2_Src})
target_link_libraries(RTOS2 PUBLIC stm32cubemx)

# Create STM32_WPAN static library
add_library(STM32_WPAN OBJECT)
target_sources(STM32_WPAN PRIVATE ${STM32_WPAN_Src})
target_link_libraries(STM32_WPAN PUBLIC stm32cubemx)

# Create Common static library
add_library(Common OBJECT)
target_sources(Common PRIVATE ${Common_Src})
target_link_libraries(Common PUBLIC stm32cubemx)

# Add STM32CubeMX generated application sources to the project
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MX_Application_Src})

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE ${MX_LINK_DIRS})

# Add libraries to the project
target_link_libraries(${CMAKE_PROJECT_NAME} ${MX_LINK_LIBS})

# Add the map file to the list of files to be removed with 'clean' target
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_PROJECT_NAME}.map)

# Validate that STM32CubeMX code is compatible with C standard
if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "Generated code requires C11 or higher")
endif()
