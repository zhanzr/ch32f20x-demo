/**
  * @file    eth_http/src/ethernetif.c
  * @brief   Ethernet network interface driver for lwIP (NO_SYS / raw API)
  *          on the CH32F207VCT6 (f207-evt-r1 board).
  *
  * The CH32F207 integrates the Ethernet MAC together with a **10BASE-T PHY
  * transceiver** (the board's RJ45 connects straight to the chip - there is
  * no external PHY). MAC/PHY init follows the WCH EVT eth_driver_D8C_10M.c:
  *   - a 60 MHz clock for the built-in PHY is derived from the 8 MHz HSE via
  *     PLL3 (PREDIV2 /2 -> 4 MHz, x15 -> 60 MHz);
  *   - the internal 10M PHY is enabled with EXTEN_ETH_10M_EN;
  *   - the PHY registers are still reachable over the MAC SMI bus (address 1);
  *   - link is 10BASE-T only, so the MAC speed bits stay cleared (10M) and the
  *     vendor's auto-negotiation / MDI-X polarity recovery is used.
  *
  * The MAC is started/stopped from the link state machine, and the ETH DMA
  * interrupt recovers the RX path from a receive-buffer underrun (a known
  * CH32F20x quirk) by re-initialising the MAC, the same workaround the WCH
  * EVT library uses.
  */

/* Includes ------------------------------------------------------------------*/
#include "board.h"
#include "ch32f20x.h"
#include "ch32f20x_eth.h"
#include "lwip/opt.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include "uart_printf.h"
#include <stdio.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/
#define IFNAME0 'e'
#define IFNAME1 'n'

#define PHY_ADDRESS     1     /* built-in 10M PHY on the SMI bus */
#define PHY_LINK_TASK_PERIOD   50

/* MAC queue configuration for the D8C silicon (from the WCH net_config.h).
 * The SPL descriptor chain strides buffers by ETH_MAX_PACKET_SIZE (1536). */
#define ETH_RXBUFNB      7
#define ETH_TXBUFNB      2
#define ETH_RX_BUF_SZE   1536  /* = ETH_MAX_PACKET_SIZE (multiple of 4) */
#define ETH_TX_BUF_SZE   1536
#define ETH_TX_BUF_SIZE  1536  /* bounce buffer size (full Ethernet frame) */

#define ROM_CFG_USERADR_ID   0x1FFFF7E8   /* unique 48-bit MAC stored in the chip */

#define PHY_ANLPAR_SELECTOR_FIELD   0x1F   /* ANLPAR selector field (802.3) */

/* Private variables ---------------------------------------------------------*/
/* The ETH DMA reaches all of the SRAM on the Cortex-M3 (no D-cache), so the
 * descriptors and buffers are plain 4-byte-aligned arrays in .bss. */
__attribute__((aligned(4))) ETH_DMADESCTypeDef DMARxDscrTab[ETH_RXBUFNB];
__attribute__((aligned(4))) ETH_DMADESCTypeDef DMATxDscrTab[ETH_TXBUFNB];
__attribute__((aligned(4))) static uint8_t      MACRxBuf[ETH_RXBUFNB * ETH_RX_BUF_SZE];
__attribute__((aligned(4))) static uint8_t      MACTxBuf[ETH_TXBUFNB * ETH_TX_BUF_SZE];
__attribute__((aligned(4))) static uint8_t      tx_bounce[ETH_TX_BUF_SIZE];
__attribute__((aligned(4))) static uint8_t      rx_scratch[ETH_RX_BUF_SZE];

static uint16_t gPHYAddress = PHY_ADDRESS;
static uint32_t ChipId;
static uint8_t  MACAddr[6];

volatile uint32_t eth_rx_cnt;         /* received-frame counter (status) */
volatile uint32_t eth_tx_cnt;         /* transmitted-frame counter (status) */

/* 10M PHY link / auto-negotiation state (WCH EVT D8C_10M port). */
static uint8_t  LinkSta;              /* 0: link down  1: link up */
static uint8_t  phyStatus;            /* 0: negotiating, else PHY_Linked_Status */
static uint8_t  phyLinkReset;         /* 1: wait 500 ms, then re-enable the 10M PHY */
static uint32_t phyLinkTime;
static uint8_t  PhyPolarityDetect;
static uint32_t LinkSuccTime;
static uint8_t  phyPN = (2u << 2);    /* PHY_PN_SWITCH_AUTO */
static uint32_t link_task_time;
static uint32_t RandVal;

