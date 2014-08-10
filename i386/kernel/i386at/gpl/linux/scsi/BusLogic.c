/*

  Linux Driver for BusLogic MultiMaster SCSI Host Adapters

  Copyright 1995 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation, provided that none of the source code or runtime
  copyright notices are removed or modified.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that all modifications to this software be
  sent directly to him for evaluation and testing.

  Special thanks to Alex T. Win of BusLogic, whose advice has been invaluable,
  to David B. Gentzel, for writing the original Linux BusLogic driver, and to
  Paul Gortmaker, for being such a dedicated test site.

*/


#define BusLogic_DriverVersion		"1.3.1"
#define BusLogic_DriverDate		"31 December 1995"


#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/pci.h>
#include <linux/bios32.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>
#include "scsi.h"
#include "hosts.h"
#include "sd.h"
#include "BusLogic.h"


/*
  BusLogic_CommandLineEntryCount is a count of the number of "BusLogic="
  entries provided on the Linux Kernel Command Line.
*/

static int
  BusLogic_CommandLineEntryCount =	0;


/*
  BusLogic_CommandLineEntries is an array of Command Line Entry structures
  representing the "BusLogic=" entries provided on the Linux Kernel Command
  Line.
*/

static BusLogic_CommandLineEntry_T
  BusLogic_CommandLineEntries[BusLogic_MaxHostAdapters];


/*
  BusLogic_GlobalOptions is a bit mask of Global Options to be applied
  across all Host Adapters.
*/

static int
  BusLogic_GlobalOptions =		0;


/*
  BusLogic_RegisteredHostAdapters is a linked list of all the registered
  BusLogic Host Adapters.
*/

static BusLogic_HostAdapter_T
  *BusLogic_RegisteredHostAdapters =	NULL;


/*
  BusLogic_Standard_IO_Addresses is the list of standard I/O Addresses at which
  BusLogic Host Adapters may potentially be found.
*/

static unsigned short
  BusLogic_IO_StandardAddresses[] =
    { 0x330, 0x334, 0x230, 0x234, 0x130, 0x134, 0 };


/*
  BusLogic_IO_AddressProbeList is the list of I/O Addresses to be probed for
  potential BusLogic Host Adapters.  It is initialized by interrogating the
  PCI Configuration Space on PCI machines as well as from the list of
  standard BusLogic I/O Addresses.
*/

static unsigned short
  BusLogic_IO_AddressProbeList[BusLogic_IO_MaxProbeAddresses+1] =   { 0 };


/*
  BusLogic_IRQ_UsageCount stores a count of the number of Host Adapters using
  a given IRQ Channel, which is necessary to support PCI, EISA, or MCA shared
  interrupts.  Only IRQ Channels 9, 10, 11, 12, 14, and 15 are supported by
  BusLogic Host Adapters.
*/

static short
  BusLogic_IRQ_UsageCount[7] =		{ 0 };


/*
  BusLogic_CommandFailureReason holds a string identifying the reason why a
  call to BusLogic_Command failed.  It is only valid when BusLogic_Command
  returns a failure code.
*/

static char
  *BusLogic_CommandFailureReason;


/*
  BusLogic_ProcDirectoryEntry is the BusLogic /proc/scsi directory entry.
*/

static struct proc_dir_entry
  BusLogic_ProcDirectoryEntry =
    { PROC_SCSI_BUSLOGIC, 8, "BusLogic", S_IFDIR | S_IRUGO | S_IXUGO, 2 };


/*
  BusLogic_AnnounceDriver announces the Driver Version and Date, Author's
  Name, Copyright Notice, and Contact Address.
*/

static void BusLogic_AnnounceDriver(void)
{
  static boolean DriverAnnouncementPrinted = false;
  if (DriverAnnouncementPrinted) return;
  printk("scsi: ***** BusLogic SCSI Driver Version "
	 BusLogic_DriverVersion " of " BusLogic_DriverDate " *****\n");
  printk("scsi: Copyright 1995 by Leonard N. Zubkoff <lnz@dandelion.com>\n");
  DriverAnnouncementPrinted = true;
}


/*
  BusLogic_DriverInfo returns the Board Name to identify this SCSI Driver
  and Host Adapter.
*/

const char *BusLogic_DriverInfo(SCSI_Host_T *Host)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Host->hostdata;
  return HostAdapter->BoardName;
}


/*
  BusLogic_InitializeAddressProbeList initializes the list of I/O Addresses
  to be probed for potential BusLogic SCSI Host Adapters by interrogating the
  PCI Configuration Space on PCI machines as well as from the list of standard
  BusLogic I/O Addresses.
*/

static void BusLogic_InitializeAddressProbeList(void)
{
  int DestinationIndex = 0, SourceIndex = 0;
  /*
    If BusLogic_Setup has been called, do not override the Kernel Command
    Line specifications.
  */
  if (BusLogic_IO_AddressProbeList[0] != 0) return;
#ifdef CONFIG_PCI
  /*
    Interrogate PCI Configuration Space for any BusLogic SCSI Host Adapters.
  */
  if (pcibios_present())
    {
      unsigned short Index = 0, VendorID;
      unsigned char Bus, DeviceAndFunction;
      unsigned int BaseAddress0;
      while (pcibios_find_class(PCI_CLASS_STORAGE_SCSI<<8, Index++,
				&Bus, &DeviceAndFunction) == 0)
	if (pcibios_read_config_word(Bus, DeviceAndFunction,
				     PCI_VENDOR_ID, &VendorID) == 0 &&
	    VendorID == PCI_VENDOR_ID_BUSLOGIC &&
	    pcibios_read_config_dword(Bus, DeviceAndFunction,
				      PCI_BASE_ADDRESS_0, &BaseAddress0) == 0 &&
	    (BaseAddress0 & PCI_BASE_ADDRESS_SPACE) ==
	      PCI_BASE_ADDRESS_SPACE_IO)
	  {
	    BusLogic_IO_AddressProbeList[DestinationIndex++] =
	      BaseAddress0 & PCI_BASE_ADDRESS_IO_MASK;
	  }
    }
#endif
  /*
    Append the list of standard BusLogic I/O Addresses.
  */
  while (DestinationIndex < BusLogic_IO_MaxProbeAddresses &&
	 BusLogic_IO_StandardAddresses[SourceIndex] > 0)
    BusLogic_IO_AddressProbeList[DestinationIndex++] =
      BusLogic_IO_StandardAddresses[SourceIndex++];
  BusLogic_IO_AddressProbeList[DestinationIndex] = 0;
}


/*
  BusLogic_RegisterHostAdapter adds Host Adapter to the list of registered
  BusLogic Host Adapters.
*/

static void BusLogic_RegisterHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  HostAdapter->Next = NULL;
  if (BusLogic_RegisteredHostAdapters != NULL)
    {
      BusLogic_HostAdapter_T *LastHostAdapter = BusLogic_RegisteredHostAdapters;
      BusLogic_HostAdapter_T *NextHostAdapter;
      while ((NextHostAdapter = LastHostAdapter->Next) != NULL)
	LastHostAdapter = NextHostAdapter;
      LastHostAdapter->Next = HostAdapter;
    }
  else BusLogic_RegisteredHostAdapters = HostAdapter;
}


/*
  BusLogic_UnregisterHostAdapter removes Host Adapter from the list of
  registered BusLogic Host Adapters.
*/

static void BusLogic_UnregisterHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  if (BusLogic_RegisteredHostAdapters != HostAdapter)
    {
      BusLogic_HostAdapter_T *LastHostAdapter = BusLogic_RegisteredHostAdapters;
      while (LastHostAdapter != NULL && LastHostAdapter->Next != HostAdapter)
	LastHostAdapter = LastHostAdapter->Next;
      if (LastHostAdapter != NULL)
	LastHostAdapter->Next = HostAdapter->Next;
    }
  else BusLogic_RegisteredHostAdapters = HostAdapter->Next;
  HostAdapter->Next = NULL;
}


/*
  BusLogic_CreateCCBs allocates the initial Command Control Blocks (CCBs)
  for Host Adapter.
*/

static boolean BusLogic_CreateCCBs(BusLogic_HostAdapter_T *HostAdapter)
{
  int i;
  for (i = 0; i < BusLogic_InitialCCBs; i++)
    {
      BusLogic_CCB_T *CCB = (BusLogic_CCB_T *)
	scsi_init_malloc(sizeof(BusLogic_CCB_T), GFP_ATOMIC | GFP_DMA);
      if (CCB == NULL)
	{
	  printk("scsi%d: UNABLE TO ALLOCATE CCB %d - DETACHING\n",
		 HostAdapter->HostNumber, i);
	  return false;
	}
      memset(CCB, 0, sizeof(BusLogic_CCB_T));
      CCB->HostAdapter = HostAdapter;
      CCB->Status = BusLogic_CCB_Free;
      CCB->Next = HostAdapter->Free_CCBs;
      CCB->NextAll = HostAdapter->All_CCBs;
      HostAdapter->Free_CCBs = CCB;
      HostAdapter->All_CCBs = CCB;
    }
  return true;
}


/*
  BusLogic_DestroyCCBs deallocates the CCBs for Host Adapter.
*/

static void BusLogic_DestroyCCBs(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_CCB_T *NextCCB = HostAdapter->All_CCBs, *CCB;
  HostAdapter->All_CCBs = NULL;
  HostAdapter->Free_CCBs = NULL;
  while ((CCB = NextCCB) != NULL)
    {
      NextCCB = CCB->NextAll;
      scsi_init_free((char *) CCB, sizeof(BusLogic_CCB_T));
    }
}


/*
  BusLogic_AllocateCCB allocates a CCB from the Host Adapter's free list,
  allocating more memory from the Kernel if necessary.
*/

static BusLogic_CCB_T *BusLogic_AllocateCCB(BusLogic_HostAdapter_T *HostAdapter)
{
  static unsigned int SerialNumber = 0;
  BusLogic_CCB_T *CCB;
  BusLogic_LockHostAdapter(HostAdapter);
  CCB = HostAdapter->Free_CCBs;
  if (CCB != NULL)
    {
      CCB->SerialNumber = ++SerialNumber;
      HostAdapter->Free_CCBs = CCB->Next;
      CCB->Next = NULL;
      BusLogic_UnlockHostAdapter(HostAdapter);
      return CCB;
    }
  BusLogic_UnlockHostAdapter(HostAdapter);
  CCB = (BusLogic_CCB_T *) scsi_init_malloc(sizeof(BusLogic_CCB_T),
					    GFP_ATOMIC | GFP_DMA);
  if (CCB == NULL)
    {
      printk("scsi%d: Failed to allocate an additional CCB\n",
	     HostAdapter->HostNumber);
      return NULL;
    }
  printk("scsi%d: Allocated an additional CCB\n", HostAdapter->HostNumber);
  memset(CCB, 0, sizeof(BusLogic_CCB_T));
  CCB->HostAdapter = HostAdapter;
  CCB->Status = BusLogic_CCB_Free;
  BusLogic_LockHostAdapter(HostAdapter);
  CCB->SerialNumber = ++SerialNumber;
  CCB->NextAll = HostAdapter->All_CCBs;
  HostAdapter->All_CCBs = CCB;
  BusLogic_UnlockHostAdapter(HostAdapter);
  return CCB;
}


/*
  BusLogic_DeallocateCCB deallocates a CCB, returning it to the Host Adapter's
  free list.
*/

static void BusLogic_DeallocateCCB(BusLogic_CCB_T *CCB)
{
  BusLogic_HostAdapter_T *HostAdapter = CCB->HostAdapter;
  BusLogic_LockHostAdapter(HostAdapter);
  CCB->Command = NULL;
  CCB->Status = BusLogic_CCB_Free;
  CCB->Next = HostAdapter->Free_CCBs;
  HostAdapter->Free_CCBs = CCB;
  BusLogic_UnlockHostAdapter(HostAdapter);
}


/*
  BusLogic_Command sends the command OperationCode to HostAdapter, optionally
  providing ParameterLength bytes of ParameterData and receiving at most
  ReplyLength bytes of ReplyData; any excess reply data is received but
  discarded.

  On success, this function returns the number of reply bytes read from
  the Host Adapter (including any discarded data); on failure, it returns
  -1 if the command was invalid, or -2 if a timeout occurred.

  This function is only called during board detection and initialization, so
  performance and latency are not critical, and exclusive access to the Host
  Adapter hardware is assumed.  Once the board and driver are initialized, the
  only Host Adapter command that is issued is the single byte Start Mailbox
  Scan command, which does not require waiting for the Host Adapter Ready bit
  to be set in the Status Register.
*/

