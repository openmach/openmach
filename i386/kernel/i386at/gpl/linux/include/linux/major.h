#ifndef _LINUX_MAJOR_H
#define _LINUX_MAJOR_H

/*
 * This file has definitions for major device numbers
 */

/* limits */

#define MAX_CHRDEV 64
#define MAX_BLKDEV 64

/*
 * assignments
 *
 * devices are as follows (same as minix, so we can use the minix fs):
 *
 *      character              block                  comments
 *      --------------------   --------------------   --------------------
 *  0 - unnamed                unnamed                minor 0 = true nodev
 *  1 - /dev/mem               ramdisk
 *  2 - /dev/ptyp*             floppy
 *  3 - /dev/ttyp*             ide0 or hd
 *  4 - /dev/tty*
 *  5 - /dev/tty; /dev/cua*
 *  6 - lp
 *  7 - /dev/vcs*
 *  8 -                        scsi disk
 *  9 - scsi tape
 * 10 - mice
 * 11 -                        scsi cdrom
 * 12 - qic02 tape
 * 13 -                        xt disk
 * 14 - sound card
 * 15 -                        cdu31a cdrom
 * 16 - sockets                goldstar cdrom
 * 17 - af_unix                optics cdrom
 * 18 - af_inet                sanyo cdrom
 * 19 - cyclades /dev/ttyC*
 * 20 - cyclades /dev/cub*     mitsumi (mcdx) cdrom
 * 21 - scsi generic
 * 22 -                        ide1
 * 23 -                        mitsumi cdrom
 * 24 -	                       sony535 cdrom
 * 25 -                        matsushita cdrom       minors 0..3
 * 26 -                        matsushita cdrom 2     minors 0..3
 * 27 - qic117 tape            matsushita cdrom 3     minors 0..3
 * 28 -                        matsushita cdrom 4     minors 0..3
 * 29 -                        aztech/orchid/okano/wearnes cdrom
 * 32 -                        philips/lms cm206 cdrom
 * 33 -                        ide2
 * 34 - z8530 driver           ide3
 * 36 - netlink
 */

#define UNNAMED_MAJOR	0
#define MEM_MAJOR	1
#define RAMDISK_MAJOR	1
#define FLOPPY_MAJOR	2
#define PTY_MASTER_MAJOR 2
#define IDE0_MAJOR	3
#define PTY_SLAVE_MAJOR 3
#define HD_MAJOR	IDE0_MAJOR
#define TTY_MAJOR	4
#define TTYAUX_MAJOR	5
#define LP_MAJOR	6
#define VCS_MAJOR	7
#define SCSI_DISK_MAJOR	8
#define SCSI_TAPE_MAJOR	9
#define MOUSE_MAJOR	10
#define SCSI_CDROM_MAJOR 11
#define QIC02_TAPE_MAJOR 12
#define XT_DISK_MAJOR	13
#define SOUND_MAJOR	14
#define CDU31A_CDROM_MAJOR 15
#define SOCKET_MAJOR	16
#define GOLDSTAR_CDROM_MAJOR 16
#define AF_UNIX_MAJOR	17
#define OPTICS_CDROM_MAJOR 17
#define AF_INET_MAJOR	18
#define SANYO_CDROM_MAJOR 18
#define CYCLADES_MAJOR  19
#define CYCLADESAUX_MAJOR 20
#define MITSUMI_X_CDROM_MAJOR 20
#define SCSI_GENERIC_MAJOR 21
#define Z8530_MAJOR 34
#define IDE1_MAJOR	22
#define MITSUMI_CDROM_MAJOR 23
#define CDU535_CDROM_MAJOR 24
#define STL_SERIALMAJOR 24
#define MATSUSHITA_CDROM_MAJOR 25
#define STL_CALLOUTMAJOR 25
#define MATSUSHITA_CDROM2_MAJOR 26
#define QIC117_TAPE_MAJOR 27
#define MATSUSHITA_CDROM3_MAJOR 27
#define MATSUSHITA_CDROM4_MAJOR 28
#define STL_SIOMEMMAJOR 28
#define AZTECH_CDROM_MAJOR 29
#define CM206_CDROM_MAJOR 32
#define IDE2_MAJOR	33
#define IDE3_MAJOR	34
#define NETLINK_MAJOR	36
#define IDETAPE_MAJOR	37

/*
 * Tests for SCSI devices.
 */

#define SCSI_MAJOR(M) \
  ((M) == SCSI_DISK_MAJOR	\
   || (M) == SCSI_TAPE_MAJOR	\
   || (M) == SCSI_CDROM_MAJOR	\
   || (M) == SCSI_GENERIC_MAJOR)

static inline int scsi_major(int m) {
	return SCSI_MAJOR(m);
}

#endif