/* Auto-negotiation state machine variables (WCH EVT WCHNET_LinkProcess). */
static uint8_t  phyLinkStatus;        /* PHY_LINK_INIT / _SUC_P / _SUC_N / _WAIT_SUC */
static uint8_t  phyLinkCnt;
static uint8_t  phySucCnt;
static uint8_t  TRDetectStep;
static uint8_t  TRDetectCnt;
static uint32_t LinkTaskPeriod = 50;  /* ms */
static uint8_t  ReInitMACFlag;

/* Private function prototypes -----------------------------------------------*/
static void   eth_set_clock(void);
static void   eth_led_config(void);
static void   eth_init(uint8_t *mac);
static void   eth_mac_config(uint8_t *mac);
static void   eth_phy_link(struct netif *netif);
static void   eth_link_up_cfg(void);
static void   eth_phy_pn_process(void);
static void   eth_stop(void);
static void   eth_reinit_mac_reg(void);
static void   eth_rec_process(void);

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH)
*******************************************************************************/
static void eth_get_mac(uint8_t *mac)
{
  uint8_t i;
  uint8_t *p = (uint8_t *)(ROM_CFG_USERADR_ID + 5);
  for (i = 0; i < 6; i++)
  {
    *mac = *p;
    mac++;
    p--;
  }
}

/**
  * @brief  In this function, the hardware should be initialized.
  *         Called from ethernetif_init().
  */
static void low_level_init(struct netif *netif)
{
  uint8_t macaddress[6];

  eth_get_mac(macaddress);
  memcpy(MACAddr, macaddress, 6);

  eth_init(macaddress);

  netif->hwaddr_len = ETH_HWADDR_LEN;
  memcpy(netif->hwaddr, macaddress, 6);
  netif->mtu = 1500;
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  ethernet_link_check_state(netif);
}

/**
  * @brief  Transmit the pbuf chain through the MAC.
  */
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  struct pbuf *q;
  uint8_t *dst = tx_bounce;

  LWIP_UNUSED_ARG(netif);

  if (p->tot_len > ETH_TX_BUF_SIZE)
  {
    return ERR_MEM;
  }

  for (q = p; q != NULL; q = q->next)
  {
    memcpy(dst, q->payload, q->len);
    dst += q->len;
  }

  if (ETH_HandleTxPkt(tx_bounce, p->tot_len) != ETH_SUCCESS)
  {
    return ERR_IF;
  }

  eth_tx_cnt++;

  return ERR_OK;
}

/**
  * @brief  Receive a frame into a pbuf (polled).
  */
static struct pbuf *low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL;
  uint32_t len;

  LWIP_UNUSED_ARG(netif);

  len = ETH_GetRxPktSize();     /* frame length including the 4-byte CRC */
  if (len <= 4)
  {
    return NULL;
  }
  len -= 4;                     /* payload length (ETH_HandleRxPkt strips the CRC) */

  p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
  if (p != NULL)
  {
    if (ETH_HandleRxPkt((uint8_t *)p->payload) != len || p->next != NULL)
    {
      pbuf_free(p);
      return NULL;
    }
    eth_rx_cnt++;
    return p;
  }

  /* Out of pbuf memory: consume the frame so the DMA keeps running. */
  ETH_HandleRxPkt(rx_scratch);
  return NULL;
}

/**
  * @brief  Poll for received packets and feed them to lwIP.
  */
void ethernetif_input(struct netif *netif)
{
  struct pbuf *p;

  do
  {
    p = low_level_input(netif);
    if (p != NULL)
    {
      if (netif->input(p, netif) != ERR_OK)
      {
        pbuf_free(p);
      }
    }
  } while (p != NULL);
}

/**
  * @brief  lwIP netif init entry point.
  */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  netif->hostname = "ch32f207";
#endif

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  low_level_init(netif);

  return ERR_OK;
}

/**
  * @brief  Returns the current time in milliseconds (NO_SYS).
  */
u32_t sys_now(void)
{
  return HAL_GetTick();
}

/*******************************************************************************
                       MAC + built-in 10M PHY init (WCH EVT port)
*******************************************************************************/
/**
  * @brief  Clock for the built-in 10BASE-T PHY: HSE 8M / PREDIV2 /2 = 4M,
  *         PLL3 x15 = 60 MHz.
  */
