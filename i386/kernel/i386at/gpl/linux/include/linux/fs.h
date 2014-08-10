#ifndef _LINUX_FS_H
#define _LINUX_FS_H

/*
 * This file has definitions for some important file table
 * structures etc.
 */

#include <linux/linkage.h>
#include <linux/limits.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/vfs.h>
#include <linux/net.h>
#include <linux/kdev_t.h>
#include <linux/ioctl.h>

/*
 * It's silly to have NR_OPEN bigger than NR_FILE, but I'll fix
 * that later. Anyway, now the file code is no longer dependent
 * on bitmaps in unsigned longs, but uses the new fd_set structure..
 *
 * Some programs (notably those using select()) may have to be 
 * recompiled to take full advantage of the new limits..
 */

/* Fixed constants first: */
#undef NR_OPEN
#define NR_OPEN 256

#define NR_SUPER 64
#define NR_IHASH 131
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

/* And dynamically-tunable limits and defaults: */
extern int max_inodes, nr_inodes;
extern int max_files, nr_files;
#define NR_INODE 2048	/* this should be bigger than NR_FILE */
#define NR_FILE 1024	/* this can well be larger on a larger system */

#define MAY_EXEC 1
#define MAY_WRITE 2
#define MAY_READ 4

#define FMODE_READ 1
#define FMODE_WRITE 2

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define NIL_FILP	((struct file *)0)
#define SEL_IN		1
#define SEL_OUT		2
#define SEL_EX		4

/*
 * These are the fs-independent mount-flags: up to 16 flags are supported
 */
#define MS_RDONLY	 1	/* Mount read-only */
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#define MS_NODEV	 4	/* Disallow access to device special files */
#define MS_NOEXEC	 8	/* Disallow program execution */
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#define S_WRITE		128	/* Write on file/directory/symlink */
#define S_APPEND	256	/* Append-only file */
#define S_IMMUTABLE	512	/* Immutable file */

/*
 * Flags that can be altered by MS_REMOUNT
 */
#define MS_RMT_MASK (MS_RDONLY)

/*
 * Magic mount flag number. Has to be or-ed to the flag values.
 */
#define MS_MGC_VAL 0xC0ED0000	/* magic flag number to indicate "new" flags */
#define MS_MGC_MSK 0xffff0000	/* magic flag number mask */

/*
 * Note that read-only etc flags are inode-specific: setting some file-system
 * flags just means all the inodes inherit those flags by default. It might be
 * possible to override it selectively if you really wanted to with some
 * ioctl() that is not currently implemented.
 *
 * Exception: MS_RDONLY is always applied to the entire file system.
 */
#define IS_RDONLY(inode) (((inode)->i_sb) && ((inode)->i_sb->s_flags & MS_RDONLY))
#define IS_NOSUID(inode) ((inode)->i_flags & MS_NOSUID)
#define IS_NODEV(inode) ((inode)->i_flags & MS_NODEV)
#define IS_NOEXEC(inode) ((inode)->i_flags & MS_NOEXEC)
#define IS_SYNC(inode) ((inode)->i_flags & MS_SYNCHRONOUS)

#define IS_WRITABLE(inode) ((inode)->i_flags & S_WRITE)
#define IS_APPEND(inode) ((inode)->i_flags & S_APPEND)
#define IS_IMMUTABLE(inode) ((inode)->i_flags & S_IMMUTABLE)

/* the read-only stuff doesn't really belong here, but any other place is
   probably as bad and I don't want to create yet another include file. */

#define BLKROSET   _IO(0x12,93)	/* set device read-only (0 = read-write) */
#define BLKROGET   _IO(0x12,94)	/* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKRASET   _IO(0x12,98)	/* Set read ahead for block device */
#define BLKRAGET   _IO(0x12,99)	/* get current read ahead setting */

#define BMAP_IOCTL 1		/* obsolete - kept for compatibility */
#define FIBMAP	   _IO(0x00,1)	/* bmap access */
#define FIGETBSZ   _IO(0x00,2)	/* get the block size used for bmap */

#ifdef __KERNEL__

#include <asm/bitops.h>

extern void buffer_init(void);
extern unsigned long inode_init(unsigned long start, unsigned long end);
extern unsigned long file_table_init(unsigned long start, unsigned long end);
extern unsigned long name_cache_init(unsigned long start, unsigned long end);

