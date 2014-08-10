#ifndef _LINUX_TTY_LDISC_H
#define _LINUX_TTY_LDISC_H

/*
 * Definitions for the tty line discipline
 */

#include <linux/fs.h>
#include <linux/wait.h>

struct tty_ldisc {
	int	magic;
	int	num;
	int	flags;
	/*
	 * The following routines are called from above.
	 */
	int	(*open)(struct tty_struct *);
	void	(*close)(struct tty_struct *);
	void	(*flush_buffer)(struct tty_struct *tty);
	int	(*chars_in_buffer)(struct tty_struct *tty);
	int	(*read)(struct tty_struct * tty, struct file * file,
			unsigned char * buf, unsigned int nr);
	int	(*write)(struct tty_struct * tty, struct file * file,
			 const unsigned char * buf, unsigned int nr);	
	int	(*ioctl)(struct tty_struct * tty, struct file * file,
			 unsigned int cmd, unsigned long arg);
	void	(*set_termios)(struct tty_struct *tty, struct termios * old);
	int	(*select)(struct tty_struct * tty, struct inode * inode,
			  struct file * file, int sel_type,
			  struct select_table_struct *wait);
	
	/*
	 * The following routines are called from below.
	 */
	void	(*receive_buf)(struct tty_struct *, const unsigned char *cp,
			       char *fp, int count);
	int	(*receive_room)(struct tty_struct *);
	void	(*write_wakeup)(struct tty_struct *);
};

#define TTY_LDISC_MAGIC	0x5403

#define LDISC_FLAG_DEFINED	0x00000001

#endif /* _LINUX_TTY_LDISC_H */