static void eth_set_clock(void)
{
  RCC_PLL3Cmd(DISABLE);
  RCC_PREDIV2Config(RCC_PREDIV2_Div2);
  RCC_PLL3Config(RCC_PLL3Mul_15);
  RCC_PLL3Cmd(ENABLE);
  while (RESET == RCC_GetFlagStatus(RCC_FLAG_PLL3RDY))
  {
  }
}

/**
  * @brief  Ethernet link/data LEDs on PC0/PC1 (low active).
  */
static void eth_led_config(void)
{
  GPIO_InitTypeDef GPIO = {0};

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
  GPIO.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
  GPIO.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOC, &GPIO);
  GPIO_SetBits(GPIOC, GPIO_Pin_0 | GPIO_Pin_1);   /* LEDs off */
}

/**
  * @brief  ETH register + built-in 10M PHY configuration (ETH_Configuration).
  */
static void eth_mac_config(uint8_t *mac)
{
  ETH_InitTypeDef ETH_InitStructure;
  uint16_t timeout = 10000;

  /* Enable the Ethernet MAC clock */
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_ETH_MAC |
                        RCC_AHBPeriph_ETH_MAC_Tx |
                        RCC_AHBPeriph_ETH_MAC_Rx, ENABLE);

  gPHYAddress = PHY_ADDRESS;
  eth_set_clock();

  /* Enable the built-in 10BASE-T PHY. */
  EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;

  /* Reset ETHERNET on AHB Bus + software reset */
  ETH_DeInit();
  ETH_SoftwareReset();
  do
  {
    Delay_Us(10);
    if (!--timeout) break;
  } while (ETH->DMABMR & ETH_DMABMR_SR);

  /* Set the SMI interface clock: main frequency divided by 42 */
  ETH->MACMIIAR = (uint32_t)ETH_MACMIIAR_CR_Div42;

  /*------------------------   MAC   -----------------------------------*/
  ETH_InitStructure.ETH_Watchdog = ETH_Watchdog_Enable;
  ETH_InitStructure.ETH_Jabber = ETH_Jabber_Enable;
  ETH_InitStructure.ETH_InterFrameGap = ETH_InterFrameGap_96Bit;
  ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Disable;
  ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
  ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
  ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
  ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
  ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
  ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
  ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
  ETH_InitStructure.ETH_PassControlFrames = ETH_PassControlFrames_BlockAll;
  ETH_InitStructure.ETH_DestinationAddrFilter = ETH_DestinationAddrFilter_Normal;
  ETH_InitStructure.ETH_SourceAddrFilter = ETH_SourceAddrFilter_Disable;
  ETH_InitStructure.ETH_HashTableHigh = 0x0;
  ETH_InitStructure.ETH_HashTableLow = 0x0;
  ETH_InitStructure.ETH_VLANTagComparison = ETH_VLANTagComparison_16Bit;
  ETH_InitStructure.ETH_VLANTagIdentifier = 0x0;
  ETH_InitStructure.ETH_PauseTime = 0x0;
  ETH_InitStructure.ETH_UnicastPauseFrameDetect = ETH_UnicastPauseFrameDetect_Disable;
  ETH_InitStructure.ETH_ReceiveFlowControl = ETH_ReceiveFlowControl_Disable;
  ETH_InitStructure.ETH_TransmitFlowControl = ETH_TransmitFlowControl_Disable;
  ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
  ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;
  ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Enable;
  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Enable;

  ETH->MACCR = (uint32_t)(ETH_InitStructure.ETH_Watchdog |
                ETH_InitStructure.ETH_Jabber |
                ETH_InitStructure.ETH_InterFrameGap |
                ETH_InitStructure.ETH_ChecksumOffload |
                ETH_InitStructure.ETH_AutomaticPadCRCStrip |
                ETH_InitStructure.ETH_LoopbackMode |
                ETH_Internal_Pull_Up_Res_Enable | (1u << 9));

  ETH->MACFFR = (uint32_t)(ETH_InitStructure.ETH_ReceiveAll |
                          ETH_InitStructure.ETH_SourceAddrFilter |
                          ETH_InitStructure.ETH_PassControlFrames |
                          ETH_InitStructure.ETH_BroadcastFramesReception |
                          ETH_InitStructure.ETH_DestinationAddrFilter |
                          ETH_InitStructure.ETH_PromiscuousMode |
                          ETH_InitStructure.ETH_MulticastFramesFilter |
                          ETH_InitStructure.ETH_UnicastFramesFilter);

  ETH->MACHTHR = (uint32_t)ETH_InitStructure.ETH_HashTableHigh;
  ETH->MACHTLR = (uint32_t)ETH_InitStructure.ETH_HashTableLow;

  ETH->MACFCR = (uint32_t)((ETH_InitStructure.ETH_PauseTime << 16) |
                   ETH_InitStructure.ETH_UnicastPauseFrameDetect |
                   ETH_InitStructure.ETH_ReceiveFlowControl |
                   ETH_InitStructure.ETH_TransmitFlowControl);

  ETH->MACVLANTR = (uint32_t)(ETH_InitStructure.ETH_VLANTagComparison |
                             ETH_InitStructure.ETH_VLANTagIdentifier);

  ETH->DMAOMR = (uint32_t)(ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame |
                  ETH_InitStructure.ETH_TransmitStoreForward |
                  ETH_InitStructure.ETH_ForwardErrorFrames |
                  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames);

  /* Reset the physical layer + start with auto MDIX polarity. */
  ETH_WritePHYRegister(PHY_ADDRESS, PHY_BCR, PHY_Reset);
  ETH_WritePHYRegister(PHY_ADDRESS, PHY_MDIX, (2u << 2));   /* PHY_PN_SWITCH_AUTO */

  /* Configure the MAC address */
  ETH->MACA0HR = (uint32_t)((mac[5] << 8) | mac[4]);
  ETH->MACA0LR = (uint32_t)(mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24));

  /* Mask interrupt counters */
  ETH->MMCTIMR = ETH_MMCTIMR_TGFM;
  ETH->MMCRIMR = ETH_MMCRIMR_RGUFM | ETH_MMCRIMR_RFCEM;

  ETH_DMAITConfig(ETH_DMA_IT_NIS |
                  ETH_DMA_IT_R |
                  ETH_DMA_IT_T |
                  ETH_DMA_IT_AIS |
                  ETH_DMA_IT_RBU |
                  ETH_DMA_IT_PHYLINK, ENABLE);

  eth_led_config();
}

