/*
 * hp100.h: Hewlett Packard HP10/100VG ANY LAN ethernet driver for Linux.
 *
 * Author:  Jaroslav Kysela, <perex@pf.jcu.cz>
 *
 * Header file...
 *
 * This driver is based on the 'hpfepkt' crynwr packet driver.
 *
 * This source/code is public free; you can distribute it and/or modify 
 * it under terms of the GNU General Public License (published by the
 * Free Software Foundation) either version two of this License, or any 
 * later version.
 */

/****************************************************************************
 *  Hardware Constants
 ****************************************************************************/
 
/*
 *  ATT2MD01 Register Page Constants
 */

#define HP100_PAGE_PERFORMANCE	0x0	/* Page 0 */
#define HP100_PAGE_MAC_ADDRESS	0x1	/* Page 1 */
#define HP100_PAGE_HW_MAP	0x2	/* Page 2 */
#define HP100_PAGE_EEPROM_CTRL	0x3	/* Page 3 */
#define HP100_PAGE_MAC_CTRL	0x4	/* Page 4 */
#define HP100_PAGE_MMU_CFG	0x5	/* Page 5 */
#define HP100_PAGE_ID_MAC_ADDR	0x6	/* Page 6 */
#define HP100_PAGE_MMU_POINTER	0x7	/* Page 7 */

/*
 *  ATT2MD01 Register Addresses
 */

/*  Present on all pages  */

#define HP100_REG_HW_ID		0x00	/* R:  (16) Unique card ID           */
#define HP100_REG_TRACE		0x00	/* W:  (16) Used for debug output    */
#define HP100_REG_PAGING	0x02	/* R:  (16),15:4 Card ID             */
                                        /* W:  (16),3:0 Switch pages         */
#define HP100_REG_OPTION_LSW	0x04	/* RW: (16) Select card functions    */
#define HP100_REG_OPTION_MSW	0x06	/* RW: (16) Select card functions    */
                                        
/*  Page 0 - Performance  */

#define HP100_REG_IRQ_STATUS	0x08	/* RW: (16) Which ints are pending   */
#define HP100_REG_IRQ_MASK	0x0a	/* RW: (16) Select ints to allow     */
#define HP100_REG_FRAGMENT_LEN	0x0c	/* RW: (16)12:0 Current fragment len */
#define HP100_REG_OFFSET	0x0e	/* RW: (16)12:0 Offset to start read */
#define HP100_REG_DATA32	0x10	/* RW: (32) I/O mode data port       */
#define HP100_REG_DATA16	0x12	/* RW: WORDs must be read from here  */
#define HP100_REG_TX_MEM_FREE	0x14	/* RD: (32) Amount of free Tx mem    */
#define HP100_REG_RX_PKT_CNT	0x18	/* RD: (8) Rx count of pkts on card  */
#define HP100_REG_TX_PKT_CNT	0x19	/* RD: (8) Tx count of pkts on card  */
                                        
/*  Page 1 - MAC Address/Hash Table  */

#define HP100_REG_MAC_ADDR	0x08	/* RW: (8) Cards MAC address	     */
#define HP100_REG_HASH_BYTE0	0x10	/* RW: (8) Cards multicast filter    */
                                        
/*  Page 2 - Hardware Mapping  */

#define HP100_REG_MEM_MAP_LSW	0x08	/* RW: (16) LSW of cards mem addr    */
#define HP100_REG_MEM_MAP_MSW	0x0a	/* RW: (16) MSW of cards mem addr    */
#define HP100_REG_IO_MAP	0x0c	/* RW: (8) Cards I/O address         */
#define HP100_REG_IRQ_CHANNEL	0x0d	/* RW: (8) IRQ and edge/level int    */
#define HP100_REG_SRAM		0x0e	/* RW: (8) How much RAM on card      */
#define HP100_REG_BM		0x0f	/* RW: (8) Controls BM functions     */
                                        
/*  Page 3 - EEPROM/Boot ROM  */

#define HP100_REG_EEPROM_CTRL	0x08	/* RW: (16) Used to load EEPROM      */
                                        
/*  Page 4 - LAN Configuration  */