typedef char buffer_block[BLOCK_SIZE];

/* bh state bits */
#define BH_Uptodate	0	/* 1 if the buffer contains valid data */
#define BH_Dirty	1	/* 1 if the buffer is dirty */
#define BH_Lock		2	/* 1 if the buffer is locked */
#define BH_Req		3	/* 0 if the buffer has been invalidated */
#define BH_Touched	4	/* 1 if the buffer has been touched (aging) */
#define BH_Has_aged	5	/* 1 if the buffer has been aged (aging) */
#define BH_Protected	6	/* 1 if the buffer is protected */
#define BH_FreeOnIO	7	/* 1 to discard the buffer_head after IO */

/*
 * Try to keep the most commonly used fields in single cache lines (16
 * bytes) to improve performance.  This ordering should be
 * particularly beneficial on 32-bit processors.
 * 
 * We use the first 16 bytes for the data which is used in searches
 * over the block hash lists (ie. getblk(), find_buffer() and
 * friends).
 * 
 * The second 16 bytes we use for lru buffer scans, as used by
 * sync_buffers() and refill_freelist().  -- sct
 */
#ifdef MACH
struct buffer_head
{
  unsigned long b_blocknr;
  kdev_t b_dev;
  unsigned long b_state;
  unsigned long b_size;
  char *b_data;
  struct wait_queue *b_wait;
  struct buffer_head *b_reqnext;
  void *b_page_list;
  int b_index;
  int b_off;
  int b_usrcnt;
  struct request *b_request;
  struct semaphore *b_sem;
};
#else /* ! MACH */
struct buffer_head {
	/* First cache line: */
	unsigned long b_blocknr;	/* block number */
	kdev_t b_dev;			/* device (B_FREE = free) */
	struct buffer_head * b_next;	/* Hash queue list */
	struct buffer_head * b_this_page;	/* circular list of buffers in one page */

	/* Second cache line: */
	unsigned long b_state;		/* buffer state bitmap (see above) */
	struct buffer_head * b_next_free;
	unsigned int b_count;		/* users using this block */
	unsigned long b_size;		/* block size */

	/* Non-performance-critical data follows. */
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned int b_list;		/* List that this buffer appears */
	unsigned long b_flushtime;      /* Time when this (dirty) buffer
					 * should be written */
	unsigned long b_lru_time;       /* Time when this buffer was 
					 * last used. */
	struct wait_queue * b_wait;
	struct buffer_head * b_prev;		/* doubly linked list of hash-queue */
	struct buffer_head * b_prev_free;	/* doubly linked list of buffers */
	struct buffer_head * b_reqnext;		/* request queue */
	char *b_usrbuf;
	struct request *b_request;
	struct semaphore *b_sem;
};
#endif /* ! MACH */

static inline int buffer_uptodate(struct buffer_head * bh)
{
	return test_bit(BH_Uptodate, &bh->b_state);
}	

static inline int buffer_dirty(struct buffer_head * bh)
{
	return test_bit(BH_Dirty, &bh->b_state);
}

static inline int buffer_locked(struct buffer_head * bh)
{
	return test_bit(BH_Lock, &bh->b_state);
}

static inline int buffer_req(struct buffer_head * bh)
{
	return test_bit(BH_Req, &bh->b_state);
}

static inline int buffer_touched(struct buffer_head * bh)
{
	return test_bit(BH_Touched, &bh->b_state);
}

static inline int buffer_has_aged(struct buffer_head * bh)
{
	return test_bit(BH_Has_aged, &bh->b_state);
}

static inline int buffer_protected(struct buffer_head * bh)
{
	return test_bit(BH_Protected, &bh->b_state);
}

#ifndef MACH
#include <linux/pipe_fs_i.h>
#include <linux/minix_fs_i.h>
#include <linux/ext_fs_i.h>
#include <linux/ext2_fs_i.h>
#include <linux/hpfs_fs_i.h>
#include <linux/msdos_fs_i.h>
#include <linux/umsdos_fs_i.h>
#include <linux/iso_fs_i.h>
#include <linux/nfs_fs_i.h>
#include <linux/xia_fs_i.h>
#include <linux/sysv_fs_i.h>
#endif