/**
  * @brief  Full Ethernet init: clocks, MAC registers, descriptor chain,
  *         PHY, and the DMA interrupt.
  */
static void eth_init(uint8_t *mac)
{
  ChipId = DBGMCU_GetCHIPID();
  RandVal = (mac[3] ^ mac[4] ^ mac[5]) * 214017 + 2531017;
  eth_mac_config(mac);
  ETH_DMATxDescChainInit(DMATxDscrTab, MACTxBuf, ETH_TXBUFNB);
  ETH_DMARxDescChainInit(DMARxDscrTab, MACRxBuf, ETH_RXBUFNB);
  NVIC_SetPriority(ETH_IRQn, 0);
  /* The ETH IRQ is not enabled yet: with the MAC stopped the built-in PHY
   * link-change interrupt can fire continuously and starve the main loop.
   * It is enabled from eth_link_up_cfg() once the link is up (RX is running). */
  TICK_Init();   /* Delay_Us() disables SysTick - restore the 1 ms tick */
}

/*******************************************************************************
                       Link state machine (WCH EVT D8C_10M port)
*******************************************************************************/
/**
  * @brief  Restart the PHY auto-negotiation (WCH's PHY_RESTART_AUTONEGOTIATION).
  */
static void eth_phy_restart_autoneg(void)
{
  uint32_t RegVal;

  RegVal = ETH_ReadPHYRegister(gPHYAddress, PHY_BCR);
  RegVal &= ~0x01;
  RegVal |= PHY_Restart_AutoNegotiation;
  ETH_WritePHYRegister(gPHYAddress, PHY_BCR, RegVal);

  RegVal = ETH_ReadPHYRegister(gPHYAddress, PHY_BCR);
  RegVal |= 0x03 | PHY_Restart_AutoNegotiation;
  ETH_WritePHYRegister(gPHYAddress, PHY_BCR, RegVal);
}

/**
  * @brief  Toggle the PHY TX/RX (MDIX) pair (WCH's PHY_TR_SWITCH).
  */
