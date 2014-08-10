#ifndef _LINUX_PROC_FS_H
#define _LINUX_PROC_FS_H

#include <linux/fs.h>
#include <linux/malloc.h>

/*
 * The proc filesystem constants/structures
 */

/*
 * We always define these enumerators
 */

enum root_directory_inos {
	PROC_ROOT_INO = 1,
	PROC_LOADAVG,
	PROC_UPTIME,
	PROC_MEMINFO,
	PROC_KMSG,
	PROC_VERSION,
	PROC_CPUINFO,
	PROC_PCI,
	PROC_SELF,	/* will change inode # */
	PROC_NET,
        PROC_SCSI,
	PROC_MALLOC,
	PROC_KCORE,
	PROC_MODULES,
	PROC_STAT,
	PROC_DEVICES,
	PROC_INTERRUPTS,
	PROC_FILESYSTEMS,
	PROC_KSYMS,
	PROC_DMA,	
	PROC_IOPORTS,
	PROC_APM,
#ifdef __SMP_PROF__
	PROC_SMP_PROF,
#endif
	PROC_PROFILE, /* whether enabled or not */
	PROC_CMDLINE,
	PROC_SYS,
	PROC_MTAB
};

enum pid_directory_inos {
	PROC_PID_INO = 2,
	PROC_PID_STATUS,
	PROC_PID_MEM,
	PROC_PID_CWD,
	PROC_PID_ROOT,
	PROC_PID_EXE,
	PROC_PID_FD,
	PROC_PID_ENVIRON,
	PROC_PID_CMDLINE,
	PROC_PID_STAT,
	PROC_PID_STATM,
	PROC_PID_MAPS
};

enum pid_subdirectory_inos {
	PROC_PID_FD_DIR = 1
};

enum net_directory_inos {
	PROC_NET_UNIX = 128,
	PROC_NET_ARP,
	PROC_NET_ROUTE,
	PROC_NET_DEV,
	PROC_NET_RAW,
	PROC_NET_TCP,
	PROC_NET_UDP,
	PROC_NET_SNMP,
	PROC_NET_RARP,
	PROC_NET_IGMP,
	PROC_NET_IPMR_VIF,
	PROC_NET_IPMR_MFC,
	PROC_NET_IPFWFWD,
	PROC_NET_IPFWIN,
	PROC_NET_IPFWOUT,
	PROC_NET_IPACCT,
	PROC_NET_IPMSQHST,
	PROC_NET_WAVELAN,
	PROC_NET_IPX_INTERFACE,
	PROC_NET_IPX_ROUTE,
	PROC_NET_IPX,
	PROC_NET_ATALK,
	PROC_NET_AT_ROUTE,
	PROC_NET_ATIF,
	PROC_NET_AX25_ROUTE,
	PROC_NET_AX25,
	PROC_NET_AX25_CALLS,
	PROC_NET_NR_NODES,
	PROC_NET_NR_NEIGH,
	PROC_NET_NR,
	PROC_NET_SOCKSTAT,
	PROC_NET_RTCACHE,
	PROC_NET_AX25_BPQETHER,
	PROC_NET_ALIAS_TYPES,
	PROC_NET_ALIASES,
	PROC_NET_LAST
};

enum scsi_directory_inos {
	PROC_SCSI_SCSI = 256,
	PROC_SCSI_ADVANSYS,
	PROC_SCSI_EATA,
	PROC_SCSI_EATA_PIO,
	PROC_SCSI_AHA152X,
	PROC_SCSI_AHA1542,
	PROC_SCSI_AHA1740,
	PROC_SCSI_AIC7XXX,
	PROC_SCSI_BUSLOGIC,
	PROC_SCSI_U14_34F,
	PROC_SCSI_FDOMAIN,
	PROC_SCSI_GENERIC_NCR5380,
	PROC_SCSI_IN2000,
	PROC_SCSI_PAS16,
	PROC_SCSI_QLOGIC,
	PROC_SCSI_SEAGATE,
	PROC_SCSI_T128,
	PROC_SCSI_NCR53C7xx,
	PROC_SCSI_ULTRASTOR,
	PROC_SCSI_7000FASST,
	PROC_SCSI_EATA2X,
	PROC_SCSI_AM53C974,
	PROC_SCSI_SSC,
	PROC_SCSI_NCR53C406A,
	PROC_SCSI_SCSI_DEBUG,	
	PROC_SCSI_NOT_PRESENT,
	PROC_SCSI_FILE,                        /* I'm asuming here that we */
	PROC_SCSI_LAST = (PROC_SCSI_FILE + 16) /* won't ever see more than */
};                                             /* 16 HBAs in one machine   */

