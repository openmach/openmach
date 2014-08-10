/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef STUB
#include <atm.h>
#else
#include "atm.h"
#endif

#if NATM > 0

#ifndef STUB
#include <sys/types.h>
#include <kern/thread.h>
#include <kern/lock.h>
#include <kern/eventcount.h>
#include <machine/machspl.h>		/* spl definitions */
#include <mips/mips_cpu.h>
#include <vm/vm_kern.h>
#include <device/io_req.h>
#include <device/device_types.h>
#include <device/net_status.h>
#include <chips/busses.h>
#include <chips/nc.h>
#include <chips/tca100.h>
#include <chips/tca100_if.h>

decl_simple_lock_data(, atm_simple_lock);

#else
#include "stub.h"
#include "nc.h"
#include "tca100_if.h"
#include "tca100.h"

int atm_simple_lock;

#endif

struct bus_device *atm_info[NATM];

int atm_probe();
void atm_attach();
struct bus_driver atm_driver =
       { atm_probe, 0, atm_attach, 0, /* csr */ 0, "atm", atm_info,
       	 "", 0, /* flags */ 0 };	       

atm_device_t atmp[NATM] = {NULL};
u_int atm_open_count[NATM];
u_int atm_mapped[NATM];
u_int atm_control_mask[NATM];
struct evc atm_event_counter[NATM];

#define DEVICE(unit) ((unit == 0) ? NW_TCA100_1 : NW_TCA100_2)

void atm_initialize(int unit) {

  atmp[unit]->creg = (CR_RX_RESET | CR_TX_RESET);
  atmp[unit]->creg = 0;
  atmp[unit]->rxtimerv = 0;
  atmp[unit]->rxthresh = 1;
  atmp[unit]->txthresh = 0;
  atmp[unit]->sreg = 0;
  atmp[unit]->creg = atm_control_mask[unit] = (CR_RX_ENABLE | CR_TX_ENABLE);
  atm_open_count[unit] = 0;
  atm_mapped[unit] = 0;
}

/*** Device entry points ***/

int atm_probe(vm_offset_t reg, struct bus_device *ui) {
  int un;

  un = ui->unit;
  if (un >= NATM || check_memory(reg, 0))  {
    return 0;
  }

  atm_info[un] = ui;
  atmp[un] = (atm_device_t) reg;
  nc_initialize();
  if (nc_device_register(DEVICE(un), NW_CONNECTION_ORIENTED, (char *) reg,
			 &tca100_entry_table) == NW_SUCCESS &&
      tca100_initialize(DEVICE(un)) == NW_SUCCESS) {
    atm_initialize(un);
    evc_init(&atm_event_counter[un]);
    return 1;
  } else {
    atmp[un] = NULL;
    (void) nc_device_unregister(DEVICE(un), NW_FAILURE);
    return 0;
  }
}

void atm_attach(struct bus_device *ui) {
  int un;

  un = ui->unit;
  if (un >= NATM) {
    printf("atm: stray attach\n");
  } else {
    atmp[un]->creg = 
      atm_control_mask[un] = CR_TX_ENABLE | CR_RX_ENABLE | RX_COUNT_INTR;
                                              /*Enable ATM interrupts*/
  }
}

void atm_intr(int unit, int spl_level) {

  if (unit >= NATM || atmp[unit] == NULL) {
    printf("atm: stray interrupt\n");
  } else {
    atmp[unit]->creg = CR_TX_ENABLE | CR_RX_ENABLE;  /*Disable ATM interrupts*/
    wbflush();
    if (atm_mapped[unit]) {
      splx(spl_level);
      evc_signal(&atm_event_counter[unit]);
    } else {
      simple_lock(&atm_simple_lock);
      tca100_poll(DEVICE(unit));
      atmp[unit]->creg = atm_control_mask[unit];
      simple_unlock(&atm_simple_lock);
      splx(spl_level);
    }
  }
}

io_return_t atm_open(dev_t dev, int mode, io_req_t ior) {
  int un;
	
  un = minor(dev);
  if (un >= NATM || atmp[un] == NULL) {
    return D_NO_SUCH_DEVICE;
/*
  } else if (atm_open_count[un] > 0 && (atm_mapped[un] || (mode & D_WRITE))) {
    return D_ALREADY_OPEN;
*/
  } else {
    atm_open_count[un]++;
    atm_mapped[un] = ((mode & D_WRITE) != 0);
    if (atm_mapped[un])
      (void) nc_device_unregister(DEVICE(un), NW_NOT_SERVER);
    return D_SUCCESS;
  }
}

io_return_t atm_close(dev_t dev) {
  int un;

  un = minor(dev);
  if (un >= NATM || atmp[un] == NULL) {
    return D_NO_SUCH_DEVICE;
  } else if (atm_open_count[un] == 0) {
    return D_INVALID_OPERATION;
  } else {
    if (atm_mapped[un]) {
      (void) nc_device_register(DEVICE(un), NW_CONNECTION_ORIENTED,
				(char *) atmp[un],
				&tca100_entry_table);
      atm_mapped[un] = 0;
    }
    atm_open_count[un]--;
    return D_SUCCESS;
  }
}

unsigned int *frc = 0xbe801000;
char data[66000];