static void eth_phy_tr_switch(void)
{
  uint32_t phy_mdix;

  phy_mdix = ETH_ReadPHYRegister(gPHYAddress, PHY_MDIX);
  if (phy_mdix & 0x01)
  {
    phy_mdix &= ~0x03;
    phy_mdix |= 1 << 1;
  }
  else
  {
    phy_mdix &= ~0x03;
    phy_mdix |= 1 << 0;
  }
  ETH_WritePHYRegister(gPHYAddress, PHY_MDIX, phy_mdix);
  eth_phy_restart_autoneg();
}

/**
  * @brief  Set the PHY PN polarity (WCH's PHY_PN_SWITCH).
  */
static void eth_phy_pn_switch(uint8_t mode)
{
  uint32_t phy_pn;

  if (mode == (2u << 2))                     /* PHY_PN_SWITCH_AUTO */
  {
    phyPN = (2u << 2);
  }
  else
  {
    phyPN = (ETH_ReadPHYRegister(gPHYAddress, PHY_MDIX) & (~(0x03 << 2))) | mode;
  }
  ETH_WritePHYRegister(gPHYAddress, PHY_MDIX, phyPN);
  phyPN = mode;
  eth_phy_restart_autoneg();
}

/**
  * @brief  Reset the PHY negotiation parameters (WCH's PHY_NEGOTIATION_PARAM_INIT).
  */
static void eth_phy_neg_param_init(void)
{
  phyStatus = 0;
  phySucCnt = 0;
  phyLinkCnt = 0;
  TRDetectStep = 0;
  PhyPolarityDetect = 0;
  phyLinkStatus = 0;                        /* PHY_LINK_INIT */
  phyPN = (2u << 2);                        /* PHY_PN_SWITCH_AUTO */
  ETH_WritePHYRegister(gPHYAddress, PHY_MDIX, phyPN);
}

/**
  * @brief  PHY reset + negotiation re-init (WCH's PHY_LINK_RESET).
  */
static void eth_phy_link_reset(void)
{
  ETH_WritePHYRegister(gPHYAddress, PHY_BCR, PHY_Reset);
  eth_phy_neg_param_init();
}

/**
  * @brief  MAC configuration when the 10M link is established
  *         (WCH's ETH_LinkUpCfg).
  */
static void eth_link_up_cfg(void)
{
  ETH->MACCR &= ~(ETH_Speed_100M | ETH_Speed_1000M);   /* 10M only */
  phyStatus = PHY_Linked_Status;

  /* Promiscuous + receive-all during the MDIX polarity check. */
  ETH->MACFFR |= (ETH_ReceiveAll_Enable | ETH_PromiscuousMode_Enable);

  /* Reset the MMC counters. */
  ETH->MMCCR |= ETH_MMCCR_CR;
  while (ETH->MMCCR & ETH_MMCCR_CR)
  {
  }

  PhyPolarityDetect = 1;
  LinkSuccTime = HAL_GetTick();
  LinkSta = 1;
  ETH_Start();
  NVIC_EnableIRQ(ETH_IRQn);   /* RX is running now: enable the DMA interrupt */
}

/**
  * @brief  10BASE-T MDIX polarity recovery (WCH's WCHNET_PhyPNProcess).
  */
static void eth_phy_pn_process(void)
{
  uint32_t PhyVal;

  LinkSuccTime = HAL_GetTick();
  if ((ETH->MMCRGUFCR == 0) && (ETH->MMCRFCECR >= 3))
  {
    PhyVal = ETH_ReadPHYRegister(gPHYAddress, PHY_MDIX);
    if ((PhyVal >> 2) & 0x01)
    {
      PhyVal &= ~(3u << 2);              /* change to normal polarity */
    }
    else
    {
      PhyVal |= 1u << 2;                 /* change to reverse polarity */
    }
    ETH_WritePHYRegister(gPHYAddress, PHY_MDIX, PhyVal);
    ETH->MMCCR |= ETH_MMCCR_CR;          /* reset the MMC counters */
    while (ETH->MMCCR & ETH_MMCCR_CR)
    {
    }
  }

  if (ETH->MMCRGUFCR != 0)               /* good frames now arrive */
  {
    PhyPolarityDetect = 0;
    ETH->MACFFR &= ~(ETH_ReceiveAll_Enable | ETH_PromiscuousMode_Enable);
  }
}

/**
  * @brief  Configure the MAC after the 10M link is up/down (WCH's ETH_PHYLink).
  */
