/* 
 * This controls whether or not we use a serial line for the console
 * (ie, remote console).
 */

/*
 * Values for RCLINE:
 *	-1 = disable
 *	 0 = port 0x3f8/irq 4 (DOS COM1)
 *	 1 = port 0x2f8/irq 3 (DOS COM2)
 *	 2 = port 0x3e8/irq 5 (DOS COM3)
 *	 3 = port 0x2e8/irq 9 (DOS COM4)
 */

#define RCLINE	-1		/* com port for console */
#define RCADDR	0x3f8		/* where it is */
