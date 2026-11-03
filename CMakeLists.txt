cmake_minimum_required(VERSION 3.29.6)

#此文件从模板自动生成! 请勿更改!
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)

# specify cross-compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
# NOTE: do not preset CMAKE_AR / CMAKE_OBJCOPY / CMAKE_OBJDUMP here. Pre-setting
# them makes CMake's binutils detection crash with 0xC0000409 in this
# environment; CMake finds arm-none-eabi-* next to the compiler automatically.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS_INIT "--specs=nosys.specs")

# project settings
# Only the C language is enabled: enabling CXX makes CMake's compiler/binutils
# detection crash with 0xC0000409 on this machine. The C++ application layer
# (App/*.cpp, Core/Src/main.cpp) is therefore compiled by the C driver with
# "-x c++" — see the set_source_files_properties() call further down.
project(WHEELTEC C)
set(CMAKE_C_STANDARD 11)

#Uncomment for hardware floating point
#add_compile_definitions(ARM_MATH_CM4;ARM_MATH_MATRIX_CHECK;ARM_MATH_ROUNDING)
#add_compile_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)
#add_link_options(-mfloat-abi=hard -mfpu=fpv4-sp-d16)

#Uncomment for software floating point
#add_compile_options(-mfloat-abi=soft)

add_compile_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_compile_options(-ffunction-sections -fdata-sections -fno-common -fmessage-length=0)

# uncomment to mitigate c++17 absolute addresses warnings
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-register")

# NOTE: ASM is not enabled as a CMake language. Enabling it makes the ARM
# toolchain detection crash with 0xC0000409 in this environment, so the startup
# assembler file is compiled as C with "-x assembler-with-cpp" instead.
# Enable assembler files preprocessing

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
endif ()

include_directories(Platform App App/Ui Drivers/OLED Drivers/STM32F1xx_HAL_Driver/Inc Drivers/STM32F1xx_HAL_Driver/Inc/Legacy Drivers/CMSIS/Device/ST/STM32F1xx/Include Drivers/CMSIS/Include .)

add_definitions(-DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB)

set(SOURCES
    App/main.cpp
    App/Control.cpp
    App/Balance.cpp
    App/Keys.cpp
    App/SwingUp.cpp
    App/Calibration.cpp
    App/Ui/show.c
    App/Ui/app_view.cpp
    App/Ui/DataScope_DP.C
    Platform/gpio.c
    Platform/adc.c
    Platform/tim.c
    Platform/usart.c
    Platform/stm32f1xx_it.c
    Platform/stm32f1xx_hal_msp.c
    Drivers/OLED/oled.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_exti.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c
    Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
    Platform/system_stm32f1xx.c
    Platform/syscalls.c
    Platform/sysmem.c
    Platform/startup_stm32f103c8tx.s
)
# The vendor uses uppercase .C for C sources; GCC otherwise treats them as C++.
set_source_files_properties(
    App/Ui/DataScope_DP.C
    PROPERTIES LANGUAGE C
)

# C++ application layer. Enabling the CMake CXX language reliably crashes the
# compiler detection in this environment (exit code 0xC0000409), so .cpp files
# are fed to the C driver with "-x c++". No STL / exceptions / RTTI are used,
# which keeps libstdc++ out of the link.
set(APP_CPP_FILES
    App/main.cpp
    App/Ui/app_view.cpp
    App/Control.cpp
    App/Balance.cpp
    App/Keys.cpp
    App/SwingUp.cpp
    App/Calibration.cpp
)
set_source_files_properties(${APP_CPP_FILES}
    PROPERTIES LANGUAGE C
    COMPILE_OPTIONS "-x;c++;-std=gnu++17;-fno-exceptions;-fno-rtti;-fno-threadsafe-statics;-fno-use-cxa-atexit"
)
add_link_options(-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit)

# Assembler startup file: CMake's ASM language is avoided (see note above), so
# compile it through the C compiler with assembler-with-cpp preprocessing.
set_source_files_properties(
    Platform/startup_stm32f103c8tx.s
    PROPERTIES LANGUAGE C COMPILE_OPTIONS "-x;assembler-with-cpp"
)

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/STM32F103C8TX_FLASH.ld)

add_link_options(-Wl,-gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)
add_link_options(-mcpu=cortex-m3 -mthumb -mthumb-interwork)
add_link_options("-T${LINKER_SCRIPT}" --specs=nano.specs)

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf m)

set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)

add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -Oihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -Obinary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin
        COMMENT "Building ${HEX_FILE}
Building ${BIN_FILE}")
