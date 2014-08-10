#ifndef _IDE_MODES_H
#define _IDE_MODES_H
/*
 *  linux/drivers/block/ide_modes.h
 *
 *  Copyright (C) 1996  Linus Torvalds, Igor Abramov, and Mark Lord
 */

/*
 * Shared data/functions for determining best PIO mode for an IDE drive.
 * Most of this stuff originally lived in cmd640.c, and changes to the
 * ide_pio_blacklist[] table should be made with EXTREME CAUTION to avoid
 * breaking the fragile cmd640.c support.
 */

#if defined(CONFIG_BLK_DEV_CMD640) || defined(CONFIG_IDE_CHIPSETS)

#ifndef _IDE_C

int ide_scan_pio_blacklist (char *model);
unsigned int ide_get_best_pio_mode (ide_drive_t *drive);

#else /* _IDE_C */

/*
 * Black list. Some drives incorrectly report their maximal PIO mode,
 * at least in respect to CMD640. Here we keep info on some known drives.
 */
static struct ide_pio_info {
	const char	*name;
	int		pio;
} ide_pio_blacklist [] = {
/*	{ "Conner Peripherals 1275MB - CFS1275A", 4 }, */

	{ "WDC AC2700",  3 },
	{ "WDC AC2540",  3 },
	{ "WDC AC2420",  3 },
	{ "WDC AC2340",  3 },
	{ "WDC AC2250",  0 },
	{ "WDC AC2200",  0 },
	{ "WDC AC2120",  0 },
	{ "WDC AC2850",  3 },
	{ "WDC AC1270",  3 },
	{ "WDC AC1170",  3 },
	{ "WDC AC1210",  1 },
	{ "WDC AC280",   0 },
/*	{ "WDC AC21000", 4 }, */
	{ "WDC AC31000", 3 },
/*	{ "WDC AC21200", 4 }, */
	{ "WDC AC31200", 3 },
/*	{ "WDC AC31600", 4 }, */

	{ "Maxtor 7131 AT", 1 },
	{ "Maxtor 7171 AT", 1 },
	{ "Maxtor 7213 AT", 1 },
	{ "Maxtor 7245 AT", 1 },
	{ "Maxtor 7345 AT", 1 },
	{ "Maxtor 7546 AT", 3 },
	{ "Maxtor 7540 AV", 3 },

	{ "SAMSUNG SHD-3121A", 1 },
	{ "SAMSUNG SHD-3122A", 1 },
	{ "SAMSUNG SHD-3172A", 1 },

/*	{ "ST51080A", 4 },
 *	{ "ST51270A", 4 },
 *	{ "ST31220A", 4 },
 *	{ "ST31640A", 4 },
 *	{ "ST32140A", 4 },
 *	{ "ST3780A",  4 },
 */
	{ "ST5660A",  3 },
	{ "ST3660A",  3 },
	{ "ST3630A",  3 },
	{ "ST3655A",  3 },
	{ "ST3391A",  3 },
	{ "ST3390A",  1 },
	{ "ST3600A",  1 },
	{ "ST3290A",  0 },
	{ "ST3144A",  0 },

	{ "QUANTUM ELS127A", 0 },
	{ "QUANTUM ELS170A", 0 },
	{ "QUANTUM LPS240A", 0 },
	{ "QUANTUM LPS210A", 3 },
	{ "QUANTUM LPS270A", 3 },
	{ "QUANTUM LPS365A", 3 },
	{ "QUANTUM LPS540A", 3 },
	{ "QUANTUM FIREBALL", 3 }, /* For models 540/640/1080/1280 */
				   /* 1080A works fine in mode4 with triton */
	{ NULL,	0 }
};

/*
 * This routine searches the ide_pio_blacklist for an entry
 * matching the start/whole of the supplied model name.
 *
 * Returns -1 if no match found.
 * Otherwise returns the recommended PIO mode from ide_pio_blacklist[].
 */
int ide_scan_pio_blacklist (char *model)
{
	struct ide_pio_info *p;

	for (p = ide_pio_blacklist; p->name != NULL; p++) {
		if (strncmp(p->name, model, strlen(p->name)) == 0)
			return p->pio;
	}
	return -1;
}

/*
 * This routine returns the recommended PIO mode for a given drive,
 * based on the drive->id information and the ide_pio_blacklist[].
 * This is used by most chipset support modules when "auto-tuning".
 */
unsigned int ide_get_best_pio_mode (ide_drive_t *drive)
{
	unsigned int pio = 0;
	struct hd_driveid *id = drive->id;

	if (id != NULL) {
		if (HWIF(drive)->chipset != ide_cmd640 && !strcmp("QUANTUM FIREBALL1080A", id->model))
			pio = 4;
		else
			pio = ide_scan_pio_blacklist(id->model);
		if (pio == -1) {
			pio = (id->tPIO < 2) ? id->tPIO : 2;
			if (id->field_valid & 2) {
				byte modes = id->eide_pio_modes;
				if      (modes & 4)	pio = 5;
				else if (modes & 2)	pio = 4;
				else if (modes & 1)	pio = 3;
			}
		}
	}
	return pio;
}

#endif /* _IDE_C */
#endif /* defined(CONFIG_BLK_DEV_CMD640) || defined(CONFIG_IDE_CHIPSETS) */
#endif /* _IDE_MODES_H */