io_return_t atm_read(dev_t dev, io_req_t ior) {
  unsigned int ck1, ck2;
  int i, j;
  char c[16];

  ck1 = *frc;
  device_read_alloc(ior, ior->io_count);
  for (i = 0, j = 0; i < ior->io_count; i += 4096, j++)
    c[j] = (ior->io_data)[i];
  ck2 = *frc;
  ((int *) ior->io_data)[0] = ck1;
  ((int *) ior->io_data)[1] = ck2;
  return D_SUCCESS;
}

io_return_t atm_write(dev_t dev, io_req_t ior) {
  int i, j;
  char c[16];
  boolean_t wait;

  device_write_get(ior, &wait);
  for (i = 0, j = 0; i < ior->io_total; i += 4096, j++)
    c[j] = (ior->io_data)[i];
  ior->io_residual = ior->io_total - *frc;
  return D_SUCCESS;
}

io_return_t atm_get_status(dev_t dev, int flavor, dev_status_t status,
			   u_int *status_count) {
  int un;

  un = minor(dev);
  if (un >= NATM || atmp[un] == NULL) {
    return D_NO_SUCH_DEVICE;
  } else {
    switch ((atm_status) flavor) {
    case ATM_MAP_SIZE:
      status[0] = sizeof(atm_device_s);
      *status_count = sizeof(int);
      return D_SUCCESS;
    case ATM_MTU_SIZE:
      status[0] = 65535;   /*MTU size*/
      *status_count = sizeof(int);
      return D_SUCCESS;
    case ATM_EVC_ID:
      status[0] = atm_event_counter[un].ev_id;
      *status_count = sizeof(int);
      return D_SUCCESS;
    case ATM_ASSIGNMENT:
      status[0] = atm_mapped[un];
      status[1] = atm_open_count[un];
      *status_count = 2 * sizeof(int);
      return D_SUCCESS;
    default:
      return D_INVALID_OPERATION;
    }
  }
}

io_return_t atm_set_status(dev_t dev, int flavor, dev_status_t status,
			   u_int status_count) {
  io_return_t rc;
  int un, s;
  nw_pvc_t pvcp;
  nw_plist_t pel;
  nw_ep lep;

  un = minor(dev);
  if (un >= NATM || atmp[un] == NULL) {
    return D_NO_SUCH_DEVICE;
  } else switch ((atm_status) flavor) {
  case ATM_INITIALIZE:
    if (status_count != 0) {
      return D_INVALID_OPERATION;
    } else {
      s = splsched();
      if (nc_device_register(DEVICE(un), NW_CONNECTION_ORIENTED,
			     (char *) atmp[un],
			     &tca100_entry_table) == NW_SUCCESS &&
	  tca100_initialize(DEVICE(un)) == NW_SUCCESS) {
	atm_initialize(un);
	rc = D_SUCCESS;
      } else {
	atmp[un] = NULL;
	(void) nc_device_unregister(DEVICE(un), NW_FAILURE);
	rc = D_INVALID_OPERATION;
      }
      splx(s);
      return rc;
    }
    break;

#if PERMANENT_VIRTUAL_CONNECTIONS
  case ATM_PVC_SET:
    pvcp = (nw_pvc_t) status;
    if (status_count != sizeof(nw_pvc_s) || pvcp->pvc.local_ep >= MAX_EP) {
      rc = D_INVALID_OPERATION;
    } else if ((pel = nc_peer_allocate()) == NULL) {
      rc = D_INVALID_OPERATION;
    } else {
      lep = pvcp->pvc.local_ep;
      tct[lep].rx_sar_header = SSM | 1;
      tct[lep].tx_atm_header = pvcp->tx_vp << ATM_VPVC_SHIFT;
      tct[lep].tx_sar_header = 1;
      ect[lep].state = NW_DUPLEX_ACCEPTED;
      pel->peer = pvcp->pvc;
      pel->next = NULL;
      ect[lep].conn = pel;
      if (pvcp->protocol == NW_LINE) {
	if (nc_line_update(&pel->peer, lep) == NW_SUCCESS) {
	  ect[lep].protocol = pvcp->protocol;
	  if (nw_free_line_last == 0)
	    nw_free_line_first = lep;
	  else
	    ect[nw_free_line_last].next = lep;
	  ect[lep].previous = nw_free_line_last;
	  ect[lep].next = 0;
	  nw_free_line_last = lep;
	  rc = D_SUCCESS;
	} else {
	  rc = D_INVALID_OPERATION;
	}
      } else {
	rc = D_SUCCESS;
      }
    }
    return rc;
#endif

  default:
    return D_INVALID_OPERATION;
  }
}

int atm_mmap(dev_t dev, vm_offset_t off, int prot) {
  int un;
  vm_offset_t addr;

  un = minor(dev);
  if (un >= NATM || atmp[un] == NULL || !atm_mapped[un] ||
      off >= sizeof(atm_device_s)) {
    return -1;
  } else {
    return mips_btop(K1SEG_TO_PHYS( (vm_offset_t) atmp[un] ) + off );
  }
}

io_return_t atm_restart(int u) {

  return D_INVALID_OPERATION;
}

io_return_t atm_setinput(dev_t dev, ipc_port_t receive_port, int priority,
			 filter_array_t *filter, u_int filter_count) {

  return D_INVALID_OPERATION;
}

int atm_portdeath(dev_t dev, mach_port_t port) {

  return D_INVALID_OPERATION;
}


#endif NATM > 0