/*
 * Attribute flags.  These should be or-ed together to figure out what
 * has been changed!
 */
#define ATTR_MODE	1
#define ATTR_UID	2
#define ATTR_GID	4
#define ATTR_SIZE	8
#define ATTR_ATIME	16
#define ATTR_MTIME	32
#define ATTR_CTIME	64
#define ATTR_ATIME_SET	128
#define ATTR_MTIME_SET	256
#define ATTR_FORCE	512	/* Not a change, but a change it */

/*
 * This is the Inode Attributes structure, used for notify_change().  It
 * uses the above definitions as flags, to know which values have changed.
 * Also, in this manner, a Filesystem can look at only the values it cares
 * about.  Basically, these are the attributes that the VFS layer can
 * request to change from the FS layer.
 *
 * Derek Atkins <warlord@MIT.EDU> 94-10-20
 */
struct iattr {
	unsigned int	ia_valid;
	umode_t		ia_mode;
	uid_t		ia_uid;
	gid_t		ia_gid;
	off_t		ia_size;
	time_t		ia_atime;
	time_t		ia_mtime;
	time_t		ia_ctime;
};

#include <linux/quota.h>

#ifdef MACH
struct inode
{
  umode_t i_mode;
  kdev_t i_rdev;
};

struct file
{
  mode_t f_mode;
  loff_t f_pos;
  unsigned short f_flags;
  int f_resid;
  void *f_object;
  void *f_np;
};

struct vm_area_struct;
struct page;
#else /* ! MACH */
struct inode {
	kdev_t		i_dev;
	unsigned long	i_ino;
	umode_t		i_mode;
	nlink_t		i_nlink;
	uid_t		i_uid;
	gid_t		i_gid;
	kdev_t		i_rdev;
	off_t		i_size;
	time_t		i_atime;
	time_t		i_mtime;
	time_t		i_ctime;
	unsigned long	i_blksize;
	unsigned long	i_blocks;
	unsigned long	i_version;
	unsigned long	i_nrpages;
	struct semaphore i_sem;
	struct inode_operations *i_op;
	struct super_block *i_sb;
	struct wait_queue *i_wait;
	struct file_lock *i_flock;
	struct vm_area_struct *i_mmap;
	struct page *i_pages;
	struct dquot *i_dquot[MAXQUOTAS];
	struct inode *i_next, *i_prev;
	struct inode *i_hash_next, *i_hash_prev;
	struct inode *i_bound_to, *i_bound_by;
	struct inode *i_mount;
	unsigned short i_count;
	unsigned short i_flags;
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe;
	unsigned char i_sock;
	unsigned char i_seek;
	unsigned char i_update;
	unsigned short i_writecount;
	union {
		struct pipe_inode_info pipe_i;
		struct minix_inode_info minix_i;
		struct ext_inode_info ext_i;
		struct ext2_inode_info ext2_i;
		struct hpfs_inode_info hpfs_i;
		struct msdos_inode_info msdos_i;
		struct umsdos_inode_info umsdos_i;
		struct iso_inode_info isofs_i;
		struct nfs_inode_info nfs_i;
		struct xiafs_inode_info xiafs_i;
		struct sysv_inode_info sysv_i;
		struct socket socket_i;
		void * generic_ip;
	} u;
};

struct file {
	mode_t f_mode;
	loff_t f_pos;
	unsigned short f_flags;
	unsigned short f_count;
	off_t f_reada;
	struct file *f_next, *f_prev;
	int f_owner;		/* pid or -pgrp where SIGIO should be sent */
	struct inode * f_inode;
	struct file_operations * f_op;
	unsigned long f_version;
	void *private_data;	/* needed for tty driver, and maybe others */
};
#endif /* ! MACH */

struct file_lock {
	struct file_lock *fl_next;	/* singly linked list for this inode  */
	struct file_lock *fl_nextlink;	/* doubly linked list of all locks */
	struct file_lock *fl_prevlink;	/* used to simplify lock removal */
	struct file_lock *fl_block;
	struct task_struct *fl_owner;
	struct wait_queue *fl_wait;
	struct file *fl_file;
	char fl_flags;
	char fl_type;
	off_t fl_start;
	off_t fl_end;
};

