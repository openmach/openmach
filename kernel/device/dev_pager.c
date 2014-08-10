/* 
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
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
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	3/89
 *
 * 	Device pager.
 */
#include <norma_vm.h>

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <kern/queue.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>

#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#include <device/device_types.h>
#include <device/ds_routines.h>
#include <device/dev_hdr.h>
#include <device/io_req.h>

extern vm_offset_t	block_io_mmap();	/* dummy routine to allow
						   mmap for block devices */

/*
 *	The device pager routines are called directly from the message
 *	system (via mach_msg), and thus run in the kernel-internal
 *	environment.  All ports are in internal form (ipc_port_t),
 *	and must be correctly reference-counted in order to be saved
 *	in other data structures.  Kernel routines may be called
 *	directly.  Kernel types are used for data objects (tasks,
 *	memory objects, ports).  The only IPC routines that may be
 *	called are ones that masquerade as the kernel task (via
 *	msg_send_from_kernel).
 *
 *	Port rights and references are maintained as follows:
 *		Memory object port:
 *			The device_pager task has all rights.
 *		Memory object control port:
 *			The device_pager task has only send rights.
 *		Memory object name port:
 *			The device_pager task has only send rights.
 *			The name port is not even recorded.
 *	Regardless how the object is created, the control and name
 *	ports are created by the kernel and passed through the memory
 *	management interface.
 *
 *	The device_pager assumes that access to its memory objects
 *	will not be propagated to more that one host, and therefore
 *	provides no consistency guarantees beyond those made by the
 *	kernel.
 *
 *	In the event that more than one host attempts to use a device
 *	memory object, the device_pager will only record the last set
 *	of port names.  [This can happen with only one host if a new
 *	mapping is being established while termination of all previous
 *	mappings is taking place.]  Currently, the device_pager assumes
 *	that its clients adhere to the initialization and termination
 *	protocols in the memory management interface; otherwise, port
 *	rights or out-of-line memory from erroneous messages may be
 *	allowed to accumulate.
 *
 *	[The phrase "currently" has been used above to denote aspects of
 *	the implementation that could be altered without changing the rest
 *	of the basic documentation.]
 */

/*
 * Basic device pager structure.
 */
struct dev_pager {
	decl_simple_lock_data(, lock)	/* lock for reference count */
	int		ref_count;	/* reference count */
	int		client_count;	/* How many memory_object_create
					 * calls have we received */
	ipc_port_t	pager;		/* pager port */
	ipc_port_t	pager_request;	/* Known request port */
	ipc_port_t	pager_name;	/* Known name port */
	mach_device_t	device;		/* Device handle */
	int		type;		/* to distinguish */
#define DEV_PAGER_TYPE	0
#define CHAR_PAGER_TYPE	1
	/* char pager specifics */
	int		prot;
	vm_size_t	size;
};
typedef struct dev_pager *dev_pager_t;
#define	DEV_PAGER_NULL	((dev_pager_t)0)


zone_t		dev_pager_zone;

void dev_pager_reference(register dev_pager_t	ds)
{
	simple_lock(&ds->lock);
	ds->ref_count++;
	simple_unlock(&ds->lock);
}

void dev_pager_deallocate(register dev_pager_t	ds)
{
	simple_lock(&ds->lock);
	if (--ds->ref_count > 0) {
	    simple_unlock(&ds->lock);
	    return;
	}

	simple_unlock(&ds->lock);
	zfree(dev_pager_zone, (vm_offset_t)ds);
}

/*
 * A hash table of ports for device_pager backed objects.
 */

#define	DEV_PAGER_HASH_COUNT		127

struct dev_pager_entry {
	queue_chain_t	links;
	ipc_port_t	name;
	dev_pager_t	pager_rec;
};
typedef struct dev_pager_entry *dev_pager_entry_t;

queue_head_t	dev_pager_hashtable[DEV_PAGER_HASH_COUNT];
zone_t		dev_pager_hash_zone;
decl_simple_lock_data(,
		dev_pager_hash_lock)

#define	dev_pager_hash(name_port) \
		(((natural_t)(name_port) & 0xffffff) % DEV_PAGER_HASH_COUNT)