static void eth_phy_link(struct netif *netif)
{
  uint16_t phy_bsr, phy_bcr, phy_anlpar, phy_stat;

  phy_bsr = ETH_ReadPHYRegister(gPHYAddress, PHY_BSR);
  phy_bcr = ETH_ReadPHYRegister(gPHYAddress, PHY_BCR);
  phy_anlpar = ETH_ReadPHYRegister(gPHYAddress, PHY_ANLPAR);

  if (phy_bsr & PHY_Linked_Status)                 /* link up */
  {
    if (phy_bcr & PHY_AutoNegotiation)
    {
      if (phy_anlpar == 0)
      {
        if (phy_bsr & PHY_AutoNego_Complete)
        {
          ETH->MACCR &= ~ETH_Mode_FullDuplex;
          eth_link_up_cfg();
          netif_set_up(netif);
          netif_set_link_up(netif);
        }
        /* else: still negotiating - the periodic poll will re-check. */
      }
      else
      {
        if (phy_bsr & PHY_AutoNego_Complete)
        {
          phy_stat = ETH_ReadPHYRegister(gPHYAddress, PHY_STATUS);
          if (phy_stat & (1 << 2))
          {
            ETH->MACCR |= ETH_Mode_FullDuplex;
          }
          else
          {
            ETH->MACCR &= ~ETH_Mode_FullDuplex;
          }
          eth_link_up_cfg();
          netif_set_up(netif);
          netif_set_link_up(netif);
        }
        else
        {
          /* Negotiation not complete: restart the PHY in 500 ms. */
          EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
          phyLinkReset = 1;
          phyLinkTime = HAL_GetTick();
        }
      }
    }
    else
    {
      ETH->MACCR &= ~ETH_Mode_FullDuplex;
      eth_link_up_cfg();
      netif_set_up(netif);
      netif_set_link_up(netif);
    }
  }
  else                                             /* link down */
  {
    if (LinkSta)
    {
      eth_stop();
      LinkSta = 0;
      phyStatus = 0;
      PhyPolarityDetect = 0;
      netif_set_link_down(netif);
      netif_set_down(netif);
    }
    EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
    phyLinkReset = 1;
    phyLinkTime = HAL_GetTick();
  }
}

/**
  * @brief  Auto-negotiation state machine (WCH's WCHNET_LinkProcess).
  */
static void eth_link_process(void)
{
  uint16_t phy_anlpar, phy_bmsr;

  phy_anlpar = ETH_ReadPHYRegister(gPHYAddress, PHY_ANLPAR);
  phy_bmsr = ETH_ReadPHYRegister(gPHYAddress, PHY_BSR);

  if (phy_anlpar & PHY_ANLPAR_SELECTOR_FIELD)      /* partner is Ethernet */
  {
    if (TRDetectStep == 0)
    {
      TRDetectStep = 1;
      TRDetectCnt = 1;
      eth_phy_tr_switch();
      LinkTaskPeriod = RandVal % 100 + 50;
      return;
    }
    else if (TRDetectStep == 1)
    {
      TRDetectStep = 2;
      TRDetectCnt = 0;
    }
    if (!(phyLinkStatus & (1u << 7)))              /* PHY_LINK_WAIT_SUC */
    {
      if (phyPN == (2u << 2))                      /* AUTO */
      {
        eth_phy_pn_switch(0u << 2);                /* PHY_PN_SWITCH_P */
      }
      else if (phyPN == (0u << 2))
      {
        phyLinkStatus = 1u << 7;
      }
      else
      {
        phyLinkStatus = 1u << 7;
      }
    }
    else
    {
      if ((phySucCnt++ == 5) && ((phy_bmsr & (1 << 5)) == 0))
      {
        phySucCnt = 0;
        if (phyPN == (1u << 2))                    /* PHY_PN_SWITCH_N */
        {
          eth_phy_pn_switch(0u << 2);              /* -> P */
        }
        else
        {
          eth_phy_pn_switch(1u << 2);              /* -> N */
        }
      }
    }
    phyLinkCnt = 0;
  }
  else
  {
    if (TRDetectStep == 1)
    {
      TRDetectCnt++;
      if (TRDetectCnt == 8)
      {
        TRDetectCnt = 0;
        TRDetectStep = 0;
        ETH_WritePHYRegister(gPHYAddress, PHY_MDIX, (2u << 2));   /* AUTO */
        return;
      }
      eth_phy_tr_switch();
      return;
    }
    if (phyLinkStatus == (1u << 7))                /* WAIT_SUC */
    {
      if (phyLinkCnt++ == 15)
      {
        phyLinkCnt = 0;
        phySucCnt = 0;
        TRDetectStep = 0;
        phyLinkStatus = 0;                         /* PHY_LINK_INIT */
        eth_phy_pn_switch(2u << 2);                /* AUTO */
      }
    }
    else
    {
      if (phyPN == (0u << 2))                      /* P */
      {
        if (phyLinkCnt++ == 4)
        {
          phyLinkCnt = 0;
          eth_phy_pn_switch(1u << 2);              /* -> N */
        }
      }
      else if (phyPN == (1u << 2))                 /* N */
      {
        if (phyLinkCnt++ == 15)
        {
          phyLinkCnt = 0;
          phySucCnt = 0;
          TRDetectStep = 0;
          phyLinkStatus = 0;                       /* PHY_LINK_INIT */
          eth_phy_pn_switch(2u << 2);              /* AUTO */
        }
      }
      else
      {
        if (phyLinkCnt++ == (5000 / PHY_LINK_TASK_PERIOD))
        {
          eth_phy_link_reset();
        }
      }
    }
  }
}

