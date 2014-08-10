/*
 * PCI defines and function prototypes
 * Copyright 1994, Drew Eckhardt
 *
 * For more information, please consult 
 * 
 * PCI BIOS Specification Revision
 * PCI Local Bus Specification
 * PCI System Design Guide
 *
 * PCI Special Interest Group
 * M/S HF3-15A
 * 5200 N.E. Elam Young Parkway
 * Hillsboro, Oregon 97124-6497
 * +1 (503) 696-2000 
 * +1 (800) 433-5177
 * 
 * Manuals are $25 each or $50 for all three, plus $7 shipping 
 * within the United States, $35 abroad.
 */



/*	PROCEDURE TO REPORT NEW PCI DEVICES
 * We are trying to collect informations on new PCI devices, using
 * the standart PCI identification procedure. If some warning is
 * displayed at boot time, please report 
 *	- /proc/pci
 *	- your exact hardware description. Try to find out
 *	  which device is unknown. It may be you mainboard chipset.
 *	  PCI-CPU bridge or PCI-ISA bridge.
 *	- If you can't find the actual information in your hardware
 *	  booklet, try to read the references of the chip on the board.
 *	- Send all that, with the word PCIPROBE in the subject,
 *	  to frederic@cao-vlsi.ibp.fr, and I'll add your device to 
 *	  the list as soon as possible
 *		fred.
 */



#ifndef PCI_H
#define PCI_H

/*
 * Under PCI, each device has 256 bytes of configuration address space,
 * of which the first 64 bytes are standardized as follows:
 */
#define PCI_VENDOR_ID		0x00	/* 16 bits */
#define PCI_DEVICE_ID		0x02	/* 16 bits */
#define PCI_COMMAND		0x04	/* 16 bits */
#define  PCI_COMMAND_IO		0x1	/* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY	0x2	/* Enable response in Memory space */
#define  PCI_COMMAND_MASTER	0x4	/* Enable bus mastering */
#define  PCI_COMMAND_SPECIAL	0x8	/* Enable response to special cycles */
#define  PCI_COMMAND_INVALIDATE	0x10	/* Use memory write and invalidate */
#define  PCI_COMMAND_VGA_PALETTE 0x20	/* Enable palette snooping */
#define  PCI_COMMAND_PARITY	0x40	/* Enable parity checking */
#define  PCI_COMMAND_WAIT 	0x80	/* Enable address/data stepping */
#define  PCI_COMMAND_SERR	0x100	/* Enable SERR */
#define  PCI_COMMAND_FAST_BACK	0x200	/* Enable back-to-back writes */

#define PCI_STATUS		0x06	/* 16 bits */
#define  PCI_STATUS_66MHZ	0x20	/* Support 66 Mhz PCI 2.1 bus */
#define  PCI_STATUS_UDF		0x40	/* Support User Definable Features */

#define  PCI_STATUS_FAST_BACK	0x80	/* Accept fast-back to back */
#define  PCI_STATUS_PARITY	0x100	/* Detected parity error */
#define  PCI_STATUS_DEVSEL_MASK	0x600	/* DEVSEL timing */
#define  PCI_STATUS_DEVSEL_FAST	0x000	
#define  PCI_STATUS_DEVSEL_MEDIUM 0x200
#define  PCI_STATUS_DEVSEL_SLOW 0x400
#define  PCI_STATUS_SIG_TARGET_ABORT 0x800 /* Set on target abort */
#define  PCI_STATUS_REC_TARGET_ABORT 0x1000 /* Master ack of " */
#define  PCI_STATUS_REC_MASTER_ABORT 0x2000 /* Set on master abort */
#define  PCI_STATUS_SIG_SYSTEM_ERROR 0x4000 /* Set when we drive SERR */
#define  PCI_STATUS_DETECTED_PARITY 0x8000 /* Set on parity error */

#define PCI_CLASS_REVISION	0x08	/* High 24 bits are class, low 8
					   revision */
