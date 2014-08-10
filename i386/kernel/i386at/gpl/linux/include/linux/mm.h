#ifndef _LINUX_MM_H
#define _LINUX_MM_H

#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

extern unsigned long high_memory;

#include <asm/page.h>

#ifdef __KERNEL__

#define VERIFY_READ 0
#define VERIFY_WRITE 1

extern int verify_area(int, const void *, unsigned long);

/*
 * Linux kernel virtual memory manager primitives.
 * The idea being to have a "virtual" mm in the same way
 * we have a virtual fs - giving a cleaner interface to the
 * mm details, and allowing different kinds of memory mappings
 * (from shared memory to executable loading to arbitrary
 * mmap() functions).
 */

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
struct vm_area_struct {
	struct mm_struct * vm_mm;	/* VM area parameters */
	unsigned long vm_start;
	unsigned long vm_end;
	pgprot_t vm_page_prot;
	unsigned short vm_flags;
/* AVL tree of VM areas per task, sorted by address */
	short vm_avl_height;
	struct vm_area_struct * vm_avl_left;
	struct vm_area_struct * vm_avl_right;
/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct * vm_next;
/* for areas with inode, the circular list inode->i_mmap */
/* for shm areas, the circular list of attaches */
/* otherwise unused */
	struct vm_area_struct * vm_next_share;
	struct vm_area_struct * vm_prev_share;
/* more */
	struct vm_operations_struct * vm_ops;
	unsigned long vm_offset;
	struct inode * vm_inode;
	unsigned long vm_pte;			/* shared mem */
};

/*
 * vm_flags..
 */
#define VM_READ		0x0001	/* currently active flags */
#define VM_WRITE	0x0002
#define VM_EXEC		0x0004
#define VM_SHARED	0x0008

#define VM_MAYREAD	0x0010	/* limits for mprotect() etc */
#define VM_MAYWRITE	0x0020
#define VM_MAYEXEC	0x0040
#define VM_MAYSHARE	0x0080

#define VM_GROWSDOWN	0x0100	/* general info on the segment */
#define VM_GROWSUP	0x0200
#define VM_SHM		0x0400	/* shared memory area, don't swap out */
#define VM_DENYWRITE	0x0800	/* ETXTBSY on write attempts.. */

#define VM_EXECUTABLE	0x1000
#define VM_LOCKED	0x2000

#define VM_STACK_FLAGS	0x0177

/*
 * mapping from the currently active vm_flags protection bits (the
 * low four bits) to a page protection mask..
 */
extern pgprot_t protection_map[16];


/*
 * These are the virtual MM functions - opening of an area, closing and
 * unmapping it (needed to keep files on disk up-to-date etc), pointer
 * to the functions called when a no-page or a wp-page exception occurs. 
 */
struct vm_operations_struct {
	void (*open)(struct vm_area_struct * area);
	void (*close)(struct vm_area_struct * area);
	void (*unmap)(struct vm_area_struct *area, unsigned long, size_t);
	void (*protect)(struct vm_area_struct *area, unsigned long, size_t, unsigned int newprot);
	int (*sync)(struct vm_area_struct *area, unsigned long, size_t, unsigned int flags);
	void (*advise)(struct vm_area_struct *area, unsigned long, size_t, unsigned int advise);
	unsigned long (*nopage)(struct vm_area_struct * area, unsigned long address, int write_access);
	unsigned long (*wppage)(struct vm_area_struct * area, unsigned long address,
		unsigned long page);
	int (*swapout)(struct vm_area_struct *,  unsigned long, pte_t *);
	pte_t (*swapin)(struct vm_area_struct *, unsigned long, unsigned long);
};

/*
 * Try to keep the most commonly accessed fields in single cache lines
 * here (16 bytes or greater).  This ordering should be particularly
 * beneficial on 32-bit processors.
 *
 * The first line is data used in linear searches (eg. clock algorithm
 * scans).  The second line is data used in page searches through the
 * page-cache.  -- sct 
 */
typedef struct page {
	unsigned int count;
	unsigned dirty:16,
		 age:8,
		 uptodate:1,
		 error:1,
		 referenced:1,
		 locked:1,
		 free_after:1,
		 unused:2,
		 reserved:1;
	struct wait_queue *wait;
	struct page *next;

	struct page *next_hash;
	unsigned long offset;
	struct inode *inode;
	struct page *write_list;

	struct page *prev;
	struct page *prev_hash;
} mem_map_t;

extern mem_map_t * mem_map;

/*
 * Free area management
 */

#define NR_MEM_LISTS 6

struct mem_list {
	struct mem_list * next;
	struct mem_list * prev;
};

extern struct mem_list free_area_list[NR_MEM_LISTS];
extern unsigned int * free_area_map[NR_MEM_LISTS];

/*
 * This is timing-critical - most of the time in getting a new page
 * goes to clearing the page. If you want a page without the clearing
 * overhead, just use __get_free_page() directly..
 */
#define __get_free_page(priority) __get_free_pages((priority),0,~0UL)
#define __get_dma_pages(priority, order) __get_free_pages((priority),(order),MAX_DMA_ADDRESS)
extern unsigned long __get_free_pages(int priority, unsigned long gfporder, unsigned long max_addr);

