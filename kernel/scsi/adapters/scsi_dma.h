/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS AS-IS
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
 *	File: scsi_dma.h
 * 	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	7/91
 *
 *	DMA operations that an HBA driver might invoke.
 *
 */

/*
 * This defines much more than usually needed, mainly
 * to cover for the case of no DMA at all and/or only
 * DMA from/to a specialized buffer ( which means the
 * CPU has to copy data into/outof it ).
 */

typedef struct {
	opaque_t	(*init)(
				int 		dev_unit,
				vm_offset_t 	base,
				int		*dma_bsizep,
				boolean_t	*oddbp);

	void		(*new_target)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	void		(*map)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	int		(*start_cmd)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	void		(*end_xfer)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	void		(*end_cmd)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				io_req_t	ior);

	int		(*start_datain)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	int		(*start_msgin)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	void		(*end_msgin)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	boolean_t	(*start_dataout)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				volatile unsigned *regp,
				unsigned	value,
				unsigned char	*prefetch_count);

	int		(*restart_datain_1)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	int		(*restart_datain_2)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	void		(*restart_datain_3)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	int		(*restart_dataout_1)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	int		(*restart_dataout_2)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	int		(*restart_dataout_3)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				volatile unsigned *regp);

	void		(*restart_dataout_4)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	boolean_t	(*disconn_1)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	boolean_t	(*disconn_2)(
				opaque_t	dma_state,
				target_info_t	*tgt);

	boolean_t	(*disconn_3)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	boolean_t	(*disconn_4)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	boolean_t	(*disconn_5)(
				opaque_t	dma_state,
				target_info_t	*tgt,
				int		xferred);

	void		(*disconn_callback)(
				opaque_t	dma_state,
				target_info_t	*tgt);

} scsi_dma_ops_t;

