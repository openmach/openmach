
#include <mach/machine/vm_types.h>
#include <mach/lmm.h>
#include <malloc.h>
#include <assert.h>

/* Remove any memory in the specified range from the free memory list.  */
void malloc_reserve(vm_offset_t rstart, vm_offset_t rend)
{
	/*printf("malloc_reserve %08x-%08x\n", rstart, rend);*/
	while (rstart < rend)
	{
		vm_offset_t size;
		lmm_flags_t flags;
		void *ptr;

		/*printf("lmm_find_free %08x ", rstart);*/
		lmm_find_free(&malloc_lmm, &rstart, &size, &flags);
		/*printf(" returned %08x-%08x (size %08x) flags %08x\n",
			rstart, rstart+size, size, flags);*/
		if ((size == 0) || (rstart >= rend))
			break;
		if (rstart + size > rend)
			size = rend - rstart;
		/*printf("reserving %08x-%08x\n", rstart, rstart+size);*/
		ptr = lmm_alloc_gen(&malloc_lmm, size, flags, 0, 0,
				    rstart, size);
		assert((vm_offset_t)ptr == rstart);
	}
}

void *mustmalloc(vm_size_t size)
{
	void *buf;
	if (!(buf = malloc(size)))
		die("out of memory");
	/*printf("malloc returning %08x-%08x\n", buf, buf+size);*/
	return buf;
}

void *mustcalloc(vm_size_t size)
{
	void *buf;
	if (!(buf = calloc(size, 1)))
		die("out of memory");
	/*printf("calloc returning %08x-%08x\n", buf, buf+size);*/
	return buf;
}

