/* 
 * Mach Operating System
 * Copyright (c) 1992,1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Bootstrap the various built-in servers.
 */

#include <mach.h>
#include <mach/message.h>

#include <file_io.h>
#include "translate_root.h"

/*
 *	Use 8 Kbyte stacks instead of the default 64K.
 *	Use 4 Kbyte waiting stacks instead of the default 8K.
 */
#if	defined(alpha)
vm_size_t	cthread_stack_size = 16 * 1024;
#else
vm_size_t	cthread_stack_size = 8 * 1024;
#endif

extern
vm_size_t	cthread_wait_stack_size;

mach_port_t	bootstrap_master_device_port;	/* local name */
mach_port_t	bootstrap_master_host_port;	/* local name */

int	boot_load_program();

char	*root_name;
char	server_dir_name[MAXPATHLEN] = "mach_servers";

char	*startup_name = "startup";

extern void	default_pager();
extern void	default_pager_initialize();
extern void	default_pager_setup();

/* initialized in default_pager_initialize */
extern mach_port_t default_pager_exception_port;
extern mach_port_t default_pager_bootstrap_port;

/*
 * Convert ASCII to integer.
 */
int atoi(str)
	register const char *str;
{
	register int	n;
	register int	c;
	int	is_negative = 0;

	n = 0;
	while (*str == ' ')
	    str++;
	if (*str == '-') {
	    is_negative = 1;
	    str++;
	}
	while ((c = *str++) >= '0' && c <= '9') {
	    n = n * 10 + (c - '0');
	}
	if (is_negative)
	    n = -n;
	return (n);
}

__main ()
{
}

/*
 * Bootstrap task.
 * Runs in user spacep.
 *
 * Called as 'boot -switches host_port device_port root_name'
 *
 */
