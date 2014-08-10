/*
 * Mach Operating System
 * Copyright (c) 1992-1989 Carnegie Mellon University.
 * Copyright (c) 1995-1993 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
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
 * Bootstrap the various built-in servers.
 */
#include <mach_kdb.h>
#include <bootstrap_symbols.h>

#include <mach/port.h>
#include <mach/message.h>
#include "vm_param.h"
#include <ipc/ipc_port.h>
#include <kern/host.h>
#include <kern/strings.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <vm/vm_kern.h>
#include <device/device_port.h>

#include <sys/varargs.h>

#include <mach/machine/multiboot.h>
#include <mach/exec/exec.h>

#if	MACH_KDB
#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#endif


static mach_port_t	boot_device_port;	/* local name */
static mach_port_t	boot_host_port;		/* local name */

extern struct multiboot_info *boot_info;
extern char *kernel_cmdline;

static void user_bootstrap();	/* forward */
static void bootstrap_exec(void *exec_data);

static mach_port_t
task_insert_send_right(
	task_t task,
	ipc_port_t port)
{
	mach_port_t name;

	for (name = 1;; name++) {
		kern_return_t kr;

		kr = mach_port_insert_right(task->itk_space, name,
			    (ipc_object_t)port, MACH_MSG_TYPE_PORT_SEND);
		if (kr == KERN_SUCCESS)
			break;
		assert(kr == KERN_NAME_EXISTS);
	}

	return name;
}

void bootstrap_create()
{
	struct multiboot_module *bmod;

	if (!(boot_info->flags & MULTIBOOT_MODS)
	    || (boot_info->mods_count == 0))
		panic("No bootstrap code loaded with the kernel!");
	if (boot_info->mods_count > 1)
		printf("Warning: only one boot module currently used by Mach\n");
	bmod = (struct multiboot_module *)phystokv(boot_info->mods_addr);
	bootstrap_exec((void*)phystokv(bmod->mod_start));

	/* XXX at this point, we could free all the memory used
	   by the boot modules and the boot loader's descriptors and such.  */
}

/* XXX won't work with more than one bootstrap service */
static void *boot_exec;

static void
bootstrap_exec(void *e)
{
	task_t		bootstrap_task;
	thread_t	bootstrap_thread;

	/*
	 * Create the bootstrap task.
	 */

	(void) task_create(TASK_NULL, FALSE, &bootstrap_task);
	(void) thread_create(bootstrap_task, &bootstrap_thread);

	/*
	 * Insert send rights to the master host and device ports.
	 */

	boot_host_port =
		task_insert_send_right(bootstrap_task,
			ipc_port_make_send(realhost.host_priv_self));

	boot_device_port =
		task_insert_send_right(bootstrap_task,
			ipc_port_make_send(master_device_port));

	/*
	 * Start the bootstrap thread.
	 */
	boot_exec = e;
	thread_start(bootstrap_thread, user_bootstrap);
	(void) thread_resume(bootstrap_thread);
}

/*
 * The following code runs as the kernel mode portion of the
 * first user thread.
 */

/*
 * Convert an unsigned integer to its decimal representation.
 */
static void
itoa(
	char		*str,
	vm_size_t	num)
{
	char	buf[sizeof(vm_size_t)*2+3];
	register char *np;

	np = buf + sizeof(buf);
	*--np = 0;

	do {
	    *--np = '0' + num % 10;
	    num /= 10;
	} while (num != 0);

	strcpy(str, np);
}

/*
 * Collect the boot flags into a single argument string,
 * for compatibility with existing bootstrap and startup code.
 * Format as a standard flag argument: '-qsdn...'
 */
static void get_compat_strings(char *flags_str, char *root_str)
{
	register char *ip, *cp;

	cp = flags_str;
	*cp++ = '-';

	for (ip = kernel_cmdline; *ip; )
	{
		if (*ip == ' ')
		{
			ip++;
		}
		else if (*ip == '-')
		{
			ip++;
			while (*ip > ' ')
				*cp++ = *ip++;
		}
		else if (strncmp(ip, "root=", 5) == 0)
		{
			char *rp = root_str;

			ip += 5;
			if (strncmp(ip, "/dev/", 5) == 0)
				ip += 5;
			while (*ip > ' ')
				*rp++ = *ip++;
			*rp = '\0';
		}
		else
		{
			while (*ip > ' ')
				ip++;
		}
	}

	if (cp == &flags_str[1])	/* no flags */
	    *cp++ = 'x';
	*cp = '\0';
}