#define PCI_REVISION_ID         0x08    /* Revision ID */
#define PCI_CLASS_PROG          0x09    /* Reg. Level Programming Interface */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */

#define PCI_CACHE_LINE_SIZE	0x0c	/* 8 bits */
#define PCI_LATENCY_TIMER	0x0d	/* 8 bits */
#define PCI_HEADER_TYPE		0x0e	/* 8 bits */
#define PCI_BIST		0x0f	/* 8 bits */
#define PCI_BIST_CODE_MASK	0x0f	/* Return result */
#define PCI_BIST_START		0x40	/* 1 to start BIST, 2 secs or less */
#define PCI_BIST_CAPABLE	0x80	/* 1 if BIST capable */

/*
 * Base addresses specify locations in memory or I/O space.
 * Decoded size can be determined by writing a value of 
 * 0xffffffff to the register, and reading it back.  Only 
 * 1 bits are decoded.
 */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 bits */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 bits */
#define PCI_BASE_ADDRESS_3	0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4	0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5	0x24	/* 32 bits */
#define  PCI_BASE_ADDRESS_SPACE	0x01	/* 0 = memory, 1 = I/O */
#define  PCI_BASE_ADDRESS_SPACE_IO 0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY 0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32 bit address */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Below 1M */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* prefetchable? */
#define  PCI_BASE_ADDRESS_MEM_MASK	(~0x0f)
#define  PCI_BASE_ADDRESS_IO_MASK	(~0x03)
/* bit 1 is reserved if address_space = 1 */

#define PCI_CARDBUS_CIS		0x28
#define PCI_SUBSYSTEM_ID	0x2c
#define PCI_SUBSYSTEM_VENDOR_ID	0x2e  
#define PCI_ROM_ADDRESS		0x30	/* 32 bits */
#define  PCI_ROM_ADDRESS_ENABLE	0x01	/* Write 1 to enable ROM,
					   bits 31..11 are address,
					   10..2 are reserved */
/* 0x34-0x3b are reserved */
#define PCI_INTERRUPT_LINE	0x3c	/* 8 bits */
#define PCI_INTERRUPT_PIN	0x3d	/* 8 bits */
#define PCI_MIN_GNT		0x3e	/* 8 bits */
#define PCI_MAX_LAT		0x3f	/* 8 bits */

#define PCI_CLASS_NOT_DEFINED		0x0000
#define PCI_CLASS_NOT_DEFINED_VGA	0x0001

#define PCI_BASE_CLASS_STORAGE		0x01
#define PCI_CLASS_STORAGE_SCSI		0x0100
#define PCI_CLASS_STORAGE_IDE		0x0101
#define PCI_CLASS_STORAGE_FLOPPY	0x0102
#define PCI_CLASS_STORAGE_IPI		0x0103
#define PCI_CLASS_STORAGE_RAID		0x0104
#define PCI_CLASS_STORAGE_OTHER		0x0180

#define PCI_BASE_CLASS_NETWORK		0x02
#define PCI_CLASS_NETWORK_ETHERNET	0x0200
#define PCI_CLASS_NETWORK_TOKEN_RING	0x0201
#define PCI_CLASS_NETWORK_FDDI		0x0202
#define PCI_CLASS_NETWORK_ATM		0x0203
#define PCI_CLASS_NETWORK_OTHER		0x0280

#define PCI_BASE_CLASS_DISPLAY		0x03
#define PCI_CLASS_DISPLAY_VGA		0x0300
#define PCI_CLASS_DISPLAY_XGA		0x0301
#define PCI_CLASS_DISPLAY_OTHER		0x0380

#define PCI_BASE_CLASS_MULTIMEDIA	0x04
#define PCI_CLASS_MULTIMEDIA_VIDEO	0x0400
#define PCI_CLASS_MULTIMEDIA_AUDIO	0x0401
#define PCI_CLASS_MULTIMEDIA_OTHER	0x0480