void dev_pager_hash_init(void)
{
	register int	i;
	register vm_size_t	size;

	size = sizeof(struct dev_pager_entry);
	dev_pager_hash_zone = zinit(
				size,
				size * 1000,
				PAGE_SIZE,
				FALSE,
				"dev_pager port hash");
	for (i = 0; i < DEV_PAGER_HASH_COUNT; i++)
	    queue_init(&dev_pager_hashtable[i]);
	simple_lock_init(&dev_pager_hash_lock);
}

void dev_pager_hash_insert(
	ipc_port_t	name_port,
	dev_pager_t	rec)
{
	register dev_pager_entry_t new_entry;

	new_entry = (dev_pager_entry_t) zalloc(dev_pager_hash_zone);
	new_entry->name = name_port;
	new_entry->pager_rec = rec;

	simple_lock(&dev_pager_hash_lock);
	queue_enter(&dev_pager_hashtable[dev_pager_hash(name_port)],
			new_entry, dev_pager_entry_t, links);
	simple_unlock(&dev_pager_hash_lock);
}

void dev_pager_hash_delete(ipc_port_t	name_port)
{
	register queue_t	bucket;
	register dev_pager_entry_t	entry;

	bucket = &dev_pager_hashtable[dev_pager_hash(name_port)];

	simple_lock(&dev_pager_hash_lock);
	for (entry = (dev_pager_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_pager_entry_t)queue_next(&entry->links)) {
	    if (entry->name == name_port) {
		queue_remove(bucket, entry, dev_pager_entry_t, links);
		break;
	    }
	}
	simple_unlock(&dev_pager_hash_lock);
	if (entry)
	    zfree(dev_pager_hash_zone, (vm_offset_t)entry);
}

dev_pager_t dev_pager_hash_lookup(ipc_port_t	name_port)
{
	register queue_t	bucket;
	register dev_pager_entry_t	entry;
	register dev_pager_t	pager;

	bucket = &dev_pager_hashtable[dev_pager_hash(name_port)];

	simple_lock(&dev_pager_hash_lock);
	for (entry = (dev_pager_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_pager_entry_t)queue_next(&entry->links)) {
	    if (entry->name == name_port) {
		pager = entry->pager_rec;
		dev_pager_reference(pager);
		simple_unlock(&dev_pager_hash_lock);
		return (pager);
	    }
	}
	simple_unlock(&dev_pager_hash_lock);
	return (DEV_PAGER_NULL);
}

kern_return_t	device_pager_setup(
	mach_device_t	device,
	int		prot,
	vm_offset_t	offset,
	vm_size_t	size,
	mach_port_t	*pager)
{
	register dev_pager_t	d;

	/*
	 *	Verify the device is indeed mappable
	 */
	if (!device->dev_ops->d_mmap || (device->dev_ops->d_mmap == nomap))
		return (D_INVALID_OPERATION);

	/*
	 *	Allocate a structure to hold the arguments
	 *	and port to represent this object.
	 */

	d = dev_pager_hash_lookup((ipc_port_t)device);	/* HACK */
	if (d != DEV_PAGER_NULL) {
		*pager = (mach_port_t) ipc_port_make_send(d->pager);
		dev_pager_deallocate(d);
		return (D_SUCCESS);
	}

	d = (dev_pager_t) zalloc(dev_pager_zone);
	if (d == DEV_PAGER_NULL)
		return (KERN_RESOURCE_SHORTAGE);

	simple_lock_init(&d->lock);
	d->ref_count = 1;

	/*
	 * Allocate the pager port.
	 */
	d->pager = ipc_port_alloc_kernel();
	if (d->pager == IP_NULL) {
		dev_pager_deallocate(d);
		return (KERN_RESOURCE_SHORTAGE);
	}

	d->client_count = 0;
	d->pager_request = IP_NULL;
	d->pager_name = IP_NULL;
	d->device = device;
	mach_device_reference(device);
	d->prot = prot;
	d->size = round_page(size);
	if (device->dev_ops->d_mmap == block_io_mmap) {
		d->type = DEV_PAGER_TYPE;
	} else {
		d->type = CHAR_PAGER_TYPE;
	}

	dev_pager_hash_insert(d->pager, d);
	dev_pager_hash_insert((ipc_port_t)device, d);	/* HACK */

	*pager = (mach_port_t) ipc_port_make_send(d->pager);
	return (KERN_SUCCESS);
}