/*
 * Copy boot_data (executable) to the user portion of this task.
 */
static boolean_t	load_protect_text = TRUE;
#if MACH_KDB
		/* if set, fault in the text segment */
static boolean_t	load_fault_in_text = TRUE;
#endif

static vm_offset_t
boot_map(
	void *		data,	/* private data */
	vm_offset_t	offset)	/* offset to map */
{
	vm_offset_t	start_offset = (vm_offset_t) data;

	return pmap_extract(kernel_pmap, start_offset + offset);
}


#if BOOTSTRAP_SYMBOLS
static boolean_t load_bootstrap_symbols = TRUE;
#else
static boolean_t load_bootstrap_symbols = FALSE;
#endif



static int boot_read(void *handle, vm_offset_t file_ofs, void *buf, vm_size_t size,
		     vm_size_t *out_actual)
{
	memcpy(buf, handle + file_ofs, size);
	*out_actual = size;
	return 0;
}

static int read_exec(void *handle, vm_offset_t file_ofs, vm_size_t file_size,
		     vm_offset_t mem_addr, vm_size_t mem_size,
		     exec_sectype_t sec_type)
{
	vm_map_t user_map = current_task()->map;
	vm_offset_t start_page, end_page;
	vm_prot_t mem_prot = sec_type & EXEC_SECTYPE_PROT_MASK;
	int err;

	if (!(sec_type & EXEC_SECTYPE_ALLOC))
		return 0;

	assert(mem_size > 0);
	assert(mem_size >= file_size);

	start_page = trunc_page(mem_addr);
	end_page = round_page(mem_addr + mem_size);

	/*
	printf("reading bootstrap section %08x-%08x-%08x prot %d pages %08x-%08x\n",
		mem_addr, mem_addr+file_size, mem_addr+mem_size, mem_prot, start_page, end_page);
	*/

	err = vm_allocate(user_map, &start_page, end_page - start_page, FALSE);
	assert(err == 0);
	assert(start_page == trunc_page(mem_addr));

	if (file_size > 0)
	{
		err = copyout(handle + file_ofs, mem_addr, file_size);
		assert(err == 0);
	}

	if (mem_prot != VM_PROT_ALL)
	{
		err = vm_protect(user_map, start_page, end_page - start_page, FALSE, mem_prot);
		assert(err == 0);
	}
}

static void copy_bootstrap(void *e, struct exec_info *boot_exec_info)
{
	register vm_map_t	user_map = current_task()->map;
	int err;

printf("loading...\n");
	if (err = exec_load(boot_read, read_exec, e, boot_exec_info))
		panic("Cannot load user-bootstrap image: error code %d", err);

#if	MACH_KDB
	/*
	 * Enter the bootstrap symbol table.
	 */

#if 0 /*XXX*/
	if (load_bootstrap_symbols)
	(void) X_db_sym_init(
		(char*) boot_start+lp->sym_offset,
		(char*) boot_start+lp->sym_offset+lp->sym_size,
		"bootstrap",
		(char *) user_map);
#endif

#if 0 /*XXX*/
	if (load_fault_in_text)
	  {
	    vm_offset_t lenp = round_page(lp->text_start+lp->text_size) -
	      		     trunc_page(lp->text_start);
	    vm_offset_t i = 0;

	    while (i < lenp)
	      {
		vm_fault(user_map, text_page_start +i, 
		        load_protect_text ?  
			 VM_PROT_READ|VM_PROT_EXECUTE :
			 VM_PROT_READ|VM_PROT_EXECUTE | VM_PROT_WRITE,
			 0,0,0);
		i = round_page (i+1);
	      }
	  }
#endif
#endif	MACH_KDB
}

/*
 * Allocate the stack, and build the argument list.
 */
extern vm_offset_t	user_stack_low();
extern vm_offset_t	set_user_regs();