/* Finally, the dynamically allocatable proc entries are reserved: */

#define PROC_DYNAMIC_FIRST 4096
#define PROC_NDYNAMIC      4096

#define PROC_SUPER_MAGIC 0x9fa0

/*
 * This is not completely implemented yet. The idea is to
 * create a in-memory tree (like the actual /proc filesystem
 * tree) of these proc_dir_entries, so that we can dynamically
 * add new files to /proc.
 *
 * The "next" pointer creates a linked list of one /proc directory,
 * while parent/subdir create the directory structure (every
 * /proc file has a parent, but "subdir" is NULL for all
 * non-directory entries).
 *
 * "get_info" is called at "read", while "fill_inode" is used to
 * fill in file type/protection/owner information specific to the
 * particular /proc file.
 */
struct proc_dir_entry {
	unsigned short low_ino;
	unsigned short namelen;
	const char *name;
	mode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	unsigned long size;
	struct inode_operations * ops;
	int (*get_info)(char *, char **, off_t, int, int);
	void (*fill_inode)(struct inode *);
	struct proc_dir_entry *next, *parent, *subdir;
	void *data;
};

extern int (* dispatch_scsi_info_ptr) (int ino, char *buffer, char **start,
				off_t offset, int length, int inout);

extern struct proc_dir_entry proc_root;
extern struct proc_dir_entry proc_net;
extern struct proc_dir_entry proc_scsi;
extern struct proc_dir_entry proc_sys;
extern struct proc_dir_entry proc_pid;
extern struct proc_dir_entry proc_pid_fd;

extern struct inode_operations proc_scsi_inode_operations;

extern void proc_root_init(void);
extern void proc_base_init(void);
extern void proc_net_init(void);

extern int proc_register(struct proc_dir_entry *, struct proc_dir_entry *);
extern int proc_register_dynamic(struct proc_dir_entry *, 
				 struct proc_dir_entry *);
extern int proc_unregister(struct proc_dir_entry *, int);

static inline int proc_net_register(struct proc_dir_entry * x)
{
	return proc_register(&proc_net, x);
}

static inline int proc_net_unregister(int x)
{
	return proc_unregister(&proc_net, x);
}

static inline int proc_scsi_register(struct proc_dir_entry *driver, 
				     struct proc_dir_entry *x)
{
    x->ops = &proc_scsi_inode_operations;
    if(x->low_ino < PROC_SCSI_FILE){
	return(proc_register(&proc_scsi, x));
    }else{
	return(proc_register(driver, x));
    }
}

static inline int proc_scsi_unregister(struct proc_dir_entry *driver, int x)
{
    extern void scsi_init_free(char *ptr, unsigned int size);

    if(x <= PROC_SCSI_FILE)
	return(proc_unregister(&proc_scsi, x));
    else {
	struct proc_dir_entry **p = &driver->subdir, *dp;
	int ret;

	while ((dp = *p) != NULL) {
		if (dp->low_ino == x) 
		    break;
		p = &dp->next;
	}
	ret = proc_unregister(driver, x);
	scsi_init_free((char *) dp, sizeof(struct proc_dir_entry) + 4);
	return(ret);
    }
}

extern struct super_block *proc_read_super(struct super_block *,void *,int);
extern int init_proc_fs(void);
extern struct inode * proc_get_inode(struct super_block *, int, struct proc_dir_entry *);
extern void proc_statfs(struct super_block *, struct statfs *, int);
extern void proc_read_inode(struct inode *);
extern void proc_write_inode(struct inode *);
extern int proc_match(int, const char *, struct proc_dir_entry *);

/*
 * These are generic /proc routines that use the internal
 * "struct proc_dir_entry" tree to traverse the filesystem.
 *
 * The /proc root directory has extended versions to take care
 * of the /proc/<pid> subdirectories.
 */
extern int proc_readdir(struct inode *, struct file *, void *, filldir_t);
extern int proc_lookup(struct inode *, const char *, int, struct inode **);

extern struct inode_operations proc_dir_inode_operations;
extern struct inode_operations proc_net_inode_operations;
extern struct inode_operations proc_netdir_inode_operations;
extern struct inode_operations proc_scsi_inode_operations;
extern struct inode_operations proc_mem_inode_operations;
extern struct inode_operations proc_sys_inode_operations;
extern struct inode_operations proc_array_inode_operations;
extern struct inode_operations proc_arraylong_inode_operations;
extern struct inode_operations proc_kcore_inode_operations;
extern struct inode_operations proc_profile_inode_operations;
extern struct inode_operations proc_kmsg_inode_operations;
extern struct inode_operations proc_link_inode_operations;
extern struct inode_operations proc_fd_inode_operations;

#endif
