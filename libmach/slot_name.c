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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
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
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 *	File:	slot_name.c
 *	Author:	Avadis Tevanian, Jr.
 *	Date:	Feb 1987
 *
 *	Convert machine slot values to human readable strings.
 */

#include <mach.h>

/*
 *	Convert the specified cpu_type/cpu_subtype pair to their
 *	human readable form.
 */
slot_name(cpu_type, cpu_subtype, cpu_name, cpu_subname)
	cpu_type_t	cpu_type;
	cpu_subtype_t	cpu_subtype;
	char		**cpu_name, **cpu_subname;
{
	register char	*name, *subname;

	switch (cpu_type) {
	case CPU_TYPE_VAX:
		name = "VAX";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_VAX780:	subname = "780"; break;
		case CPU_SUBTYPE_VAX785:	subname = "785"; break;
		case CPU_SUBTYPE_VAX750:	subname = "750"; break;
		case CPU_SUBTYPE_VAX730:	subname = "730"; break;
		case CPU_SUBTYPE_UVAXI:		subname = "MicroVax I"; break;
		case CPU_SUBTYPE_UVAXII:	subname = "MicroVax II"; break;
		case CPU_SUBTYPE_VAX8200:	subname = "8200"; break;
		case CPU_SUBTYPE_VAX8500:	subname = "8500"; break;
		case CPU_SUBTYPE_VAX8600:	subname = "8600"; break;
		case CPU_SUBTYPE_VAX8650:	subname = "8650"; break;
		case CPU_SUBTYPE_VAX8800:	subname = "8800"; break;
		case CPU_SUBTYPE_UVAXIII:	subname = "MicroVax III"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_ROMP:
		name = "ROMP";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_RT_PC:		subname = "RT/PC"; break;
		case CPU_SUBTYPE_RT_APC:	subname = "RT/APC"; break;
		case CPU_SUBTYPE_RT_135:	subname = "RT/135"; break;
		default:	      subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_MC68020:
		name = "MC68020";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_SUN3_50:	subname = "Sun-3/50"; break;
		case CPU_SUBTYPE_SUN3_160:	subname = "Sun-3/160"; break;
		case CPU_SUBTYPE_SUN3_260:	subname = "Sun-3/260"; break;
		case CPU_SUBTYPE_SUN3_110:	subname = "Sun-3/110"; break;
		case CPU_SUBTYPE_SUN3_60:	subname = "Sun-3/60"; break;
		case CPU_SUBTYPE_HP_320:	subname = "HP 320"; break;
		case CPU_SUBTYPE_HP_330:	subname = "HP 330"; break;
		case CPU_SUBTYPE_HP_350:	subname = "HP 350"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_NS32032:
		name = "NS32032";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_MMAX_DPC: subname = "Multimax [DPC]"; break;
		case CPU_SUBTYPE_SQT:	subname = "Balance 21000"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_NS32332:
		name = "NS32332";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_MMAX_APC_FPU: subname = "Multimax [APC]";
							break;
		case CPU_SUBTYPE_MMAX_APC_FPA:
			subname = "Multimax [APC] with FPA"; break;
		default:	subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_NS32532:
		name = "NS32532";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_MMAX_XPC: subname = "Multimax [XPC]"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_I386:
		name = "I386";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_AT386: subname = "AT"; break;
		case CPU_SUBTYPE_EXL: subname = "EXL"; break;
		case CPU_SUBTYPE_iPSC386: subname = "PSC386"; break;
		case CPU_SUBTYPE_SYMMETRY: subname = "Symmetry"; break;
		default: 	 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_MIPS:
		name = "MIPS";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_MIPS_R2300: subname = "R2300"; break;
		case CPU_SUBTYPE_MIPS_R2600: subname = "R2600"; break;
		case CPU_SUBTYPE_MIPS_R2800: subname = "R2800"; break;
		case CPU_SUBTYPE_MIPS_R2000a: subname = "R2000a"; break;
		case CPU_SUBTYPE_MIPS_R2000: subname = "R2000"; break;
		case CPU_SUBTYPE_MIPS_R3000a: subname = "R3000a"; break;
		case CPU_SUBTYPE_MIPS_R3000: subname = "R3000"; break;
		default: 	    subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_MC68030:
		name = "MC68030";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_NeXT: subname = "NeXT"; break;
		case CPU_SUBTYPE_HP_340: subname = "HP 340"; break;
		case CPU_SUBTYPE_HP_360: subname = "HP 360"; break;
		case CPU_SUBTYPE_HP_370: subname = "HP 370"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_MC68040:
		name = "MC68040";
		switch (cpu_subtype) {
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_HPPA:
		name = "HPPA";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_HPPA_825: subname = "HP 825"; break;
		case CPU_SUBTYPE_HPPA_835: subname = "HP 835"; break;
		case CPU_SUBTYPE_HPPA_840: subname = "HP 840"; break;
		case CPU_SUBTYPE_HPPA_850: subname = "HP 850"; break;
		case CPU_SUBTYPE_HPPA_855: subname = "HP 855"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_ARM:
		name = "ARM";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_ARM_A500_ARCH: subname = "A500 Arch"; break;
		case CPU_SUBTYPE_ARM_A500: subname = "A500"; break;
		case CPU_SUBTYPE_ARM_A440: subname = "A440"; break;
		case CPU_SUBTYPE_ARM_M4: subname = "M4"; break;
		case CPU_SUBTYPE_ARM_A680: subname = "A680"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_MC88000:
		name = "MC88000";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_MMAX_JPC: subname = "Multimax [JPC]"; break;
		case CPU_SUBTYPE_LUNA88K: subname = "Omron Luna"; break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_SPARC:
		name = "SPARC";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_SUN4_260:	subname = "Sun-4/260"; break;
		case CPU_SUBTYPE_SUN4_110:	subname = "Sun-4/110"; break;
		case CPU_SUBTYPE_SUN4_330:	subname = "Sun-4/330"; break;
		case CPU_SUBTYPE_SUN4C_60:	subname = "Sun-4/60";  break;
		case CPU_SUBTYPE_SUN4C_65:	subname = "Sun-4/65";  break;
		case CPU_SUBTYPE_SUN4C_20:	subname = "Sun-4/20";  break;
		case CPU_SUBTYPE_SUN4C_30:	subname = "Sun-4/30";  break;
		case CPU_SUBTYPE_SUN4C_40:	subname = "Sun-4/40";  break;
		case CPU_SUBTYPE_SUN4C_50:	subname = "Sun-4/50";  break;
		case CPU_SUBTYPE_SUN4C_75:	subname = "Sun-4/75";  break;
		default: 		 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_I860:
		name = "I860";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_iPSC860: subname = "iPSC"; break;
		case CPU_SUBTYPE_OKI860: subname = "OKI"; break;
		default: 	 subname = "Unknown Subtype";  break;
		}
		break;
	case CPU_TYPE_ALPHA:
		name = "alpha";
		switch (cpu_subtype) {
		case CPU_SUBTYPE_ALPHA_EV3: subname = "EV3"; break;
		case CPU_SUBTYPE_ALPHA_EV4: subname = "EV4"; break;
		case CPU_SUBTYPE_ALPHA_ISP: subname = "ISP"; break;
		case CPU_SUBTYPE_ALPHA_21064: subname = "21064"; break;
		default: 	 subname = "Unknown Subtype";  break;
		}
		break;
	default:
		name = "Unknown CPU";
		subname = "";
		break;
	}
	*cpu_name = name;
	*cpu_subname = subname;
}