extern inline unsigned long get_free_page(int priority)
{
	unsigned long page;

	page = __get_free_page(priority);
	if (page)
		memset((void *) page, 0, PAGE_SIZE);
	return page;
}

/* memory.c & swap.c*/

#define free_page(addr) free_pages((addr),0)
extern void free_pages(unsigned long addr, unsigned long order);

extern void show_free_areas(void);
extern unsigned long put_dirty_page(struct task_struct * tsk,unsigned long page,
	unsigned long address);

extern void free_page_tables(struct task_struct * tsk);
extern void clear_page_tables(struct task_struct * tsk);
extern int new_page_tables(struct task_struct * tsk);
extern int copy_page_tables(struct task_struct * to);

extern int zap_page_range(struct mm_struct *mm, unsigned long address, unsigned long size);
extern int copy_page_range(struct mm_struct *dst, struct mm_struct *src, struct vm_area_struct *vma);
extern int remap_page_range(unsigned long from, unsigned long to, unsigned long size, pgprot_t prot);
extern int zeromap_page_range(unsigned long from, unsigned long size, pgprot_t prot);

extern void vmtruncate(struct inode * inode, unsigned long offset);
extern void handle_mm_fault(struct vm_area_struct *vma, unsigned long address, int write_access);
extern void do_wp_page(struct task_struct * tsk, struct vm_area_struct * vma, unsigned long address, int write_access);
extern void do_no_page(struct task_struct * tsk, struct vm_area_struct * vma, unsigned long address, int write_access);

extern unsigned long paging_init(unsigned long start_mem, unsigned long end_mem);
extern void mem_init(unsigned long start_mem, unsigned long end_mem);
extern void show_mem(void);
extern void oom(struct task_struct * tsk);
extern void si_meminfo(struct sysinfo * val);

/* vmalloc.c */

extern void * vmalloc(unsigned long size);
extern void * vremap(unsigned long offset, unsigned long size);
extern void vfree(void * addr);
extern int vread(char *buf, char *addr, int count);

/* mmap.c */
extern unsigned long do_mmap(struct file * file, unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags, unsigned long off);
extern void merge_segments(struct task_struct *, unsigned long, unsigned long);
extern void insert_vm_struct(struct task_struct *, struct vm_area_struct *);
extern void remove_shared_vm_struct(struct vm_area_struct *);
extern void build_mmap_avl(struct mm_struct *);
extern void exit_mmap(struct mm_struct *);
extern int do_munmap(unsigned long, size_t);
extern unsigned long get_unmapped_area(unsigned long, unsigned long);

/* filemap.c */
extern unsigned long page_unuse(unsigned long);
extern int shrink_mmap(int, unsigned long);
extern void truncate_inode_pages(struct inode *, unsigned long);

#define GFP_BUFFER	0x00
#define GFP_ATOMIC	0x01
#define GFP_USER	0x02
#define GFP_KERNEL	0x03
#define GFP_NOBUFFER	0x04
#define GFP_NFS		0x05

/* Flag - indicates that the buffer will be suitable for DMA.  Ignored on some
   platforms, used as appropriate on others */

#define GFP_DMA		0x80

#define GFP_LEVEL_MASK 0xf

#define avl_empty	(struct vm_area_struct *) NULL

#ifndef MACH
static inline int expand_stack(struct vm_area_struct * vma, unsigned long address)
{
	unsigned long grow;

	address &= PAGE_MASK;
	if (vma->vm_end - address > current->rlim[RLIMIT_STACK].rlim_cur)
		return -ENOMEM;
	grow = vma->vm_start - address;
	vma->vm_start = address;
	vma->vm_offset -= grow;
	vma->vm_mm->total_vm += grow >> PAGE_SHIFT;
	if (vma->vm_flags & VM_LOCKED)
		vma->vm_mm->locked_vm += grow >> PAGE_SHIFT;
	return 0;
}

/* Look up the first VMA which satisfies  addr < vm_end,  NULL if none. */
static inline struct vm_area_struct * find_vma (struct task_struct * task, unsigned long addr)
{
	struct vm_area_struct * result = NULL;
	struct vm_area_struct * tree;

	if (!task->mm)
		return NULL;
	for (tree = task->mm->mmap_avl ; ; ) {
		if (tree == avl_empty)
			return result;
		if (tree->vm_end > addr) {
			if (tree->vm_start <= addr)
				return tree;
			result = tree;
			tree = tree->vm_avl_left;
		} else
			tree = tree->vm_avl_right;
	}
}

/* Look up the first VMA which intersects the interval start_addr..end_addr-1,
   NULL if none.  Assume start_addr < end_addr. */
static inline struct vm_area_struct * find_vma_intersection (struct task_struct * task, unsigned long start_addr, unsigned long end_addr)
{
	struct vm_area_struct * vma;

	vma = find_vma(task,start_addr);
	if (!vma || end_addr <= vma->vm_start)
		return NULL;
	return vma;
}
#endif /* ! MACH */

#endif /* __KERNEL__ */

#endif