#define HP100_REG_LAN_CFG_10	0x08	/* RW: (16) Set 10M XCVR functions   */
#define HP100_REG_LAN_CFG_VG	0x0a	/* RW: (16) Set 100M XCVR functions  */
#define HP100_REG_MAC_CFG_1	0x0c	/* RW: (8) Types of pkts to accept   */
#define HP100_REG_MAC_CFG_2	0x0d	/* RW: (8) Misc MAC functions        */
/* The follow clear when read: */
#define HP100_REG_DROPPED	0x10	/* R:  (16),11:0 Pkts cant fit in mem*/
#define HP100_REG_CRC		0x12	/* R:  (8) Pkts with CRC             */
#define HP100_REG_ABORT		0x13	/* R:  (8) Aborted Tx pkts           */
                                        
/*  Page 5 - MMU  */

#define HP100_REG_RX_MEM_STOP	0x0c	/* RW: (16) End of Rx ring addr      */
#define HP100_REG_TX_MEM_STOP	0x0e	/* RW: (16) End of Tx ring addr      */
                                        
/*  Page 6 - Card ID/Physical LAN Address  */

#define HP100_REG_BOARD_ID	0x08	/* R:  (8) EISA/ISA card ID          */
#define HP100_REG_BOARD_IO_CHCK 0x0c	/* R:  (8) Added to ID to get FFh    */
#define HP100_REG_SOFT_MODEL	0x0d	/* R:  (8) Config program defined    */
#define HP100_REG_LAN_ADDR	0x10	/* R:  (8) MAC addr of card          */
#define HP100_REG_LAN_ADDR_CHCK 0x16	/* R:  (8) Added to addr to get FFh  */
                                        
/*  Page 7 - MMU Current Pointers  */

#define HP100_REG_RX_MEM_BR	0x08	/* R:  (16) Current begin of Rx ring */
#define HP100_REG_RX_MEM_ER	0x0a	/* R:  (16) Current end of Rx ring   */
#define HP100_REG_TX_MEM_BR	0x0c	/* R:  (16) Current begin of Tx ring */
#define HP100_REG_TX_MEM_ER	0x0e	/* R:  (16) Current end of Rx ring   */
#define HP100_REG_MEM_DEBUG	0x1a	/* RW: (16) Used for memory tests    */
                                                
/*
 *  HardwareIDReg bits/masks
 */

#define HP100_HW_ID_0		0x50	/* Hardware ID bytes.                */
#define HP100_HW_ID_1		0x48
#define HP100_HW_ID_2_REVA	0x50	/* Rev. A ID. NOTE: lower nibble not used */
#define HP100_HW_ID_3		0x53

/*
 *  OptionLSWReg bits/masks
 */

#define HP100_DEBUG_EN		0x8000	/* 0:Disable, 1:Enable Debug Dump Pointer */
#define HP100_RX_HDR		0x4000	/* 0:Disable, 1:Enable putting pkt into */
                                        /*   system memory before Rx interrupt */
#define HP100_MMAP_DIS		0x2000	/* 0:Enable, 1:Disable memory mapping. */
                                        /*   MMAP_DIS must be 0 and MEM_EN must */
                                        /*   be 1 for memory-mapped mode to be */
                                        /*   enabled */
#define HP100_EE_EN		0x1000	/* 0:Disable,1:Enable EEPROM writing */
#define HP100_BM_WRITE		0x0800	/* 0:Slave, 1:Bus Master for Tx data */
#define HP100_BM_READ		0x0400	/* 0:Slave, 1:Bus Master for Rx data */
#define HP100_TRI_INT		0x0200	/* 0:Dont, 1:Do tri-state the int */
#define HP100_MEM_EN		0x0040	/* Config program set this to */
                                        /*   0:Disable, 1:Enable mem map. */
                                        /*   See MMAP_DIS. */
#define HP100_IO_EN		0x0020	/* 0:Disable, 1:Enable I/O transfers */
#define HP100_BOOT_EN		0x0010	/* 0:Disable, 1:Enable boot ROM access */
#define HP100_FAKE_INT		0x0008	/* 0:No int, 1:int */
#define HP100_INT_EN		0x0004	/* 0:Disable, 1:Enable ints from card */
#define HP100_HW_RST		0x0002	/* 0:Reset, 1:Out of reset */

/*
 *  OptionMSWReg bits/masks
 */
#define HP100_PRIORITY_TX	0x0080	/* 0:Don't, 1:Do all Tx pkts as priority */
#define HP100_EE_LOAD		0x0040	/* 1:EEPROM loading, 0 when done */
#define HP100_ADV_NXT_PKT	0x0004	/* 1:Advance to next pkt in Rx queue, */
                                        /*   h/w will set to 0 when done */