static int BusLogic_Command(BusLogic_HostAdapter_T *HostAdapter,
			    BusLogic_OperationCode_T OperationCode,
			    void *ParameterData,
			    int ParameterLength,
			    void *ReplyData,
			    int ReplyLength)
{
  unsigned char *ParameterPointer = (unsigned char *) ParameterData;
  unsigned char *ReplyPointer = (unsigned char *) ReplyData;
  unsigned char StatusRegister = 0, InterruptRegister;
  long TimeoutCounter;
  int ReplyBytes = 0;
  /*
    Clear out the Reply Data if provided.
  */
  if (ReplyLength > 0)
    memset(ReplyData, 0, ReplyLength);
  /*
    Wait for the Host Adapter Ready bit to be set and the Command/Parameter
    Register Busy bit to be reset in the Status Register.
  */
  TimeoutCounter = loops_per_sec >> 3;
  while (--TimeoutCounter >= 0)
    {
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if ((StatusRegister & BusLogic_HostAdapterReady) &&
	  !(StatusRegister & BusLogic_CommandParameterRegisterBusy))
	break;
    }
  BusLogic_CommandFailureReason = "Timeout waiting for Host Adapter Ready";
  if (TimeoutCounter < 0) return -2;
  /*
    Write the OperationCode to the Command/Parameter Register.
  */
  HostAdapter->HostAdapterCommandCompleted = false;
  BusLogic_WriteCommandParameterRegister(HostAdapter, OperationCode);
  /*
    Write any additional Parameter Bytes.
  */
  TimeoutCounter = 10000;
  while (ParameterLength > 0 && --TimeoutCounter >= 0)
    {
      /*
	Wait 100 microseconds to give the Host Adapter enough time to determine
	whether the last value written to the Command/Parameter Register was
	valid or not.  If the Command Complete bit is set in the Interrupt
	Register, then the Command Invalid bit in the Status Register will be
	reset if the Operation Code or Parameter was valid and the command
	has completed, or set if the Operation Code or Parameter was invalid.
	If the Data In Register Ready bit is set in the Status Register, then
	the Operation Code was valid, and data is waiting to be read back
	from the Host Adapter.  Otherwise, wait for the Command/Parameter
	Register Busy bit in the Status Register to be reset.
      */
      udelay(100);
      InterruptRegister = BusLogic_ReadInterruptRegister(HostAdapter);
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if (InterruptRegister & BusLogic_CommandComplete) break;
      if (HostAdapter->HostAdapterCommandCompleted) break;
      if (StatusRegister & BusLogic_DataInRegisterReady) break;
      if (StatusRegister & BusLogic_CommandParameterRegisterBusy) continue;
      BusLogic_WriteCommandParameterRegister(HostAdapter, *ParameterPointer++);
      ParameterLength--;
    }
  BusLogic_CommandFailureReason = "Timeout waiting for Parameter Acceptance";
  if (TimeoutCounter < 0) return -2;
  /*
    The Modify I/O Address command does not cause a Command Complete Interrupt.
  */
  if (OperationCode == BusLogic_ModifyIOAddress)
    {
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      BusLogic_CommandFailureReason = "Modify I/O Address Invalid";
      if (StatusRegister & BusLogic_CommandInvalid) return -1;
      BusLogic_CommandFailureReason = NULL;
      return 0;
    }
  /*
    Select an appropriate timeout value for awaiting command completion.
  */
  switch (OperationCode)
    {
    case BusLogic_InquireInstalledDevicesID0to7:
    case BusLogic_InquireInstalledDevicesID8to15:
      /* Approximately 60 seconds. */
      TimeoutCounter = loops_per_sec << 2;
      break;
    default:
      /* Approximately 1 second. */
      TimeoutCounter = loops_per_sec >> 4;
      break;
    }
  /*
    Receive any Reply Bytes, waiting for either the Command Complete bit to
    be set in the Interrupt Register, or for the Interrupt Handler to set the
    Host Adapter Command Completed bit in the Host Adapter structure.
  */
  while (--TimeoutCounter >= 0)
    {
      InterruptRegister = BusLogic_ReadInterruptRegister(HostAdapter);
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if (InterruptRegister & BusLogic_CommandComplete) break;
      if (HostAdapter->HostAdapterCommandCompleted) break;
      if (StatusRegister & BusLogic_DataInRegisterReady)
	if (++ReplyBytes <= ReplyLength)
	  *ReplyPointer++ = BusLogic_ReadDataInRegister(HostAdapter);
	else BusLogic_ReadDataInRegister(HostAdapter);
    }
  BusLogic_CommandFailureReason = "Timeout waiting for Command Complete";
  if (TimeoutCounter < 0) return -2;
  /*
    If testing Command Complete Interrupts, wait a short while in case the
    loop immediately above terminated due to the Command Complete bit being
    set in the Interrupt Register, but the interrupt hasn't actually been
    processed yet.  Otherwise, acknowledging the interrupt here could prevent
    the interrupt test from succeeding.
  */
  if (OperationCode == BusLogic_TestCommandCompleteInterrupt)
    udelay(10000);
  /*
    Clear any pending Command Complete Interrupt.
  */
  BusLogic_WriteControlRegister(HostAdapter, BusLogic_InterruptReset);
  if (BusLogic_GlobalOptions & BusLogic_TraceConfiguration)
    if (OperationCode != BusLogic_TestCommandCompleteInterrupt)
      {
	int i;
	printk("BusLogic_Command(%02X) Status = %02X: %2d ==> %2d:",
	       OperationCode, StatusRegister, ReplyLength, ReplyBytes);
	if (ReplyLength > ReplyBytes) ReplyLength = ReplyBytes;
	for (i = 0; i < ReplyLength; i++)
	  printk(" %02X", ((unsigned char *) ReplyData)[i]);
	printk("\n");
      }
  /*
    Process Command Invalid conditions.
  */
  if (StatusRegister & BusLogic_CommandInvalid)
    {
      /*
	Some early BusLogic Host Adapters may not recover properly from
	a Command Invalid condition, so if this appears to be the case,
	a Soft Reset is issued to the Host Adapter.  Potentially invalid
	commands are never attempted after Mailbox Initialization is
	performed, so there should be no Host Adapter state lost by a
	Soft Reset in response to a Command Invalid condition.
      */
      udelay(1000);
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister != (BusLogic_HostAdapterReady |
			     BusLogic_InitializationRequired))
	{
	  BusLogic_WriteControlRegister(HostAdapter, BusLogic_SoftReset);
	  udelay(1000);
	}
      BusLogic_CommandFailureReason = "Command Invalid";
      return -1;
    }
  /*
    Handle Excess Parameters Supplied conditions.
  */
  BusLogic_CommandFailureReason = "Excess Parameters Supplied";
  if (ParameterLength > 0) return -1;
  /*
    Indicate the command completed successfully.
  */
  BusLogic_CommandFailureReason = NULL;
  return ReplyBytes;
}


/*
  BusLogic_Failure prints a standardized error message, and then returns false.
*/

static boolean BusLogic_Failure(BusLogic_HostAdapter_T *HostAdapter,
				char *ErrorMessage)
{
  BusLogic_AnnounceDriver();
  printk("While configuring BusLogic Host Adapter at I/O Address 0x%X:\n",
	 HostAdapter->IO_Address);
  printk("%s FAILED - DETACHING\n", ErrorMessage);
  if (BusLogic_CommandFailureReason != NULL)
    printk("ADDITIONAL FAILURE INFO - %s\n", BusLogic_CommandFailureReason);
  return false;
}


/*
  BusLogic_ProbeHostAdapter probes for a BusLogic Host Adapter.
*/

static boolean BusLogic_ProbeHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  boolean TraceProbe = (BusLogic_GlobalOptions & BusLogic_TraceProbe);
  unsigned char StatusRegister, GeometryRegister;
  /*
    Read the Status Register to test if there is an I/O port that responds.  A
    nonexistent I/O port will return 0xFF, in which case there is definitely no
    BusLogic Host Adapter at this base I/O Address.
  */
  StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
  if (TraceProbe)
    printk("BusLogic_Probe(0x%X): Status 0x%02X\n",
	   HostAdapter->IO_Address, StatusRegister);
  if (StatusRegister == 0xFF) return false;
  /*
    Read the undocumented BusLogic Geometry Register to test if there is an I/O
    port that responds.  Adaptec Host Adapters do not implement the Geometry
    Register, so this test helps serve to avoid incorrectly recognizing an
    Adaptec 1542A or 1542B as a BusLogic.  Unfortunately, the Adaptec 1542C
    series does respond to the Geometry Register I/O port, but it will be
    rejected later when the Inquire Extended Setup Information command is
    issued in BusLogic_CheckHostAdapter.  The AMI FastDisk Host Adapter is a
    BusLogic clone that implements the same interface as earlier BusLogic
    boards, including the undocumented commands, and is therefore supported by
    this driver.  However, the AMI FastDisk always returns 0x00 upon reading
    the Geometry Register, so the extended translation option should always be
    left disabled on the AMI FastDisk.
  */
  GeometryRegister = BusLogic_ReadGeometryRegister(HostAdapter);
  if (TraceProbe)
    printk("BusLogic_Probe(0x%X): Geometry 0x%02X\n",
	   HostAdapter->IO_Address, GeometryRegister);
  if (GeometryRegister == 0xFF) return false;
  /*
    Indicate the Host Adapter Probe completed successfully.
  */
  return true;
}


/*
  BusLogic_HardResetHostAdapter issues a Hard Reset to the Host Adapter,
  and waits for Host Adapter Diagnostics to complete.
*/

static boolean BusLogic_HardResetHostAdapter(BusLogic_HostAdapter_T
					     *HostAdapter)
{
  boolean TraceHardReset = (BusLogic_GlobalOptions & BusLogic_TraceHardReset);
  long TimeoutCounter = loops_per_sec >> 2;
  unsigned char StatusRegister = 0;
  /*
    Issue a Hard Reset Command to the Host Adapter.  The Host Adapter should
    respond by setting Diagnostic Active in the Status Register.
  */
  BusLogic_WriteControlRegister(HostAdapter, BusLogic_HardReset);
  /*
    Wait until Diagnostic Active is set in the Status Register.
  */
  while (--TimeoutCounter >= 0)
    {
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if ((StatusRegister & BusLogic_DiagnosticActive)) break;
    }
  if (TraceHardReset)
    printk("BusLogic_HardReset(0x%X): Diagnostic Active, Status 0x%02X\n",
	   HostAdapter->IO_Address, StatusRegister);
  if (TimeoutCounter < 0) return false;
  /*
    Wait 100 microseconds to allow completion of any initial diagnostic
    activity which might leave the contents of the Status Register
    unpredictable.
  */
  udelay(100);
  /*
    Wait until Diagnostic Active is reset in the Status Register.
  */
  while (--TimeoutCounter >= 0)
    {
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if (!(StatusRegister & BusLogic_DiagnosticActive)) break;
    }
  if (TraceHardReset)
    printk("BusLogic_HardReset(0x%X): Diagnostic Completed, Status 0x%02X\n",
	   HostAdapter->IO_Address, StatusRegister);
  if (TimeoutCounter < 0) return false;
  /*
    Wait until at least one of the Diagnostic Failure, Host Adapter Ready,
    or Data In Register Ready bits is set in the Status Register.
  */
  while (--TimeoutCounter >= 0)
    {
      StatusRegister = BusLogic_ReadStatusRegister(HostAdapter);
      if (StatusRegister & (BusLogic_DiagnosticFailure |
			    BusLogic_HostAdapterReady |
			    BusLogic_DataInRegisterReady))
	break;
    }
  if (TraceHardReset)
    printk("BusLogic_HardReset(0x%X): Host Adapter Ready, Status 0x%02X\n",
	   HostAdapter->IO_Address, StatusRegister);
  if (TimeoutCounter < 0) return false;
  /*
    If Diagnostic Failure is set or Host Adapter Ready is reset, then an
    error occurred during the Host Adapter diagnostics.  If Data In Register
    Ready is set, then there is an Error Code available.
  */
  if ((StatusRegister & BusLogic_DiagnosticFailure) ||
      !(StatusRegister & BusLogic_HostAdapterReady))
    {
      BusLogic_CommandFailureReason = NULL;
      BusLogic_Failure(HostAdapter, "HARD RESET DIAGNOSTICS");
      printk("HOST ADAPTER STATUS REGISTER = %02X\n", StatusRegister);
      if (StatusRegister & BusLogic_DataInRegisterReady)
	{
	  unsigned char ErrorCode = BusLogic_ReadDataInRegister(HostAdapter);
	  printk("HOST ADAPTER ERROR CODE = %d\n", ErrorCode);
	}
      return false;
    }
  /*
    Indicate the Host Adapter Hard Reset completed successfully.
  */
  return true;
}


/*
  BusLogic_CheckHostAdapter checks to be sure this really is a BusLogic
  Host Adapter.
*/

static boolean BusLogic_CheckHostAdapter(BusLogic_HostAdapter_T *HostAdapter)
{
  BusLogic_ExtendedSetupInformation_T ExtendedSetupInformation;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  unsigned long ProcessorFlags;
  int Result;
  /*
    Issue the Inquire Extended Setup Information command.  Only genuine
    BusLogic Host Adapters and true clones support this command.  Adaptec 1542C
    series Host Adapters that respond to the Geometry Register I/O port will
    fail this command.  Interrupts must be disabled around the call to
    BusLogic_Command since a Command Complete interrupt could occur if the IRQ
    Channel was previously enabled for another BusLogic Host Adapter sharing
    the same IRQ Channel.
  */
  save_flags(ProcessorFlags);
  cli();
  RequestedReplyLength = sizeof(ExtendedSetupInformation);
  Result = BusLogic_Command(HostAdapter,
			    BusLogic_InquireExtendedSetupInformation,
			    &RequestedReplyLength, sizeof(RequestedReplyLength),
			    &ExtendedSetupInformation,
			    sizeof(ExtendedSetupInformation));
  restore_flags(ProcessorFlags);
  if (BusLogic_GlobalOptions & BusLogic_TraceProbe)
    printk("BusLogic_Check(0x%X): Result %d\n",
	   HostAdapter->IO_Address, Result);
  return (Result == sizeof(ExtendedSetupInformation));
}


/*
  BusLogic_ReadHostAdapterConfiguration reads the Configuration Information
  from Host Adapter.
*/