struct fasync_struct {
	int    magic;
	struct fasync_struct	*fa_next; /* singly linked list */
	struct file 		*fa_file;
};

#define FASYNC_MAGIC 0x4601

extern int fasync_helper(struct inode *, struct file *, int, struct fasync_struct **);

#ifndef MACH
#include <linux/minix_fs_sb.h>
#include <linux/ext_fs_sb.h>
#include <linux/ext2_fs_sb.h>
#include <linux/hpfs_fs_sb.h>
#include <linux/msdos_fs_sb.h>
#include <linux/iso_fs_sb.h>
#include <linux/nfs_fs_sb.h>
#include <linux/xia_fs_sb.h>
#include <linux/sysv_fs_sb.h>

struct super_block {
	kdev_t s_dev;
	unsigned long s_blocksize;
	unsigned char s_blocksize_bits;
	unsigned char s_lock;
	unsigned char s_rd_only;
	unsigned char s_dirt;
	struct file_system_type *s_type;
	struct super_operations *s_op;
	struct dquot_operations *dq_op;
	unsigned long s_flags;
	unsigned long s_magic;
	unsigned long s_time;
	struct inode * s_covered;
	struct inode * s_mounted;
	struct wait_queue * s_wait;
	union {
		struct minix_sb_info minix_sb;
		struct ext_sb_info ext_sb;
		struct ext2_sb_info ext2_sb;
		struct hpfs_sb_info hpfs_sb;
		struct msdos_sb_info msdos_sb;
		struct isofs_sb_info isofs_sb;
		struct nfs_sb_info nfs_sb;
		struct xiafs_sb_info xiafs_sb;
		struct sysv_sb_info sysv_sb;
		void *generic_sbp;
	} u;
};
#endif /* ! MACH */

/*
 * This is the "filldir" function type, used by readdir() to let
 * the kernel specify what kind of dirent layout it wants to have.
 * This allows the kernel to read directories into kernel space or
 * to have different dirent layouts depending on the binary type.
 */
typedef int (*filldir_t)(void *, const char *, int, off_t, ino_t);
	
struct file_operations {
	int (*lseek) (struct inode *, struct file *, off_t, int);
	int (*read) (struct inode *, struct file *, char *, int);
	int (*write) (struct inode *, struct file *, const char *, int);
	int (*readdir) (struct inode *, struct file *, void *, filldir_t);
	int (*select) (struct inode *, struct file *, int, select_table *);
	int (*ioctl) (struct inode *, struct file *, unsigned int, unsigned long);
	int (*mmap) (struct inode *, struct file *, struct vm_area_struct *);
	int (*open) (struct inode *, struct file *);
	void (*release) (struct inode *, struct file *);
	int (*fsync) (struct inode *, struct file *);
	int (*fasync) (struct inode *, struct file *, int);
	int (*check_media_change) (kdev_t dev);
	int (*revalidate) (kdev_t dev);
};

struct inode_operations {
	struct file_operations * default_file_ops;
	int (*create) (struct inode *,const char *,int,int,struct inode **);
	int (*lookup) (struct inode *,const char *,int,struct inode **);
	int (*link) (struct inode *,struct inode *,const char *,int);
	int (*unlink) (struct inode *,const char *,int);
	int (*symlink) (struct inode *,const char *,int,const char *);
	int (*mkdir) (struct inode *,const char *,int,int);
	int (*rmdir) (struct inode *,const char *,int);
	int (*mknod) (struct inode *,const char *,int,int,int);
	int (*rename) (struct inode *,const char *,int,struct inode *,const char *,int);
	int (*readlink) (struct inode *,char *,int);
	int (*follow_link) (struct inode *,struct inode *,int,int,struct inode **);
	int (*readpage) (struct inode *, struct page *);
	int (*writepage) (struct inode *, struct page *);
	int (*bmap) (struct inode *,int);
	void (*truncate) (struct inode *);
	int (*permission) (struct inode *, int);
	int (*smap) (struct inode *,int);
};

struct super_operations {
	void (*read_inode) (struct inode *);
	int (*notify_change) (struct inode *, struct iattr *);
	void (*write_inode) (struct inode *);
	void (*put_inode) (struct inode *);
	void (*put_super) (struct super_block *);
	void (*write_super) (struct super_block *);
	void (*statfs) (struct super_block *, struct statfs *, int);
	int (*remount_fs) (struct super_block *, int *, char *);
};