#define HP100_TX_CMD		0x0002	/* 1:Tell h/w download done, h/w will set */
                                        /*   to 0 when done */

/*
 *  InterruptStatusReg/InterruptMaskReg bits/masks.  These bits will 0 when a 1 
 *  is written to them.
 */
#define HP100_RX_PACKET		0x0400	/* 0:No, 1:Yes pkt has been Rx */
#define HP100_RX_ERROR		0x0200	/* 0:No, 1:Yes Rx pkt had error */
#define HP100_TX_SPACE_AVAIL	0x0010	/* 0:<8192, 1:>=8192 Tx free bytes */
#define HP100_TX_COMPLETE	0x0008	/* 0:No, 1:Yes a Tx has completed */
#define HP100_TX_ERROR		0x0002	/* 0:No, 1:Yes Tx pkt had error */
                                        
/*
 *  TxMemoryFreeCountReg bits/masks.
 */
#define HP100_AUTO_COMPARE	0x8000	/* Says at least 8k is available for Tx. */
                                        /*   NOTE: This mask is for the upper */
                                        /*   word of the register. */

/*
 *  IRQChannelReg bits/masks.
 */
#define HP100_ZERO_WAIT_EN	0x80	/* 0:No, 1:Yes assers NOWS signal */
#define HP100_LEVEL_IRQ		0x10	/* 0:Edge, 1:Level type interrupts. */
                                        /*   Only valid on EISA cards. */
#define HP100_IRQ_MASK		0x0F	/* Isolate the IRQ bits */

/*
 *  SRAMReg bits/masks.
 */
#define HP100_RAM_SIZE_MASK	0xe0	/* AND to get SRAM size index */
#define HP100_RAM_SIZE_SHIFT	0x05	/* Shift count to put index in lower bits */

/*
 *  BMReg bits/masks.
 */
#define HP100_BM_SLAVE		0x04	/* 0:Slave, 1:BM mode */

/*
 *  EEPROMControlReg bits/masks.
 */
#define HP100_EEPROM_LOAD	0x0001	/* 0->1 loads the EEPROM into registers. */
                                        /*   When it goes back to 0, load is  */
                                        /*   complete.  This should take ~600us. */

/*
 *  LANCntrCfg10Reg bits/masks.
 */
#define HP100_SQU_ST		0x0100	/* 0:No, 1:Yes collision signal sent */
                                        /*   after Tx.  Only used for AUI. */
#define HP100_MAC10_SEL		0x00c0	/* Get bits to indicate MAC */
#define HP100_AUI_SEL		0x0020	/* Status of AUI selection */
#define HP100_LOW_TH		0x0010	/* 0:No, 1:Yes allow better cabling */
#define HP100_LINK_BEAT_DIS	0x0008	/* 0:Enable, 1:Disable link beat */
#define HP100_LINK_BEAT_ST	0x0004	/* 0:No, 1:Yes link beat being Rx */
#define HP100_R_ROL_ST		0x0002	/* 0:No, 1:Yes Rx twisted pair has been */
                                        /*   reversed */
#define HP100_AUI_ST		0x0001	/* 0:No, 1:Yes use AUI on TP card */

/* MAC Selection, use with MAC10_SEL bits */
#define HP100_AUTO_SEL_10	0x0	/* Auto select */
#define HP100_XCVR_LXT901_10	0x1	/* LXT901 10BaseT transceiver */
#define HP100_XCVR_7213		0x2	/* 7213 transceiver */
#define HP100_XCVR_82503	0x3	/* 82503 transceiver */


/*
 *  LANCntrCfgVGReg bits/masks.
 */
#define HP100_FRAME_FORMAT	0x0800	/* 0:802.3, 1:802.5 frames */
#define HP100_BRIDGE		0x0400	/* 0:No, 1:Yes tell hub it's a bridge */
#define HP100_PROM_MODE		0x0200	/* 0:No, 1:Yes tell hub card is */
                                        /*   promiscuous */
#define HP100_REPEATER		0x0100	/* 0:No, 1:Yes tell hub MAC wants to be */
                                        /*   a cascaded repeater */
#define HP100_MAC100_SEL	0x0080	/* 0:No, 1:Yes use 100 Mbit MAC */
#define HP100_LINK_UP_ST	0x0040	/* 0:No, 1:Yes endnode logged in */
#define HP100_LINK_CABLE_ST	0x0020	/* 0:No, 1:Yes cable can hear tones from */
                                        /*   hub */
