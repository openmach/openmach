/*
 * misc.c
 * 
 * This is a collection of several routines from gzip-1.0.3 
 * adapted for Linux.
 *
 * malloc by Hannu Savolainen 1993 and Matthias Urlichs 1994
 * puts by Nick Holloway 1993
 */

#include <stdlib.h>

#include "vm_param.h"

#include "gzip.h"
#include "lzw.h"
#include "real.h"

#include <linux/config.h>
#include <linux/segment.h>

#define fcalloc calloc

/*
 * These are set up by the setup-routine at boot-time:
 */

struct screen_info {
	unsigned char  orig_x;
	unsigned char  orig_y;
	unsigned char  unused1[2];
	unsigned short orig_video_page;
	unsigned char  orig_video_mode;
	unsigned char  orig_video_cols;
	unsigned short orig_video_ega_ax;
	unsigned short orig_video_ega_bx;
	unsigned short orig_video_ega_cx;
	unsigned char  orig_video_lines;
};

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define SCREEN_INFO (*(struct screen_info *)0x90000)
#define RAMDISK_SIZE (*(unsigned short *)0x901F8)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
#define AUX_DEVICE_INFO (*(unsigned char *)0x901FF)

#define EOF -1

DECLARE(uch, inbuf, INBUFSIZ);
DECLARE(uch, outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
DECLARE(uch, window, WSIZE);

unsigned outcnt;
unsigned insize;
unsigned inptr;

static char *input_data;
static int input_len;
static int input_ptr;

static int output_ptr;

static char *output_data;
static int output_start;
static int output_size;
static int output_next_mod;

int method, exit_code, part_nb, last_member;
int test = 0;
int force = 0;
int verbose = 1;
long bytes_in, bytes_out;

int to_stdout = 0;
int hard_math = 0;

void (*work)(int inf, int outf);
void makecrc(void);

void (*do_flush)(void *data, int bytes);

local int get_method(int);

extern ulg crc_32_tab[];   /* crc table, defined below */

/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
ulg updcrc(s, n)
    uch *s;                 /* pointer to bytes to pump through */
    unsigned n;             /* number of bytes in s[] */
{
    register ulg c;         /* temporary variable */

    static ulg crc = (ulg)0xffffffffL; /* shift register contents */

    if (s == NULL) {
	c = 0xffffffffL;
    } else {
	c = crc;
	while (n--) {
	    c = crc_32_tab[((int)c ^ (*s++)) & 0xff] ^ (c >> 8);
	}
    }
    crc = c;
    return c ^ 0xffffffffL;       /* (instead of ~c for 64-bit machines) */
}

/* ===========================================================================
 * Clear input and output buffers
 */
void clear_bufs()
{
    outcnt = 0;
    insize = inptr = 0;
    bytes_in = bytes_out = 0L;
}

/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty
 * and at least one byte is really needed.
 */
int fill_inbuf()
{
    int len, i;

    /* Read as much as possible */
    insize = 0;
    do {
	len = INBUFSIZ-insize;
	if (len > (input_len-input_ptr+1)) len=input_len-input_ptr+1;
        if (len == 0 || len == EOF) break;

        for (i=0;i<len;i++) inbuf[insize+i] = input_data[input_ptr+i];
	insize += len;
	input_ptr += len;
    } while (insize < INBUFSIZ);

    if (insize == 0) {
	error("unable to fill buffer\n");
    }
    bytes_in += (ulg)insize;
    inptr = 1;
    return inbuf[0];
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
	if (outcnt == 0) return;
	updcrc(window, outcnt);

	(*do_flush)(window, outcnt);

	bytes_out += (ulg)outcnt;
	outcnt = 0;
}

/*
 * Code to compute the CRC-32 table. Borrowed from 
 * gzip-1.0.3/makecrc.c.
 */

ulg crc_32_tab[256];

void
makecrc(void)
{
/* Not copyrighted 1990 Mark Adler	*/

  unsigned long c;      /* crc shift register */
  unsigned long e;      /* polynomial exclusive-or pattern */
  int i;                /* counter for all possible eight bit values */
  int k;                /* byte being shifted into crc apparatus */

  /* terms of polynomial defining this crc (except x^32): */
  static int p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  /* Make exclusive-or pattern from polynomial */
  e = 0;
  for (i = 0; i < sizeof(p)/sizeof(int); i++)
    e |= 1L << (31 - p[i]);

  crc_32_tab[0] = 0;

  for (i = 1; i < 256; i++)
  {
    c = 0;
    for (k = i | 256; k != 1; k >>= 1)
    {
      c = c & 1 ? (c >> 1) ^ e : c >> 1;
      if (k & 1)
        c ^= e;
    }
    crc_32_tab[i] = c;
  }
}

void error(char *x)
{
	die(x);
}

#include "debug.h"
#include "boot.h"

struct multiboot_info *boot_info;
static struct multiboot_module *boot_mods;

struct multiboot_header boot_kern_hdr;
void *boot_kern_image;

static void flush_rest(void *data, int bytes)
{
	assert(output_start >= 0);
	assert(output_data != 0);
	assert(output_size > 0);

	/* See if this chunk of data contains anything useful.  */
	if (output_start < output_ptr + bytes)
	{
		void *src = data;
		void *dest = output_data + (output_ptr - output_start);
		int size = bytes;

		if (output_start > output_ptr)
		{
			src += output_start - output_ptr;
			size -= output_start - output_ptr;
			dest = output_data;
		}
		if (output_start + output_size < output_ptr + bytes)
		{
			size -= (output_ptr + bytes) - (output_start + output_size);
		}

		bcopy(src, dest, size);

		if (output_start + output_size <= output_ptr + bytes)
		{
			/* We're done here - move to the next boot module.  */
			if (output_next_mod >= boot_info->mods_count)
			{
				/* Is no next module.  */
				output_start = 0x70000000;
			}
			else
			{
				if (boot_mods[output_next_mod].reserved < output_start)
					die("misordered boot modules in boot image");
				output_size = boot_mods[output_next_mod].mod_end
						- boot_mods[output_next_mod].mod_start;
				output_data = (char*)phystokv(
					boot_mods[output_next_mod].mod_start);
				output_start = boot_mods[output_next_mod].reserved;
printf("module %d at %08x size %d\n", output_next_mod, kvtophys(output_data), output_size);

				boot_mods[output_next_mod].reserved = 0;

				output_next_mod++;
			}
		}
	}

	output_ptr += bytes;
}

static void flush_first(void *data, int bytes)
{
	struct zmod
	{
		vm_offset_t start, end;
		vm_offset_t cmdline;
	};
	struct zhdr
	{
		int magic;
		struct zmod zmods[0];
	};

	struct zhdr *z = (struct zhdr*)data;
	struct multiboot_header *h;
	int i;

	if (z->magic != 0xf00baabb)
		die("bad magic value in compressed boot module set");
	if (z->zmods[0].start == 0)
		die("compressed boot module set contains no modules");
	if (bytes < z->zmods[0].start + MULTIBOOT_SEARCH)
		die("not enough data decompressed in first block");

	/* Scan for the multiboot_header.  */
	for (i = z->zmods[0].start; ; i += 4)
	{
		if (i >= z->zmods[0].start + MULTIBOOT_SEARCH)
			die("kernel image has no multiboot_header");
		h = (struct multiboot_header*)(data+i);
		if (h->magic == MULTIBOOT_MAGIC
		    && !(h->magic + h->flags + h->checksum))
			break;
	}
	if (h->flags & MULTIBOOT_MUSTKNOW & ~MULTIBOOT_MEMORY_INFO)
		die("unknown multiboot_header flag bits %08x",
			h->flags & MULTIBOOT_MUSTKNOW & ~MULTIBOOT_MEMORY_INFO);
	boot_kern_hdr = *h;

	if (!(h->flags & MULTIBOOT_AOUT_KLUDGE))
		die("MULTIBOOT_AOUT_KLUDGE not set, and can't interpret the image header");
	printf("kernel at %08x-%08x text+data %d bss %d\n",
		h->load_addr, h->bss_end_addr,
		h->load_end_addr - h->load_addr,
		h->bss_end_addr - h->load_end_addr);
	if (h->load_addr < 0x1000)
		die("kernel image expects to be loaded too low");
	if ((boot_kern_hdr.load_addr < 0x100000)
	    && (boot_kern_hdr.bss_end_addr > 0xa0000))
		panic("kernel wants to be loaded on top of I/O space!");
	/* XXX other checks */

	/* Allocate memory to load the kernel into.
	   We do this before reserving the memory for the final kernel location,
	   because the code in boot_start.c to copy the kernel to its final location
	   can handle overlapping sources and destinations,
	   and this way we may not need as much memory during bootup.  */
	boot_kern_image = mustmalloc(h->load_end_addr - h->load_addr);

	/* Set up to decompress the kernel and boot module images.  */
	output_start = i + h->load_addr - h->header_addr;
	output_size = h->load_end_addr - h->load_addr;
	output_next_mod = 0;
	output_data = boot_kern_image;

	/* Reserve the memory that the kernel will occupy.
	   All malloc calls after this are guaranteed to stay out of this region.  */
	malloc_reserve(phystokv(h->load_addr), phystokv(h->bss_end_addr));

	/* Allocate memory for the boot_info structure and modules array.  */
	boot_info = (struct multiboot_info*)mustcalloc(sizeof(*boot_info));
	for (i = 1; z->zmods[i].start; i++);
	boot_info->mods_count = i-1;
	boot_mods = (struct multiboot_module*)mustcalloc(
		boot_info->mods_count * sizeof(*boot_mods));
	boot_info->mods_addr = kvtophys(boot_mods);

	/* Fill in the upper and lower memory size fields in the boot_info.  */
	boot_info->flags |= MULTIBOOT_MEMORY;
	{
		struct real_call_data rcd;

		/* Find the top of lower memory (up to 640K).  */
		rcd.flags = 0;
		real_int(0x12, &rcd);
		boot_info->mem_lower = rcd.eax & 0xFFFF;

		/* Find the top of extended memory (up to 64MB).  */
		rcd.eax = 0x8800;
		rcd.flags = 0;
		real_int(0x15, &rcd);
		boot_info->mem_upper = rcd.eax & 0xFFFF;
	}

	/* Build the kernel command line.  */
	boot_info->flags |= MULTIBOOT_CMDLINE;
	if (*((unsigned short*)phystokv(0x90020)) == 0xA33F) /* CL_MAGIC */
	{
		void *src = (void*)phystokv(0x90000 +
			*((unsigned short*)phystokv(0x90022)));
		void *dest = mustcalloc(2048);
		boot_info->cmdline = kvtophys(dest);
		memcpy(dest, src, 2048);
	}

	/* Initialize each boot module entry.  */
	boot_info->flags |= MULTIBOOT_MODS;
	for (i = 0; i < boot_info->mods_count; i++)
	{
		struct zmod *zm = &z->zmods[1+i];
		vm_size_t size = zm->end - zm->start;

		/* Allocate memory to load the module into.  */
		boot_mods[i].mod_start = kvtophys(mustmalloc(size));
		boot_mods[i].mod_end = boot_mods[i].mod_start + size;

		/* XXX command line */
		boot_mods[i].string = 0;

		/* Temporarily use reserved for the file offset of each module.
		   Remember to clear it again after we're done loading.  */
		boot_mods[i].reserved = zm->start;
	}

	/* Handle the rest of this block and every other block with flush_rest().  */
	do_flush = flush_rest;
	flush_rest(data, bytes);
}

void raw_start(void)
{
	extern char edata[];

	/* Find the zipped boot module package.  */
	input_data = (char*)phystokv(DEF_SYSSEG*16 + ((vm_offset_t)edata - 5*512));
	input_len = (DEF_INITSEG - DEF_SYSSEG)*16;

	/* Start decompressing - we'll deal with the output data on the fly.  */
	do_flush = flush_first;

	ALLOC(uch, inbuf, INBUFSIZ);
	ALLOC(uch, outbuf, OUTBUFSIZ+OUTBUF_EXTRA);
	ALLOC(uch, window, WSIZE);

	exit_code = 0;
	test = 0;
	input_ptr = 0;
	part_nb = 0;

	clear_bufs();
	makecrc();

	method = get_method(0);

	work(0, 0);

	boot_start();
}

void idt_irq_init()
{
}

/* ========================================================================
 * Check the magic number of the input file and update ofname if an
 * original name was given and to_stdout is not set.
 * Return the compression method, -1 for error, -2 for warning.
 * Set inptr to the offset of the next byte to be processed.
 * This function may be called repeatedly for an input file consisting
 * of several contiguous gzip'ed members.
 * IN assertions: there is at least one remaining compressed member.
 *   If the member is a zip file, it must be the only one.
 */
local int get_method(in)
    int in;        /* input file descriptor */
{
    uch flags;
    char magic[2]; /* magic header */

    magic[0] = (char)get_byte();
    magic[1] = (char)get_byte();

    method = -1;                 /* unknown yet */
    part_nb++;                   /* number of parts in gzip file */
    last_member = 0;
    /* assume multiple members in gzip file except for record oriented I/O */

    if (memcmp(magic, GZIP_MAGIC, 2) == 0
        || memcmp(magic, OLD_GZIP_MAGIC, 2) == 0) {

	work = unzip;
	method = (int)get_byte();
	flags  = (uch)get_byte();
	if ((flags & ENCRYPTED) != 0)
	    error("Input is encrypted\n");
	if ((flags & CONTINUATION) != 0)
	       error("Multi part input\n");
	if ((flags & RESERVED) != 0) {
	    error("Input has invalid flags\n");
	    exit_code = ERROR;
	    if (force <= 1) return -1;
	}
	(ulg)get_byte();	/* Get timestamp */
	((ulg)get_byte()) << 8;
	((ulg)get_byte()) << 16;
	((ulg)get_byte()) << 24;

	(void)get_byte();  /* Ignore extra flags for the moment */
	(void)get_byte();  /* Ignore OS type for the moment */

	if ((flags & EXTRA_FIELD) != 0) {
	    unsigned len = (unsigned)get_byte();
	    len |= ((unsigned)get_byte())<<8;
	    while (len--) (void)get_byte();
	}

	/* Get original file name if it was truncated */
	if ((flags & ORIG_NAME) != 0) {
	    if (to_stdout || part_nb > 1) {
		/* Discard the old name */
		while (get_byte() != 0) /* null */ ;
	    } else {
	    } /* to_stdout */
	} /* orig_name */

	/* Discard file comment if any */
	if ((flags & COMMENT) != 0) {
	    while (get_byte() != 0) /* null */ ;
	}
    } else
	error("unknown compression method");
    return method;
}