#define PCI_BASE_CLASS_MEMORY		0x05
#define  PCI_CLASS_MEMORY_RAM		0x0500
#define  PCI_CLASS_MEMORY_FLASH		0x0501
#define  PCI_CLASS_MEMORY_OTHER		0x0580

#define PCI_BASE_CLASS_BRIDGE		0x06
#define  PCI_CLASS_BRIDGE_HOST		0x0600
#define  PCI_CLASS_BRIDGE_ISA		0x0601
#define  PCI_CLASS_BRIDGE_EISA		0x0602
#define  PCI_CLASS_BRIDGE_MC		0x0603
#define  PCI_CLASS_BRIDGE_PCI		0x0604
#define  PCI_CLASS_BRIDGE_PCMCIA	0x0605
#define  PCI_CLASS_BRIDGE_NUBUS		0x0606
#define  PCI_CLASS_BRIDGE_CARDBUS	0x0607
#define  PCI_CLASS_BRIDGE_OTHER		0x0680


#define PCI_BASE_CLASS_COMMUNICATION	0x07
#define PCI_CLASS_COMMUNICATION_SERIAL	0x0700
#define PCI_CLASS_COMMUNICATION_PARALLEL 0x0701
#define PCI_CLASS_COMMUNICATION_OTHER	0x0780

#define PCI_BASE_CLASS_SYSTEM		0x08
#define PCI_CLASS_SYSTEM_PIC		0x0800
#define PCI_CLASS_SYSTEM_DMA		0x0801
#define PCI_CLASS_SYSTEM_TIMER		0x0802
#define PCI_CLASS_SYSTEM_RTC		0x0803
#define PCI_CLASS_SYSTEM_OTHER		0x0880

#define PCI_BASE_CLASS_INPUT		0x09
#define PCI_CLASS_INPUT_KEYBOARD	0x0900
#define PCI_CLASS_INPUT_PEN		0x0901
#define PCI_CLASS_INPUT_MOUSE		0x0902
#define PCI_CLASS_INPUT_OTHER		0x0980

#define PCI_BASE_CLASS_DOCKING		0x0a
#define PCI_CLASS_DOCKING_GENERIC	0x0a00
#define PCI_CLASS_DOCKING_OTHER		0x0a01

#define PCI_BASE_CLASS_PROCESSOR	0x0b
#define PCI_CLASS_PROCESSOR_386		0x0b00
#define PCI_CLASS_PROCESSOR_486		0x0b01
#define PCI_CLASS_PROCESSOR_PENTIUM	0x0b02
#define PCI_CLASS_PROCESSOR_ALPHA	0x0b10
#define PCI_CLASS_PROCESSOR_POWERPC	0x0b20
#define PCI_CLASS_PROCESSOR_CO		0x0b40

#define PCI_BASE_CLASS_SERIAL		0x0c
#define PCI_CLASS_SERIAL_FIREWIRE	0x0c00
#define PCI_CLASS_SERIAL_ACCESS		0x0c01
#define PCI_CLASS_SERIAL_SSA		0x0c02
#define PCI_CLASS_SERIAL_USB		0x0c03
#define PCI_CLASS_SERIAL_FIBER		0x0c04

#define PCI_CLASS_OTHERS		0xff

/*
 * Vendor and card ID's: sort these numerically according to vendor
 * (and according to card ID within vendor)
 */
#define PCI_VENDOR_ID_COMPAQ		0x0e11
#define PCI_DEVICE_ID_COMPAQ_1280	0x3033
#define PCI_DEVICE_ID_COMPAQ_THUNDER	0xf130

#define PCI_VENDOR_ID_NCR		0x1000
#define PCI_DEVICE_ID_NCR_53C810	0x0001
#define PCI_DEVICE_ID_NCR_53C820	0x0002
#define PCI_DEVICE_ID_NCR_53C825	0x0003
#define PCI_DEVICE_ID_NCR_53C815	0x0004

