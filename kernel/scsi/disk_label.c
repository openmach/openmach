/*
 * Copyright (c) 1996 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Kevin T. Van Maren, University of Utah CSL
 */

/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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
 * Copyright (c) 1994 Shantanu Goel
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE AUTHOR ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE AUTHOR DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 */

/* This file contains the partition code that is used by the Mach
 * device drivers (ide & scsi).  */

#include <scsi/compat_30.h>
#include <sys/types.h> 

#include <scsi/rz_labels.h>
#include <i386at/disk.h>	/* combine & rename these... */

#define SECTOR_SIZE 512		/* BAD!!! */

#define DSLICE(dev) ((dev >> 4) & 0x3f)
#define DPART(dev) (dev & 0xf)

/* note: 0 will supress ALL output;
	1 is 'normal' output; 2 is verbose; 3 is Very verbose */
#define PARTITION_DEBUG 1

#define min(x,y) (x<y?x:y)

/*
 * Label that is filled in with extra info
 */
struct disklabel default_label =
{
        DISKMAGIC, DTYPE_SCSI, 0,
        "SCSI", "",
        DEV_BSIZE, 1, 1, 1, 1, 1, 0, 0, 0,
        3600, 1, 1, 1, 0, 0, 0,
        {0,}, {0,},
        DISKMAGIC, 0,
        8, 8192, 8192,
        {{ -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 },
         { -1, 0, 1024, FS_BSDFFS, 8, 3 }}
};
 


/* the device driver calls this just to save some info it got from the HW */
/* This is a bad holdover from the disklabel days, and needs to go */
fudge_bsd_label(struct disklabel *label, int type, int total_secs, int heads, int sectors, int sectorsize, int n)
{
	*label=default_label;

        label->d_ncylinders = total_secs/(heads*sectors);
        label->d_ntracks = heads; 
        label->d_nsectors = sectors;

        label->d_secpercyl = heads*sectors;
        label->d_secperunit = total_secs;

	/* this is never used, but ... */
        label->d_partitions[MAXPARTITIONS].p_offset = 0;
        label->d_partitions[MAXPARTITIONS].p_size = total_secs;
        
        /* ??   
         */             
        label->d_secsize = sectorsize;
        label->d_type = type;
        label->d_subtype = 0xa;  /* ??? */
                        
        label->d_npartitions = n;               /* up to 'c' */ 
        label->d_checksum = 0;

	/* should do a checksum on it now */
}
        


/* This is placed here to
	a. provide comparability with existing servers
	b. allow the use of FreeBSD-style slices to access ANY disk partition
	c. provide an easy migration path to lites-based partition code
		by only passing the drive name to get the entire disk (sd0).

   This will be called by every routine that needs to access partition info
   based on a device number.  It is slower than the old method of indexing
   into a disklabel, but is more flexible, and reasonably fast in the (future)
   case where Lites will access the whole disk.  An array of disklabels
   could have been used, but this is more compact and general.  The underlying
   structure does not limit it to 2-levels, but keeping the kernel interface
   simple does. */


/* this code and data structure based on conversation with Bryan Ford */
/* Note: this is called ON EVERY read or write.  It makes sense to
   optimize this for the common case.  Hopefully the common case
   will become the '0,0' case, as partitioning is moved out of the
   kernel.  (Downside is kernel can't protect filesystems from each other).
   It is slower than indexing into a 1-D array, but not much. */

struct diskpart *lookup_part(struct diskpart *array, int dev_number)
{
/* Note: 10 bit encoding to get partitions 0-15 (0,a-h typically), and slices
 * 0-63 
 */

        int slice = DSLICE(dev_number);
        int part = DPART(dev_number);
        struct diskpart *s;

