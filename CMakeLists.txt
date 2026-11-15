cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME JZ-F407-MQTT-CAN)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# CubeMX lists a legacy fsdata.c removed by the current LwIP package.
set(MX_GENERATED_CMAKE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32cubemx/CMakeLists.txt")
set(MX_LEGACY_FSDATA_SOURCE
    [=[    ${CMAKE_CURRENT_SOURCE_DIR}/../../Middlewares/Third_Party/LwIP/src/apps/httpd/fsdata.c]=]
)
file(READ "${MX_GENERATED_CMAKE}" MX_GENERATED_CMAKE_CONTENT)
string(REPLACE "${MX_LEGACY_FSDATA_SOURCE}" ""
    MX_GENERATED_CMAKE_CONTENT "${MX_GENERATED_CMAKE_CONTENT}"
)
file(WRITE "${MX_GENERATED_CMAKE}" "${MX_GENERATED_CMAKE_CONTENT}")

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

target_include_directories(LwIP PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/mkfs")
target_sources(STM32_Drivers PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_i2c_ex.c"
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/eeprom.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/i2c.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/my_can.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/my_httpd.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/my_log.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/my_mcu.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/my_mqtt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ssd1306.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/ssd1306_fonts.c"
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