#define PCI_VENDOR_ID_ATI		0x1002
#define PCI_DEVICE_ID_ATI_68800		0x4158
#define PCI_DEVICE_ID_ATI_215CT222	0x4354
#define PCI_DEVICE_ID_ATI_210888CX	0x4358
#define PCI_DEVICE_ID_ATI_210888GX	0x4758

#define PCI_VENDOR_ID_VLSI		0x1004
#define PCI_DEVICE_ID_VLSI_82C592	0x0005
#define PCI_DEVICE_ID_VLSI_82C593	0x0006
#define PCI_DEVICE_ID_VLSI_82C594	0x0007
#define PCI_DEVICE_ID_VLSI_82C597	0x0009

#define PCI_VENDOR_ID_ADL		0x1005
#define PCI_DEVICE_ID_ADL_2301		0x2301

#define PCI_VENDOR_ID_NS		0x100b
#define PCI_DEVICE_ID_NS_87410		0xd001

#define PCI_VENDOR_ID_TSENG		0x100c
#define PCI_DEVICE_ID_TSENG_W32P_2	0x3202
#define PCI_DEVICE_ID_TSENG_W32P_b	0x3205
#define PCI_DEVICE_ID_TSENG_W32P_c	0x3206
#define PCI_DEVICE_ID_TSENG_W32P_d	0x3207

#define PCI_VENDOR_ID_WEITEK		0x100e
#define PCI_DEVICE_ID_WEITEK_P9000	0x9001
#define PCI_DEVICE_ID_WEITEK_P9100	0x9100

#define PCI_VENDOR_ID_DEC		0x1011
#define PCI_DEVICE_ID_DEC_BRD		0x0001
#define PCI_DEVICE_ID_DEC_TULIP		0x0002
#define PCI_DEVICE_ID_DEC_TGA		0x0004
#define PCI_DEVICE_ID_DEC_TULIP_FAST	0x0009
#define PCI_DEVICE_ID_DEC_FDDI		0x000F
#define PCI_DEVICE_ID_DEC_TULIP_PLUS	0x0014

#define PCI_VENDOR_ID_CIRRUS		0x1013
#define PCI_DEVICE_ID_CIRRUS_5430	0x00a0
#define PCI_DEVICE_ID_CIRRUS_5434_4	0x00a4
#define PCI_DEVICE_ID_CIRRUS_5434_8	0x00a8
#define PCI_DEVICE_ID_CIRRUS_5436	0x00ac
#define PCI_DEVICE_ID_CIRRUS_6205	0x0205
#define PCI_DEVICE_ID_CIRRUS_6729	0x1100
#define PCI_DEVICE_ID_CIRRUS_7542	0x1200
#define PCI_DEVICE_ID_CIRRUS_7543	0x1202

#define PCI_VENDOR_ID_IBM		0x1014
#define PCI_DEVICE_ID_IBM_82G2675	0x001d

#define PCI_VENDOR_ID_WD		0x101c
#define PCI_DEVICE_ID_WD_7197		0x3296

#define PCI_VENDOR_ID_AMD		0x1022
#define PCI_DEVICE_ID_AMD_LANCE		0x2000
#define PCI_DEVICE_ID_AMD_SCSI		0x2020

#define PCI_VENDOR_ID_TRIDENT		0x1023
#define PCI_DEVICE_ID_TRIDENT_9420	0x9420
#define PCI_DEVICE_ID_TRIDENT_9440	0x9440
#define PCI_DEVICE_ID_TRIDENT_9660	0x9660

#define PCI_VENDOR_ID_AI		0x1025
#define PCI_DEVICE_ID_AI_M1435		0x1435

#define PCI_VENDOR_ID_MATROX		0x102B
#define PCI_DEVICE_ID_MATROX_MGA_2	0x0518
#define PCI_DEVICE_ID_MATROX_MIL	0x0519
#define PCI_DEVICE_ID_MATROX_MGA_IMP	0x0d10

