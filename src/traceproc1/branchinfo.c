/* branchinfo */

/* branch information gathering */


#define	CF_DEBUGS	0		/* compile-time debugging */
#define	CF_DEBUG	0		/* run-time debugging */
#define	CF_SAFE		1


/* revision history:

	= 2002-02-01, David Morano

	This was originally written.


*/

/* Copyright � 1998 David A�D� Morano.  All rights reserved. */

/*******************************************************************************

	This module gathers information on branches in a SS program
	execution.


*******************************************************************************/


#define	BRANCHINFO_MASTER	0


#include	<envstandards.h>

#include	<sys/types.h>
#include	<stdlib.h>
#include	<string.h>
#include	<math.h>

#include	<vsystem.h>
#include	<localmisc.h>

#include	"config.h"
#include	"defs.h"
#include	"branchinfo.h"


/* local defines */

#define	BRANCHINFO_MAGIC	0x96427312


/* external subroutines */

#if	defined(SOLARIS) && (SOLARIS == 8)
extern double	sqrt(double) ;
#endif

extern char	*strwcpy(char *,const char *,int) ;


/* forward references */

static int	brcmp(BRANCHINFO_ENTRY *,BRANCHINFO_ENTRY *) ;


/* exported subroutines */


int branchinfo_init(rfp,pip,size)
BRANCHINFO	*rfp ;
struct proginfo	*pip ;
int		size ;
{
	int	rs = SR_OK ;


	if (rfp == NULL)
	    return SR_FAULT ;

#if	CF_DEBUGS
	    debugprintf("branchinfo_init: entered size=%d\n",
	        size) ;
#endif

	(void) memset(rfp,0,sizeof(BRANCHINFO)) ;

	rfp->pip = pip ;

	if (size < 100)
		size = 100 ;

	rs = hdb_start(&rfp->bi,size,NULL,NULL) ;

	if (rs < 0)
		return rs ;


	rfp->magic = BRANCHINFO_MAGIC ;
	return rs ;

/* bad things come here */
bad1:

bad0:
	return rs ;
}
/* end subroutine (branchinfo_init) */


int branchinfo_free(rfp)
BRANCHINFO	*rfp ;
{
	struct proginfo	*pip ;


	if (rfp == NULL)
	    return SR_FAULT ;

	if (rfp->magic != BRANCHINFO_MAGIC)
	    return SR_NOTOPEN ;

	pip = rfp->pip ;

#if	CF_MASTERDEBUG && CF_DEBUG
	if (pip->debuglevel > 4)
	    debugprintf("branchinfo_free: entered\n") ;
#endif


	(void) hdb_finish(&rfp->bi) ;


#if	CF_MASTERDEBUG && CF_DEBUG
	if (pip->debuglevel > 4)
	    debugprintf("branchinfo_free: exiting\n") ;
#endif

	rfp->magic = 0 ;
	return SR_OK ;
}
/* end subroutine (branchinfo_free) */


/* identify the given branch (IA) as a hammock branch */
int branchinfo_hammock(rfp,ia,f_hammock)
BRANCHINFO	*rfp ;
uint		ia ;
int		f_hammock ;
{
	struct proginfo	*pip ;

	HDB_DATUM	key, value ;

	BRANCHINFO_ENTRY	e, *ep ;

	int	rs = SR_OK ;


#if	CF_SAFE
	if (rfp == NULL)
	    return SR_FAULT ;

	if (rfp->magic != BRANCHINFO_MAGIC)
	    return SR_NOTOPEN ;
#endif

	pip = rfp->pip ;

/* load 'em up */

	key.buf = &ia ;
	key.len = sizeof(uint) ;

/* check for */

	rs = hdb_fetch(&rfp->bi,key,NULL,&value) ;

	if (rs >= 0) {

		ep = (BRANCHINFO_ENTRY *) value.buf ;

		ep->f.hammock = TRUE ;

	}

	return rs ;
}
/* end subroutine (branchinfo_hammock) */


/* record a visit to this branch */
int branchinfo_visit(rfp,ia,ta,f_taken)
BRANCHINFO	*rfp ;
uint		ia, ta ;
int		f_taken ;
{
	struct proginfo	*pip ;

	HDB_DATUM	key, value ;

	BRANCHINFO_ENTRY	e, *ep ;

	int	rs, size ;


#if	CF_SAFE
	if (rfp == NULL)
	    return SR_FAULT ;

	if (rfp->magic != BRANCHINFO_MAGIC)
	    return SR_NOTOPEN ;
#endif

	pip = rfp->pip ;

#if	CF_MASTERDEBUG && CF_DEBUG && 0
	if (pip->debuglevel > 1)
	    debugprintf("branchinfo_visit: entered\n") ;
#endif

/* load 'em up */

	key.buf = &ia ;
	key.len = sizeof(uint) ;

/* check for */

	rs = hdb_fetch(&rfp->bi,key,NULL,&value) ;

	if (rs >= 0) {

		ep = (BRANCHINFO_ENTRY *) value.buf ;

		ep->visited += 1 ;
		if (f_taken)
			ep->taken += 1 ;

	} else {

/* allocate a new entry */

		size = sizeof(BRANCHINFO_ENTRY) ;
		rs = uc_malloc(size,&ep) ;

		if (rs < 0)
			return rs ;

		ep->ia = ia ;
		ep->ta = ta ;
		ep->visited = ep->taken = 0 ;
		ep->f.hammock = FALSE ;

		key.buf = &ep->ia ;
		key.len = sizeof(uint) ;

		value.buf = ep ;
		value.len = sizeof(BRANCHINFO_ENTRY) ;

		rs = hdb_store(&rfp->bi,key,value) ;

	} /* end if */


	return rs ;
}
/* end subroutine (branchinfo_visit) */



/* PRIVATE SUBROUTINES */



static int brcmp(e1p,e2p)
BRANCHINFO_ENTRY	*e1p, *e2p ;
{


	return (e1p->ia != e2p->ia) ? 1 : 0 ;
}
/* end subroutine (brcmp) */


