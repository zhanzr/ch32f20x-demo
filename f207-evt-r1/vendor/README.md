# Vendored third-party code

| Source                                        | Path in this repo   |
| --------------------------------------------- | ------------------- |
| lwIP 2.1.2 (`core/`, `netif/`, `include/`)    | `LwIP/`             |
| lwIP 2.1.2 port headers (`system/arch/`)      | `LwIP/system/arch/` |

The lwIP tree is the standard open-source lwIP 2.1.2 (MIT-style license),
copied from the sibling `stm32f769_prj` vendor tree with only the raw-API /
NO_SYS subset used (`app/eth_http/CMakeLists.txt` compiles `core/` + `netif/`).
`system/arch/` holds the compiler port headers (`cc.h`, `sys_arch.h`, ...).

No local patches are applied to lwIP itself; all board specifics live in
`app/eth_http/src/ethernetif.c`.