#define PCI_VENDOR_ID_CT		0x102c
#define PCI_DEVICE_ID_CT_65545		0x00d8

#define PCI_VENDOR_ID_FD		0x1036
#define PCI_DEVICE_ID_FD_36C70		0x0000

#define PCI_VENDOR_ID_SI		0x1039
#define PCI_DEVICE_ID_SI_6201		0x0001
#define PCI_DEVICE_ID_SI_6202		0x0002
#define PCI_DEVICE_ID_SI_503		0x0008
#define PCI_DEVICE_ID_SI_501		0x0406
#define PCI_DEVICE_ID_SI_496		0x0496
#define PCI_DEVICE_ID_SI_601		0x0601
#define PCI_DEVICE_ID_SI_5511		0x5511
#define PCI_DEVICE_ID_SI_5513		0x5513

#define PCI_VENDOR_ID_HP		0x103c
#define PCI_DEVICE_ID_HP_J2585A		0x1030

#define PCI_VENDOR_ID_PCTECH		0x1042
#define PCI_DEVICE_ID_PCTECH_RZ1000	0x1000

#define PCI_VENDOR_ID_DPT               0x1044   
#define PCI_DEVICE_ID_DPT               0xa400  

#define PCI_VENDOR_ID_OPTI		0x1045
#define PCI_DEVICE_ID_OPTI_92C178	0xc178
#define PCI_DEVICE_ID_OPTI_82C557	0xc557
#define PCI_DEVICE_ID_OPTI_82C558	0xc558
#define PCI_DEVICE_ID_OPTI_82C621	0xc621
#define PCI_DEVICE_ID_OPTI_82C822	0xc822

#define PCI_VENDOR_ID_SGS		0x104a
#define PCI_DEVICE_ID_SGS_2000		0x0008
#define PCI_DEVICE_ID_SGS_1764		0x0009

#define PCI_VENDOR_ID_BUSLOGIC		0x104B
#define PCI_DEVICE_ID_BUSLOGIC_946C_2	0x0140
#define PCI_DEVICE_ID_BUSLOGIC_946C	0x1040
#define PCI_DEVICE_ID_BUSLOGIC_930	0x8130

#define PCI_VENDOR_ID_OAK		0x104e
#define PCI_DEVICE_ID_OAK_OTI107	0x0107

#define PCI_VENDOR_ID_PROMISE		0x105a
#define PCI_DEVICE_ID_PROMISE_5300	0x5300

#define PCI_VENDOR_ID_N9		0x105d
#define PCI_DEVICE_ID_N9_I128		0x2309
#define PCI_DEVICE_ID_N9_I128_2		0x2339

#define PCI_VENDOR_ID_UMC		0x1060
#define PCI_DEVICE_ID_UMC_UM8673F	0x0101
#define PCI_DEVICE_ID_UMC_UM8891A	0x0891
#define PCI_DEVICE_ID_UMC_UM8886BF	0x673a
#define PCI_DEVICE_ID_UMC_UM8886A	0x886a
#define PCI_DEVICE_ID_UMC_UM8881F	0x8881
#define PCI_DEVICE_ID_UMC_UM8886F	0x8886
#define PCI_DEVICE_ID_UMC_UM9017F	0x9017
#define PCI_DEVICE_ID_UMC_UM8886N	0xe886
#define PCI_DEVICE_ID_UMC_UM8891N	0xe891

#define PCI_VENDOR_ID_X			0x1061
#define PCI_DEVICE_ID_X_AGX016		0x0001

#define PCI_VENDOR_ID_NEXGEN		0x1074
#define PCI_DEVICE_ID_NEXGEN_82C501	0x4e78

#define PCI_VENDOR_ID_QLOGIC		0x1077
#define PCI_DEVICE_ID_QLOGIC_ISP1020	0x1020
#define PCI_DEVICE_ID_QLOGIC_ISP1022	0x1022

#define PCI_VENDOR_ID_LEADTEK		0x107d
#define PCI_DEVICE_ID_LEADTEK_805	0x0000

