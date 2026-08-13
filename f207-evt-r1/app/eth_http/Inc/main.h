#ifndef MAIN_H
#define MAIN_H

/* Static IP fallback, used only if DHCP times out (192.168.5.200). */
#define IP_ADDR0      192
#define IP_ADDR1      168
#define IP_ADDR2      5
#define IP_ADDR3      200

#define NETMASK_ADDR0 255
#define NETMASK_ADDR1 255
#define NETMASK_ADDR2 255
#define NETMASK_ADDR3 0

#define GW_ADDR0      192
#define GW_ADDR1      168
#define GW_ADDR2      5
#define GW_ADDR3      1

#endif /* MAIN_H */