/*
 *	Routine:	device_pager_release
 *	Purpose:
 *		Relinquish any references or rights that were
 *		associated with the result of a call to
 *		device_pager_setup.
 */
void	device_pager_release(memory_object_t	object)
{
	if (MACH_PORT_VALID(object))
		ipc_port_release_send((ipc_port_t) object);
}

boolean_t	device_pager_debug = FALSE;

boolean_t	device_pager_data_request_done();	/* forward */
boolean_t	device_pager_data_write_done();		/* forward */


kern_return_t	device_pager_data_request(
	ipc_port_t	pager,
	ipc_port_t	pager_request,
	vm_offset_t	offset,
	vm_size_t	length,
	vm_prot_t	protection_required)
{
	register dev_pager_t	ds;

#ifdef lint
	protection_required++;
#endif lint

	if (device_pager_debug)
		printf("(device_pager)data_request: pager=%d, offset=0x%x, length=0x%x\n",
			pager, offset, length);

	ds = dev_pager_hash_lookup((ipc_port_t)pager);
	if (ds == DEV_PAGER_NULL)
		panic("(device_pager)data_request: lookup failed");

	if (ds->pager_request != pager_request)
		panic("(device_pager)data_request: bad pager_request");

	if (ds->type == CHAR_PAGER_TYPE) {
	    register vm_object_t	object;
	    vm_offset_t			device_map_page(void *,vm_offset_t);

#if	NORMA_VM
	    object = vm_object_lookup(pager);
#else	NORMA_VM
	    object = vm_object_lookup(pager_request);
#endif	NORMA_VM
	    if (object == VM_OBJECT_NULL) {
		    (void) r_memory_object_data_error(pager_request,
						      offset, length,
						      KERN_FAILURE);
		    dev_pager_deallocate(ds);
		    return (KERN_SUCCESS);
	    }

	    vm_object_page_map(object,
			       offset, length,
			       device_map_page, (char *)ds);

	    vm_object_deallocate(object);
	}
	else {
	    register io_req_t		ior;
	    register mach_device_t	device;
	    io_return_t			result;

	    panic("(device_pager)data_request: dev pager");
	    
	    device = ds->device;
	    mach_device_reference(device);
	    dev_pager_deallocate(ds);
	    
	    /*
	     * Package the read for the device driver.
	     */
	    io_req_alloc(ior, 0);
	    
	    ior->io_device	= device;
	    ior->io_unit	= device->dev_number;
	    ior->io_op		= IO_READ | IO_CALL;
	    ior->io_mode	= 0;
	    ior->io_recnum	= offset / device->bsize;
	    ior->io_data	= 0;		/* driver must allocate */
	    ior->io_count	= length;
	    ior->io_alloc_size	= 0;		/* no data allocated yet */
	    ior->io_residual	= 0;
	    ior->io_error	= 0;
	    ior->io_done	= device_pager_data_request_done;
	    ior->io_reply_port	= pager_request;
	    ior->io_reply_port_type = MACH_MSG_TYPE_PORT_SEND;
	    
	    result = (*device->dev_ops->d_read)(device->dev_number, ior);
	    if (result == D_IO_QUEUED)
		return (KERN_SUCCESS);
	    
	    /*
	     * Return by queuing IOR for io_done thread, to reply in
	     * correct environment (kernel).
	     */
	    ior->io_error = result;
	    iodone(ior);
	}

	dev_pager_deallocate(ds);

	return (KERN_SUCCESS);
}

/*
 * Always called by io_done thread.
 */
