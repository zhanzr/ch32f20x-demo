# eth_http: embedded HTTP server on the CH32F207VCT6 (f207-evt-r1 board).
#
# lwIP 2.1.2 raw API / NO_SYS + DHCP, serving the bundled single-page site
# (web/ + public/ packed by build_web.py into Inc/web_assets.h). The HTTP
# server is a custom raw-TCP app (src/http_server.c), not lwIP httpd.
# Behaviour mirrors the STM32F769 disco/bare/eth_http reference, adapted for
# this board's single LED (PA0, low active) and two internal ADC channels.
#
# Workflow:
#   python build_web.py                  # regenerate Inc/web_assets.h
#   bash build.sh                        # CMake + Ninja + GNU arm-none-eabi-gcc
#   ninja flash                          # WCH OpenOCD + WCH-Link (CMSIS-DAP/SWD)
#
# Toolchain: ARM_GCC_ROOT env var (or -DARM_GCC_ROOT=...) / PATH lookup.
# Flashing:  WCH_OPENOCD_HOME env var (or -DWCH_OPENOCD=...) / PATH lookup.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/arm-none-eabi-toolchain.cmake")
endif()

project(eth_http C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/ch32f207_board.cmake)

set(LWIP ${CMAKE_CURRENT_SOURCE_DIR}/../../vendor/LwIP)

add_executable(${PROJECT_NAME}.elf
    src/main.c
    src/ethernetif.c
    src/app_ethernet.c
    src/http_server.c

    # lwIP 2.1.2, NO_SYS / raw API subset
    ${LWIP}/core/def.c
    ${LWIP}/core/inet_chksum.c
    ${LWIP}/core/init.c
    ${LWIP}/core/ip.c
    ${LWIP}/core/ipv4/ip4.c
    ${LWIP}/core/ipv4/ip4_addr.c
    ${LWIP}/core/ipv4/etharp.c
    ${LWIP}/core/ipv4/dhcp.c
    ${LWIP}/core/ipv4/icmp.c
    ${LWIP}/core/mem.c
    ${LWIP}/core/memp.c
    ${LWIP}/core/netif.c
    ${LWIP}/core/pbuf.c
    ${LWIP}/core/sys.c
    ${LWIP}/core/tcp.c
    ${LWIP}/core/tcp_in.c
    ${LWIP}/core/tcp_out.c
    ${LWIP}/core/timeouts.c
    ${LWIP}/core/udp.c
    ${LWIP}/netif/ethernet.c

    # CH32F20x SPL: Ethernet MAC + debug MCU (DBGMCU_GetCHIPID)
    ${CH32_SPL}/src/ch32f20x_eth.c
    ${CH32_SPL}/src/ch32f20x_dbgmcu.c
)

ch32f207_apply_board(${PROJECT_NAME}.elf "-O2")

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LWIP}/include
    ${LWIP}/system
)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/objcopy-targets.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/flash-targets.cmake)