#define PCI_VENDOR_ID_CONTAQ		0x1080
#define PCI_DEVICE_ID_CONTAQ_82C599	0x0600

#define PCI_VENDOR_ID_FOREX		0x1083

#define PCI_VENDOR_ID_OLICOM		0x108d

#define PCI_VENDOR_ID_CMD		0x1095
#define PCI_DEVICE_ID_CMD_640		0x0640
#define PCI_DEVICE_ID_CMD_646		0x0646

#define PCI_VENDOR_ID_VISION		0x1098
#define PCI_DEVICE_ID_VISION_QD8500	0x0001
#define PCI_DEVICE_ID_VISION_QD8580	0x0002

#define PCI_VENDOR_ID_SIERRA		0x10a8
#define PCI_DEVICE_ID_SIERRA_STB	0x0000

#define PCI_VENDOR_ID_ACC		0x10aa
#define PCI_DEVICE_ID_ACC_2056		0x0000

#define PCI_VENDOR_ID_WINBOND		0x10ad
#define PCI_DEVICE_ID_WINBOND_83769	0x0001
#define PCI_DEVICE_ID_WINBOND_82C105	0x0105

#define PCI_VENDOR_ID_3COM		0x10b7
#define PCI_DEVICE_ID_3COM_3C590	0x5900
#define PCI_DEVICE_ID_3COM_3C595TX	0x5950
#define PCI_DEVICE_ID_3COM_3C595T4	0x5951
#define PCI_DEVICE_ID_3COM_3C595MII	0x5952

#define PCI_VENDOR_ID_AL		0x10b9
#define PCI_DEVICE_ID_AL_M1445		0x1445
#define PCI_DEVICE_ID_AL_M1449		0x1449
#define PCI_DEVICE_ID_AL_M1451		0x1451
#define PCI_DEVICE_ID_AL_M1461		0x1461
#define PCI_DEVICE_ID_AL_M1489		0x1489
#define PCI_DEVICE_ID_AL_M1511		0x1511
#define PCI_DEVICE_ID_AL_M1513		0x1513
#define PCI_DEVICE_ID_AL_M4803		0x5215

#define PCI_VENDOR_ID_ASP		0x10cd
#define PCI_DEVICE_ID_ASP_ABP940	0x1200

#define PCI_VENDOR_ID_IMS		0x10e0
#define PCI_DEVICE_ID_IMS_8849		0x8849

#define PCI_VENDOR_ID_TEKRAM2		0x10e1
#define PCI_DEVICE_ID_TEKRAM2_690c	0x690c

#define PCI_VENDOR_ID_AMCC		0x10e8
#define PCI_DEVICE_ID_AMCC_MYRINET	0x8043

#define PCI_VENDOR_ID_INTERG		0x10ea
#define PCI_DEVICE_ID_INTERG_1680	0x1680

#define PCI_VENDOR_ID_REALTEK		0x10ec
#define PCI_DEVICE_ID_REALTEK_8029	0x8029

#define PCI_VENDOR_ID_INIT		0x1101
#define PCI_DEVICE_ID_INIT_320P		0x9100

#define PCI_VENDOR_ID_VIA		0x1106
#define PCI_DEVICE_ID_VIA_82C505	0x0505
#define PCI_DEVICE_ID_VIA_82C561	0x0561
#define PCI_DEVICE_ID_VIA_82C576	0x0576
#define PCI_DEVICE_ID_VIA_82C416	0x1571

#define PCI_VENDOR_ID_VORTEX		0x1119
#define PCI_DEVICE_ID_VORTEX_GDT	0x0001

#define PCI_VENDOR_ID_EF		0x111a
#define PCI_DEVICE_ID_EF_ATM_FPGA	0x0000
#define PCI_DEVICE_ID_EF_ATM_ASIC	0x0002

#define PCI_VENDOR_ID_FORE		0x1127
#define PCI_DEVICE_ID_FORE_PCA200PC	0x0210