struct dquot_operations {
	void (*initialize) (struct inode *, short);
	void (*drop) (struct inode *);
	int (*alloc_block) (const struct inode *, unsigned long);
	int (*alloc_inode) (const struct inode *, unsigned long);
	void (*free_block) (const struct inode *, unsigned long);
	void (*free_inode) (const struct inode *, unsigned long);
	int (*transfer) (struct inode *, struct iattr *, char);
};

struct file_system_type {
	struct super_block *(*read_super) (struct super_block *, void *, int);
	const char *name;
	int requires_dev;
	struct file_system_type * next;
};

extern int register_filesystem(struct file_system_type *);
extern int unregister_filesystem(struct file_system_type *);

asmlinkage int sys_open(const char *, int, int);
asmlinkage int sys_close(unsigned int);		/* yes, it's really unsigned */

extern void kill_fasync(struct fasync_struct *fa, int sig);

extern int getname(const char * filename, char **result);
extern void putname(char * name);
extern int do_truncate(struct inode *, unsigned long);
extern int register_blkdev(unsigned int, const char *, struct file_operations *);
extern int unregister_blkdev(unsigned int major, const char * name);
extern int blkdev_open(struct inode * inode, struct file * filp);
extern struct file_operations def_blk_fops;
extern struct inode_operations blkdev_inode_operations;

extern int register_chrdev(unsigned int, const char *, struct file_operations *);
extern int unregister_chrdev(unsigned int major, const char * name);
extern int chrdev_open(struct inode * inode, struct file * filp);
extern struct file_operations def_chr_fops;
extern struct inode_operations chrdev_inode_operations;

extern void init_fifo(struct inode * inode);

extern struct file_operations connecting_fifo_fops;
extern struct file_operations read_fifo_fops;
extern struct file_operations write_fifo_fops;
extern struct file_operations rdwr_fifo_fops;
extern struct file_operations read_pipe_fops;
extern struct file_operations write_pipe_fops;
extern struct file_operations rdwr_pipe_fops;

extern struct file_system_type *get_fs_type(const char *name);

extern int fs_may_mount(kdev_t dev);
extern int fs_may_umount(kdev_t dev, struct inode * mount_root);
extern int fs_may_remount_ro(kdev_t dev);

extern struct file *first_file;
extern struct super_block super_blocks[NR_SUPER];

extern void refile_buffer(struct buffer_head * buf);
extern void set_writetime(struct buffer_head * buf, int flag);
extern void refill_freelist(int size);
extern int try_to_free_buffer(struct buffer_head*, struct buffer_head**, int);

extern struct buffer_head ** buffer_pages;
extern int nr_buffers;
extern int buffermem;
extern int nr_buffer_heads;

#define BUF_CLEAN 0
#define BUF_UNSHARED 1 /* Buffers that were shared but are not any more */
#define BUF_LOCKED 2   /* Buffers scheduled for write */
#define BUF_LOCKED1 3  /* Supers, inodes */
#define BUF_DIRTY 4    /* Dirty buffers, not yet scheduled for write */
#define BUF_SHARED 5   /* Buffers shared */
#define NR_LIST 6

#ifdef MACH
extern inline void
mark_buffer_uptodate (struct buffer_head *bh, int on)
{
  if (on)
    set_bit (BH_Uptodate, &bh->b_state);
  else
    clear_bit (BH_Uptodate, &bh->b_state);
}
#else
void mark_buffer_uptodate(struct buffer_head * bh, int on);
#endif

extern inline void mark_buffer_clean(struct buffer_head * bh)
{
#ifdef MACH
	clear_bit(BH_Dirty, &bh->b_state);
#else
	if (clear_bit(BH_Dirty, &bh->b_state)) {
		if (bh->b_list == BUF_DIRTY)
			refile_buffer(bh);
	}
#endif
}

extern inline void mark_buffer_dirty(struct buffer_head * bh, int flag)
{
#ifdef MACH
	set_bit(BH_Dirty, &bh->b_state);
#else
	if (!set_bit(BH_Dirty, &bh->b_state)) {
		set_writetime(bh, flag);
		if (bh->b_list != BUF_DIRTY)
			refile_buffer(bh);
	}
#endif
}

