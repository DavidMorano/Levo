/* mipsdis */

/* MIPS instruction disassembly object */


/* revision history:

	= 1998-11-01, David Morano

	Originally written for Audix Database Processor (DBP) work.


*/

/* Copyright � 1998 David A�D� Morano.  All rights reserved. */

#ifndef	MIPSDIS_INCLUDE
#define	MIPSDIS_INCLUDE	1


#include	<envstandards.h>

#include	<sys/types.h>

#include	<hdb.h>
#include	<localmisc.h>


/* object defines */

#define	MIPSDIS		struct mipsdis_head


struct mipsdis_flags {
	uint	disfile : 1 ;		/* we have a disassembly file */
	uint	server : 1 ;		/* we have a server */
} ;

struct mipsdis_entry {
	char	*std ;
	char	*levo ;
	uint	sig ;
	uint	instr ;
} ;

struct mipsdis_head {
	unsigned long	magic ;
	struct mipsdis_flags	f ;
	HDB		dis ;		/* all unique instr/dis pairs */
	HDB		addr, instr ;
	char		*server ;
	int		mode ;
} ;


typedef struct mipsdis_head	mipsdis ;


#if	(! defined(MIPSDIS_MASTER)) || (MIPSDIS_MASTER == 1)

#ifdef	__cplusplus
extern "C" {
#endif

extern int mipsdis_init(MIPSDIS *,char *,char *) ;
extern int mipsdis_getstd(MIPSDIS *,uint,uint,char *,int) ;
extern int mipsdis_getlevo(MIPSDIS *,uint,uint,char *,int) ;
extern int mipsdis_free(MIPSDIS *) ;

extern mipsdis	obj_mipsdis(int,int) ;
extern mipsdis	*new_mipsdis(int,int) ;
extern void	free_mipsdis(MIPSDIS *) ;

#ifdef	__cplusplus
}
#endif

#endif /* MIPSDIS_MASTER */

#endif /* MIPSDIS_INCLUDE */