static boolean BusLogic_ReadHostAdapterConfiguration(BusLogic_HostAdapter_T
						     *HostAdapter)
{
  BusLogic_BoardID_T BoardID;
  BusLogic_Configuration_T Configuration;
  BusLogic_SetupInformation_T SetupInformation;
  BusLogic_ExtendedSetupInformation_T ExtendedSetupInformation;
  BusLogic_BoardModelNumber_T BoardModelNumber;
  BusLogic_FirmwareVersion3rdDigit_T FirmwareVersion3rdDigit;
  BusLogic_FirmwareVersionLetter_T FirmwareVersionLetter;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  unsigned char GeometryRegister, *TargetPointer, Character;
  unsigned short AllTargetsMask, DisconnectPermitted;
  unsigned short TaggedQueuingPermitted, TaggedQueuingPermittedDefault;
  boolean CommonErrorRecovery;
  int TargetID, i;
  /*
    Issue the Inquire Board ID command.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0,
		       &BoardID, sizeof(BoardID)) != sizeof(BoardID))
    return BusLogic_Failure(HostAdapter, "INQUIRE BOARD ID");
  /*
    Issue the Inquire Configuration command.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireConfiguration, NULL, 0,
		       &Configuration, sizeof(Configuration))
      != sizeof(Configuration))
    return BusLogic_Failure(HostAdapter, "INQUIRE CONFIGURATION");
  /*
    Issue the Inquire Setup Information command.
  */
  RequestedReplyLength = sizeof(SetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &SetupInformation, sizeof(SetupInformation))
      != sizeof(SetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
  /*
    Issue the Inquire Extended Setup Information command.
  */
  RequestedReplyLength = sizeof(ExtendedSetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &ExtendedSetupInformation,
		       sizeof(ExtendedSetupInformation))
      != sizeof(ExtendedSetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE EXTENDED SETUP INFORMATION");
  /*
    Issue the Inquire Board Model Number command.
  */
  if (!(BoardID.FirmwareVersion1stDigit == '2' &&
	ExtendedSetupInformation.BusType == 'A'))
    {
      RequestedReplyLength = sizeof(BoardModelNumber);
      if (BusLogic_Command(HostAdapter, BusLogic_InquireBoardModelNumber,
			   &RequestedReplyLength, sizeof(RequestedReplyLength),
			   &BoardModelNumber, sizeof(BoardModelNumber))
	  != sizeof(BoardModelNumber))
	return BusLogic_Failure(HostAdapter, "INQUIRE BOARD MODEL NUMBER");
    }
  else strcpy(BoardModelNumber, "542B");
  /*
    Issue the Inquire Firmware Version 3rd Digit command.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersion3rdDigit,
		       NULL, 0, &FirmwareVersion3rdDigit,
		       sizeof(FirmwareVersion3rdDigit))
      != sizeof(FirmwareVersion3rdDigit))
    return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE 3RD DIGIT");
  /*
    Issue the Inquire Firmware Version Letter command.
  */
  FirmwareVersionLetter = '\0';
  if (BoardID.FirmwareVersion1stDigit > '3' ||
      (BoardID.FirmwareVersion1stDigit == '3' &&
       BoardID.FirmwareVersion2ndDigit >= '3'))
    if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersionLetter,
			 NULL, 0, &FirmwareVersionLetter,
			 sizeof(FirmwareVersionLetter))
	!= sizeof(FirmwareVersionLetter))
      return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE VERSION LETTER");
  /*
    BusLogic Host Adapters can be identified by their model number and
    the major version number of their firmware as follows:

    4.xx	BusLogic "C" Series Host Adapters:
		  BT-946C/956C/956CD/747C/757C/757CD/445C/545C/540CF
    3.xx	BusLogic "S" Series Host Adapters:
		  BT-747S/747D/757S/757D/445S/545S/542D
		  BT-542B/742A (revision H)
    2.xx	BusLogic "A" Series Host Adapters:
		  BT-542B/742A (revision G and below)
    0.xx	AMI FastDisk VLB/EISA BusLogic Clone Host Adapter
  */
  /*
    Save the Model Name and Board Name in the Host Adapter structure.
  */
  TargetPointer = HostAdapter->ModelName;
  *TargetPointer++ = 'B';
  *TargetPointer++ = 'T';
  *TargetPointer++ = '-';
  for (i = 0; i < sizeof(BoardModelNumber); i++)
    {
      Character = BoardModelNumber[i];
      if (Character == ' ' || Character == '\0') break;
      *TargetPointer++ = Character;
    }
  *TargetPointer++ = '\0';
  strcpy(HostAdapter->BoardName, "BusLogic ");
  strcat(HostAdapter->BoardName, HostAdapter->ModelName);
  strcpy(HostAdapter->InterruptLabel, HostAdapter->BoardName);
  /*
    Save the Firmware Version in the Host Adapter structure.
  */
  TargetPointer = HostAdapter->FirmwareVersion;
  *TargetPointer++ = BoardID.FirmwareVersion1stDigit;
  *TargetPointer++ = '.';
  *TargetPointer++ = BoardID.FirmwareVersion2ndDigit;
  if (FirmwareVersion3rdDigit != ' ' && FirmwareVersion3rdDigit != '\0')
    *TargetPointer++ = FirmwareVersion3rdDigit;
  if (FirmwareVersionLetter != ' ' && FirmwareVersionLetter != '\0')
    *TargetPointer++ = FirmwareVersionLetter;
  *TargetPointer++ = '\0';
  /*
    Determine the IRQ Channel and save it in the Host Adapter structure.
  */
  if (Configuration.IRQ_Channel9)
    HostAdapter->IRQ_Channel = 9;
  else if (Configuration.IRQ_Channel10)
    HostAdapter->IRQ_Channel = 10;
  else if (Configuration.IRQ_Channel11)
    HostAdapter->IRQ_Channel = 11;
  else if (Configuration.IRQ_Channel12)
    HostAdapter->IRQ_Channel = 12;
  else if (Configuration.IRQ_Channel14)
    HostAdapter->IRQ_Channel = 14;
  else if (Configuration.IRQ_Channel15)
    HostAdapter->IRQ_Channel = 15;
  /*
    Determine the DMA Channel and save it in the Host Adapter structure.
  */
  if (Configuration.DMA_Channel5)
    HostAdapter->DMA_Channel = 5;
  else if (Configuration.DMA_Channel6)
    HostAdapter->DMA_Channel = 6;
  else if (Configuration.DMA_Channel7)
    HostAdapter->DMA_Channel = 7;
  /*
    Save the Host Adapter SCSI ID in the Host Adapter structure.
  */
  HostAdapter->SCSI_ID = Configuration.HostAdapterID;
  /*
    Save the Synchronous Initiation flag and SCSI Parity Checking flag
    in the Host Adapter structure.
  */
  HostAdapter->SynchronousInitiation =
    SetupInformation.SynchronousInitiationEnabled;
  HostAdapter->ParityChecking = SetupInformation.ParityCheckEnabled;
  /*
    Determine the Bus Type and save it in the Host Adapter structure,
    overriding the DMA Channel if it is inappropriate for the bus type.
  */
  if (ExtendedSetupInformation.BusType == 'A')
    HostAdapter->BusType = BusLogic_ISA_Bus;
  else
    switch (HostAdapter->ModelName[3])
      {
      case '4':
	HostAdapter->BusType = BusLogic_VESA_Bus;
	HostAdapter->DMA_Channel = 0;
	break;
      case '5':
	HostAdapter->BusType = BusLogic_ISA_Bus;
	break;
      case '6':
	HostAdapter->BusType = BusLogic_MCA_Bus;
	HostAdapter->DMA_Channel = 0;
	break;
      case '7':
	HostAdapter->BusType = BusLogic_EISA_Bus;
	HostAdapter->DMA_Channel = 0;
	break;
      case '9':
	HostAdapter->BusType = BusLogic_PCI_Bus;
	HostAdapter->DMA_Channel = 0;
	break;
      }
  /*
    Determine whether Extended Translation is enabled and save it in
    the Host Adapter structure.
  */
  GeometryRegister = BusLogic_ReadGeometryRegister(HostAdapter);
  if (GeometryRegister & BusLogic_ExtendedTranslationEnabled)
    HostAdapter->ExtendedTranslation = true;
  /*
    Save the Disconnect/Reconnect Permitted flag bits in the Host Adapter
    structure.  The Disconnect Permitted information is only valid on "C"
    Series boards, but Disconnect/Reconnect is always permitted on "S" and
    "A" Series boards.
  */
  if (HostAdapter->FirmwareVersion[0] >= '4')
    HostAdapter->DisconnectPermitted =
      (SetupInformation.DisconnectPermittedID8to15 << 8)
      | SetupInformation.DisconnectPermittedID0to7;
  else HostAdapter->DisconnectPermitted = 0xFF;
  /*
    Save the Scatter Gather Limits, Level Sensitive Interrupts flag,
    Wide SCSI flag, and Differential SCSI flag in the Host Adapter structure.
  */
  HostAdapter->HostAdapterScatterGatherLimit =
    ExtendedSetupInformation.ScatterGatherLimit;
  HostAdapter->DriverScatterGatherLimit =
    HostAdapter->HostAdapterScatterGatherLimit;
  if (HostAdapter->HostAdapterScatterGatherLimit > BusLogic_ScatterGatherLimit)
    HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
  if (ExtendedSetupInformation.Misc.LevelSensitiveInterrupts)
    HostAdapter->LevelSensitiveInterrupts = true;
  if (ExtendedSetupInformation.HostWideSCSI)
    {
      HostAdapter->HostWideSCSI = true;
      HostAdapter->MaxTargetIDs = 16;
      HostAdapter->MaxLogicalUnits = 64;
    }
  else
    {
      HostAdapter->HostWideSCSI = false;
      HostAdapter->MaxTargetIDs = 8;
      HostAdapter->MaxLogicalUnits = 8;
    }
  HostAdapter->HostDifferentialSCSI =
    ExtendedSetupInformation.HostDifferentialSCSI;
  /*
    Determine the Host Adapter BIOS Address if the BIOS is enabled and
    save it in the Host Adapter structure.  The BIOS is disabled if the
    BIOS_Address is 0.
  */
  HostAdapter->BIOS_Address = ExtendedSetupInformation.BIOS_Address << 12;
  /*
    BusLogic BT-445S Host Adapters prior to board revision D have a hardware
    bug whereby when the BIOS is enabled, transfers to/from the same address
    range the BIOS occupies modulo 16MB are handled incorrectly.  Only properly
    functioning BT-445S boards have firmware version 3.37, so we require that
    ISA bounce buffers be used for the buggy BT-445S models as well as for all
    ISA models.
  */
  if (HostAdapter->BusType == BusLogic_ISA_Bus ||
      (HostAdapter->BIOS_Address > 0 &&
       strcmp(HostAdapter->ModelName, "BT-445S") == 0 &&
       strcmp(HostAdapter->FirmwareVersion, "3.37") < 0))
    HostAdapter->BounceBuffersRequired = true;
  /*
    Select an appropriate value for Concurrency (Commands per Logical Unit)
    either from a Command Line Entry, or based on whether this Host Adapter
    requires that ISA bounce buffers be used.
  */
  if (HostAdapter->CommandLineEntry != NULL &&
      HostAdapter->CommandLineEntry->Concurrency > 0)
    HostAdapter->Concurrency = HostAdapter->CommandLineEntry->Concurrency;
  else if (HostAdapter->BounceBuffersRequired)
    HostAdapter->Concurrency = BusLogic_Concurrency_BB;
  else HostAdapter->Concurrency = BusLogic_Concurrency;
  /*
    Select an appropriate value for Bus Settle Time either from a Command
    Line Entry, or from BusLogic_DefaultBusSettleTime.
  */
  if (HostAdapter->CommandLineEntry != NULL &&
      HostAdapter->CommandLineEntry->BusSettleTime > 0)
    HostAdapter->BusSettleTime = HostAdapter->CommandLineEntry->BusSettleTime;
  else HostAdapter->BusSettleTime = BusLogic_DefaultBusSettleTime;
  /*
    Select an appropriate value for Local Options from a Command Line Entry.
  */
  if (HostAdapter->CommandLineEntry != NULL)
    HostAdapter->LocalOptions = HostAdapter->CommandLineEntry->LocalOptions;
  /*
    Select appropriate values for the Error Recovery Option array either from
    a Command Line Entry, or using BusLogic_ErrorRecoveryDefault.
  */
  if (HostAdapter->CommandLineEntry != NULL)
    memcpy(HostAdapter->ErrorRecoveryOption,
	   HostAdapter->CommandLineEntry->ErrorRecoveryOption,
	   sizeof(HostAdapter->ErrorRecoveryOption));
  else memset(HostAdapter->ErrorRecoveryOption,
	      BusLogic_ErrorRecoveryDefault,
	      sizeof(HostAdapter->ErrorRecoveryOption));
  /*
    Tagged Queuing support is available and operates properly only on "C"
    Series boards with firmware version 4.22 and above and on "S" Series
    boards with firmware version 3.35 and above.  Tagged Queuing is disabled
    by default when the Concurrency value is 1 since queuing multiple commands
    is not possible.
  */
  TaggedQueuingPermittedDefault = 0;
  if (HostAdapter->Concurrency > 1)
    switch (HostAdapter->FirmwareVersion[0])
      {
      case '5':
	TaggedQueuingPermittedDefault = 0xFFFF;
	break;
      case '4':
	if (strcmp(HostAdapter->FirmwareVersion, "4.22") >= 0)
	  TaggedQueuingPermittedDefault = 0xFFFF;
	break;
      case '3':
	if (strcmp(HostAdapter->FirmwareVersion, "3.35") >= 0)
	  TaggedQueuingPermittedDefault = 0xFFFF;
	break;
      }
  /*
    Tagged Queuing is only useful if Disconnect/Reconnect is permitted.
    Therefore, mask the Tagged Queuing Permitted Default bits with the
    Disconnect/Reconnect Permitted bits.
  */
  TaggedQueuingPermittedDefault &= HostAdapter->DisconnectPermitted;
  /*
    Combine the default Tagged Queuing Permitted Default bits with any
    Command Line Entry Tagged Queuing specification.
  */
  if (HostAdapter->CommandLineEntry != NULL)
    HostAdapter->TaggedQueuingPermitted =
      (HostAdapter->CommandLineEntry->TaggedQueuingPermitted &
       HostAdapter->CommandLineEntry->TaggedQueuingPermittedMask) |
      (TaggedQueuingPermittedDefault &
       ~HostAdapter->CommandLineEntry->TaggedQueuingPermittedMask);
  else HostAdapter->TaggedQueuingPermitted = TaggedQueuingPermittedDefault;
  /*
    Announce the Host Adapter Configuration.
  */
  printk("scsi%d: Configuring BusLogic Model %s %s%s%s SCSI Host Adapter\n",
	 HostAdapter->HostNumber, HostAdapter->ModelName,
	 BusLogic_BusNames[HostAdapter->BusType],
	 (HostAdapter->HostWideSCSI ? " Wide" : ""),
	 (HostAdapter->HostDifferentialSCSI ? " Differential" : ""));
  printk("scsi%d:   Firmware Version: %s, I/O Address: 0x%X, "
	 "IRQ Channel: %d/%s\n",
	 HostAdapter->HostNumber, HostAdapter->FirmwareVersion,
	 HostAdapter->IO_Address, HostAdapter->IRQ_Channel,
	 (HostAdapter->LevelSensitiveInterrupts ? "Level" : "Edge"));
  printk("scsi%d:   DMA Channel: ", HostAdapter->HostNumber);
  if (HostAdapter->DMA_Channel > 0)
    printk("%d, ", HostAdapter->DMA_Channel);
  else printk("None, ");
  if (HostAdapter->BIOS_Address > 0)
    printk("BIOS Address: 0x%lX, ", HostAdapter->BIOS_Address);
  else printk("BIOS Address: None, ");
  printk("Host Adapter SCSI ID: %d\n", HostAdapter->SCSI_ID);
  printk("scsi%d:   Scatter/Gather Limit: %d segments, "
	 "Synchronous Initiation: %s\n", HostAdapter->HostNumber,
	 HostAdapter->HostAdapterScatterGatherLimit,
	 (HostAdapter->SynchronousInitiation ? "Enabled" : "Disabled"));
  printk("scsi%d:   SCSI Parity Checking: %s, "
	 "Extended Disk Translation: %s\n", HostAdapter->HostNumber,
	 (HostAdapter->ParityChecking ? "Enabled" : "Disabled"),
	 (HostAdapter->ExtendedTranslation ? "Enabled" : "Disabled"));
  AllTargetsMask = (1 << HostAdapter->MaxTargetIDs) - 1;
  DisconnectPermitted = HostAdapter->DisconnectPermitted & AllTargetsMask;
  printk("scsi%d:   Disconnect/Reconnect: ", HostAdapter->HostNumber);
  if (DisconnectPermitted == 0)
    printk("Disabled");
  else if (DisconnectPermitted == AllTargetsMask)
    printk("Enabled");
  else
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
      printk("%c", (DisconnectPermitted & (1 << TargetID)) ? 'Y' : 'N');
  printk(", Tagged Queuing: ");
  TaggedQueuingPermitted =
    HostAdapter->TaggedQueuingPermitted & AllTargetsMask;
  if (TaggedQueuingPermitted == 0)
    printk("Disabled");
  else if (TaggedQueuingPermitted == AllTargetsMask)
    printk("Enabled");
  else
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
      printk("%c", (TaggedQueuingPermitted & (1 << TargetID)) ? 'Y' : 'N');
  printk("\n");
  CommonErrorRecovery = true;
  for (TargetID = 1; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
    if (HostAdapter->ErrorRecoveryOption[TargetID] !=
	HostAdapter->ErrorRecoveryOption[0])
      {
	CommonErrorRecovery = false;
	break;
      }
  printk("scsi%d:   Error Recovery: ", HostAdapter->HostNumber);
  if (CommonErrorRecovery)
    printk("%s", BusLogic_ErrorRecoveryOptions[
		   HostAdapter->ErrorRecoveryOption[0]]);
  else
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
      printk("%s", BusLogic_ErrorRecoveryOptions2[
		     HostAdapter->ErrorRecoveryOption[TargetID]]);
  printk(", Mailboxes: %d, Initial CCBs: %d\n",
	 BusLogic_MailboxCount, BusLogic_InitialCCBs);
  printk("scsi%d:   Driver Scatter/Gather Limit: %d segments, "
	 "Concurrency: %d\n", HostAdapter->HostNumber,
	 HostAdapter->DriverScatterGatherLimit, HostAdapter->Concurrency);
  /*
    Indicate reading the Host Adapter Configuration completed successfully.
  */
  return true;
}


/*
  BusLogic_AcquireResources acquires the system resources necessary to use Host
  Adapter, and initializes the fields in the SCSI Host structure.  The base,
  io_port, n_io_ports, irq, and dma_channel fields in the SCSI Host structure
  are intentionally left uninitialized, as this driver handles acquisition and
  release of these resources explicitly, as well as ensuring exclusive access
  to the Host Adapter hardware and data structures through explicit locking.
*/

static boolean BusLogic_AcquireResources(BusLogic_HostAdapter_T *HostAdapter,
					 SCSI_Host_T *Host)
{
  /*
    Acquire exclusive or shared access to the IRQ Channel.  A usage count is
    maintained so that PCI, EISA, or MCA shared Interrupts can be supported.
  */
  if (BusLogic_IRQ_UsageCount[HostAdapter->IRQ_Channel - 9]++ == 0)
    {
      if (request_irq(HostAdapter->IRQ_Channel, BusLogic_InterruptHandler,
		      SA_INTERRUPT, HostAdapter->InterruptLabel) < 0)
	{
	  BusLogic_IRQ_UsageCount[HostAdapter->IRQ_Channel - 9]--;
	  printk("scsi%d: UNABLE TO ACQUIRE IRQ CHANNEL %d - DETACHING\n",
		 HostAdapter->HostNumber, HostAdapter->IRQ_Channel);
	  return false;
	}
    }
  else
    {
      BusLogic_HostAdapter_T *FirstHostAdapter =
	BusLogic_RegisteredHostAdapters;
      while (FirstHostAdapter != NULL)
	{
	  if (FirstHostAdapter->IRQ_Channel == HostAdapter->IRQ_Channel)
	    {
	      if (strlen(FirstHostAdapter->InterruptLabel) + 11
		  < sizeof(FirstHostAdapter->InterruptLabel))
		{
		  strcat(FirstHostAdapter->InterruptLabel, " + ");
		  strcat(FirstHostAdapter->InterruptLabel,
			 HostAdapter->ModelName);
		}
	      break;
	    }
	  FirstHostAdapter = FirstHostAdapter->Next;
	}
    }
  HostAdapter->IRQ_ChannelAcquired = true;
  /*
    Acquire exclusive access to the DMA Channel.
  */
  if (HostAdapter->DMA_Channel > 0)
    {
      if (request_dma(HostAdapter->DMA_Channel, HostAdapter->BoardName) < 0)
	{
	  printk("scsi%d: UNABLE TO ACQUIRE DMA CHANNEL %d - DETACHING\n",
		 HostAdapter->HostNumber, HostAdapter->DMA_Channel);
	  return false;
	}
      set_dma_mode(HostAdapter->DMA_Channel, DMA_MODE_CASCADE);
      enable_dma(HostAdapter->DMA_Channel);
      HostAdapter->DMA_ChannelAcquired = true;
    }
  /*
    Initialize necessary fields in the SCSI Host structure.
  */
  Host->max_id = HostAdapter->MaxTargetIDs;
  Host->max_lun = HostAdapter->MaxLogicalUnits;
  Host->max_channel = 0;
  Host->this_id = HostAdapter->SCSI_ID;
  Host->can_queue = BusLogic_MailboxCount;
  Host->cmd_per_lun = HostAdapter->Concurrency;
  Host->sg_tablesize = HostAdapter->DriverScatterGatherLimit;
  Host->unchecked_isa_dma = HostAdapter->BounceBuffersRequired;
  /*
    Indicate the System Resource Acquisition completed successfully,
  */
  return true;
}


/*
  BusLogic_ReleaseResources releases any system resources previously acquired
  by BusLogic_AcquireResources.
*/

static void BusLogic_ReleaseResources(BusLogic_HostAdapter_T *HostAdapter)
{
  /*
    Release exclusive or shared access to the IRQ Channel.
  */
  if (HostAdapter->IRQ_ChannelAcquired)
    if (--BusLogic_IRQ_UsageCount[HostAdapter->IRQ_Channel - 9] == 0)
      free_irq(HostAdapter->IRQ_Channel);
  /*
    Release exclusive access to the DMA Channel.
  */
  if (HostAdapter->DMA_ChannelAcquired)
    free_dma(HostAdapter->DMA_Channel);
}


/*
  BusLogic_TestInterrupts tests for proper functioning of the Host Adapter
  Interrupt Register and that interrupts generated by the Host Adapter are
  getting through to the Interrupt Handler.  A large proportion of initial
  problems with installing PCI Host Adapters are due to configuration problems
  where either the Host Adapter or Motherboard is configured incorrectly, and
  interrupts do not get through as a result.
*/

static boolean BusLogic_TestInterrupts(BusLogic_HostAdapter_T *HostAdapter)
{
  unsigned int InitialInterruptCount, FinalInterruptCount;
  int TestCount = 5, i;
  InitialInterruptCount = kstat.interrupts[HostAdapter->IRQ_Channel];
  /*
    Issue the Test Command Complete Interrupt commands.
  */
  for (i = 0; i < TestCount; i++)
    BusLogic_Command(HostAdapter, BusLogic_TestCommandCompleteInterrupt,
		     NULL, 0, NULL, 0);
  /*
    Verify that BusLogic_InterruptHandler was called at least TestCount times.
    Shared IRQ Channels could cause more than TestCount interrupts to occur,
    but there should never be fewer than TestCount.
  */
  FinalInterruptCount = kstat.interrupts[HostAdapter->IRQ_Channel];
  if (FinalInterruptCount < InitialInterruptCount + TestCount)
    {
      BusLogic_Failure(HostAdapter, "HOST ADAPTER INTERRUPT TEST");
      printk("\n\
Interrupts are not getting through from the Host Adapter to the BusLogic\n\
Driver Interrupt Handler. The most likely cause is that either the Host\n\
Adapter or Motherboard is configured incorrectly.  Please check the Host\n\
Adapter configuration with AutoSCSI or by examining any dip switch and\n\
jumper settings on the Host Adapter, and verify that no other device is\n\
attempting to use the same IRQ Channel.  For PCI Host Adapters, it may also\n\
be necessary to investigate and manually set the PCI interrupt assignments\n\
and edge/level interrupt type selection in the BIOS Setup Program or with\n\
Motherboard jumpers.\n\n");
      return false;
    }
  /*
    Indicate the Host Adapter Interrupt Test completed successfully.
  */
  return true;
}


/*
  BusLogic_InitializeHostAdapter initializes Host Adapter.  This is the only
  function called during SCSI Host Adapter detection which modifies the state
  of the Host Adapter from its initial power on or hard reset state.
*/

static boolean BusLogic_InitializeHostAdapter(BusLogic_HostAdapter_T
					      *HostAdapter)
{
  BusLogic_ExtendedMailboxRequest_T ExtendedMailboxRequest;
  BusLogic_RoundRobinModeRequest_T RoundRobinModeRequest;
  BusLogic_WideModeCCBRequest_T WideModeCCBRequest;
  BusLogic_ModifyIOAddressRequest_T ModifyIOAddressRequest;
  /*
    Initialize the Command Successful Flag, Read/Write Operation Count,
    and Queued Operation Count for each Target.
  */
  memset(HostAdapter->CommandSuccessfulFlag, false,
	 sizeof(HostAdapter->CommandSuccessfulFlag));
  memset(HostAdapter->ReadWriteOperationCount, 0,
	 sizeof(HostAdapter->ReadWriteOperationCount));
  memset(HostAdapter->QueuedOperationCount, 0,
	 sizeof(HostAdapter->QueuedOperationCount));
  /*
    Initialize the Outgoing and Incoming Mailbox structures.
  */
  memset(HostAdapter->OutgoingMailboxes, 0,
	 sizeof(HostAdapter->OutgoingMailboxes));
  memset(HostAdapter->IncomingMailboxes, 0,
	 sizeof(HostAdapter->IncomingMailboxes));
  /*
    Initialize the pointers to the First, Last, and Next Mailboxes.
  */
  HostAdapter->FirstOutgoingMailbox = &HostAdapter->OutgoingMailboxes[0];
  HostAdapter->LastOutgoingMailbox =
    &HostAdapter->OutgoingMailboxes[BusLogic_MailboxCount-1];
  HostAdapter->NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
  HostAdapter->FirstIncomingMailbox = &HostAdapter->IncomingMailboxes[0];
  HostAdapter->LastIncomingMailbox =
    &HostAdapter->IncomingMailboxes[BusLogic_MailboxCount-1];
  HostAdapter->NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;
  /*
    Initialize the Host Adapter's Pointer to the Outgoing/Incoming Mailboxes.
  */
  ExtendedMailboxRequest.MailboxCount = BusLogic_MailboxCount;
  ExtendedMailboxRequest.BaseMailboxAddress = HostAdapter->OutgoingMailboxes;
  if (BusLogic_Command(HostAdapter, BusLogic_InitializeExtendedMailbox,
		       &ExtendedMailboxRequest,
		       sizeof(ExtendedMailboxRequest), NULL, 0) < 0)
    return BusLogic_Failure(HostAdapter, "MAILBOX INITIALIZATION");
  /*
    Enable Strict Round Robin Mode if supported by the Host Adapter.  In Strict
    Round Robin Mode, the Host Adapter only looks at the next Outgoing Mailbox
    for each new command, rather than scanning through all the Outgoing
    Mailboxes to find any that have new commands in them.  BusLogic indicates
    that Strict Round Robin Mode is significantly more efficient.
  */
  if (strcmp(HostAdapter->FirmwareVersion, "3.31") >= 0)
    {
      RoundRobinModeRequest = BusLogic_StrictRoundRobinMode;
      if (BusLogic_Command(HostAdapter, BusLogic_EnableStrictRoundRobinMode,
			   &RoundRobinModeRequest,
			   sizeof(RoundRobinModeRequest), NULL, 0) < 0)
	return BusLogic_Failure(HostAdapter, "ENABLE STRICT ROUND ROBIN MODE");
    }
  /*
    For Wide SCSI Host Adapters, issue the Enable Wide Mode CCB command to
    allow more than 8 Logical Units per Target to be supported.
  */
  if (HostAdapter->HostWideSCSI)
    {
      WideModeCCBRequest = BusLogic_WideModeCCB;
      if (BusLogic_Command(HostAdapter, BusLogic_EnableWideModeCCB,
			   &WideModeCCBRequest,
			   sizeof(WideModeCCBRequest), NULL, 0) < 0)
	return BusLogic_Failure(HostAdapter, "ENABLE WIDE MODE CCB");
    }
  /*
    For PCI Host Adapters being accessed through the PCI compliant I/O
    Address, disable the ISA compatible I/O Address to avoid detecting the
    same Host Adapter at both I/O Addresses.
  */
  if (HostAdapter->BusType == BusLogic_PCI_Bus)
    {
      int Index;
      for (Index = 0; BusLogic_IO_StandardAddresses[Index] > 0; Index++)
	if (HostAdapter->IO_Address == BusLogic_IO_StandardAddresses[Index])
	  break;
      if (BusLogic_IO_StandardAddresses[Index] == 0)
	{
	  ModifyIOAddressRequest = BusLogic_ModifyIO_Disable;
	  if (BusLogic_Command(HostAdapter, BusLogic_ModifyIOAddress,
			       &ModifyIOAddressRequest,
			       sizeof(ModifyIOAddressRequest), NULL, 0) < 0)
	    return BusLogic_Failure(HostAdapter, "MODIFY I/O ADDRESS");
	}
    }
  /*
    Announce Successful Initialization.
  */
  printk("scsi%d: *** %s Initialized Successfully ***\n",
	 HostAdapter->HostNumber, HostAdapter->BoardName);
  /*
    Indicate the Host Adapter Initialization completed successfully.
  */
  return true;
}


/*
  BusLogic_InquireTargetDevices inquires about the Target Devices accessible
  through Host Adapter and reports on the results.
*/

static boolean BusLogic_InquireTargetDevices(BusLogic_HostAdapter_T
					     *HostAdapter)
{
  BusLogic_InstalledDevices8_T InstalledDevicesID0to7;
  BusLogic_InstalledDevices8_T InstalledDevicesID8to15;
  BusLogic_SetupInformation_T SetupInformation;
  BusLogic_SynchronousPeriod_T SynchronousPeriod;
  BusLogic_RequestedReplyLength_T RequestedReplyLength;
  int TargetDevicesFound = 0, TargetID;
  /*
    Wait a few seconds between the Host Adapter Hard Reset which initiates
    a SCSI Bus Reset and issuing any SCSI commands.  Some SCSI devices get
    confused if they receive SCSI commands too soon after a SCSI Bus Reset.
  */
  BusLogic_Delay(HostAdapter->BusSettleTime);
  /*
    Inhibit the Target Devices Inquiry if requested.
  */
  if (HostAdapter->LocalOptions & BusLogic_InhibitTargetInquiry)
    {
      printk("scsi%d:   Target Device Inquiry Inhibited\n",
	     HostAdapter->HostNumber);
      return true;
    }
  /*
    Issue the Inquire Installed Devices ID 0 to 7 command, and for Wide SCSI
    Host Adapters the Inquire Installed Devices ID 8 to 15 command.  This is
    necessary to force Synchronous Transfer Negotiation so that the Inquire
    Setup Information and Inquire Synchronous Period commands will return
    valid data.
  */
  if (BusLogic_Command(HostAdapter, BusLogic_InquireInstalledDevicesID0to7,
		       NULL, 0, &InstalledDevicesID0to7,
		       sizeof(InstalledDevicesID0to7))
      != sizeof(InstalledDevicesID0to7))
    return BusLogic_Failure(HostAdapter, "INQUIRE INSTALLED DEVICES ID 0 TO 7");
  if (HostAdapter->HostWideSCSI)
    if (BusLogic_Command(HostAdapter, BusLogic_InquireInstalledDevicesID8to15,
			 NULL, 0, &InstalledDevicesID8to15,
			 sizeof(InstalledDevicesID8to15))
	!= sizeof(InstalledDevicesID8to15))
      return BusLogic_Failure(HostAdapter,
			      "INQUIRE INSTALLED DEVICES ID 8 TO 15");
  /*
    Issue the Inquire Setup Information command.
  */
  RequestedReplyLength = sizeof(SetupInformation);
  if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation,
		       &RequestedReplyLength, sizeof(RequestedReplyLength),
		       &SetupInformation, sizeof(SetupInformation))
      != sizeof(SetupInformation))
    return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
  /*
    Issue the Inquire Synchronous Period command.
  */
  if (HostAdapter->FirmwareVersion[0] >= '3')
    {
      RequestedReplyLength = sizeof(SynchronousPeriod);
      if (BusLogic_Command(HostAdapter, BusLogic_InquireSynchronousPeriod,
			   &RequestedReplyLength, sizeof(RequestedReplyLength),
			   &SynchronousPeriod, sizeof(SynchronousPeriod))
	  != sizeof(SynchronousPeriod))
	return BusLogic_Failure(HostAdapter, "INQUIRE SYNCHRONOUS PERIOD");
    }
  else
    for (TargetID = 0; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
      if (SetupInformation.SynchronousValuesID0to7[TargetID].Offset > 0)
	SynchronousPeriod[TargetID] =
	  20 + 5 * SetupInformation.SynchronousValuesID0to7[TargetID]
				   .TransferPeriod;
      else SynchronousPeriod[TargetID] = 0;
  /*
    Save the Installed Devices, Synchronous Values, and Synchronous Period
    information in the Host Adapter structure.
  */
  memcpy(HostAdapter->InstalledDevices, InstalledDevicesID0to7,
	 sizeof(BusLogic_InstalledDevices8_T));
  memcpy(HostAdapter->SynchronousValues,
	 SetupInformation.SynchronousValuesID0to7,
	 sizeof(BusLogic_SynchronousValues8_T));
  if (HostAdapter->HostWideSCSI)
    {
      memcpy(&HostAdapter->InstalledDevices[8], InstalledDevicesID8to15,
	     sizeof(BusLogic_InstalledDevices8_T));
      memcpy(&HostAdapter->SynchronousValues[8],
	     SetupInformation.SynchronousValuesID8to15,
	     sizeof(BusLogic_SynchronousValues8_T));
    }
  memcpy(HostAdapter->SynchronousPeriod, SynchronousPeriod,
	 sizeof(BusLogic_SynchronousPeriod_T));
  for (TargetID = 0; TargetID < HostAdapter->MaxTargetIDs; TargetID++)
    if (HostAdapter->InstalledDevices[TargetID] != 0)
      {
	int SynchronousPeriod = HostAdapter->SynchronousPeriod[TargetID];
	if (SynchronousPeriod > 10)
	  {
	    int SynchronousTransferRate = 100000000 / SynchronousPeriod;
	    int RoundedSynchronousTransferRate =
	      (SynchronousTransferRate + 5000) / 10000;
	    printk("scsi%d:   Target %d: Synchronous at "
		   "%d.%02d mega-transfers/second, offset %d\n",
		   HostAdapter->HostNumber, TargetID,
		   RoundedSynchronousTransferRate / 100,
		   RoundedSynchronousTransferRate % 100,
		   HostAdapter->SynchronousValues[TargetID].Offset);
	  }
	else if (SynchronousPeriod > 0)
	  {
	    int SynchronousTransferRate = 100000000 / SynchronousPeriod;
	    int RoundedSynchronousTransferRate =
	      (SynchronousTransferRate + 50000) / 100000;
	    printk("scsi%d:   Target %d: Synchronous at "
		   "%d.%01d mega-transfers/second, offset %d\n",
		   HostAdapter->HostNumber, TargetID,
		   RoundedSynchronousTransferRate / 10,
		   RoundedSynchronousTransferRate % 10,
		   HostAdapter->SynchronousValues[TargetID].Offset);
	  }
	else printk("scsi%d:   Target %d: Asynchronous\n",
		    HostAdapter->HostNumber, TargetID);
	TargetDevicesFound++;
      }
  if (TargetDevicesFound == 0)
    printk("scsi%d:   No Target Devices Found\n", HostAdapter->HostNumber);
  /*
    Indicate the Target Device Inquiry completed successfully.
  */
  return true;
}