        if (slice == 0) 	/* compatability slice */
        {
                if (part == 0)	/* whole disk */
                        return &array[0];

                if (array[0].type == DISKPART_DOS)
                {
                        int i;
                        for (i = 0; i < array[0].nsubs; i++)
                        {
                                s = &array[0].subs[i];
                                if (   s->type == DISKPART_BSD
				    || s->type == DISKPART_VTOC)
                                {
                                        if (part > s->nsubs)
                                                return 0;
                                        return (&s->subs[part-1]);
                                }
                        }
		}

                if (part > array[0].nsubs)
                        return 0;
                return(&array[0].subs[part-1]);
        }
        else
        {
                if (   array[0].type != DISKPART_DOS
		    || slice > array[0].nsubs)
                        return 0;

                s = &array[0].subs[slice-1];

                if (part == 0)	/* whole slice */
                        return (s);
                if (part > s->nsubs)
                        return 0;

                return (&s->subs[part-1]);
        }
}




static inline void fill_array(struct diskpart *array, int start, int size, 
		struct diskpart *subs, int nsubs, short type, short fsys)
{
	array->start=start;
	array->size=size;
	array->subs=subs;
	array->nsubs=nsubs;
	array->type=type;
	array->fsys=fsys;
#if (PARTITION_DEBUG > 2) 
	printf("fill: type %d:%d, start %d, size %d, %d parts\n",type,fsys,
			start,size,nsubs);
#endif
}




void print_array(struct diskpart *array, int level)
{
	int i,j;
	struct diskpart *subs;

#if (PARTITION_DEBUG)
	subs=array[0].subs;

        for (i=0;i<array[0].nsubs;i++) {
		for (j=0;j<level;j++)
			printf("  ");
		printf("%c: %d, %d, %d, %d (%d subparts)\n",'a'+i,
			subs[i].start, subs[i].size, subs[i].fsys, 
			subs[i].type, subs[i].nsubs);
		if (subs[i].nsubs>0)
			print_array(&subs[i], level+1);
	}
#endif
}



/* individual routines to find the drive labels.
   There needs to be a function for every different method for partitioning
   much of the following code is derived from the SCSI/IDE drivers */

int get_dos(struct diskpart *array, char *buff, int start,
		void *driver_info, int (*bottom_read_fun)(),
		char *name, int max_part)
{

	bios_label_t *mp;
	struct bios_partition_info *pp;

	int count, i, j;
	int pstart, psize;
	int ext=-1, mystart=start, mybase;
	int first=1;

	/* note: start is added, although a start != 0 is meaningless
	   to DOS and anything else... */

	/* check the boot sector for a partition table. */
        (*bottom_read_fun)(driver_info, start, buff);  /* always in sector 0 */

        /*
         * Check for valid partition table.
         */
        mp = (bios_label_t *)&buff[BIOS_LABEL_BYTE_OFFSET];
        if (mp->magic != BIOS_LABEL_MAGIC) {
#if (PARTITION_DEBUG>1)
                printf("%s invalid partition table\n", name);
#endif
                return(0);   /* didn't add any partitions */
        }
#if (PARTITION_DEBUG>1)
	printf("DOS partition table found\n");
#endif 

	count=min(4,max_part);	/* always 4 (primary) partitions */
#if (PARTITION_DEBUG)
	if (count<4) printf("More partitions than space!\n");
#endif


	/* fill the next 4 entries in the array */
	for (i=0, pp=(struct bios_partition_info *)mp->partitions;
		i<count; i++,pp++) {

		fill_array(&array[i], pp->offset, pp->n_sectors, NULL, 0, 
			DISKPART_NONE, pp->systid);
		if ((pp->systid == DOS_EXTENDED) &&(ext<0)) {
			mystart+=pp->offset; 
			ext=i;
		}
	}

	/* if there is an extended partition, find all the logical partitions */
	/* note: logical start at '5' (extended is one of the numbered 1-4) */

	/* logical partitions 'should' be nested inside the primary, but
	   then it would be impossible to NAME a disklabel inside a logical
	   partition, which would be nice to do */
#if (PARTITION_DEBUG>1)
	if (ext>=0)
		printf("extended partition found: %d\n",ext);
#endif 0

	while (ext>=0) {
		pp = &(((struct bios_partition_info *)mp->partitions)[ext]);

		/* read the EXTENDED partition table */
		if (first) {
			mybase=mystart;
			first=0;
		} else {
			mybase=mystart+pp->offset;
		}

		(*bottom_read_fun)(driver_info, mybase, buff);

        	if (mp->magic != BIOS_LABEL_MAGIC) {
#if (PARTITION_DEBUG>1)
        	        printf("%s invalid expanded magic\n", name);
#endif 
               		return(count);/*don't add any more partitions*/
        	}

		/* just in case more than one partition is there...*/
		/* search the EXTENDED partition table */
		ext=-1;
         	for (j=0,pp=(struct bios_partition_info *)mp->partitions;
               		j<4; j++,pp++) {    

			if (pp->systid && (pp->systid!=DOS_EXTENDED)) {
			   if (count<max_part) {
				fill_array(&array[count], 
				    mybase +pp->offset, 
				    pp->n_sectors, NULL, 0, DISKPART_NONE, 
				    pp->systid);
				count++; }
			    else {
#if (PARTITION_DEBUG)
				printf("More partitions than space!\n");
#endif
				return(count);
			    }
			} else if ((ext<0) &&(pp->systid==DOS_EXTENDED)) {
				ext=j;
				/* recursivly search the chain here */
			}
       		}
	}
#if (PARTITION_DEBUG>1)
	printf("%d dos partitions\n",count);
#endif 0
	return(count);  /* number dos partitions found */

}