#define HP100_LOAD_ADDR		0x0010	/* 0->1 card addr will be sent to hub. */
                                        /*   100ms later the link status bits are */
                                        /*   valid */
#define HP100_LINK_CMD		0x0008	/* 0->1 link will attempt to log in. */
                                        /*   100ms later the link status bits are */
                                        /*   valid */
#define HP100_LINK_GOOD_ST	0x0002	/* 0:No, 1:Yes cable passed training */
#define HP100_VG_RESET		0x0001	/* 0:Yes, 1:No reset the 100VG MAC */


/*
 *  MACConfiguration1Reg bits/masks.
 */
#define HP100_RX_IDLE		0x80	/* 0:Yes, 1:No currently receiving pkts */
#define HP100_TX_IDLE		0x40	/* 0:Yes, 1:No currently Txing pkts */
#define HP100_RX_EN		0x20	/* 0:No, 1:Yes allow receiving of pkts */
#define HP100_TX_EN		0x10	/* 0:No, 1:Yes allow transmiting of pkts */
#define HP100_ACC_ERRORED	0x08	/* 0:No, 1:Yes allow Rx of errored pkts */
#define HP100_ACC_MC		0x04	/* 0:No, 1:Yes allow Rx of multicast pkts */
#define HP100_ACC_BC		0x02	/* 0:No, 1:Yes allow Rx of broadcast pkts */
#define HP100_ACC_PHY		0x01	/* 0:No, 1:Yes allow Rx of ALL physical pkts */

#define HP100_MAC1MODEMASK	0xf0	/* Hide ACC bits */
#define HP100_MAC1MODE1		0x00	/* Receive nothing, must also disable RX */
#define HP100_MAC1MODE2		0x00
#define HP100_MAC1MODE3		HP100_MAC1MODE2 | HP100_ACC_BC
#define HP100_MAC1MODE4		HP100_MAC1MODE3 | HP100_ACC_MC
#define HP100_MAC1MODE5		HP100_MAC1MODE4 /* set mc hash to all ones also */
#define HP100_MAC1MODE6		HP100_MAC1MODE5 | HP100_ACC_PHY	/* Promiscuous */

/* Note MODE6 will receive all GOOD packets on the LAN. This really needs
   a mode 7 defined to be LAN Analyzer mode, which will receive errored and
   runt packets, and keep the CRC bytes. */

#define HP100_MAC1MODE7		MAC1MODE6 OR ACC_ERRORED

/*
 *  MACConfiguration2Reg bits/masks.
 */
#define HP100_TR_MODE		0x80	/* 0:No, 1:Yes support Token Ring formats */
#define HP100_TX_SAME		0x40	/* 0:No, 1:Yes Tx same packet continuous */
#define HP100_LBK_XCVR		0x20	/* 0:No, 1:Yes loopback through MAC & */
                                        /*   transceiver */
#define HP100_LBK_MAC		0x10	/* 0:No, 1:Yes loopback through MAC */
#define HP100_CRC_I		0x08	/* 0:No, 1:Yes inhibit CRC on Tx packets */
#define HP100_KEEP_CRC		0x02	/* 0:No, 1:Yes keep CRC on Rx packets. */
                                        /*   The length will reflect this. */

#define HP100_MAC2MODEMASK	0x02
#define HP100_MAC2MODE1		0x00
#define HP100_MAC2MODE2		0x00
#define HP100_MAC2MODE3		0x00
#define HP100_MAC2MODE4		0x00
#define HP100_MAC2MODE5		0x00
#define HP100_MAC2MODE6		0x00
#define HP100_MAC2MODE7		KEEP_CRC

/*
 *  Set/Reset bits
 */
#define HP100_SET_HB		0x0100	/* 0:Set fields to 0 whose mask is 1 */
#define HP100_SET_LB		0x0001	/* HB sets upper byte, LB sets lower byte */
#define HP100_RESET_HB		0x0000	/* For readability when resetting bits */
#define HP100_RESET_LB		0x0000	/* For readability when resetting bits */

/*
 *  Misc. Constants
 */
#define HP100_LAN_100		100     /* lan_type value for VG */
#define HP100_LAN_10		10	/* lan_type value for 10BaseT */
#define HP100_LAN_ERR		(-1)	/* lan_type value for link down */