/*
  BusLogic_DetectHostAdapter probes for BusLogic Host Adapters at the standard
  I/O Addresses where they may be located, initializing, registering, and
  reporting the configuration of each BusLogic Host Adapter it finds.  It
  returns the number of BusLogic Host Adapters successfully initialized and
  registered.
*/

int BusLogic_DetectHostAdapter(SCSI_Host_Template_T *HostTemplate)
{
  int BusLogicHostAdapterCount = 0, CommandLineEntryIndex = 0;
  int AddressProbeIndex = 0;
  BusLogic_InitializeAddressProbeList();
  while (BusLogic_IO_AddressProbeList[AddressProbeIndex] > 0)
    {
      BusLogic_HostAdapter_T HostAdapterPrototype;
      BusLogic_HostAdapter_T *HostAdapter = &HostAdapterPrototype;
      SCSI_Host_T *Host;
      memset(HostAdapter, 0, sizeof(BusLogic_HostAdapter_T));
      HostAdapter->IO_Address =
	BusLogic_IO_AddressProbeList[AddressProbeIndex++];
      /*
	Initialize the Command Line Entry field if an explicit I/O Address
	was specified.
      */
      if (CommandLineEntryIndex < BusLogic_CommandLineEntryCount &&
	  BusLogic_CommandLineEntries[CommandLineEntryIndex].IO_Address ==
	  HostAdapter->IO_Address)
	HostAdapter->CommandLineEntry =
	  &BusLogic_CommandLineEntries[CommandLineEntryIndex++];
      /*
	Check whether the I/O Address range is already in use.
      */
      if (check_region(HostAdapter->IO_Address, BusLogic_IO_PortCount) < 0)
	continue;
      /*
	Probe the Host Adapter.  If unsuccessful, abort further initialization.
      */
      if (!BusLogic_ProbeHostAdapter(HostAdapter)) continue;
      /*
	Hard Reset the Host Adapter.  If unsuccessful, abort further
	initialization.
      */
      if (!BusLogic_HardResetHostAdapter(HostAdapter)) continue;
      /*
	Check the Host Adapter.  If unsuccessful, abort further initialization.
      */
      if (!BusLogic_CheckHostAdapter(HostAdapter)) continue;
      /*
	Initialize the Command Line Entry field if an explicit I/O Address
	was not specified.
      */
      if (CommandLineEntryIndex < BusLogic_CommandLineEntryCount &&
	  BusLogic_CommandLineEntries[CommandLineEntryIndex].IO_Address == 0)
	HostAdapter->CommandLineEntry =
	  &BusLogic_CommandLineEntries[CommandLineEntryIndex++];
      /*
	Announce the Driver Version and Date, Author's Name, Copyright Notice,
	and Contact Address.
      */
      BusLogic_AnnounceDriver();
      /*
	Register usage of the I/O Address range.  From this point onward, any
	failure will be assumed to be due to a problem with the Host Adapter,
	rather than due to having mistakenly identified this port as belonging
	to a BusLogic Host Adapter.  The I/O Address range will not be
	released, thereby preventing it from being incorrectly identified as
	any other type of Host Adapter.
      */
      request_region(HostAdapter->IO_Address, BusLogic_IO_PortCount,
		     "BusLogic");
      /*
	Register the SCSI Host structure.
      */
      HostTemplate->proc_dir = &BusLogic_ProcDirectoryEntry;
      Host = scsi_register(HostTemplate, sizeof(BusLogic_HostAdapter_T));
      HostAdapter = (BusLogic_HostAdapter_T *) Host->hostdata;
      memcpy(HostAdapter, &HostAdapterPrototype,
	     sizeof(BusLogic_HostAdapter_T));
      HostAdapter->SCSI_Host = Host;
      HostAdapter->HostNumber = Host->host_no;
      /*
	Add Host Adapter to the end of the list of registered BusLogic
	Host Adapters.  In order for Command Complete Interrupts to be
	properly dismissed by BusLogic_InterruptHandler, the Host Adapter
	must be registered.  This must be done before the IRQ Channel is
	acquired, and in a shared IRQ Channel environment, must be done
	before any Command Complete Interrupts occur, since the IRQ Channel
	may have already been acquired by a previous BusLogic Host Adapter.
      */
      BusLogic_RegisterHostAdapter(HostAdapter);
      /*
	Read the Host Adapter Configuration, Acquire the System Resources
	necessary to use Host Adapter and initialize the fields in the SCSI
	Host structure, then Test Interrupts, Create the CCBs, Initialize
	the Host Adapter, and finally Inquire about the Target Devices.
      */
      if (BusLogic_ReadHostAdapterConfiguration(HostAdapter) &&
	  BusLogic_AcquireResources(HostAdapter, Host) &&
	  BusLogic_TestInterrupts(HostAdapter) &&
	  BusLogic_CreateCCBs(HostAdapter) &&
	  BusLogic_InitializeHostAdapter(HostAdapter) &&
	  BusLogic_InquireTargetDevices(HostAdapter))
	{
	  /*
	    Initialization has been completed successfully.  Release and
	    re-register usage of the I/O Address range so that the Model
	    Name of the Host Adapter will appear.
	  */
	  release_region(HostAdapter->IO_Address, BusLogic_IO_PortCount);
	  request_region(HostAdapter->IO_Address, BusLogic_IO_PortCount,
			 HostAdapter->BoardName);
	  BusLogicHostAdapterCount++;
	}
      else
	{
	  /*
	    An error occurred during Host Adapter Configuration Querying,
	    Resource Acquisition, Interrupt Testing, CCB Creation, Host
	    Adapter Initialization, or Target Device Inquiry, so remove
	    Host Adapter from the list of registered BusLogic Host Adapters,
	    destroy the CCBs, Release the System Resources, and Unregister
	    the SCSI Host.
	  */
	  BusLogic_DestroyCCBs(HostAdapter);
	  BusLogic_ReleaseResources(HostAdapter);
	  BusLogic_UnregisterHostAdapter(HostAdapter);
	  scsi_unregister(Host);
	}
    }
  return BusLogicHostAdapterCount;
}