/**
  * @brief  Periodic negotiation driver (WCH's WCHNET_HandlePhyNegotiation).
  */
static void eth_handle_phy_negotiation(struct netif *netif)
{
  uint32_t now = HAL_GetTick();

  if (phyLinkReset)              /* after a link-down wait 500 ms, then re-enable */
  {
    if (now - phyLinkTime >= 500)
    {
      phyLinkReset = 0;
      EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
      eth_phy_link_reset();
    }
    return;
  }

  if (!phyStatus)                /* negotiating */
  {
    /* Accelerate: if the partner is already present, run the machine at once. */
    if ((TRDetectStep < 2) &&
        (ETH_ReadPHYRegister(gPHYAddress, PHY_ANLPAR) & PHY_ANLPAR_SELECTOR_FIELD))
    {
      LinkTaskPeriod = 0;
    }

    if (now - phyLinkTime >= LinkTaskPeriod)
    {
      if (TRDetectStep == 1)
      {
        RandVal = RandVal * 214017 + 2531017;
        LinkTaskPeriod = RandVal % 100 + 50;
      }
      else
      {
        LinkTaskPeriod = 50;
      }
      phyLinkTime = now;
      eth_link_process();
    }
    ReInitMACFlag = 0;
  }
  else                           /* link up */
  {
    if (ReInitMACFlag)
    {
      if (now - phyLinkTime >= 5 * PHY_LINK_TASK_PERIOD)
      {
        uint32_t phy_stat;
        ReInitMACFlag = 0;
        phy_stat = ETH_ReadPHYRegister(gPHYAddress, PHY_BSR);
        if ((phy_stat & PHY_Linked_Status) == 0)
        {
          eth_phy_link_reset();
        }
      }
    }
    if (PhyPolarityDetect)
    {
      if (now - LinkSuccTime >= 2 * PHY_LINK_TASK_PERIOD)
      {
        eth_phy_pn_process();
      }
    }
  }
}

/**
  * @brief  Periodic link/negotiation handler, called from app_ethernet.c
  *         every 100 ms (paced here to ~50 ms).
  */
void ethernet_link_check_state(struct netif *netif)
{
  uint32_t now = HAL_GetTick();

  if (now - link_task_time < PHY_LINK_TASK_PERIOD)
  {
    return;
  }
  link_task_time = now;

  eth_handle_phy_negotiation(netif);

  /* The built-in PHY raises its link-change interrupt when the link settles;
   * with the ETH IRQ disabled until link-up, detect it by polling BSR here. */
  if (ETH_ReadPHYRegister(gPHYAddress, PHY_BSR) & PHY_Linked_Status)
  {
    if (!LinkSta)
    {
      eth_phy_link(netif);
    }
  }
}

/*******************************************************************************
                       RX error recovery (WCH EVT port)
*******************************************************************************/
static void eth_stop(void)
{
  ETH_MACTransmissionCmd(DISABLE);
  ETH_FlushTransmitFIFO();
  ETH_MACReceptionCmd(DISABLE);
}