/* this should work on the bare drive, or in a dos partition */
int get_disklabel(struct diskpart *array, char *buff, int start,
		void *driver_info, int (*bottom_read_fun)(),
		char *name, int max_part) 
{
	struct disklabel *dlp;
	int mybase = start + (512 * LBLLOC)/SECTOR_SIZE, i;
	int count;

        (*bottom_read_fun)(driver_info, mybase, buff); 
                
        dlp = (struct disklabel *)buff;
        if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
#if (PARTITION_DEBUG>1)
                printf("%s no BSD label found\n",name);
#endif
		return(0);  /* no partitions added */
        }
#if (PARTITION_DEBUG>1)
        printf(" BSD LABEL\n");
#endif 0
	/* note: BSD disklabel offsets are from start of DRIVE -- uuggh */

	count=min(8,max_part);  /* always 8 in a disklabel */
#if (PARTITION_DEBUG)   
	if (count<8) printf("More partitions than space!\n");
#endif
        /* COPY into the array */
	for (i=0;i<count;i++)
		fill_array(&array[i], /* mybase + */ 
			dlp->d_partitions[i].p_offset,
			dlp->d_partitions[i].p_size,
			NULL, 0, DISKPART_NONE, dlp->d_partitions[i].p_fstype);

		/* note: p_fstype is not the same set as the DOS types */
                        
	return(count);  /* 'always' 8 partitions in disklabel -- if space */

/* UNREACHED CODE FOLLOWS: (alternative method in scsi) */
#if 0
        (*bottom_read_fun)(driver_info, (start)+LABELSECTOR, buff);

                register int    j;
                boolean_t       found;

                for (j = LABELOFFSET, found = FALSE;
                     j < (SECTOR_SIZE-sizeof(struct disklabel));
                     j += sizeof(int)) {
                        search =  (struct disklabel *)&buff[j];
                        if (search->d_magic  == DISKMAGIC &&
                            search->d_magic2 == DISKMAGIC) {
                                found = TRUE;
                                break;
                        }
                }
                if (found) {
#if (PARTITION_DEBUG>1)
                        printf("Label found in LABELSECTOR\n");
#endif
                } else {
                        search = 0;
		}

          
#endif 0

}