/*
  BusLogic_ReleaseHostAdapter releases all resources previously acquired to
  support a specific Host Adapter, including the I/O Address range, and
  unregisters the BusLogic Host Adapter.
*/

int BusLogic_ReleaseHostAdapter(SCSI_Host_T *Host)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Host->hostdata;
  /*
    Destroy the CCBs and release any system resources acquired to use
    Host Adapter.
  */
  BusLogic_DestroyCCBs(HostAdapter);
  BusLogic_ReleaseResources(HostAdapter);
  /*
    Release usage of the I/O Address range.
  */
  release_region(HostAdapter->IO_Address, BusLogic_IO_PortCount);
  /*
    Remove Host Adapter from the list of registered BusLogic Host Adapters.
  */
  BusLogic_UnregisterHostAdapter(HostAdapter);
  return 0;
}


/*
  BusLogic_ComputeResultCode computes a SCSI Subsystem Result Code from
  the Host Adapter Status and Target Device Status.
*/

static int BusLogic_ComputeResultCode(BusLogic_HostAdapterStatus_T
					HostAdapterStatus,
				      BusLogic_TargetDeviceStatus_T
					TargetDeviceStatus)
{
  int HostStatus;
  switch (HostAdapterStatus)
    {
    case BusLogic_CommandCompletedNormally:
    case BusLogic_LinkedCommandCompleted:
    case BusLogic_LinkedCommandCompletedWithFlag:
      HostStatus = DID_OK;
      break;
    case BusLogic_SCSISelectionTimeout:
      HostStatus = DID_TIME_OUT;
      break;
    case BusLogic_InvalidOutgoingMailboxActionCode:
    case BusLogic_InvalidCommandOperationCode:
    case BusLogic_InvalidCommandParameter:
      printk("BusLogic: BusLogic Driver Protocol Error 0x%02X\n",
	     HostAdapterStatus);
    case BusLogic_DataOverUnderRun:
    case BusLogic_UnexpectedBusFree:
    case BusLogic_LinkedCCBhasInvalidLUN:
    case BusLogic_AutoRequestSenseFailed:
    case BusLogic_TaggedQueuingMessageRejected:
    case BusLogic_UnsupportedMessageReceived:
    case BusLogic_HostAdapterHardwareFailed:
    case BusLogic_TargetDeviceReconnectedImproperly:
    case BusLogic_AbortQueueGenerated:
    case BusLogic_HostAdapterSoftwareError:
    case BusLogic_HostAdapterHardwareTimeoutError:
    case BusLogic_SCSIParityErrorDetected:
      HostStatus = DID_ERROR;
      break;
    case BusLogic_InvalidBusPhaseRequested:
    case BusLogic_TargetFailedResponseToATN:
    case BusLogic_HostAdapterAssertedRST:
    case BusLogic_OtherDeviceAssertedRST:
    case BusLogic_HostAdapterAssertedBusDeviceReset:
      HostStatus = DID_RESET;
      break;
    default:
      printk("BusLogic: unknown Host Adapter Status 0x%02X\n",
	     HostAdapterStatus);
      HostStatus = DID_ERROR;
      break;
    }
  return (HostStatus << 16) | TargetDeviceStatus;
}


