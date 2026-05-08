cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(BetaRobot C ASM)

if(DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_TOOLCHAIN_FILE_USED "${CMAKE_TOOLCHAIN_FILE}")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(TARGET_NAME BetaRobot)

function(read_define output file_name define_name)
    file(STRINGS "${file_name}" define_line REGEX "^[ \t]*#[ \t]*define[ \t]+${define_name}[ \t]+[A-Za-z0-9_]+")
    if(NOT define_line)
        message(FATAL_ERROR "Cannot find ${define_name} in ${file_name}")
    endif()
    list(GET define_line 0 define_line)
    string(REGEX REPLACE "^[ \t]*#[ \t]*define[ \t]+${define_name}[ \t]+([A-Za-z0-9_]+).*$" "\\1" define_value "${define_line}")
    set(${output} "${define_value}" PARENT_SCOPE)
endfunction()

function(prefix_paths output base)
    set(result)
    foreach(item IN LISTS ARGN)
        list(APPEND result "${base}/${item}")
    endforeach()
    set(${output} "${result}" PARENT_SCOPE)
endfunction()

read_define(APP_NAME "${CMAKE_SOURCE_DIR}/user_cfg.h" APP_NAME)
set(APP_DIR "${CMAKE_SOURCE_DIR}/app/${APP_NAME}")