/* NOT TESTED! */
/* VTOC in sector 29 */
int get_vtoc(struct diskpart *array, char *buff, int start,
		void *driver_info, int (*bottom_read_fun)(),
		char *name, int max_part) 
{
	struct evtoc *evp;
	int n,i;
	struct disklabel lpl;
	struct disklabel *lp = &lpl;

#if (PARTITION_DEBUG)
        printf("Read VTOC.\n");
#endif
        (*bottom_read_fun)(driver_info, start +PDLOCATION, buff);
        evp = (struct evtoc *)buff;
        if (evp->sanity != VTOC_SANE) {
#if (PARTITION_DEBUG) 
                printf("%s evtoc corrupt or not found\n", name);
#endif
		return(0);
        }
	n = min(evp->nparts,max_part); /* no longer DISKLABEL limitations... */
#if 0
        n = (evp->nparts > MAXPARTITIONS) ? MAXPARTITIONS : evp->nparts;
#endif 0
   
        for (i = 0; i < n; i++)
		fill_array(&array[i], /* mybase + */ 
			evp->part[i].p_start,
			evp->part[i].p_size,
			NULL, 0, DISKPART_NONE, FS_BSDFFS);

	return(n);  /* (evp->nparts) */
}


/* NOT TESTED! */
int get_omron(struct diskpart *array, char *buff, int start,
		void *driver_info, int (*bottom_read_fun)(),
		char *name, int max_part) 
{

        struct disklabel        *label;

        /* here look for an Omron label */
        register omron_label_t  *part;
        int                             i;

#if (PARTITION_DEBUG) 
        printf("Looking for Omron label...\n");
#endif

        (*bottom_read_fun)(driver_info, start+
		OMRON_LABEL_BYTE_OFFSET/SECTOR_SIZE, buff);

        part = (omron_label_t*)&buff[OMRON_LABEL_BYTE_OFFSET%SECTOR_SIZE];
        if (part->magic == OMRON_LABEL_MAGIC) {
#if (PARTITION_DEBUG) 
                printf("{Using OMRON label}");
#endif
                for (i = 0; i < 8; i++) {
                        label->d_partitions[i].p_size = part->partitions[i].n_sectors;
                        label->d_partitions[i].p_offset = part->partitions[i].offset;
                }
                bcopy(part->packname, label->d_packname, 16);
                label->d_ncylinders = part->ncyl;
                label->d_acylinders = part->acyl;
                label->d_ntracks = part->nhead;
                label->d_nsectors = part->nsect;
                /* Many disks have this wrong, therefore.. */
#if 0
                label->d_secperunit = part->maxblk;
#else
                label->d_secperunit = label->d_ncylinders * label->d_ntracks *
                                        label->d_nsectors;
#endif 0
   
                return(8); 
        }
#if (PARTITION_DEBUG) 
        printf("No Omron label found.\n");
#endif
	return(0);
}


/* NOT TESTED! */
int get_dec(struct diskpart *array, char *buff, int start,
		void *driver_info, int (*bottom_read_fun)(),
		char *name, int max_part) 
{
        struct disklabel        *label;

        /* here look for a DEC label */
        register dec_label_t    *part;
        int                             i;
  
#if (PARTITION_DEBUG) 
        printf("Checking for dec_label...\n");
#endif

        (*bottom_read_fun)(driver_info, start +
			DEC_LABEL_BYTE_OFFSET/SECTOR_SIZE, buff); 
        
        if (part->magic == DEC_LABEL_MAGIC) {
#if (PARTITION_DEBUG) 
                printf("{Using DEC label}");
#endif
                for (i = 0; i < 8; i++) {
                        label->d_partitions[i].p_size = part->partitions[i].n_sectors;
                        label->d_partitions[i].p_offset = part->partitions[i].offset;
                }
  		return(8); 
        }
#if (PARTITION_DEBUG) 
        printf("No dec label found.\n");
#endif

        return(0);
}