/*
  BusLogic_InterruptHandler handles hardware interrupts from BusLogic Host
  Adapters.  To simplify handling shared IRQ Channels, all installed BusLogic
  Host Adapters are scanned whenever any one of them signals a hardware
  interrupt.
*/

static void BusLogic_InterruptHandler(int IRQ_Channel,
				      Registers_T *InterruptRegisters)
{
  BusLogic_CCB_T *FirstCompletedCCB = NULL, *LastCompletedCCB = NULL;
  BusLogic_HostAdapter_T *HostAdapter;
  int HostAdapterResetPendingCount = 0;
  /*
    Iterate over the installed BusLogic Host Adapters accepting any Incoming
    Mailbox entries and saving the completed CCBs for processing.  This
    interrupt handler is installed with SA_INTERRUPT, so interrupts are
    disabled when the interrupt handler is entered.
  */
  for (HostAdapter = BusLogic_RegisteredHostAdapters;
       HostAdapter != NULL;
       HostAdapter = HostAdapter->Next)
    {
      unsigned char InterruptRegister;
      /*
	Acquire exclusive access to Host Adapter.
      */
      BusLogic_LockHostAdapterID(HostAdapter);
      /*
	Read the Host Adapter Interrupt Register.
      */
      InterruptRegister = BusLogic_ReadInterruptRegister(HostAdapter);
      if (InterruptRegister & BusLogic_InterruptValid)
	{
	  /*
	    Acknowledge the interrupt and reset the Host Adapter
	    Interrupt Register.
	  */
	  BusLogic_WriteControlRegister(HostAdapter, BusLogic_InterruptReset);
	  /*
	    Process valid SCSI Reset State and Incoming Mailbox Loaded
	    interrupts.  Command Complete interrupts are noted, and
	    Outgoing Mailbox Available interrupts are ignored, as they
	    are never enabled.
	  */
	  if (InterruptRegister & BusLogic_SCSIResetState)
	    {
	      HostAdapter->HostAdapterResetPending = true;
	      HostAdapterResetPendingCount++;
	    }
	  else if (InterruptRegister & BusLogic_IncomingMailboxLoaded)
	    {
	      /*
		Scan through the Incoming Mailboxes in Strict Round Robin
		fashion, saving any completed CCBs for further processing.
		It is essential that for each CCB and SCSI Command issued,
		command completion processing is performed exactly once.
		Therefore, only Incoming Mailboxes with completion code
		Command Completed Without Error, Command Completed With
		Error, or Command Aborted At Host Request are saved for
		completion processing.  When an Incoming Mailbox has a
		completion code of Aborted Command Not Found, the CCB had
		already completed or been aborted before the current Abort
		request was processed, and so completion processing has
		already occurred and no further action should be taken.
	      */
	      BusLogic_IncomingMailbox_T *NextIncomingMailbox =
		HostAdapter->NextIncomingMailbox;
	      BusLogic_CompletionCode_T MailboxCompletionCode;
	      while ((MailboxCompletionCode =
		      NextIncomingMailbox->CompletionCode) !=
		     BusLogic_IncomingMailboxFree)
		{
		  BusLogic_CCB_T *CCB = NextIncomingMailbox->CCB;
		  if (MailboxCompletionCode != BusLogic_AbortedCommandNotFound)
		    if (CCB->Status == BusLogic_CCB_Active)
		      {
			/*
			  Mark this CCB as completed and add it to the end
			  of the list of completed CCBs.
			*/
			CCB->Status = BusLogic_CCB_Completed;
			CCB->MailboxCompletionCode = MailboxCompletionCode;
			CCB->Next = NULL;
			if (FirstCompletedCCB == NULL)
			  {
			    FirstCompletedCCB = CCB;
			    LastCompletedCCB = CCB;
			  }
			else
			  {
			    LastCompletedCCB->Next = CCB;
			    LastCompletedCCB = CCB;
			  }
			HostAdapter->QueuedOperationCount[CCB->TargetID]--;
		      }
		    else
		      {
			/*
			  If a CCB ever appears in an Incoming Mailbox and
			  is not marked as status Active, then there is
			  most likely a bug in the Host Adapter firmware.
			*/
			printk("scsi%d: Illegal CCB #%d status %d in "
			       "Incoming Mailbox\n", HostAdapter->HostNumber,
			       CCB->SerialNumber, CCB->Status);
		      }
		  else printk("scsi%d: Aborted CCB #%d to Target %d "
			      "Not Found\n", HostAdapter->HostNumber,
			      CCB->SerialNumber, CCB->TargetID);
		  NextIncomingMailbox->CompletionCode =
		    BusLogic_IncomingMailboxFree;
		  if (++NextIncomingMailbox > HostAdapter->LastIncomingMailbox)
		    NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;
		}
	      HostAdapter->NextIncomingMailbox = NextIncomingMailbox;
	    }
	  else if (InterruptRegister & BusLogic_CommandComplete)
	    HostAdapter->HostAdapterCommandCompleted = true;
	}
      /*
	Release exclusive access to Host Adapter.
      */
      BusLogic_UnlockHostAdapterID(HostAdapter);
    }
  /*
    Enable interrupts while the completed CCBs are processed.
  */
  sti();
  /*
    Iterate over the Host Adapters performing any pending Host Adapter Resets.
  */
  if (HostAdapterResetPendingCount > 0)
    for (HostAdapter = BusLogic_RegisteredHostAdapters;
	 HostAdapter != NULL;
	 HostAdapter = HostAdapter->Next)
      if (HostAdapter->HostAdapterResetPending)
	{
	  BusLogic_ResetHostAdapter(HostAdapter, NULL);
	  HostAdapter->HostAdapterResetPending = false;
	  scsi_mark_host_bus_reset(HostAdapter->SCSI_Host);
	}
  /*
    Iterate over the completed CCBs setting the SCSI Command Result Codes,
    deallocating the CCBs, and calling the Completion Routines.
  */
  while (FirstCompletedCCB != NULL)
    {
      BusLogic_CCB_T *CCB = FirstCompletedCCB;
      SCSI_Command_T *Command = CCB->Command;
      FirstCompletedCCB = FirstCompletedCCB->Next;
      HostAdapter = CCB->HostAdapter;
      /*
	Bus Device Reset CCBs have the Command field non-NULL only when a Bus
	Device Reset was requested for a command that was not currently active
	in the Host Adapter, and hence would not have its Completion Routine
	called otherwise.
      */
      if (CCB->Opcode == BusLogic_SCSIBusDeviceReset)
	{
	  printk("scsi%d: Bus Device Reset CCB #%d to Target %d Completed\n",
		 HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
	  if (Command != NULL) Command->result = DID_RESET << 16;
	}
      else
	/*
	  Translate the Mailbox Completion Code, Host Adapter Status, and
	  Target Device Status into a SCSI Subsystem Result Code.
	*/
	switch (CCB->MailboxCompletionCode)
	  {
	  case BusLogic_IncomingMailboxFree:
	  case BusLogic_AbortedCommandNotFound:
	    printk("scsi%d: CCB #%d to Target %d Impossible State\n",
		   HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
	    break;
	  case BusLogic_CommandCompletedWithoutError:
	    HostAdapter->CommandSuccessfulFlag[CCB->TargetID] = true;
	    Command->result = DID_OK << 16;
	    break;
	  case BusLogic_CommandAbortedAtHostRequest:
	    printk("scsi%d: CCB #%d to Target %d Aborted\n",
		   HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
	    Command->result = DID_ABORT << 16;
	    break;
	  case BusLogic_CommandCompletedWithError:
	    Command->result =
	      BusLogic_ComputeResultCode(CCB->HostAdapterStatus,
					 CCB->TargetDeviceStatus);
	    if (BusLogic_GlobalOptions & BusLogic_TraceErrors)
	      if (CCB->HostAdapterStatus != BusLogic_SCSISelectionTimeout)
		{
		  int i;
		  printk("scsi%d: CCB #%d Target %d: Result %X "
			 "Host Adapter Status %02X Target Status %02X\n",
			 HostAdapter->HostNumber, CCB->SerialNumber,
			 CCB->TargetID, Command->result,
			 CCB->HostAdapterStatus, CCB->TargetDeviceStatus);
		  printk("scsi%d: CDB   ", HostAdapter->HostNumber);
		  for (i = 0; i < CCB->CDB_Length; i++)
		    printk(" %02X", CCB->CDB[i]);
		  printk("\n");
		  printk("scsi%d: Sense ", HostAdapter->HostNumber);
		  for (i = 0; i < CCB->SenseDataLength; i++)
		    printk(" %02X", (*CCB->SenseDataPointer)[i]);
		  printk("\n");
		}
	    break;
	  }
      /*
	Place CCB back on the Host Adapter's free list.
      */
      BusLogic_DeallocateCCB(CCB);
      /*
	Call the SCSI Command Completion Routine if appropriate.
      */
      if (Command != NULL) Command->scsi_done(Command);
    }
}


/*
  BusLogic_WriteOutgoingMailbox places CCB and Action Code into an Outgoing
  Mailbox for execution by Host Adapter.
*/

static boolean BusLogic_WriteOutgoingMailbox(BusLogic_HostAdapter_T
					       *HostAdapter,
					     BusLogic_ActionCode_T ActionCode,
					     BusLogic_CCB_T *CCB)
{
  BusLogic_OutgoingMailbox_T *NextOutgoingMailbox;
  boolean Result = false;
  BusLogic_LockHostAdapter(HostAdapter);
  NextOutgoingMailbox = HostAdapter->NextOutgoingMailbox;
  if (NextOutgoingMailbox->ActionCode == BusLogic_OutgoingMailboxFree)
    {
      CCB->Status = BusLogic_CCB_Active;
      /*
	The CCB field must be written before the Action Code field since
	the Host Adapter is operating asynchronously and the locking code
	does not protect against simultaneous access by the Host Adapter.
      */
      NextOutgoingMailbox->CCB = CCB;
      NextOutgoingMailbox->ActionCode = ActionCode;
      BusLogic_StartMailboxScan(HostAdapter);
      if (++NextOutgoingMailbox > HostAdapter->LastOutgoingMailbox)
	NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
      HostAdapter->NextOutgoingMailbox = NextOutgoingMailbox;
      if (ActionCode == BusLogic_MailboxStartCommand)
	HostAdapter->QueuedOperationCount[CCB->TargetID]++;
      Result = true;
    }
  BusLogic_UnlockHostAdapter(HostAdapter);
  return Result;
}


/*
  BusLogic_QueueCommand creates a CCB for Command and places it into an
  Outgoing Mailbox for execution by the associated Host Adapter.
*/

int BusLogic_QueueCommand(SCSI_Command_T *Command,
			  void (*CompletionRoutine)(SCSI_Command_T *))
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  unsigned char *CDB = Command->cmnd;
  unsigned char CDB_Length = Command->cmd_len;
  unsigned char TargetID = Command->target;
  unsigned char LogicalUnit = Command->lun;
  void *BufferPointer = Command->request_buffer;
  int BufferLength = Command->request_bufflen;
  int SegmentCount = Command->use_sg;
  BusLogic_CCB_T *CCB;
  long EnableTQ;
  /*
    SCSI REQUEST_SENSE commands will be executed automatically by the Host
    Adapter for any errors, so they should not be executed explicitly unless
    the Sense Data is zero indicating that no error occurred.
  */
  if (CDB[0] == REQUEST_SENSE && Command->sense_buffer[0] != 0)
    {
      Command->result = DID_OK << 16;
      CompletionRoutine(Command);
      return 0;
    }
  /*
    Allocate a CCB from the Host Adapter's free list.  If there are none
    available and memory allocation fails, return a result code of Bus Busy
    so that this Command will be retried.
  */
  CCB = BusLogic_AllocateCCB(HostAdapter);
  if (CCB == NULL)
    {
      Command->result = DID_BUS_BUSY << 16;
      CompletionRoutine(Command);
      return 0;
    }
  /*
    Initialize the fields in the BusLogic Command Control Block (CCB).
  */
  if (SegmentCount == 0)
    {
      CCB->Opcode = BusLogic_InitiatorCCB;
      CCB->DataLength = BufferLength;
      CCB->DataPointer = BufferPointer;
    }
  else
    {
      SCSI_ScatterList_T *ScatterList = (SCSI_ScatterList_T *) BufferPointer;
      int Segment;
      CCB->Opcode = BusLogic_InitiatorCCB_ScatterGather;
      CCB->DataLength = SegmentCount * sizeof(BusLogic_ScatterGatherSegment_T);
      CCB->DataPointer = CCB->ScatterGatherList;
      for (Segment = 0; Segment < SegmentCount; Segment++)
	{
	  CCB->ScatterGatherList[Segment].SegmentByteCount =
	    ScatterList[Segment].length;
	  CCB->ScatterGatherList[Segment].SegmentDataPointer =
	    ScatterList[Segment].address;
	}
    }
  switch (CDB[0])
    {
    case READ_6:
    case READ_10:
      CCB->DataDirection = BusLogic_DataInLengthChecked;
      HostAdapter->ReadWriteOperationCount[TargetID]++;
      break;
    case WRITE_6:
    case WRITE_10:
      CCB->DataDirection = BusLogic_DataOutLengthChecked;
      HostAdapter->ReadWriteOperationCount[TargetID]++;
      break;
    default:
      CCB->DataDirection = BusLogic_UncheckedDataTransfer;
      break;
    }
  CCB->CDB_Length = CDB_Length;
  CCB->SenseDataLength = sizeof(Command->sense_buffer);
  CCB->HostAdapterStatus = 0;
  CCB->TargetDeviceStatus = 0;
  CCB->TargetID = TargetID;
  CCB->LogicalUnit = LogicalUnit;
  /*
    For Wide SCSI Host Adapters, Wide Mode CCBs are used to support more than
    8 Logical Units per Target, and this requires setting the overloaded
    TagEnable field to Logical Unit bit 5.
  */
  if (HostAdapter->HostWideSCSI)
    {
      CCB->TagEnable = LogicalUnit >> 5;
      CCB->WideModeTagEnable = false;
    }
  else CCB->TagEnable = false;
  /*
    BusLogic recommends that after a Reset the first couple of commands that
    are sent to a Target be sent in a non Tagged Queue fashion so that the Host
    Adapter and Target can establish Synchronous Transfer before Queue Tag
    messages can interfere with the Synchronous Negotiation message.  By
    waiting to enable tagged Queuing until after the first 16 read/write
    commands have been sent, it is assured that the Tagged Queuing message
    will not occur while the partition table is printed.
  */
  if ((HostAdapter->TaggedQueuingPermitted & (1 << TargetID)) &&
      Command->device->tagged_supported &&
      (EnableTQ = HostAdapter->ReadWriteOperationCount[TargetID] - 16) >= 0)
    {
      BusLogic_QueueTag_T QueueTag = BusLogic_SimpleQueueTag;
      unsigned long CurrentTime = jiffies;
      if (EnableTQ == 0)
	printk("scsi%d: Tagged Queuing now active for Target %d\n",
	       HostAdapter->HostNumber, TargetID);
      /*
	When using Tagged Queuing with Simple Queue Tags, it appears that disk
	drive controllers do not guarantee that a queued command will not
	remain in a disconnected state indefinitely if commands that read or
	write nearer the head position continue to arrive without interruption.
	Therefore, for each Target Device this driver keeps track of the last
	time either the queue was empty or an Ordered Queue Tag was issued.  If
	more than 2 seconds have elapsed since this last sequence point, this
	command will be issued with an Ordered Queue Tag rather than a Simple
	Queue Tag, which forces the Target Device to complete all previously
	queued commands before this command may be executed.
      */
      if (HostAdapter->QueuedOperationCount[TargetID] == 0)
	HostAdapter->LastSequencePoint[TargetID] = CurrentTime;
      else if (CurrentTime - HostAdapter->LastSequencePoint[TargetID] > 2*HZ)
	{
	  HostAdapter->LastSequencePoint[TargetID] = CurrentTime;
	  QueueTag = BusLogic_OrderedQueueTag;
	}
      if (HostAdapter->HostWideSCSI)
	{
	  CCB->WideModeTagEnable = true;
	  CCB->WideModeQueueTag = QueueTag;
	}
      else
	{
	  CCB->TagEnable = true;
	  CCB->QueueTag = QueueTag;
	}
    }
  memcpy(CCB->CDB, CDB, CDB_Length);
  CCB->SenseDataPointer = (SCSI_SenseData_T *) &Command->sense_buffer;
  CCB->Command = Command;
  Command->scsi_done = CompletionRoutine;
  /*
    Place the CCB in an Outgoing Mailbox.  If there are no Outgoing
    Mailboxes available, return a result code of Bus Busy so that this
    Command will be retried.
  */
  if (!(BusLogic_WriteOutgoingMailbox(HostAdapter,
				      BusLogic_MailboxStartCommand, CCB)))
    {
      printk("scsi%d: cannot write Outgoing Mailbox\n",
	     HostAdapter->HostNumber);
      BusLogic_DeallocateCCB(CCB);
      Command->result = DID_BUS_BUSY << 16;
      CompletionRoutine(Command);
    }
  return 0;
}


/*
  BusLogic_AbortCommand aborts Command if possible.
*/

int BusLogic_AbortCommand(SCSI_Command_T *Command)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  unsigned long CommandPID = Command->pid;
  unsigned char InterruptRegister;
  BusLogic_CCB_T *CCB;
  int Result;
  /*
    If the Host Adapter has posted an interrupt but the Interrupt Handler
    has not been called for some reason (i.e. the interrupt was lost), try
    calling the Interrupt Handler directly to process the commands that
    have been completed.
  */
  InterruptRegister = BusLogic_ReadInterruptRegister(HostAdapter);
  if (InterruptRegister & BusLogic_InterruptValid)
    {
      unsigned long ProcessorFlags;
      printk("scsi%d: Recovering Lost/Delayed Interrupt for IRQ Channel %d\n",
	     HostAdapter->HostNumber, HostAdapter->IRQ_Channel);
      save_flags(ProcessorFlags);
      cli();
      BusLogic_InterruptHandler(HostAdapter->IRQ_Channel, NULL);
      restore_flags(ProcessorFlags);
      return SCSI_ABORT_SNOOZE;
    }
  /*
    Find the CCB to be aborted if possible.
  */
  BusLogic_LockHostAdapter(HostAdapter);
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Command == Command) break;
  BusLogic_UnlockHostAdapter(HostAdapter);
  if (CCB == NULL)
    {
      printk("scsi%d: Unable to Abort Command to Target %d - No CCB Found\n",
	     HostAdapter->HostNumber, Command->target);
      return SCSI_ABORT_NOT_RUNNING;
    }
  /*
    Briefly pause to see if this command will complete.
  */
  printk("scsi%d: Pausing briefly to see if CCB #%d "
	 "to Target %d will complete\n",
	 HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
  BusLogic_Delay(2);
  /*
    If this CCB is still Active and still refers to the same Command, then
    actually aborting this Command is necessary.
  */
  BusLogic_LockHostAdapter(HostAdapter);
  Result = SCSI_ABORT_NOT_RUNNING;
  if (CCB->Status == BusLogic_CCB_Active &&
      CCB->Command == Command && Command->pid == CommandPID)
    {
      /*
	Attempt to abort this CCB.
      */
      if (BusLogic_WriteOutgoingMailbox(HostAdapter,
					BusLogic_MailboxAbortCommand, CCB))
	{
	  printk("scsi%d: Aborting CCB #%d to Target %d\n",
		 HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
	  Result = SCSI_ABORT_PENDING;
	}
      else
	{
	  printk("scsi%d: Unable to Abort CCB #%d to Target %d - "
		 "No Outgoing Mailboxes\n", HostAdapter->HostNumber,
		 CCB->SerialNumber, CCB->TargetID);
	  Result = SCSI_ABORT_BUSY;
	}
    }
  else printk("scsi%d: CCB #%d to Target %d completed\n",
	      HostAdapter->HostNumber, CCB->SerialNumber, CCB->TargetID);
  BusLogic_UnlockHostAdapter(HostAdapter);
  return Result;
}


/*
  BusLogic_ResetHostAdapter resets Host Adapter if possible, marking all
  currently executing SCSI commands as having been reset, as well as
  the specified Command if non-NULL.
*/

static int BusLogic_ResetHostAdapter(BusLogic_HostAdapter_T *HostAdapter,
				     SCSI_Command_T *Command)
{
  BusLogic_CCB_T *CCB;
  if (Command == NULL)
    printk("scsi%d: Resetting %s due to SCSI Reset State Interrupt\n",
	   HostAdapter->HostNumber, HostAdapter->BoardName);
  else printk("scsi%d: Resetting %s due to Target %d\n",
	      HostAdapter->HostNumber, HostAdapter->BoardName, Command->target);
  /*
    Attempt to Reset and Reinitialize the Host Adapter.
  */
  BusLogic_LockHostAdapter(HostAdapter);
  if (!(BusLogic_HardResetHostAdapter(HostAdapter) &&
	BusLogic_InitializeHostAdapter(HostAdapter)))
    {
      printk("scsi%d: Resetting %s Failed\n",
	      HostAdapter->HostNumber, HostAdapter->BoardName);
      BusLogic_UnlockHostAdapter(HostAdapter);
      return SCSI_RESET_ERROR;
    }
  BusLogic_UnlockHostAdapter(HostAdapter);
  /*
    Wait a few seconds between the Host Adapter Hard Reset which initiates
    a SCSI Bus Reset and issuing any SCSI commands.  Some SCSI devices get
    confused if they receive SCSI commands too soon after a SCSI Bus Reset.
  */
  BusLogic_Delay(HostAdapter->BusSettleTime);
  /*
    Mark all currently executing CCBs as having been reset.
  */
  BusLogic_LockHostAdapter(HostAdapter);
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Status == BusLogic_CCB_Active)
      {
	CCB->Status = BusLogic_CCB_Reset;
	if (CCB->Command == Command)
	  {
	    CCB->Command = NULL;
	    /*
	      Disable Tagged Queuing if it was active for this Target Device.
	    */
	    if (((HostAdapter->HostWideSCSI && CCB->WideModeTagEnable) ||
		 (!HostAdapter->HostWideSCSI && CCB->TagEnable)) &&
		(HostAdapter->TaggedQueuingPermitted & (1 << CCB->TargetID)))
	      {
		HostAdapter->TaggedQueuingPermitted &= ~(1 << CCB->TargetID);
		printk("scsi%d: Tagged Queuing now disabled for Target %d\n",
		       HostAdapter->HostNumber, CCB->TargetID);
	      }
	  }
      }
  BusLogic_UnlockHostAdapter(HostAdapter);
  /*
    Perform completion processing for the Command being Reset.
  */
  if (Command != NULL)
    {
      Command->result = DID_RESET << 16;
      Command->scsi_done(Command);
    }
  /*
    Perform completion processing for any other active CCBs.
  */
  for (CCB = HostAdapter->All_CCBs; CCB != NULL; CCB = CCB->NextAll)
    if (CCB->Status == BusLogic_CCB_Reset)
      {
	Command = CCB->Command;
	BusLogic_DeallocateCCB(CCB);
	if (Command != NULL)
	  {
	    Command->result = DID_RESET << 16;
	    Command->scsi_done(Command);
	  }
      }
  return SCSI_RESET_SUCCESS | SCSI_RESET_BUS_RESET;
}


/*
  BusLogic_BusDeviceReset sends a Bus Device Reset to the Target
  associated with Command.
*/

static int BusLogic_BusDeviceReset(BusLogic_HostAdapter_T *HostAdapter,
				   SCSI_Command_T *Command)
{
  BusLogic_CCB_T *CCB = BusLogic_AllocateCCB(HostAdapter), *XCCB;
  unsigned char TargetID = Command->target;
  /*
    If sending a Bus Device Reset is impossible, attempt a full Host
    Adapter Hard Reset and SCSI Bus Reset.
  */
  if (CCB == NULL)
    return BusLogic_ResetHostAdapter(HostAdapter, Command);
  printk("scsi%d: Sending Bus Device Reset CCB #%d to Target %d\n",
	 HostAdapter->HostNumber, CCB->SerialNumber, TargetID);
  CCB->Opcode = BusLogic_SCSIBusDeviceReset;
  CCB->TargetID = TargetID;
  CCB->Command = Command;
  /*
    If there is a currently executing CCB in the Host Adapter for this Command,
    then an Incoming Mailbox entry will be made with a completion code of
    BusLogic_HostAdapterAssertedBusDeviceReset.  Otherwise, the CCB's Command
    field will be left pointing to the Command so that the interrupt for the
    completion of the Bus Device Reset can call the Completion Routine for the
    Command.
  */
  BusLogic_LockHostAdapter(HostAdapter);
  for (XCCB = HostAdapter->All_CCBs; XCCB != NULL; XCCB = XCCB->NextAll)
    if (XCCB->Command == Command && XCCB->Status == BusLogic_CCB_Active)
      {
	CCB->Command = NULL;
	/*
	  Disable Tagged Queuing if it was active for this Target Device.
	*/
	if (((HostAdapter->HostWideSCSI && XCCB->WideModeTagEnable) ||
	    (!HostAdapter->HostWideSCSI && XCCB->TagEnable)) &&
	    (HostAdapter->TaggedQueuingPermitted & (1 << TargetID)))
	  {
	    HostAdapter->TaggedQueuingPermitted &= ~(1 << TargetID);
	    printk("scsi%d: Tagged Queuing now disabled for Target %d\n",
		   HostAdapter->HostNumber, TargetID);
	  }
	break;
      }
  BusLogic_UnlockHostAdapter(HostAdapter);
  /*
    Attempt to write an Outgoing Mailbox with the Bus Device Reset CCB.
    If sending a Bus Device Reset is impossible, attempt a full Host
    Adapter Hard Reset and SCSI Bus Reset.
  */
  if (!(BusLogic_WriteOutgoingMailbox(HostAdapter,
				      BusLogic_MailboxStartCommand, CCB)))
    {
      printk("scsi%d: cannot write Outgoing Mailbox for Bus Device Reset\n",
	     HostAdapter->HostNumber);
      BusLogic_DeallocateCCB(CCB);
      return BusLogic_ResetHostAdapter(HostAdapter, Command);
    }
  HostAdapter->ReadWriteOperationCount[TargetID] = 0;
  HostAdapter->QueuedOperationCount[TargetID] = 0;
  return SCSI_RESET_PENDING;
}


/*
  BusLogic_ResetCommand takes appropriate action to reset Command.
*/

int BusLogic_ResetCommand(SCSI_Command_T *Command)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Command->host->hostdata;
  unsigned char TargetID = Command->target;
  unsigned char ErrorRecoveryOption =
    HostAdapter->ErrorRecoveryOption[TargetID];
  if (ErrorRecoveryOption == BusLogic_ErrorRecoveryDefault)
    if (Command->host->suggest_bus_reset)
      ErrorRecoveryOption = BusLogic_ErrorRecoveryHardReset;
    else ErrorRecoveryOption = BusLogic_ErrorRecoveryBusDeviceReset;
  switch (ErrorRecoveryOption)
    {
    case BusLogic_ErrorRecoveryHardReset:
      return BusLogic_ResetHostAdapter(HostAdapter, Command);
    case BusLogic_ErrorRecoveryBusDeviceReset:
      if (HostAdapter->CommandSuccessfulFlag[TargetID])
	{
	  HostAdapter->CommandSuccessfulFlag[TargetID] = false;
	  return BusLogic_BusDeviceReset(HostAdapter, Command);
	}
      else return BusLogic_ResetHostAdapter(HostAdapter, Command);
    }
  printk("scsi%d: Error Recovery for Target %d Suppressed\n",
	 HostAdapter->HostNumber, TargetID);
  return SCSI_RESET_PUNT;
}