boolean_t device_pager_data_request_done(register io_req_t	ior)
{
	vm_offset_t	start_alloc, end_alloc;
	vm_size_t	size_read;

	if (ior->io_error == D_SUCCESS) {
	    size_read = ior->io_count;
	    if (ior->io_residual) {
		if (device_pager_debug)
		    printf("(device_pager)data_request_done: r: 0x%x\n",ior->io_residual);
		bzero( (char *) (&ior->io_data[ior->io_count - 
					       ior->io_residual]),
		      (unsigned) ior->io_residual);
	    }
	} else {
	    size_read = ior->io_count - ior->io_residual;
	}

	start_alloc = trunc_page((vm_offset_t)ior->io_data);
	end_alloc   = start_alloc + round_page(ior->io_alloc_size);

	if (ior->io_error == D_SUCCESS) {
	    vm_map_copy_t copy;
	    kern_return_t kr;

	    kr = vm_map_copyin(kernel_map, (vm_offset_t)ior->io_data,
				size_read, TRUE, &copy);
	    if (kr != KERN_SUCCESS)
		panic("device_pager_data_request_done");

	    (void) r_memory_object_data_provided(
					ior->io_reply_port,
					ior->io_recnum * ior->io_device->bsize,
					(vm_offset_t)copy,
					size_read,
					VM_PROT_NONE);
	}
	else {
	    (void) r_memory_object_data_error(
					ior->io_reply_port,
					ior->io_recnum * ior->io_device->bsize,
					(vm_size_t)ior->io_count,
					ior->io_error);
	}

	(void)vm_deallocate(kernel_map,
			    start_alloc,
			    end_alloc - start_alloc);
	mach_device_deallocate(ior->io_device);
	return (TRUE);
}

kern_return_t device_pager_data_write(
	ipc_port_t		pager,
	ipc_port_t		pager_request,
	register vm_offset_t	offset,
	register pointer_t	addr,
	vm_size_t		data_count)
{
	register dev_pager_t	ds;
	register mach_device_t	device;
	register io_req_t	ior;
	kern_return_t		result;

	panic("(device_pager)data_write: called");

	ds = dev_pager_hash_lookup((ipc_port_t)pager);
	if (ds == DEV_PAGER_NULL)
		panic("(device_pager)data_write: lookup failed");

	if (ds->pager_request != pager_request)
		panic("(device_pager)data_write: bad pager_request");

	if (ds->type == CHAR_PAGER_TYPE)
		panic("(device_pager)data_write: char pager");

	device = ds->device;
	mach_device_reference(device);
	dev_pager_deallocate(ds);

	/*
	 * Package the write request for the device driver.
	 */
	io_req_alloc(ior, data_count);

	ior->io_device		= device;
	ior->io_unit		= device->dev_number;
	ior->io_op		= IO_WRITE | IO_CALL;
	ior->io_mode		= 0;
	ior->io_recnum		= offset / device->bsize;
	ior->io_data		= (io_buf_ptr_t)addr;
	ior->io_count		= data_count;
	ior->io_alloc_size	= data_count;	/* amount to deallocate */
	ior->io_residual	= 0;
	ior->io_error		= 0;
	ior->io_done		= device_pager_data_write_done;
	ior->io_reply_port	= IP_NULL;

	result = (*device->dev_ops->d_write)(device->dev_number, ior);

	if (result != D_IO_QUEUED) {
	    device_write_dealloc(ior);
	    io_req_free((vm_offset_t)ior);
	    mach_device_deallocate(device);
	}

	return (KERN_SUCCESS);
}

boolean_t device_pager_data_write_done(ior)
	register io_req_t	ior;
{
	device_write_dealloc(ior);
	mach_device_deallocate(ior->io_device);

	return (TRUE);
}

kern_return_t device_pager_copy(
	ipc_port_t		pager,
	ipc_port_t		pager_request,
	register vm_offset_t	offset,
	register vm_size_t	length,
	ipc_port_t		new_pager)
{
	panic("(device_pager)copy: called");
}

kern_return_t
device_pager_supply_completed(
	ipc_port_t pager,
	ipc_port_t pager_request,
	vm_offset_t offset,
	vm_size_t length,
	kern_return_t result,
	vm_offset_t error_offset)
{
	panic("(device_pager)supply_completed: called");
}

kern_return_t
device_pager_data_return(
	ipc_port_t		pager,
	ipc_port_t		pager_request,
	vm_offset_t		offset,
	register pointer_t	addr,
	vm_size_t		data_cnt,
	boolean_t		dirty,
	boolean_t		kernel_copy)
{
	panic("(device_pager)data_return: called");
}

kern_return_t
device_pager_change_completed(
	ipc_port_t pager,
	boolean_t may_cache,
	memory_object_copy_strategy_t copy_strategy)
{
	panic("(device_pager)change_completed: called");
}

/*
 *	The mapping function takes a byte offset, but returns
 *	a machine-dependent page frame number.  We convert
 *	that into something that the pmap module will
 *	accept later.
 */