/* array is a pointer to an array of partition_info structures */
/* array_size is the number of pre-allocated entries there are */
int get_only_partition(void *driver_info, int (*bottom_read_fun)(),
                        struct diskpart *array, int array_size,
                        int disk_size, char *drive_name)
{
	char buff[SECTOR_SIZE];
	int i,n,cnt;
	int arrsize;
	struct diskpart *res;

	/* first fill in the entire disk stuff */
	/* or should the calling routine do that already? */

	fill_array(array, 0, disk_size, NULL, 0, -1, -1);

	/* while the structure does not preclude additional nestings,
	   additional ones make no sense currently, so they are not
	   checked (Mach can't handle them anyway).  It might be nice
	   if for all partitions found, all types of sub-partitions
	   were looked for (unnecessary).  This will be done when this
           is moved out of ther kernel, and there is some way to name them */

	arrsize = array_size -1;  /* 1 for whole disk */

	/* search for dos partition table */
	/* find all the partitions (including logical) */
	n=get_dos(&array[1], buff, 0,
                driver_info, (bottom_read_fun), drive_name,
		arrsize);

	if (n>0) {
		fill_array(array, 0, disk_size, &array[1], n, 
			DISKPART_DOS, 256+DISKPART_DOS);
		arrsize-=n;


		/* search each one for a BSD disklabel (iff BSDOS) */
		/* note: searchine extended and logical partitions */
		for (i=0;i<n;i++)
			if (array[i+1].fsys==BSDOS) {
#if (PARTITION_DEBUG) 
				printf("BSD OS slice: %d\n",i+1);
#endif
				cnt=get_disklabel(&array[n+1], buff, 
					array[i+1].start,
                			driver_info, (bottom_read_fun),
                			drive_name,arrsize);

				if (cnt>0) {
					arrsize-=cnt;
					fill_array(&array[i+1],array[i+1].start,
						array[i+1].size, &array[n+1], 
						cnt, DISKPART_BSD,
						array[i+1].fsys);
				}
				n+=cnt;
			}

		/* search for VTOC -- in a DOS partition as well */
		for (i=0;i<n;i++)
			if (array[i+1].fsys==UNIXOS) {
#if (PARTITION_DEBUG) 
				printf("UNIXOS (vtoc) partition\n");
#endif
                                cnt=get_vtoc(&array[n+1], buff,   
                                        array[i+1].start,
                                        driver_info, (bottom_read_fun),
                                        drive_name,arrsize);

                                if (cnt>0) {
					arrsize-=cnt;
                                        fill_array(&array[i+1],array[i+1].start,
                                                array[i+1].size, &array[n+1],
                                                cnt, DISKPART_VTOC,
						array[i+1].fsys);
				}
				n+=cnt;
			}
	}

	/* search for only disklabel */
	if (n==0) {
		fill_array(array, 0, disk_size, &array[1], n, DISKPART_BSD,
			256+DISKPART_BSD);
		n=get_disklabel(&array[1], buff, 0, driver_info, 
			(bottom_read_fun), drive_name,arrsize);
	}

	/* search for only VTOC -- NOT TESTED! */
	if (n==0) {
		fill_array(array, 0, disk_size, &array[1], n, DISKPART_VTOC,
			256+DISKPART_VTOC);
		n=get_vtoc(&array[1], buff, 0, driver_info, (bottom_read_fun), 
                	drive_name,arrsize);
	}
#if 0
	/* search for only omron -- NOT TESTED! */
	if (n==0) {
		fill_array(array, 0, disk_size, &array[1], n, DISKPART_OMRON,
			256+DISKPART_OMRON);
		n=get_omron(&array[1], buff, 0,driver_info, (bottom_read_fun), 
                	drive_name,arrsize);
	}

	/* search for only dec -- NOT TESTED! */
	if (n==0) {
		fill_array(array, 0, disk_size, &array[1], n, DISKPART_DEC,
			256+DISKPART_DEC);
		n=get_dec(&array[1], buff, 0, driver_info, (bottom_read_fun), 
                	drive_name,arrsize);
	}
#endif 0

#if (PARTITION_DEBUG)	/* print out what we found */
	print_array(array,0);
#endif

}