extern int check_disk_change(kdev_t dev);
#ifdef MACH
#define invalidate_inodes(dev)
#else
extern void invalidate_inodes(kdev_t dev);
#endif
extern void invalidate_inode_pages(struct inode *, unsigned long);
#ifdef MACH
#define invalidate_buffers(dev)
#else
extern void invalidate_buffers(kdev_t dev);
#endif
extern int floppy_is_wp(int minor);
extern void sync_inodes(kdev_t dev);
#ifdef MACH
#define sync_dev(dev)
#define fsync_dev(dev)
#else
extern void sync_dev(kdev_t dev);
extern int fsync_dev(kdev_t dev);
#endif
extern void sync_supers(kdev_t dev);
extern int bmap(struct inode * inode,int block);
extern int notify_change(struct inode *, struct iattr *);
extern int namei(const char * pathname, struct inode ** res_inode);
extern int lnamei(const char * pathname, struct inode ** res_inode);
#ifdef MACH
#define permission(i, m)	0
#else
extern int permission(struct inode * inode,int mask);
#endif
extern int get_write_access(struct inode *inode);
extern void put_write_access(struct inode *inode);
extern int open_namei(const char * pathname, int flag, int mode,
	struct inode ** res_inode, struct inode * base);
extern int do_mknod(const char * filename, int mode, dev_t dev);
extern int do_pipe(int *);
extern void iput(struct inode * inode);
extern struct inode * __iget(struct super_block * sb,int nr,int crsmnt);
extern struct inode * get_empty_inode(void);
extern void insert_inode_hash(struct inode *);
extern void clear_inode(struct inode *);
extern struct inode * get_pipe_inode(void);
extern struct file * get_empty_filp(void);
extern int close_fp(struct file *filp);
extern struct buffer_head * get_hash_table(kdev_t dev, int block, int size);
extern struct buffer_head * getblk(kdev_t dev, int block, int size);
extern void ll_rw_block(int rw, int nr, struct buffer_head * bh[]);
extern void ll_rw_page(int rw, kdev_t dev, unsigned long nr, char * buffer);
extern void ll_rw_swap_file(int rw, kdev_t dev, unsigned int *b, int nb, char *buffer);
extern int is_read_only(kdev_t dev);
extern void __brelse(struct buffer_head *buf);
extern inline void brelse(struct buffer_head *buf)
{
	if (buf)
		__brelse(buf);
}
extern void __bforget(struct buffer_head *buf);
extern inline void bforget(struct buffer_head *buf)
{
	if (buf)
		__bforget(buf);
}
extern void set_blocksize(kdev_t dev, int size);
extern struct buffer_head * bread(kdev_t dev, int block, int size);
extern struct buffer_head * breada(kdev_t dev,int block, int size, 
				   unsigned int pos, unsigned int filesize);

extern int generic_readpage(struct inode *, struct page *);
extern int generic_file_read(struct inode *, struct file *, char *, int);
extern int generic_mmap(struct inode *, struct file *, struct vm_area_struct *);
extern int brw_page(int, unsigned long, kdev_t, int [], int, int);

extern void put_super(kdev_t dev);
unsigned long generate_cluster(kdev_t dev, int b[], int size);
extern kdev_t ROOT_DEV;

extern void show_buffers(void);
extern void mount_root(void);

extern int char_read(struct inode *, struct file *, char *, int);
extern int block_read(struct inode *, struct file *, char *, int);
extern int read_ahead[];

extern int char_write(struct inode *, struct file *, const char *, int);
extern int block_write(struct inode *, struct file *, const char *, int);

extern int block_fsync(struct inode *, struct file *);
extern int file_fsync(struct inode *, struct file *);

extern void dcache_add(struct inode *, const char *, int, unsigned long);
extern int dcache_lookup(struct inode *, const char *, int, unsigned long *);

extern int inode_change_ok(struct inode *, struct iattr *);
extern void inode_setattr(struct inode *, struct iattr *);

extern inline struct inode * iget(struct super_block * sb,int nr)
{
	return __iget(sb, nr, 1);
}

/* kludge to get SCSI modules working */
#include <linux/minix_fs.h>
#include <linux/minix_fs_sb.h>

#endif /* __KERNEL__ */

#endif