/*
 *  Receive Header Definition.
 */

struct hp100_rx_header {
  u_short rx_length;			/* Pkt length is bits 12:0 */
  u_short rx_status;			/* status of the packet */
};

#define HP100_PKT_LEN_MASK	0x1FFF	/* AND with RxLength to get length bits */

/* Receive Packet Status.  Note, the error bits are only valid if ACC_ERRORED 
   bit in the MAC Configuration Register 1 is set. */
   
#define HP100_RX_PRI		0x8000	/* 0:No, 1:Yes packet is priority */
#define HP100_SDF_ERR		0x4000	/* 0:No, 1:Yes start of frame error */
#define HP100_SKEW_ERR		0x2000	/* 0:No, 1:Yes skew out of range */
#define HP100_BAD_SYMBOL_ERR	0x1000	/* 0:No, 1:Yes invalid symbol received */
#define HP100_RCV_IPM_ERR	0x0800	/* 0:No, 1:Yes pkt had an invalid packet */
                                        /*   marker */
#define HP100_SYMBOL_BAL_ERR	0x0400	/* 0:No, 1:Yes symbol balance error */
#define HP100_VG_ALN_ERR	0x0200	/* 0:No, 1:Yes non-octet received */
#define HP100_TRUNC_ERR		0x0100	/* 0:No, 1:Yes the packet was truncated */
#define HP100_RUNT_ERR		0x0040	/* 0:No, 1:Yes pkt length < Min Pkt */
                                        /*   Length Reg. */
#define HP100_ALN_ERR		0x0010	/* 0:No, 1:Yes align error. */
#define HP100_CRC_ERR		0x0008	/* 0:No, 1:Yes CRC occurred. */

/* The last three bits indicate the type of destination address */

#define HP100_MULTI_ADDR_HASH	0x0006	/* 110: Addr multicast, matched hash */
#define HP100_BROADCAST_ADDR	0x0003	/* x11: Addr broadcast */
#define HP100_MULTI_ADDR_NO_HASH 0x0002	/* 010: Addr multicast, didn't match hash */
#define HP100_PHYS_ADDR_MATCH	0x0001	/* x01: Addr was physical and mine */
#define HP100_PHYS_ADDR_NO_MATCH 0x0000	/* x00: Addr was physical but not mine */

/*
 *  macros
 */

#define hp100_inb( reg ) \
        inb( ioaddr + HP100_REG_##reg )
#define hp100_inw( reg ) \
	inw( ioaddr + HP100_REG_##reg )
#define hp100_inl( reg ) \
	inl( ioaddr + HP100_REG_##reg )
#define hp100_outb( data, reg ) \
	outb( data, ioaddr + HP100_REG_##reg )
#define hp100_outw( data, reg ) \
	outw( data, ioaddr + HP100_REG_##reg )
#define hp100_outl( data, reg ) \
	outl( data, ioaddr + HP100_REG_##reg )
#define hp100_orb( data, reg ) \
	outb( inb( ioaddr + HP100_REG_##reg ) | (data), ioaddr + HP100_REG_##reg )
#define hp100_orw( data, reg ) \
	outw( inw( ioaddr + HP100_REG_##reg ) | (data), ioaddr + HP100_REG_##reg )
#define hp100_andb( data, reg ) \
	outb( inb( ioaddr + HP100_REG_##reg ) & (data), ioaddr + HP100_REG_##reg )
#define hp100_andw( data, reg ) \
	outw( inw( ioaddr + HP100_REG_##reg ) & (data), ioaddr + HP100_REG_##reg )

#define hp100_page( page ) \
	outw( HP100_PAGE_##page, ioaddr + HP100_REG_PAGING )
#define hp100_ints_off() \
	outw( HP100_INT_EN | HP100_RESET_LB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_ints_on() \
	outw( HP100_INT_EN | HP100_SET_LB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_mem_map_enable() \
	outw( HP100_MMAP_DIS | HP100_RESET_HB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_mem_map_disable() \
	outw( HP100_MMAP_DIS | HP100_SET_HB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_reset_card() \
	outw( HP100_HW_RST | HP100_RESET_LB, ioaddr + HP100_REG_OPTION_LSW )
#define hp100_unreset_card() \
	outw( HP100_HW_RST | HP100_SET_LB, ioaddr + HP100_REG_OPTION_LSW )