#define PCI_VENDOR_ID_IMAGINGTECH	0x112f
#define PCI_DEVICE_ID_IMAGINGTECH_ICPCI	0x0000

#define PCI_VENDOR_ID_PLX		0x113c
#define PCI_DEVICE_ID_PLX_9060		0x0001

#define PCI_VENDOR_ID_ALLIANCE		0x1142
#define PCI_DEVICE_ID_ALLIANCE_PROMOTIO	0x3210
#define PCI_DEVICE_ID_ALLIANCE_PROVIDEO	0x6422

#define PCI_VENDOR_ID_MUTECH		0x1159
#define PCI_DEVICE_ID_MUTECH_MV1000	0x0001

#define PCI_VENDOR_ID_ZEITNET		0x1193
#define PCI_DEVICE_ID_ZEITNET_1221	0x0001
#define PCI_DEVICE_ID_ZEITNET_1225	0x0002

#define PCI_VENDOR_ID_SPECIALIX		0x11cb
#define PCI_DEVICE_ID_SPECIALIX_XIO	0x4000
#define PCI_DEVICE_ID_SPECIALIX_RIO	0x8000

#define PCI_VENDOR_ID_RP               0x11fe
#define PCI_DEVICE_ID_RP8OCTA          0x0001
#define PCI_DEVICE_ID_RP8INTF          0x0002
#define PCI_DEVICE_ID_RP16INTF         0x0003
#define PCI_DEVICE_ID_RP32INTF         0x0004

#define PCI_VENDOR_ID_CYCLADES		0x120e
#define PCI_DEVICE_ID_CYCLADES_Y	0x0100

#define PCI_VENDOR_ID_SYMPHONY		0x1c1c
#define PCI_DEVICE_ID_SYMPHONY_101	0x0001

#define PCI_VENDOR_ID_TEKRAM		0x1de1
#define PCI_DEVICE_ID_TEKRAM_DC290	0xdc29

#define PCI_VENDOR_ID_AVANCE		0x4005
#define PCI_DEVICE_ID_AVANCE_2302	0x2302

#define PCI_VENDOR_ID_S3		0x5333
#define PCI_DEVICE_ID_S3_811		0x8811
#define PCI_DEVICE_ID_S3_868		0x8880
#define PCI_DEVICE_ID_S3_928		0x88b0
#define PCI_DEVICE_ID_S3_864_1		0x88c0
#define PCI_DEVICE_ID_S3_864_2		0x88c1
#define PCI_DEVICE_ID_S3_964_1		0x88d0
#define PCI_DEVICE_ID_S3_964_2		0x88d1
#define PCI_DEVICE_ID_S3_968		0x88f0

#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_INTEL_82375	0x0482
#define PCI_DEVICE_ID_INTEL_82424	0x0483
#define PCI_DEVICE_ID_INTEL_82378	0x0484
#define PCI_DEVICE_ID_INTEL_82430	0x0486
#define PCI_DEVICE_ID_INTEL_82434	0x04a3
#define PCI_DEVICE_ID_INTEL_7116	0x1223
#define PCI_DEVICE_ID_INTEL_82596	0x1226
#define PCI_DEVICE_ID_INTEL_82865	0x1227
#define PCI_DEVICE_ID_INTEL_82557	0x1229
#define PCI_DEVICE_ID_INTEL_82437	0x122d
#define PCI_DEVICE_ID_INTEL_82371_0	0x122e
#define PCI_DEVICE_ID_INTEL_82371_1	0x1230
#define PCI_DEVICE_ID_INTEL_P6		0x84c4