main(argc, argv)
	int	argc;
	char	**argv;
{
	register kern_return_t	result;
	task_t			user_task;
	thread_t		user_thread;

	task_t			my_task = mach_task_self();

	char			*flag_string;

	boolean_t		ask_server_dir = FALSE;

	static char		new_root[16];

	/*
	 * Use 4Kbyte cthread wait stacks.
	 */
	cthread_wait_stack_size = 4 * 1024;

	/*
	 * Parse the arguments.
	 */
	if (argc < 5)
	    panic("Not enough arguments");

	/*
	 * Arg 0 is program name
	 */

	/*
	 * Arg 1 is flags
	 */
	if (argv[1][0] != '-') {
	    panic("No flags");
}	
	flag_string = argv[1];

	/*
	 * Arg 2 is host port number
	 */
	bootstrap_master_host_port = atoi(argv[2]);

	/*
	 * Arg 3 is device port number
	 */
	bootstrap_master_device_port = atoi(argv[3]);
 
	/*
	 * Arg 4 is root name
	 */
	root_name = argv[4];

	printf_init(bootstrap_master_device_port);
	panic_init(bootstrap_master_host_port);

	if (root_name[0] == '\0') 
                root_name = DEFAULT_ROOT;

	/*
	 * If the '-a' (ask) switch was specified, ask for
	 * the root device.
	 */

	if (index(flag_string, 'a')) {
		printf("root device? [ %s ] ", root_name);
                safe_gets(new_root, sizeof(new_root));
	}
	
	if (new_root[0] == '\0')
		strcpy(new_root, root_name);

	root_name = translate_root(new_root);


	(void) strbuild(server_dir_name,
			"/dev/",
			root_name,
			"/mach_servers",
			(char *)0);

	/*
	 * If the '-q' (query) switch was specified, ask for the
	 * server directory.
	 */

	if (index(flag_string, 'q'))
	    ask_server_dir = TRUE;

	while (TRUE) {

	    struct file	f;

	    if (ask_server_dir) {
		char new_server_dir[MAXPATHLEN];

		printf("Server directory? [ %s ] ",
			server_dir_name);
		safe_gets(new_server_dir, sizeof(new_server_dir));
		if (new_server_dir[0] != '\0')
		    strcpy(server_dir_name, new_server_dir);
	    }

	    result = open_file(bootstrap_master_device_port,
			       server_dir_name,
			       &f);
	    if (result != 0) {
		printf("Can't open server directory %s: %d\n",
			server_dir_name,
			result);
		ask_server_dir = TRUE;
		continue;
	    }
	    if (!file_is_directory(&f)) {
		printf("%s is not a directory\n",
			server_dir_name);
		ask_server_dir = TRUE;
		continue;
	    }
	    /*
	     * Found server directory.
	     */
	    close_file(&f);
	    break;
	}

	/*
	 * If the server directory name was changed,
	 * then use the new device name as the root device.
	 */
	{
		char *dev, *end;
		int len;

		dev = server_dir_name;
		if (strncmp(dev, "/dev/", 5) == 0)
			dev += 5;
		end = strchr(dev, '/');
		len = end ? end-dev : strlen(dev);
		memcpy(root_name, dev, len);
		root_name[len] = 0;
	}

	/*
	 * Set up the default pager.
	 */
	partition_init();

	default_pager_setup(bootstrap_master_device_port,
			    server_dir_name);

	default_pager_initialize(bootstrap_master_host_port);

	/*
	 * task_set_exception_port and task_set_bootstrap_port
	 * both require a send right.
	 */
	(void) mach_port_insert_right(my_task, default_pager_bootstrap_port,
				      default_pager_bootstrap_port,
				      MACH_MSG_TYPE_MAKE_SEND);
	(void) mach_port_insert_right(my_task, default_pager_exception_port,
				      default_pager_exception_port,
				      MACH_MSG_TYPE_MAKE_SEND);

	/*
	 * Change our exception port.
	 */
	(void) task_set_exception_port(my_task, default_pager_exception_port);

	/*
	 * Create the user task and thread to run the startup file.
	 */
	result = task_create(my_task, FALSE, &user_task);
	if (result != KERN_SUCCESS)
	    panic("task_create %d", result);

	(void) task_set_exception_port(user_task,
				       default_pager_exception_port);
	(void) task_set_bootstrap_port(user_task,
				       default_pager_bootstrap_port);

	result = thread_create(user_task, &user_thread);
	if (result != KERN_SUCCESS)
	    panic("thread_create %d", result);

	/*
	 *	Deallocate the excess send rights.
	 */
	(void) mach_port_deallocate(my_task, default_pager_exception_port);
	(void) mach_port_deallocate(my_task, default_pager_bootstrap_port);

	/*
	 * Load the startup file.
	 * Pass it a command line of
	 * "startup -boot_flags root_name server_dir_name"
	 */
	result = boot_load_program(bootstrap_master_host_port,
				   bootstrap_master_device_port,
				   user_task,
				   user_thread,
				   server_dir_name,
				   startup_name,
				   flag_string,
				   root_name,
				   server_dir_name,
				   (char *)0);
	if (result != 0)
	    panic("boot_load_program %d", result);

	/*
	 * Start up the thread
	 */
	result = thread_resume(user_thread);
	if (result != KERN_SUCCESS)
	    panic("thread_resume %d", result);

	(void) mach_port_deallocate(my_task, user_task);
	(void) mach_port_deallocate(my_task, user_thread);

	{
	    /*
	     * Delete the old stack (containing only the arguments).
	     */
	    vm_offset_t	addr = (vm_offset_t) argv;

	    vm_offset_t		r_addr;
	    vm_size_t		r_size;
	    vm_prot_t		r_protection, r_max_protection;
	    vm_inherit_t	r_inheritance;
	    boolean_t		r_is_shared;
	    memory_object_name_t r_object_name;
	    vm_offset_t		r_offset;
	    kern_return_t	kr;

	    r_addr = addr;

	    kr = vm_region(my_task,
			&r_addr,
			&r_size,
			&r_protection,
			&r_max_protection,
			&r_inheritance,
			&r_is_shared,
			&r_object_name,
			&r_offset);
	    if ((kr == KERN_SUCCESS) && MACH_PORT_VALID(r_object_name))
		(void) mach_port_deallocate(my_task, r_object_name);
	    if ((kr == KERN_SUCCESS) &&
		(r_addr <= addr) &&
		((r_protection & (VM_PROT_READ|VM_PROT_WRITE)) ==
					(VM_PROT_READ|VM_PROT_WRITE)))
		(void) vm_deallocate(my_task, r_addr, r_size);
	}

	/*
	 * Become the default pager
	 */
	default_pager();
	/*NOTREACHED*/
}
