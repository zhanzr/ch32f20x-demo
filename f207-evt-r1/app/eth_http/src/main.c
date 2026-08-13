/**
  * @file    eth_http/src/main.c
  * @brief   Minimal HTTP server on the CH32F207VCT6 (f207-evt-r1 board).
  *
  * lwIP raw API, DHCP. Behaviour mirrors the STM32F769 disco/bare/eth_http
  * reference (e_server site + JSON API), adapted for this board's single LED
  * (PA0, low active) and two internal ADC channels (die temperature IN16 +
  * VREFINT IN17). The CH32F207 integrates a 10BASE-T PHY (the RJ45 connects
  * straight to the chip), so no external PHY is used.
  */

#include "board.h"
#include "uart_printf.h"
#include "ch32f20x.h"
#include "lwip/opt.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "app_ethernet.h"
#include "http_server.h"
#include "main.h"

/* Global network interface */
struct netif gnetif;

/* ------------------------------------------------------------------------ */
static void Netif_Config(void)
{
    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gw;

#if LWIP_DHCP
    ip_addr_set_zero_ip4(&ipaddr);
    ip_addr_set_zero_ip4(&netmask);
    ip_addr_set_zero_ip4(&gw);
#else
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1, NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif

    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &ethernet_input);
    netif_set_default(&gnetif);

#if LWIP_NETIF_LINK_CALLBACK
    netif_set_link_callback(&gnetif, ethernet_link_status_updated);
#endif
}

/* ------------------------------------------------------------------------ */
int main(void)
{
    Board_Init();        /* 144 MHz clocks (from startup) + USART1 console + 1 ms tick */

    printf("\r\n=== eth_http on CH32F207VCT6 @ %lu Hz ===\r\n",
           (unsigned long)SystemCoreClock);
    printf("HTTP server: http://<dhcp-ip>/  (DHCP enabled)\r\n");

    lwip_init();
    Netif_Config();
    http_server_init();

    printf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\r\n",
           gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
           gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);

    while (1)
    {
        ethernetif_input(&gnetif);        /* poll RX */
        sys_check_timeouts();             /* lwIP timers */
#if LWIP_NETIF_LINK_CALLBACK
        Ethernet_Link_Periodic_Handle(&gnetif);
#endif
#if LWIP_DHCP
        DHCP_Periodic_Handle(&gnetif);
#endif
    }
}