#define PCI_VENDOR_ID_ADAPTEC		0x9004
#define PCI_DEVICE_ID_ADAPTEC_7850	0x5078
#define PCI_DEVICE_ID_ADAPTEC_7870	0x7078
#define PCI_DEVICE_ID_ADAPTEC_7871	0x7178
#define PCI_DEVICE_ID_ADAPTEC_7872	0x7278
#define PCI_DEVICE_ID_ADAPTEC_7873	0x7378
#define PCI_DEVICE_ID_ADAPTEC_7874	0x7478
#define PCI_DEVICE_ID_ADAPTEC_7880	0x8078
#define PCI_DEVICE_ID_ADAPTEC_7881	0x8178
#define PCI_DEVICE_ID_ADAPTEC_7882	0x8278
#define PCI_DEVICE_ID_ADAPTEC_7883	0x8378
#define PCI_DEVICE_ID_ADAPTEC_7884	0x8478

#define PCI_VENDOR_ID_ATRONICS		0x907f
#define PCI_DEVICE_ID_ATRONICS_2015	0x2015

#define PCI_VENDOR_ID_HER		0xedd8
#define PCI_DEVICE_ID_HER_STING		0xa091
#define PCI_DEVICE_ID_HER_STINGARK	0xa099

/*
 * The PCI interface treats multi-function devices as independent
 * devices.  The slot/function address of each device is encoded
 * in a single byte as follows:
 *
 *	7:4 = slot
 *	3:0 = function
 */
#define PCI_DEVFN(slot,func)	((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

/*
 * There is one pci_dev structure for each slot-number/function-number
 * combination:
 */
struct pci_dev {
	struct pci_bus	*bus;		/* bus this device is on */
	struct pci_dev	*sibling;	/* next device on this bus */
	struct pci_dev	*next;		/* chain of all devices */

	void		*sysdata;	/* hook for sys-specific extension */

	unsigned int	devfn;		/* encoded device & function index */
	unsigned short	vendor;
	unsigned short	device;
	unsigned int	class;		/* 3 bytes: (base,sub,prog-if) */
	unsigned int	master : 1;	/* set if device is master capable */
	/*
	 * In theory, the irq level can be read from configuration
	 * space and all would be fine.  However, old PCI chips don't
	 * support these registers and return 0 instead.  For example,
	 * the Vision864-P rev 0 chip can uses INTA, but returns 0 in
	 * the interrupt line and pin registers.  pci_init()
	 * initializes this field with the value at PCI_INTERRUPT_LINE
	 * and it is the job of pcibios_fixup() to change it if
	 * necessary.  The field must not be 0 unless the device
	 * cannot generate interrupts at all.
	 */
	unsigned char	irq;		/* irq generated by this device */
};

struct pci_bus {
	struct pci_bus	*parent;	/* parent bus this bridge is on */
	struct pci_bus	*children;	/* chain of P2P bridges on this bus */
	struct pci_bus	*next;		/* chain of all PCI buses */

	struct pci_dev	*self;		/* bridge device as seen by parent */
	struct pci_dev	*devices;	/* devices behind this bridge */

	void		*sysdata;	/* hook for sys-specific extension */

	unsigned char	number;		/* bus number */
	unsigned char	primary;	/* number of primary bridge */
	unsigned char	secondary;	/* number of secondary bridge */
	unsigned char	subordinate;	/* max number of subordinate buses */
};

/*
 * This is used to map a vendor-id/device-id pair into device-specific
 * information.
 */
struct pci_dev_info {
	unsigned short	vendor;		/* vendor id */
	unsigned short	device;		/* device id */

	const char	*name;		/* device name */
	unsigned char	bridge_type;	/* bridge type or 0xff */
};

extern struct pci_bus	pci_root;	/* root bus */
extern struct pci_dev	*pci_devices;	/* list of all devices */


extern unsigned long pci_init (unsigned long mem_start, unsigned long mem_end);

extern struct pci_dev_info *pci_lookup_dev (unsigned int vendor,
					    unsigned int dev);
extern const char *pci_strclass (unsigned int class);
extern const char *pci_strvendor (unsigned int vendor);
extern const char *pci_strdev (unsigned int vendor, unsigned int device);

extern int get_pci_list (char *buf);

#endif /* PCI_H */
