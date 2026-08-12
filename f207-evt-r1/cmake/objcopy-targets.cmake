# Shared objcopy targets: turn the linked .elf into flashable images.
#
#   ninja        - builds the .elf + .hex image. The Intel HEX file is exactly
#                  what `ninja flash` and tools/bench_capture.sh program the
#                  board with, so the default build stops there.
#   ninja flash  - flashes the .hex (see flash-targets.cmake)
#   ninja hex    - (re)build just the .hex image, "in case" you need it
#   ninja bin    - (re)build the raw .bin image, "in case" you need it
#
# Images are written to the build directory:
#   ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.{elf,hex,bin}
# Only the .hex is produced by the default build; the .bin is opt-in.

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}.elf>
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "objcopy -> ${PROJECT_NAME}.hex (Intel HEX)")

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}.elf>
            ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    DEPENDS ${PROJECT_NAME}.elf
    COMMENT "objcopy -> ${PROJECT_NAME}.bin (raw binary)")

# `ninja hex` / `ninja bin` — explicit image builds "in case" you need them.
# `hex` is marked ALL so a plain `ninja` also produces it (that is the file
# `ninja flash` needs); `bin` is only built on demand.
add_custom_target(hex ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex)
add_custom_target(bin DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin)
