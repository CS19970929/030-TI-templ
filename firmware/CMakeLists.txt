cmake_minimum_required(VERSION 3.25)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(stm32f030_bq769x0_app C ASM)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(APP_ENABLE_LTO "Enable link time optimization for firmware build" ON)
set(APP_OPT_LEVEL "s" CACHE STRING "Firmware optimization level for GCC builds (0,1,2,3,s,g)")
set(APP_HEAP_SIZE "0x0" CACHE STRING "Reserved heap bytes in linker script")
set(APP_STACK_SIZE "0xC00" CACHE STRING "Reserved stack bytes in linker script")

set(APP_LINKER_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/linker/stm32f030c8_bq769x0_app.ld")
set(APP_LINKER_SCRIPT "${CMAKE_BINARY_DIR}/generated/stm32f030c8_bq769x0_app.ld")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file("${APP_LINKER_TEMPLATE}" "${APP_LINKER_SCRIPT}" @ONLY)

set(FIRMWARE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/startup/startup_stm32f0xx_gcc.S"
    "${PROJECT_ROOT}/Code/Drivers/stm32f0xx_it.c"
    "${PROJECT_ROOT}/Code/Drivers/system_stm32f0xx.c"
    "${PROJECT_ROOT}/Code/Source/ADC.c"
    "${PROJECT_ROOT}/Code/Source/BQ769X0_Func.c"
    "${PROJECT_ROOT}/Code/Source/Cell_balance.c"
    "${PROJECT_ROOT}/Code/Source/ChargerLoadFunc.c"
    "${PROJECT_ROOT}/Code/Source/DataDeal.c"
    "${PROJECT_ROOT}/Code/Source/EEPROM.c"
    "${PROJECT_ROOT}/Code/Source/Fault.c"
    "${PROJECT_ROOT}/Code/Source/Flash.c"
    "${PROJECT_ROOT}/Code/Source/Heat_Cool.c"
    "${PROJECT_ROOT}/Code/Source/I2C_AFE1.c"
    "${PROJECT_ROOT}/Code/Source/IODrivers_030.c"
    "${PROJECT_ROOT}/Code/Source/IO_Control.c"
    "${PROJECT_ROOT}/Code/Source/LedBar.c"
    "${PROJECT_ROOT}/Code/Source/lk8625.c"
    "${PROJECT_ROOT}/Code/Source/LogRecord.c"
    "${PROJECT_ROOT}/Code/Source/main.c"
    "${PROJECT_ROOT}/Code/Source/P12_Protocol.c"
    "${PROJECT_ROOT}/Code/Source/ProductionID.c"
    "${PROJECT_ROOT}/Code/Source/PubFunc.c"
    "${PROJECT_ROOT}/Code/Source/RTC.c"
    "${PROJECT_ROOT}/Code/Source/Sci_Upper.c"
    "${PROJECT_ROOT}/Code/Source/ShortFunc.c"
    "${PROJECT_ROOT}/Code/Source/SleepDeal.c"
    "${PROJECT_ROOT}/Code/Source/SOC.c"
    "${PROJECT_ROOT}/Code/Source/SocEnhance.c"
    "${PROJECT_ROOT}/Code/Source/System_Init.c"
    "${PROJECT_ROOT}/Code/Source/System_Monitor.c"
    "${PROJECT_ROOT}/Code/Source/bsp/bsp.c"
    "${PROJECT_ROOT}/Code/Source/bsp/bsp_cpu_flash.c"
    "${PROJECT_ROOT}/Code/Source/bsp/param.c"
    "${PROJECT_ROOT}/Code/Source/conf/conf.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_adc.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_can.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_cec.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_comp.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_crc.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_crs.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_dac.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_dbgmcu.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_dma.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_exti.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_flash.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_gpio.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_i2c.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_iwdg.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_misc.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_pwr.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_rcc.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_rtc.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_spi.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_syscfg.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_tim.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_usart.c"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/src/stm32f0xx_wwdg.c"
)

add_executable(${PROJECT_NAME}.elf ${FIRMWARE_SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    "${PROJECT_ROOT}/Code/Drivers"
    "${PROJECT_ROOT}/Code/STM32F0xx_StdPeriph_Driver/inc"
    "${PROJECT_ROOT}/Code/Source"
    "${PROJECT_ROOT}/Code/Source/bsp"
    "${PROJECT_ROOT}/Code/Source/conf"
)

target_compile_definitions(${PROJECT_NAME}.elf PRIVATE
    USE_STDPERIPH_DRIVER
    STM32F0XX
)

set(APP_GCC_OPT_FLAG "-Os")
if(APP_OPT_LEVEL STREQUAL "0")
    set(APP_GCC_OPT_FLAG "-O0")
elseif(APP_OPT_LEVEL STREQUAL "1")
    set(APP_GCC_OPT_FLAG "-O1")
elseif(APP_OPT_LEVEL STREQUAL "2")
    set(APP_GCC_OPT_FLAG "-O2")
elseif(APP_OPT_LEVEL STREQUAL "3")
    set(APP_GCC_OPT_FLAG "-O3")
elseif(APP_OPT_LEVEL STREQUAL "s")
    set(APP_GCC_OPT_FLAG "-Os")
elseif(APP_OPT_LEVEL STREQUAL "g")
    set(APP_GCC_OPT_FLAG "-Og")
endif()

target_compile_options(${PROJECT_NAME}.elf PRIVATE
    -mcpu=cortex-m0
    -mthumb
    -g3
    -ffunction-sections
    -fdata-sections
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fno-common
    -fno-strict-aliasing
    -fmessage-length=0
    -std=gnu99
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:${APP_GCC_OPT_FLAG}>
    $<$<CONFIG:Debug>:-Os>
    $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>
    $<$<COMPILE_LANGUAGE:C>:-Wno-builtin-declaration-mismatch>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-function>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-variable>
    $<$<COMPILE_LANGUAGE:C>:-Wall>
)

if(APP_ENABLE_LTO)
    target_compile_options(${PROJECT_NAME}.elf PRIVATE
        -flto
    )
endif()

target_link_options(${PROJECT_NAME}.elf PRIVATE
    -T${APP_LINKER_SCRIPT}
    -mcpu=cortex-m0
    -mthumb
    -nostartfiles
    -Wl,--nmagic
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_BINARY_DIR}/CommomBQ769x0_16series_030C8T6_C.map
    --specs=nano.specs
    --specs=nosys.specs
)

if(APP_ENABLE_LTO)
    target_link_options(${PROJECT_NAME}.elf PRIVATE
        -flto
    )
endif()

set_target_properties(${PROJECT_NAME}.elf PROPERTIES
    OUTPUT_NAME "CommomBQ769x0_16series_030C8T6_C"
    SUFFIX ".elf"
)

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/CommomBQ769x0_16series_030C8T6_C.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_BINARY_DIR}/CommomBQ769x0_16series_030C8T6_C.hex
        VERBATIM
    )
endif()

if(CMAKE_SIZE)
    add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
        VERBATIM
    )
endif()
