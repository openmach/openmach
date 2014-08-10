#ifndef _LINUX_PAGEMAP_H
#define _LINUX_PAGEMAP_H

#include <asm/system.h>

/*
 * Page-mapping primitive inline functions
 *
 * Copyright 1995 Linus Torvalds
 */

#ifndef MACH
static inline unsigned long page_address(struct page * page)
{
	return PAGE_OFFSET + PAGE_SIZE*(page - mem_map);
}

#define PAGE_HASH_BITS 10
#define PAGE_HASH_SIZE (1 << PAGE_HASH_BITS)

#define PAGE_AGE_VALUE 16

extern unsigned long page_cache_size;
extern struct page * page_hash_table[PAGE_HASH_SIZE];

/*
 * We use a power-of-two hash table to avoid a modulus,
 * and get a reasonable hash by knowing roughly how the
 * inode pointer and offsets are distributed (ie, we
 * roughly know which bits are "significant")
 */
static inline unsigned long _page_hashfn(struct inode * inode, unsigned long offset)
{
#define i (((unsigned long) inode)/sizeof(unsigned long))
#define o (offset >> PAGE_SHIFT)
#define s(x) ((x)+((x)>>PAGE_HASH_BITS))
	return s(i+o) & (PAGE_HASH_SIZE-1);
#undef i
#undef o
#undef s
}

#define page_hash(inode,offset) page_hash_table[_page_hashfn(inode,offset)]

static inline struct page * find_page(struct inode * inode, unsigned long offset)
{
	struct page *page;
	unsigned long flags;
	
	for (page = page_hash(inode, offset); page ; page = page->next_hash) {
		if (page->inode != inode)
			continue;
		if (page->offset != offset)
			continue;
		save_flags(flags);
		cli();
		page->referenced = 1;
		page->count++;
		restore_flags(flags);
		break;
	}
	return page;
}

static inline void remove_page_from_hash_queue(struct page * page)
{
	struct page **p = &page_hash(page->inode,page->offset);

	page_cache_size--;
	if (page->next_hash)
		page->next_hash->prev_hash = page->prev_hash;
	if (page->prev_hash)
		page->prev_hash->next_hash = page->next_hash;
	if (*p == page)
		*p = page->next_hash;
	page->next_hash = page->prev_hash = NULL;
}

static inline void add_page_to_hash_queue(struct inode * inode, struct page * page)
{
	struct page **p = &page_hash(inode,page->offset);

	page_cache_size++;
	page->referenced = 1;
	page->age = PAGE_AGE_VALUE;
	page->prev_hash = NULL;
	if ((page->next_hash = *p) != NULL)
		page->next_hash->prev_hash = page;
	*p = page;
}

static inline void remove_page_from_inode_queue(struct page * page)
{
	struct inode * inode = page->inode;

	page->inode = NULL;
	inode->i_nrpages--;
	if (inode->i_pages == page)
		inode->i_pages = page->next;
	if (page->next)
		page->next->prev = page->prev;
	if (page->prev)
		page->prev->next = page->next;
	page->next = NULL;
	page->prev = NULL;
}

static inline void add_page_to_inode_queue(struct inode * inode, struct page * page)
{
	struct page **p = &inode->i_pages;

	inode->i_nrpages++;
	page->inode = inode;
	page->prev = NULL;
	if ((page->next = *p) != NULL)
		page->next->prev = page;
	*p = page;
}

extern void __wait_on_page(struct page *);
static inline void wait_on_page(struct page * page)
{
	if (page->locked)
		__wait_on_page(page);
}

extern void update_vm_cache(struct inode *, unsigned long, const char *, int);

#endif /* ! MACH */

#endif