void
static build_args_and_stack(boot_exec_info, va_alist)
	struct exec_info *boot_exec_info;
	va_dcl
{
	vm_offset_t	stack_base;
	vm_size_t	stack_size;
	va_list		argv_ptr;
	register
	char *		arg_ptr;
	int		arg_len;
	int		arg_count;
	register
	char *		arg_pos;
	int		arg_item_len;
	char *		string_pos;
	char *		zero = (char *)0;

#define	STACK_SIZE	(64*1024)

	/*
	 * Calculate the size of the argument list.
	 */
	va_start(argv_ptr);
	arg_len = 0;
	arg_count = 0;
	for (;;) {
	    arg_ptr = va_arg(argv_ptr, char *);
	    if (arg_ptr == 0)
		break;
	    arg_count++;
	    arg_len += strlen(arg_ptr) + 1;
	}
	va_end(argv_ptr);

	/*
	 * Add space for:
	 *	arg count
	 *	pointers to arguments
	 *	trailing 0 pointer
	 *	dummy 0 pointer to environment variables
	 *	and align to integer boundary
	 */
	arg_len += sizeof(integer_t)
		 + (2 + arg_count) * sizeof(char *);
	arg_len = (arg_len + sizeof(integer_t) - 1) & ~(sizeof(integer_t)-1);

	/*
	 * Allocate the stack.
	 */
	stack_size = round_page(STACK_SIZE);
	stack_base = user_stack_low(stack_size);
	(void) vm_allocate(current_task()->map,
			&stack_base,
			stack_size,
			FALSE);

	arg_pos = (char *)
		set_user_regs(stack_base, stack_size, boot_exec_info, arg_len);

	/*
	 * Start the strings after the arg-count and pointers
	 */
	string_pos = arg_pos
		+ sizeof(integer_t)
		+ arg_count * sizeof(char *)
		+ 2 * sizeof(char *);

	/*
	 * first the argument count
	 */
	(void) copyout((char *)&arg_count,
			arg_pos,
			sizeof(integer_t));
	arg_pos += sizeof(integer_t);

	/*
	 * Then the strings and string pointers for each argument
	 */
	va_start(argv_ptr);
	while (--arg_count >= 0) {
	    arg_ptr = va_arg(argv_ptr, char *);
	    arg_item_len = strlen(arg_ptr) + 1; /* include trailing 0 */

	    /* set string pointer */
	    (void) copyout((char *)&string_pos,
			arg_pos,
			sizeof (char *));
	    arg_pos += sizeof(char *);

	    /* copy string */
	    (void) copyout(arg_ptr, string_pos, arg_item_len);
	    string_pos += arg_item_len;
	}
	va_end(argv_ptr);

	/*
	 * last, the trailing 0 argument and a null environment pointer.
	 */
	(void) copyout((char *)&zero, arg_pos, sizeof(char *));
	arg_pos += sizeof(char *);
	(void) copyout((char *)&zero, arg_pos, sizeof(char *));
}

static void user_bootstrap()
{
	struct exec_info boot_exec_info;

	char	host_string[12];
	char	device_string[12];
	char	flag_string[12];
	char	root_string[12];

	/*
	 * Copy the bootstrap code from boot_exec into the user task.
	 */
	copy_bootstrap(boot_exec, &boot_exec_info);

	/*
	 * Convert the host and device ports to strings,
	 * to put in the argument list.
	 */
	itoa(host_string, boot_host_port);
	itoa(device_string, boot_device_port);

	/*
	 * Get the (compatibility) boot flags and root name strings.
	 */
	get_compat_strings(flag_string, root_string);

	/*
	 * Build the argument list and insert in the user task.
	 * Argument list is
	 * "bootstrap -<boothowto> <host_port> <device_port> <root_name>"
	 */
	build_args_and_stack(&boot_exec_info,
			"bootstrap",
			flag_string,
			host_string,
			device_string,
			root_string,
			(char *)0);

printf("Starting bootstrap at %x\n", boot_exec_info.entry);

	/*
	 * Exit to user thread.
	 */
	thread_bootstrap_return();
	/*NOTREACHED*/
}

