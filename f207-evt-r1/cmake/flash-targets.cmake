# Shared flashing targets for the CH32F207VC (f207-evt-r1 board).
#
# Default: the WCH OpenOCD (the build bundled with MounRiver Studio / WCH
# tools, which ships the "wch_arm" flash driver) driving the on-board
# WCH-Link in CMSIS-DAP mode over SWD.
#
#   ninja flash       - WCH OpenOCD download + verify + reset over SWD.
#
# Overrides:
#   -DWCH_OPENOCD=/path/to/openocd
#   -DWCHLINK_SERIAL=<WCH-Link serial>  (needed only with >1 WCH-Link)
#   -DOPENOCD_CFG=<board config .cfg>   (default: f207-evt-r1/scripts/wch-link.cfg)

set(BIN_HEX "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex")

# ---------------------------------------------------------------------------
# Locate the WCH OpenOCD (with the wch_arm flash driver). The copy bundled
# with MounRiver Studio is the default.
set(_WCH_OPENOCD_HINTS
    "D:/MounRiver/MounRiver_Studio2/resources/app/resources/win32/components/WCH/OpenOCD/OpenOCD/bin"
    "$ENV{WCH_OPENOCD_HOME}/bin"
)
find_program(WCH_OPENOCD NAMES openocd openocd.exe HINTS ${_WCH_OPENOCD_HINTS}
    DOC "WCH OpenOCD binary (with wch_arm flash driver)")

# Board/probe config: f207-evt-r1 + WCH-Link (CMSIS-DAP) over SWD.
set(OPENOCD_CFG "${CMAKE_CURRENT_LIST_DIR}/../scripts/wch-link.cfg" CACHE FILEPATH
    "OpenOCD board config for the f207-evt-r1 + WCH-Link")

# Optional probe serial (cmsis_dap_serial) when multiple WCH-Links are present.
set(WCHLINK_SERIAL "" CACHE STRING
    "WCH-Link serial to pin (cmsis_dap_serial) - only needed with >1 WCH-Link")
if(WCHLINK_SERIAL)
    set(_SERIAL_ARGS -c "set WCHLINK_SERIAL ${WCHLINK_SERIAL}")
else()
    set(_SERIAL_ARGS)
endif()

if(WCH_OPENOCD)
    add_custom_target(flash
        COMMAND "${WCH_OPENOCD}"
                    ${_SERIAL_ARGS}
                    -f "${OPENOCD_CFG}"
                    -c "program ${BIN_HEX} verify reset exit"
        DEPENDS hex
        COMMENT "Flashing ${PROJECT_NAME}.hex to CH32F207VCT6 via WCH OpenOCD + WCH-Link (CMSIS-DAP/SWD) ..."
        USES_TERMINAL)
else()
    add_custom_target(flash
        COMMAND ${CMAKE_COMMAND} -E echo
            "WCH OpenOCD not found. Point to it with -DWCH_OPENOCD=/path/to/openocd")
endif()