static void eth_reinit_mac_reg(void)
{
  uint16_t timeout = 10000;
  uint32_t maccr, macmiiar, macffr, machthr, machtlr;
  uint32_t macfcr, macvlantr, dmaomr;

  /* Wait for the transmit process to suspend. */
  while ((ETH->DMASR & (7 << 20)) != ETH_DMA_TransmitProcess_Suspended);

  eth_stop();

  /* Save the register values. */
  macmiiar = ETH->MACMIIAR;
  maccr = ETH->MACCR;
  macffr = ETH->MACFFR;
  machthr = ETH->MACHTHR;
  machtlr = ETH->MACHTLR;
  macfcr = ETH->MACFCR;
  macvlantr = ETH->MACVLANTR;
  dmaomr = ETH->DMAOMR;

  /* Software reset. */
  ETH_SoftwareReset();
  do
  {
    Delay_Us(10);
    if (!--timeout) break;
  } while (ETH->DMABMR & ETH_DMABMR_SR);

  /* Configure the MAC address. */
  ETH->MACA0HR = (uint32_t)((MACAddr[5] << 8) | MACAddr[4]);
  ETH->MACA0LR = (uint32_t)(MACAddr[0] | (MACAddr[1] << 8) |
                            (MACAddr[2] << 16) | (MACAddr[3] << 24));

  /* Mask interrupt counters. */
  ETH->MMCTIMR = ETH_MMCTIMR_TGFM;
  ETH->MMCRIMR = ETH_MMCRIMR_RGUFM | ETH_MMCRIMR_RFCEM;

  ETH_DMAITConfig(ETH_DMA_IT_NIS |
                  ETH_DMA_IT_R |
                  ETH_DMA_IT_T |
                  ETH_DMA_IT_AIS |
                  ETH_DMA_IT_RBU |
                  ETH_DMA_IT_PHYLINK, ENABLE);

  ETH_DMATxDescChainInit(DMATxDscrTab, MACTxBuf, ETH_TXBUFNB);
  ETH_DMARxDescChainInit(DMARxDscrTab, MACRxBuf, ETH_RXBUFNB);

  /* Restore the register values. */
  ETH->MACMIIAR = macmiiar;
  ETH->MACCR = maccr;
  ETH->MACFFR = macffr;
  ETH->MACHTHR = machthr;
  ETH->MACHTLR = machtlr;
  ETH->MACFCR = macfcr;
  ETH->MACVLANTR = macvlantr;
  ETH->DMAOMR = dmaomr;

  if (LinkSta)
  {
    ETH_Start();
  }
  TICK_Init();   /* Delay_Us() disables SysTick - restore the 1 ms tick */
}

static void eth_rec_process(void)
{
  if (((ChipId & 0xf0) <= 0x20) &&
      ((ETH->DMAMFBOCR & 0x1FFE0000) != 0))
  {
    eth_reinit_mac_reg();
    ETH->DMARPDR = 0;
    ETH->DMATPDR = 0;
  }
  else
  {
    /* No missed frames: just resume the RX DMA. */
    if (ETH->DMASR & ETH_DMASR_RBUS)
    {
      ETH->DMASR = ETH_DMASR_RBUS;
    }
    ETH->DMARPDR = 0;
  }
}

/**
  * @brief  Ethernet DMA interrupt (RX buffer underrun recovery + PHY link).
  */
void ethernetif_isr(void)
{
  uint32_t int_sta = ETH->DMASR;

  if (int_sta & ETH_DMA_IT_AIS)
  {
    if (int_sta & ETH_DMA_IT_RBU)
    {
      eth_rec_process();
      ETH_DMAClearITPendingBit(ETH_DMA_IT_RBU);
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_AIS);
  }

  if (int_sta & ETH_DMA_IT_NIS)
  {
    if (int_sta & ETH_DMA_IT_R)
    {
      ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    }
    if (int_sta & ETH_DMA_IT_T)
    {
      ETH_DMAClearITPendingBit(ETH_DMA_IT_T);
    }
    if (int_sta & ETH_DMA_IT_PHYLINK)
    {
      /* Link changes are picked up by the periodic link check. */
      ETH_DMAClearITPendingBit(ETH_DMA_IT_PHYLINK);
    }
    ETH_DMAClearITPendingBit(ETH_DMA_IT_NIS);
  }
}

/**
  * @brief  ETH global interrupt vector.
  */
void ETH_IRQHandler(void)
{
  ethernetif_isr();
}