/*
  BusLogic_BIOSDiskParameters returns the Heads/Sectors/Cylinders BIOS Disk
  Parameters for Disk.  The default disk geometry is 64 heads, 32 sectors, and
  the appropriate number of cylinders so as not to exceed drive capacity.  In
  order for disks equal to or larger than 1 GB to be addressable by the BIOS
  without exceeding the BIOS limitation of 1024 cylinders, Extended Translation
  may be enabled in AutoSCSI on "C" Series boards or by a dip switch setting
  on older boards.  With Extended Translation enabled, drives between 1 GB
  inclusive and 2 GB exclusive are given a disk geometry of 128 heads and 32
  sectors, and drives between 2 GB inclusive and 8 GB exclusive are given a
  disk geometry of 255 heads and 63 sectors.  On "C" Series boards the firmware
  can be queried for the precise translation in effect for each drive
  individually, but there is really no need to do so since we know the total
  capacity of the drive and whether Extended Translation is enabled, hence we
  can deduce the BIOS disk geometry that must be in effect.
*/

int BusLogic_BIOSDiskParameters(SCSI_Disk_T *Disk, KernelDevice_T Device,
				int *Parameters)
{
  BusLogic_HostAdapter_T *HostAdapter =
    (BusLogic_HostAdapter_T *) Disk->device->host->hostdata;
  BIOS_DiskParameters_T *DiskParameters = (BIOS_DiskParameters_T *) Parameters;
  if (HostAdapter->ExtendedTranslation &&
      Disk->capacity >= 2*1024*1024 /* 1 GB in 512 byte sectors */)
    if (Disk->capacity >= 4*1024*1024 /* 2 GB in 512 byte sectors */)
      {
	DiskParameters->Heads = 255;
	DiskParameters->Sectors = 63;
      }
    else
      {
	DiskParameters->Heads = 128;
	DiskParameters->Sectors = 32;
      }
  else
    {
      DiskParameters->Heads = 64;
      DiskParameters->Sectors = 32;
    }
  DiskParameters->Cylinders =
    Disk->capacity / (DiskParameters->Heads * DiskParameters->Sectors);
  return 0;
}


