cmake_minimum_required(VERSION 3.21)
project(Control_Device_Electral_Method LANGUAGES C ASM)

# Build the sources already generated from the checked-in .ioc. CubeMX is
# required only for regeneration; it cannot generate Application/ sources.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Control-Device-Electral-Method.ioc" IOC)
if(NOT IOC MATCHES "ProjectManager.DeviceId=STM32F407VETx")
    message(FATAL_ERROR "Review CPU flags, startup and linker script for the new .ioc MCU")
endif()
if(NOT CMAKE_C_COMPILER MATCHES "arm-none-eabi-gcc")
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake")
endif()

add_library(mcu_options INTERFACE)
target_compile_features(mcu_options INTERFACE c_std_11)
target_compile_definitions(mcu_options INTERFACE USE_HAL_DRIVER STM32F407xx
    $<$<CONFIG:Debug>:DEBUG>)
set(MCU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
target_compile_options(mcu_options INTERFACE ${MCU_FLAGS}
    -ffunction-sections -fdata-sections -Wall -Wextra -fstack-usage)
target_link_options(mcu_options INTERFACE ${MCU_FLAGS})
target_include_directories(mcu_options INTERFACE
    Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32F4xx/Include
    Drivers/CMSIS/Include
    FATFS/Target FATFS/App Middlewares/Third_Party/FatFs/src
    Application Application/board Application/core Application/drivers
    Application/interfaces Application/ui)

# Same source directories as .cproject. Do not compile CMSIS templates, test
# shims, examples, testbenches or files in the unrelated nested gitlinks.
file(GLOB HAL_SOURCES CONFIGURE_DEPENDS Drivers/STM32F4xx_HAL_Driver/Src/*.c)
file(GLOB FATFS_SOURCES CONFIGURE_DEPENDS Middlewares/Third_Party/FatFs/src/*.c)
add_library(stm32_support OBJECT ${HAL_SOURCES} ${FATFS_SOURCES}
    Middlewares/Third_Party/FatFs/src/option/syscall.c
    FATFS/Target/bsp_driver_sd.c FATFS/Target/sd_diskio.c FATFS/App/fatfs.c
    Core/Src/system_stm32f4xx.c Core/Src/stm32f4xx_hal_msp.c
    Core/Src/stm32f4xx_it.c Core/Src/syscalls.c Core/Src/sysmem.c
    Core/Startup/startup_stm32f407vetx.s)
target_link_libraries(stm32_support PUBLIC mcu_options)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    Application/*.c Application/board/*.c Application/core/*.c
    Application/drivers/*.c Application/ui/*.c)
add_executable(firmware Core/Src/main.c ${APP_SOURCES})
target_link_libraries(firmware PRIVATE stm32_support mcu_options m)
set_target_properties(firmware PROPERTIES SUFFIX .elf)
target_link_options(firmware PRIVATE
    "-T${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VETX_FLASH.ld"
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections -Wl,--print-memory-usage
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/firmware.map")
set_property(TARGET firmware APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/STM32F407VETX_FLASH.ld")
add_custom_command(TARGET firmware POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary $<TARGET_FILE:firmware>
            "${CMAKE_CURRENT_BINARY_DIR}/firmware.bin"
    COMMAND "${CMAKE_OBJCOPY}" -O ihex $<TARGET_FILE:firmware>
            "${CMAKE_CURRENT_BINARY_DIR}/firmware.hex"
    COMMAND "${CMAKE_SIZE}" $<TARGET_FILE:firmware>
    VERBATIM)