if(NOT EXISTS "${APP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "App ${APP_NAME} is not initialized or has no CMakeLists.txt: ${APP_DIR}")
endif()

read_define(DEVELOPMENT_BOARD "${APP_DIR}/app_cfg.h" DEVELOPMENT_BOARD)
set(BOARD "${DEVELOPMENT_BOARD}")
set(HAL_DIR "${CMAKE_SOURCE_DIR}/hal/${BOARD}")

if(NOT EXISTS "${HAL_DIR}")
    message(FATAL_ERROR "Unknown DEVELOPMENT_BOARD: ${DEVELOPMENT_BOARD}")
endif()

include("${APP_DIR}/CMakeLists.txt")
prefix_paths(APP_SOURCE_FILES "${APP_DIR}" ${APP_SOURCES})
prefix_paths(APP_INCLUDE_PATHS "${APP_DIR}" ${APP_INCLUDE_DIRS})

if(BOARD STREQUAL "STM32F407VET6")
    set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
    set(BOARD_DEFINES USE_HAL_DRIVER STM32F407xx)
    set(DSP_DEFINE ARM_MATH_CM4)
    set(LINKER_SCRIPT "${HAL_DIR}/STM32F407XX_FLASH.ld")
    set(STARTUP_FILE "${HAL_DIR}/startup_stm32f407xx.s")
    set(HAL_C_SOURCES
        Core/Src/gpio.c
        Core/Src/freertos.c
        Core/Src/dma.c
        Core/Src/tim.c
        Core/Src/usart.c
        Core/Src/stm32f4xx_it.c
        Core/Src/stm32f4xx_hal_msp.c
        Core/Src/stm32f4xx_hal_timebase_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
        Core/Src/system_stm32f4xx.c
        Middlewares/Third_Party/FreeRTOS/Source/croutine.c
        Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
        Middlewares/Third_Party/FreeRTOS/Source/list.c
        Middlewares/Third_Party/FreeRTOS/Source/queue.c
        Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
        Middlewares/Third_Party/FreeRTOS/Source/tasks.c
        Middlewares/Third_Party/FreeRTOS/Source/timers.c
        Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
        Core/Src/sysmem.c
        Core/Src/syscalls.c
    )
elseif(BOARD STREQUAL "DM_MC02")
    set(MCU_FLAGS -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard)
    set(BOARD_DEFINES USE_PWR_LDO_SUPPLY USE_HAL_DRIVER STM32H723xx)
    set(DSP_DEFINE ARM_MATH_CM4)
    set(LINKER_SCRIPT "${HAL_DIR}/STM32H723XG_FLASH.ld")
    set(STARTUP_FILE "${HAL_DIR}/startup_stm32h723xx.s")
    set(HAL_C_SOURCES
        Core/Src/gpio.c
        Core/Src/freertos.c
        Core/Src/adc.c
        Core/Src/dma.c
        Core/Src/spi.c
        Core/Src/tim.c
        Core/Src/usart.c
        Core/Src/stm32h7xx_it.c
        Core/Src/stm32h7xx_hal_msp.c
        Core/Src/stm32h7xx_hal_timebase_tim.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_adc_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c
        Core/Src/system_stm32h7xx.c
        Middlewares/Third_Party/FreeRTOS/Source/croutine.c
        Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
        Middlewares/Third_Party/FreeRTOS/Source/list.c
        Middlewares/Third_Party/FreeRTOS/Source/queue.c
        Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
        Middlewares/Third_Party/FreeRTOS/Source/tasks.c
        Middlewares/Third_Party/FreeRTOS/Source/timers.c
        Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
        Core/Src/sysmem.c
        Core/Src/syscalls.c
        Core/Src/fdcan.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_fdcan.c
        Core/Src/i2c.c
    )
elseif(BOARD STREQUAL "DJI_A")
    set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
    set(BOARD_DEFINES USE_HAL_DRIVER STM32F427xx)
    set(DSP_DEFINE ARM_MATH_CM4)
    set(LINKER_SCRIPT "${HAL_DIR}/STM32F427XX_FLASH.ld")
    set(STARTUP_FILE "${HAL_DIR}/startup_stm32f427xx.s")
    set(HAL_C_SOURCES
        Core/Src/gpio.c
        Core/Src/freertos.c
        Core/Src/adc.c
        Core/Src/dma.c
        Core/Src/tim.c
        Core/Src/usart.c
        Core/Src/stm32f4xx_it.c
        Core/Src/stm32f4xx_hal_msp.c
        Core/Src/stm32f4xx_hal_timebase_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
        Core/Src/system_stm32f4xx.c
        Middlewares/Third_Party/FreeRTOS/Source/croutine.c
        Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
        Middlewares/Third_Party/FreeRTOS/Source/list.c
        Middlewares/Third_Party/FreeRTOS/Source/queue.c
        Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
        Middlewares/Third_Party/FreeRTOS/Source/tasks.c
        Middlewares/Third_Party/FreeRTOS/Source/timers.c
        Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
        Core/Src/sysmem.c
        Core/Src/syscalls.c
        Core/Src/can.c
        Core/Src/dac.c
        Core/Src/i2c.c
        Core/Src/spi.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dac_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
    )
elseif(BOARD STREQUAL "DJI_C")
    set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
    set(BOARD_DEFINES USE_HAL_DRIVER STM32F407xx)
    set(DSP_DEFINE ARM_MATH_CM4)
    set(LINKER_SCRIPT "${HAL_DIR}/STM32F407XX_FLASH.ld")
    set(STARTUP_FILE "${HAL_DIR}/startup_stm32f407xx.s")
    set(HAL_C_SOURCES
        Core/Src/gpio.c
        Core/Src/freertos.c
        Core/Src/adc.c
        Core/Src/tim.c
        Core/Src/stm32f4xx_it.c
        Core/Src/stm32f4xx_hal_msp.c
        Core/Src/stm32f4xx_hal_timebase_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_tim_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ramfunc.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c
        Core/Src/system_stm32f4xx.c
        Middlewares/Third_Party/FreeRTOS/Source/croutine.c
        Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
        Middlewares/Third_Party/FreeRTOS/Source/list.c
        Middlewares/Third_Party/FreeRTOS/Source/queue.c
        Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
        Middlewares/Third_Party/FreeRTOS/Source/tasks.c
        Middlewares/Third_Party/FreeRTOS/Source/timers.c
        Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
        Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
        Core/Src/sysmem.c
        Core/Src/syscalls.c
        Core/Src/dma.c
        Core/Src/i2c.c
        Core/Src/spi.c
        Core/Src/usart.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
        Core/Src/can.c
        Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c
    )
endif()

prefix_paths(HAL_SOURCE_FILES "${HAL_DIR}" ${HAL_C_SOURCES})

set(PROJECT_SOURCES
    ${HAL_SOURCE_FILES}
    ${CMAKE_SOURCE_DIR}/hal/main.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_printf.c
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT.c
    ${APP_SOURCE_FILES}
    ${HAL_DIR}/bsp_map.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_dwt/bsp_dwt.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_gpio/bsp_gpio.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_log/bsp_log.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_math/bsp_math.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_spi/bsp_spi.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_tim/bsp_tim.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_usart/bsp_usart.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_adc/bsp_adc.c
    ${CMAKE_SOURCE_DIR}/drv/drv_motor/drv_djimotor/drv_djimotor.c
    ${CMAKE_SOURCE_DIR}/drv/drv_pid/drv_pid.c
    ${CMAKE_SOURCE_DIR}/drv/drv_chassis/drv_chassis.c
    ${CMAKE_SOURCE_DIR}/drv/drv_sbus/drv_sbus.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/CommonTables/arm_common_tables.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/CommonTables/arm_const_structs.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_sin_f32.c
    ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_cos_f32.c
    ${CMAKE_SOURCE_DIR}/drv/drv_bmi088/drv_bmi088.c
    ${CMAKE_SOURCE_DIR}/bsp/bsp_can/bsp_can.c
    ${CMAKE_SOURCE_DIR}/drv/drv_motor/drv_motor_base.c
)

if(BOARD STREQUAL "DJI_A")
    list(APPEND PROJECT_SOURCES ${HAL_DIR}/Drivers/CMSIS/DSP/Source/FastMathFunctions/arm_atan2_f32.c)
endif()

list(APPEND PROJECT_SOURCES
    ${STARTUP_FILE}
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/RTT/SEGGER_RTT_ASM_ARMv7M.s
)

add_executable(${TARGET_NAME} ${PROJECT_SOURCES})
set_target_properties(${TARGET_NAME} PROPERTIES SUFFIX ".elf")

target_compile_definitions(${TARGET_NAME} PRIVATE
    ${BOARD_DEFINES}
    DEVELOPMENT_BOARD=${DEVELOPMENT_BOARD}
    ${DSP_DEFINE}
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${APP_INCLUDE_PATHS}
    ${HAL_DIR}
    ${HAL_DIR}/Core/Inc
    ${HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${HAL_DIR}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    ${HAL_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc
    ${HAL_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    ${HAL_DIR}/Middlewares/Third_Party/FreeRTOS/Source/include
    ${HAL_DIR}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
    ${HAL_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
    ${HAL_DIR}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${HAL_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    ${HAL_DIR}/Drivers/CMSIS/Include
    ${HAL_DIR}/Drivers/CMSIS/DSP/Include
    ${HAL_DIR}/Drivers/CMSIS/DSP/PrivateInclude
    ${CMAKE_SOURCE_DIR}/bsp
    ${CMAKE_SOURCE_DIR}/bsp/bsp_adc
    ${CMAKE_SOURCE_DIR}/bsp/bsp_dwt
    ${CMAKE_SOURCE_DIR}/bsp/bsp_gpio
    ${CMAKE_SOURCE_DIR}/bsp/bsp_log
    ${CMAKE_SOURCE_DIR}/bsp/bsp_math
    ${CMAKE_SOURCE_DIR}/bsp/bsp_module
    ${CMAKE_SOURCE_DIR}/bsp/bsp_spi
    ${CMAKE_SOURCE_DIR}/bsp/bsp_tim
    ${CMAKE_SOURCE_DIR}/bsp/bsp_usart
    ${CMAKE_SOURCE_DIR}/bsp/bsp_can
    ${CMAKE_SOURCE_DIR}/drv
    ${CMAKE_SOURCE_DIR}/drv/drv_bmi088
    ${CMAKE_SOURCE_DIR}/drv/drv_chassis
    ${CMAKE_SOURCE_DIR}/drv/drv_motor
    ${CMAKE_SOURCE_DIR}/drv/drv_motor/drv_dcmotor
    ${CMAKE_SOURCE_DIR}/drv/drv_motor/drv_djimotor
    ${CMAKE_SOURCE_DIR}/drv/drv_pid
    ${CMAKE_SOURCE_DIR}/drv/drv_sbus
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/RTT
    ${CMAKE_SOURCE_DIR}/Middlewares/Third_Party/SEGGER/Config
)

target_compile_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    -Og
    -Wall
    -fdata-sections
    -ffunction-sections
    -g
    -gdwarf-2
    $<$<COMPILE_LANGUAGE:ASM>:-x>
    $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
)

target_link_options(${TARGET_NAME} PRIVATE
    ${MCU_FLAGS}
    -specs=nano.specs
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_BINARY_DIR}/${TARGET_NAME}.map,--cref
    -Wl,--gc-sections
)

target_link_libraries(${TARGET_NAME} PRIVATE c m nosys)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary -S $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_BINARY_DIR}/${TARGET_NAME}.bin
)

add_custom_target(download_dap
    COMMAND openocd -f ${HAL_DIR}/openocd_daplink.cfg -c "program $<TARGET_FILE:${TARGET_NAME}> verify reset exit"
    DEPENDS ${TARGET_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

add_custom_target(download_jlink
    COMMAND JLink.exe -CommanderScript ${HAL_DIR}/jlink_download.jlink
    DEPENDS ${TARGET_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

message(STATUS "APP_NAME: ${APP_NAME}")
message(STATUS "DEVELOPMENT_BOARD: ${DEVELOPMENT_BOARD}")
message(STATUS "HAL_DIR: ${HAL_DIR}")