/*
  BusLogic_Setup handles processing of Kernel Command Line Arguments.

  For the BusLogic driver, a kernel command line entry comprises the driver
  identifier "BusLogic=" optionally followed by a comma-separated sequence of
  integers and then optionally followed by a comma-separated sequence of
  strings.  Each command line entry applies to one BusLogic Host Adapter.
  Multiple command line entries may be used in systems which contain multiple
  BusLogic Host Adapters.

  The first integer specified is the I/O Address at which the Host Adapter is
  located.  If unspecified, it defaults to 0 which means to apply this entry to
  the first BusLogic Host Adapter found during the default probe sequence.  If
  any I/O Address parameters are provided on the command line, then the default
  probe sequence is omitted.

  The second integer specified is the number of Concurrent Commands per Logical
  Unit to allow for Target Devices on the Host Adapter.  If unspecified, it
  defaults to 0 which means to use the value of BusLogic_Concurrency for
  non-ISA Host Adapters, or BusLogic_Concurrency_ISA for ISA Host Adapters.

  The third integer specified is the Bus Settle Time in seconds.  This is
  the amount of time to wait between a Host Adapter Hard Reset which initiates
  a SCSI Bus Reset and issuing any SCSI commands.  If unspecified, it defaults
  to 0 which means to use the value of BusLogic_DefaultBusSettleTime.

  The fourth integer specified is the Local Options.  If unspecified, it
  defaults to 0.  Note that Local Options are only applied to a specific Host
  Adapter.

  The fifth integer specified is the Global Options.  If unspecified, it
  defaults to 0.  Note that Global Options are applied across all Host
  Adapters.

  The string options are used to provide control over Tagged Queuing and Error
  Recovery. If both Tagged Queuing and Error Recovery strings are provided, the
  Tagged Queuing specification string must come first.

  The Tagged Queuing specification begins with "TQ:" and allows for explicitly
  specifying whether Tagged Queuing is permitted on Target Devices that support
  it.  The following specification options are available:

  TQ:Default		Tagged Queuing will be permitted based on the firmware
			version of the BusLogic Host Adapter and based on
			whether the Concurrency value allows queuing multiple
			commands.

  TQ:Enable		Tagged Queuing will be enabled for all Target Devices
			on this Host Adapter overriding any limitation that
			would otherwise be imposed based on the Host Adapter
			firmware version.

  TQ:Disable		Tagged Queuing will be disabled for all Target Devices
			on this Host Adapter.

  TQ:<Per-Target-Spec>	Tagged Queuing will be controlled individually for each
			Target Device.  <Per-Target-Spec> is a sequence of "Y",
			"N", and "X" characters.  "Y" enabled Tagged Queuing,
			"N" disables Tagged Queuing, and "X" accepts the
			default based on the firmware version.  The first
			character refers to Target 0, the second to Target 1,
			and so on; if the sequence of "Y", "N", and "X"
			characters does not cover all the Target Devices,
			unspecified characters are assumed to be "X".

  Note that explicitly requesting Tagged Queuing may lead to problems; this
  facility is provided primarily to allow disabling Tagged Queuing on Target
  Devices that do not implement it correctly.

  The Error Recovery specification begins with "ER:" and allows for explicitly
  specifying the Error Recovery action to be performed when ResetCommand is
  called due to a SCSI Command failing to complete successfully.  The following
  specification options are available:

  ER:Default		Error Recovery will select between the Hard Reset and
			Bus Device Reset options based on the recommendation
			of the SCSI Subsystem.

  ER:HardReset		Error Recovery will initiate a Host Adapter Hard Reset
			which also causes a SCSI Bus Reset.

  ER:BusDeviceReset	Error Recovery will send a Bus Device Reset message to
			the individual Target Device causing the error.  If
			Error Recovery is again initiated for this Target
			Device and no SCSI Command to this Target Device has
			completed successfully since the Bus Device Reset
			message was sent, then a Hard Reset will be attempted.

  ER:None		Error Recovery will be suppressed.  This option should
			only be selected if a SCSI Bus Reset or Bus Device
			Reset will cause the Target Device to fail completely
			and unrecoverably.

  ER:<Per-Target-Spec>	Error Recovery will be controlled individually for each
			Target Device.  <Per-Target-Spec> is a sequence of "D",
			"H", "B", and "N" characters.  "D" selects Default, "H"
			selects Hard Reset, "B" selects Bus Device Reset, and
			"N" selects None.  The first character refers to Target
			0, the second to Target 1, and so on; if the sequence
			of "D", "H", "B", and "N" characters does not cover all
			the Target Devices, unspecified characters are assumed
			to be "D".
*/

void BusLogic_Setup(char *Strings, int *Integers)
{
  BusLogic_CommandLineEntry_T *CommandLineEntry =
    &BusLogic_CommandLineEntries[BusLogic_CommandLineEntryCount++];
  static int ProbeListIndex = 0;
  int IntegerCount = Integers[0], TargetID, i;
  CommandLineEntry->IO_Address = 0;
  CommandLineEntry->Concurrency = 0;
  CommandLineEntry->BusSettleTime = 0;
  CommandLineEntry->LocalOptions = 0;
  CommandLineEntry->TaggedQueuingPermitted = 0;
  CommandLineEntry->TaggedQueuingPermittedMask = 0;
  memset(CommandLineEntry->ErrorRecoveryOption,
	 BusLogic_ErrorRecoveryDefault,
	 sizeof(CommandLineEntry->ErrorRecoveryOption));
  if (IntegerCount > 5)
    printk("BusLogic: Unexpected Command Line Integers ignored\n");
  if (IntegerCount >= 1)
    {
      unsigned short IO_Address = Integers[1];
      if (IO_Address > 0)
	{
	  for (i = 0; ; i++)
	    if (BusLogic_IO_StandardAddresses[i] == 0)
	      {
		printk("BusLogic: Invalid Command Line Entry "
		       "(illegal I/O Address 0x%X)\n", IO_Address);
		return;
	      }
	    else if (i < ProbeListIndex &&
		     IO_Address == BusLogic_IO_AddressProbeList[i])
	      {
		printk("BusLogic: Invalid Command Line Entry "
		       "(duplicate I/O Address 0x%X)\n", IO_Address);
		return;
	      }
	    else if (IO_Address >= 0x1000 ||
		     IO_Address == BusLogic_IO_StandardAddresses[i]) break;
	  BusLogic_IO_AddressProbeList[ProbeListIndex++] = IO_Address;
	  BusLogic_IO_AddressProbeList[ProbeListIndex] = 0;
	}
      CommandLineEntry->IO_Address = IO_Address;
    }
  if (IntegerCount >= 2)
    {
      unsigned short Concurrency = Integers[2];
      if (Concurrency > BusLogic_MailboxCount)
	{
	  printk("BusLogic: Invalid Command Line Entry "
		 "(illegal Concurrency %d)\n", Concurrency);
	  return;
	}
      CommandLineEntry->Concurrency = Concurrency;
    }
  if (IntegerCount >= 3)
    CommandLineEntry->BusSettleTime = Integers[3];
  if (IntegerCount >= 4)
    CommandLineEntry->LocalOptions = Integers[4];
  if (IntegerCount >= 5)
    BusLogic_GlobalOptions |= Integers[5];
  if (!(BusLogic_CommandLineEntryCount == 0 || ProbeListIndex == 0 ||
	BusLogic_CommandLineEntryCount == ProbeListIndex))
    {
      printk("BusLogic: Invalid Command Line Entry "
	     "(all or no I/O Addresses must be specified)\n");
      return;
    }
  if (Strings == NULL) return;
  if (strncmp(Strings, "TQ:", 3) == 0)
    {
      Strings += 3;
      if (strncmp(Strings, "Default", 7) == 0)
	Strings += 7;
      else if (strncmp(Strings, "Enable", 6) == 0)
	{
	  Strings += 6;
	  CommandLineEntry->TaggedQueuingPermitted = 0xFFFF;
	  CommandLineEntry->TaggedQueuingPermittedMask = 0xFFFF;
	}
      else if (strncmp(Strings, "Disable", 7) == 0)
	{
	  Strings += 7;
	  CommandLineEntry->TaggedQueuingPermitted = 0x0000;
	  CommandLineEntry->TaggedQueuingPermittedMask = 0xFFFF;
	}
      else
	for (TargetID = 0; TargetID < BusLogic_MaxTargetIDs; TargetID++)
	  switch (*Strings++)
	    {
	    case 'Y':
	      CommandLineEntry->TaggedQueuingPermitted |= 1 << TargetID;
	      CommandLineEntry->TaggedQueuingPermittedMask |= 1 << TargetID;
	      break;
	    case 'N':
	      CommandLineEntry->TaggedQueuingPermittedMask |= 1 << TargetID;
	      break;
	    case 'X':
	      break;
	    default:
	      Strings--;
	      TargetID = BusLogic_MaxTargetIDs;
	      break;
	    }
    }
  if (*Strings == ',') Strings++;
  if (strncmp(Strings, "ER:", 3) == 0)
    {
      Strings += 3;
      if (strncmp(Strings, "Default", 7) == 0)
	Strings += 7;
      else if (strncmp(Strings, "HardReset", 9) == 0)
	{
	  Strings += 9;
	  memset(CommandLineEntry->ErrorRecoveryOption,
		 BusLogic_ErrorRecoveryHardReset,
		 sizeof(CommandLineEntry->ErrorRecoveryOption));
	}
      else if (strncmp(Strings, "BusDeviceReset", 14) == 0)
	{
	  Strings += 14;
	  memset(CommandLineEntry->ErrorRecoveryOption,
		 BusLogic_ErrorRecoveryBusDeviceReset,
		 sizeof(CommandLineEntry->ErrorRecoveryOption));
	}
      else if (strncmp(Strings, "None", 4) == 0)
	{
	  Strings += 4;
	  memset(CommandLineEntry->ErrorRecoveryOption,
		 BusLogic_ErrorRecoveryNone,
		 sizeof(CommandLineEntry->ErrorRecoveryOption));
	}
      else
	for (TargetID = 0; TargetID < BusLogic_MaxTargetIDs; TargetID++)
	  switch (*Strings++)
	    {
	    case 'D':
	      CommandLineEntry->ErrorRecoveryOption[TargetID] =
		BusLogic_ErrorRecoveryDefault;
	      break;
	    case 'H':
	      CommandLineEntry->ErrorRecoveryOption[TargetID] =
		BusLogic_ErrorRecoveryHardReset;
	      break;
	    case 'B':
	      CommandLineEntry->ErrorRecoveryOption[TargetID] =
		BusLogic_ErrorRecoveryBusDeviceReset;
	      break;
	    case 'N':
	      CommandLineEntry->ErrorRecoveryOption[TargetID] =
		BusLogic_ErrorRecoveryNone;
	      break;
	    default:
	      Strings--;
	      TargetID = BusLogic_MaxTargetIDs;
	      break;
	    }
    }
  if (*Strings != '\0')
    printk("BusLogic: Unexpected Command Line String '%s' ignored\n", Strings);
}


/*
  Include Module support if requested.
*/


#ifdef MODULE

SCSI_Host_Template_T driver_template = BUSLOGIC;

#include "scsi_module.c"

#endif