vm_offset_t device_map_page(
	void		*dsp,
	vm_offset_t	offset)
{
	register dev_pager_t	ds = (dev_pager_t) dsp;

	return pmap_phys_address(
		   (*(ds->device->dev_ops->d_mmap))
			(ds->device->dev_number, offset, ds->prot));
}

kern_return_t device_pager_init_pager(
	ipc_port_t	pager,
	ipc_port_t	pager_request,
	ipc_port_t	pager_name,
	vm_size_t	pager_page_size)
{
	register dev_pager_t	ds;

	if (device_pager_debug)
		printf("(device_pager)init: pager=%d, request=%d, name=%d\n",
		       pager, pager_request, pager_name);

	assert(pager_page_size == PAGE_SIZE);
	assert(IP_VALID(pager_request));
	assert(IP_VALID(pager_name));

	ds = dev_pager_hash_lookup(pager);
	assert(ds != DEV_PAGER_NULL);

	assert(ds->client_count == 0);
	assert(ds->pager_request == IP_NULL);
	assert(ds->pager_name == IP_NULL);

	ds->client_count = 1;

	/*
	 * We save the send rights for the request and name ports.
	 */

	ds->pager_request = pager_request;
	ds->pager_name = pager_name;

	if (ds->type == CHAR_PAGER_TYPE) {
	    /*
	     * Reply that the object is ready
	     */
	    (void) r_memory_object_set_attributes(pager_request,
						TRUE,	/* ready */
						FALSE,	/* do not cache */
						MEMORY_OBJECT_COPY_NONE);
	} else {
	    (void) r_memory_object_set_attributes(pager_request,
						TRUE,	/* ready */
						TRUE,	/* cache */
						MEMORY_OBJECT_COPY_DELAY);
	}

	dev_pager_deallocate(ds);
	return (KERN_SUCCESS);
}

kern_return_t device_pager_terminate(
	ipc_port_t	pager,
	ipc_port_t	pager_request,
	ipc_port_t	pager_name)
{
	register dev_pager_t	ds;

	assert(IP_VALID(pager_request));
	assert(IP_VALID(pager_name));

	ds = dev_pager_hash_lookup(pager);
	assert(ds != DEV_PAGER_NULL);

	assert(ds->client_count == 1);
	assert(ds->pager_request == pager_request);
	assert(ds->pager_name == pager_name);

	dev_pager_hash_delete(ds->pager);
	dev_pager_hash_delete((ipc_port_t)ds->device);	/* HACK */
	mach_device_deallocate(ds->device);

	/* release the send rights we have saved from the init call */

	ipc_port_release_send(pager_request);
	ipc_port_release_send(pager_name);

	/* release the naked receive rights we just acquired */

	ipc_port_release_receive(pager_request);
	ipc_port_release_receive(pager_name);

	/* release the kernel's receive right for the pager port */

	ipc_port_dealloc_kernel(pager);

	/* once for ref from lookup, once to make it go away */
	dev_pager_deallocate(ds);
	dev_pager_deallocate(ds);

	return (KERN_SUCCESS);
}

kern_return_t device_pager_data_unlock(
	ipc_port_t memory_object,
	ipc_port_t memory_control_port,
	vm_offset_t offset,
	vm_size_t length,
	vm_prot_t desired_access)
{
#ifdef	lint
	memory_object++; memory_control_port++; offset++; length++; desired_access++;
#endif	lint

	panic("(device_pager)data_unlock: called");
	return (KERN_FAILURE);
}

kern_return_t device_pager_lock_completed(
	ipc_port_t	memory_object,
	ipc_port_t	pager_request_port,
	vm_offset_t	offset,
	vm_size_t	length)
{
#ifdef	lint
	memory_object++; pager_request_port++; offset++; length++;
#endif	lint

	panic("(device_pager)lock_completed: called");
	return (KERN_FAILURE);
}

void device_pager_init(void)
{
	register vm_size_t	size;

	/*
	 * Initialize zone of paging structures.
	 */
	size = sizeof(struct dev_pager);
	dev_pager_zone = zinit(size,
				(vm_size_t) size * 1000,
				PAGE_SIZE,
				FALSE,
				"device pager structures");

	/*
	 *	Initialize the name port hashing stuff.
	 */
	dev_pager_hash_init();
}
