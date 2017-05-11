/* memstats */

/* maintain register statistics */
/* last modified %G% version %I% */


#define	CF_DEBUGS	0		/* non-switchable debugs */
#define	CF_DEBUGS2	0		/* non-switchable of next order */
#define	CF_SAFE		1		/* run in "safe" mode */
#define	CF_ASSERTS	1		/* include 'assert()'s in code */
#define	CF_TESTGROUP	0		/* test group allocation */


/* revision history:

	= 02/08/21, Dave Morano

	This program was originally written.


*/

/* Copyright � 2003-2007 David A�D� Morano.  All rights reserved. */

/**************************************************************************

	This object module tracks certain statistics about the
	use of memory.


**************************************************************************/


#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<sys/mman.h>
#include	<limits.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<assert.h>

#if	defined(IRIX)
#include	<standards.h>
#endif

#include	<vsystem.h>
#include	<mallocstuff.h>
#include	<bio.h>

#include	"localmisc.h"
#include	"memstats.h"



/* local defines */

#define	MEMSTATS_MAGIC		0x93726514
#define	MEMSTATS_DEFPAGESIZE	(8 * 1024)
#define	MEMSTATS_DEFLENDEN	(256 * 1024)
#define	MEMSTATS_ADDRBITS	64
#define	MEMSTATS_NGROUPS	200

#if	CF_TESTGROUP
#define	MEMSTATS_DEFGROUPSIZE	(4 * 1024)
#else
#define	MEMSTATS_DEFGROUPSIZE	(256 * 1024)
#endif

#if	defined(IRIX)
#ifndef	CF_ULOCK
#define CF_ULOCK	0	/* Unlock a previously locked region */
#endif
#ifndef	CF_LOCK
#define CF_LOCK	1	/* Lock a region for exclusive use */
#endif
#ifndef	CF_TLOCK
#define CF_TLOCK	2	/* Test and lock a region for exclusive use */
#endif
#ifndef	CF_TEST
#define CF_TEST	3	/* Test a region for other processes locks */
#endif
#endif /* defined(IRIX) */

#undef	PAGESIZE
#define	PAGESIZE	(8 * 1024)



/* external subroutines */

extern uint	elfhash(const char *,int) ;
extern uint	nextpowtwo(uint) ;

extern int	mkfname2(char *,const char *,const char *) ;
extern int	flbsi(uint) ;
extern int	fmeanvarai(uint *,int,double *,double *) ;
extern int	fmeanvaral(ULONG *,int,double *,double *) ;
extern int	denpercentsi(uint *,int,double *) ;
extern int	denpercentsl(ULONG *,int,double *) ;


/* external variables */


/* local structures */

struct percentages {
	double	p7, p8, p9 ;
} ;


/* forward references */

static ULONG countden(ULONG *,int) ;

static int memstats_lookup(MEMSTATS *,ULONG, struct memstats_te **) ;
static int memstats_allocgroup(MEMSTATS *,struct memstats_te **) ;
static int memstats_allocpage(MEMSTATS *,struct memstats_te **) ;

static int	writearray(const char *,ULONG *,int) ;

#ifdef	COMMENT
static uint	ourhash(HDB_DATUM *) ;
#endif


/* global variables */








int memstats_init(op,fname,groupsize,pagesize,elemsize,lenden)
MEMSTATS	*op ;
char		fname[] ;
int		groupsize, pagesize ;
int		elemsize ;
int		lenden ;
{
	long	syspagesize ;

	int	rs, i ;
	int	size ;
	int	n, ngroups ;
	int	oflags ;


	if (op == NULL)
	    return SR_FAULT ;

	(void) memset(op,0,sizeof(MEMSTATS)) ;

	op->sumrint = 0.0 ;
	op->sumuse = 0.0 ;
	op->sumlife = 0.0 ;

	op->sumrint1 = 0.0 ;
	op->sumuse1 = 0.0 ;
	op->sumlife1 = 0.0 ;

/* sizes */

#ifdef	COMMENT
	uc_sysconf(_SC_PAGESIZE,(long *) &syspagesize) ;
#else
	syspagesize = getpagesize() ;
#endif

	if (elemsize < 1)
		elemsize = 1 ;

	op->elemoff = flbsi(elemsize) ;

	if (pagesize <= 0)
	    pagesize = MEMSTATS_DEFPAGESIZE ;

	if (pagesize < syspagesize)
	    pagesize = syspagesize ;

	op->pagesize = nextpowtwo(pagesize) ;

	if (groupsize <= 0)
	    groupsize = MEMSTATS_DEFGROUPSIZE ;

	if (groupsize < (2 * pagesize))
	    groupsize = (2 * pagesize) ;

	op->groupsize = nextpowtwo(groupsize) ;

	op->npages = op->groupsize / op->pagesize ;

/* calculate the break in the address between offset and page number */

	n = (op->pagesize / sizeof(struct memstats_te)) ;
	op->offbits = flbsi(n) ;

	op->pagebits = (MEMSTATS_ADDRBITS - op->offbits - 2) ;

#if	CF_DEBUGS
	eprintf("memstats_init: groupsize=%u pagesize=%u npages=%u\n",
	    op->groupsize,op->pagesize,op->npages) ;
	eprintf("memstats_init: entries_per_page=%u\n",n) ;
	eprintf("memstats_init: offbits=%u pagebits=%u\n",
	    op->offbits,op->pagebits) ;
#endif /* CF_DEBUGS */

	op->offmask = ((1 << op->offbits) - 1) ;
	op->pagemask = ((1 << (MEMSTATS_ADDRBITS - op->offbits - 2)) - 1) ;

#if	CF_DEBUGS
	eprintf("memstats_init: offmask=%08x pagemask=%08x\n",
	    op->offmask,op->pagemask) ;
#endif

/* density table */

	if (lenden < 0)
	    lenden = MEMSTATS_DEFLENDEN ;

	op->lenden = lenden ;

	n = 6 ;
	size = n * op->lenden * sizeof(ULONG) ;
	rs = uc_malloc(size,&op->den) ;

	if (rs < 0)
	    goto bad1 ;

	(void) memset(op->den,0,size) ;

/* 2^0 tables */

	i = 0 ;
	op->denrint = op->den + (i++ * op->lenden) ;
	op->denuse = op->den + (i++ * op->lenden) ;
	op->denlife = op->den + (i++ * op->lenden) ;

/* 2^10 tables */

	op->denrint1 = op->den + (i++ * op->lenden) ;
	op->denuse1 = op->den + (i++ * op->lenden) ;
	op->denlife1 = op->den + (i++ * op->lenden) ;

	if (n != i)
		goto bad2 ;

/* tracking table */

	ngroups = MEMSTATS_NGROUPS ;		/* estimate of groups */
	rs = vecitem_init(&op->groups,ngroups,0) ;

	if (rs < 0)
	    goto bad2 ;

	n = ngroups * op->npages ;
	rs = hdb_init(&op->tts,n,NULL,NULL) ;

	if (rs < 0)
	    goto bad3 ;

	op->cg.pa = NULL ;
	op->cg.e = 0 ;

/* open the file */

	oflags = (O_RDWR | O_CREAT) ;
	rs = u_open(fname,oflags,0666) ;

	if (rs < 0)
	    goto bad4 ;

	op->fd = rs ;

	strlcpy(op->fname,fname,MAXPATHLEN) ;

	u_unlink(fname) ;

	op->magic = MEMSTATS_MAGIC ;
	return rs ;

/* bad stuff */
bad4:
	hdb_free(&op->tts) ;

bad3:
	vecitem_free(&op->groups) ;

bad2:
	free(op->den) ;

bad1:
bad0:
	return rs ;
}
/* end subroutine (memstats_init) */


/* process a memory "read" request */
int memstats_read(op,in,f_se,a,v)
MEMSTATS	*op ;
ULONG		in ;
int		f_se ;
ULONG		a, v ;
{
	struct memstats_te	*tep ;

	LONG	diff, diff0, diff1, diff_read, diff_write ;

	int	rs ;


#if	CF_SAFE
	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;
#endif /* CF_SAFE */

#if	CF_DEBUGS
	eprintf("memstats_read: in=%llu a=\\x%016llx\n",in,a) ;
#endif

	rs = memstats_lookup(op,a,&tep) ;

	if (rs < 0)
	    goto ret0 ;

	if (f_se) {

		op->in = in ;
		if (! op->f.started) {
			op->f.started = TRUE ;
			op->in_start = in ;
		}

	}

	if (f_se && (tep->write != 0)) {

#if	CF_DEBUGS
	    eprintf("memstats_read: gathering a=\\x%016llx tep=%p\n",
	        a,tep) ;
#endif

/* count reads */

	    op->c_read += 1 ;
	    if (tep->read == 0)
	        op->c_readnew += 1 ;

/* read density */

	    diff_read = in - tep->read ;
	    diff_write = in - tep->write ;
	    diff = MIN(diff_read,diff_write) ;

/* 2^0 table */

		diff0 = diff ;
	    op->sumrint += diff0 ;
	    if (diff0 >= op->lenden)
	        diff0 = op->lenden - 1 ;

	    op->denrint[diff0] += 1 ;

/* 2^10 table */

		diff1 = diff >> 10 ;
	    op->sumrint1 += diff1 ;
	    if (diff1 >= op->lenden)
	        diff1 = op->lenden - 1 ;

	    op->denrint1[diff1] += 1 ;

/* update def-use density */

	    diff = in - tep->write ;

/* 2^0 table */

		diff0 = diff ;
	    op->sumuse += diff0 ;
	    if (diff0 >= op->lenden)
	        diff0 = op->lenden - 1 ;

	    op->denuse[diff0] += 1 ;

/* 2^10 table */

		diff1 = diff >> 10 ;
	    op->sumuse1 += diff1 ;
	    if (diff1 >= op->lenden)
	        diff1 = op->lenden - 1 ;

	    op->denuse1[diff1] += 1 ;

	} /* end if (selection) */

/* update read interval */

#if	CF_DEBUGS
	eprintf("memstats_read: updating tep=%p in=%llu\n",tep,in) ;
#endif

	if (f_se)
	tep->read = in ;

ret0:
	return rs ;
}
/* end subroutine (memstats_read) */


int memstats_write(op,in,f_se,a,v)
MEMSTATS	*op ;
ULONG		in ;
int		f_se ;
ULONG		a, v ;
{
	struct memstats_te	*tep ;

	LONG	diff, diff0, diff1 ;

	int	rs ;


#if	CF_SAFE
	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;
#endif /* CF_SAFE */

#if	CF_DEBUGS
	eprintf("memstats_write: in=%llu a=\\x%016llx\n",in,a) ;
#endif

	rs = memstats_lookup(op,a,&tep) ;

	if (rs < 0)
	    goto ret0 ;

#if	CF_DEBUGS
	if (in & (~ INT_MAX))
	    eprintf("memstats_write: in&~INT_MAX=%08x\n",
		(in & (~ INT_MAX))) ;
#endif

	if (f_se) {

	    op->c_write += 1 ;
	    if (tep->write != 0) {

	        diff = (tep->read - tep->write) ;
	        if (diff < 0)
	            diff = 0 ;

/* 2^0 table */

		diff0 = diff ;
		op->sumlife += diff0 ;
	        if (diff0 >= op->lenden)
	            diff0 = op->lenden - 1 ;

	        op->denlife[diff0] += 1 ;

/* 2^10 table */

		diff1 = diff >> 10 ;
		op->sumlife1 += diff1 ;
	        if (diff1 >= op->lenden)
	            diff1 = op->lenden - 1 ;

	        op->denlife1[diff1] += 1 ;

	    } else
	        op->c_writenew += 1 ;

	} /* end if (selection active) */

#if	CF_DEBUGS
	eprintf("memstats_write: updating tep=%p in=%;;u\n",tep,in) ;
#endif

	if (f_se)
	tep->write = in ;

ret0:
	return rs ;
}
/* end subroutine (memstats_write) */


/* call this when the simulator is done executing instructions */
int memstats_writedone(op,in,f_se)
MEMSTATS	*op ;
ULONG		in ;
int		f_se ;
{
	struct memstats_tpe	*tpep ;

	struct memstats_te	*tep ;

	HDB_DATUM	key, value ;

	HDB_CURSOR	cur ;

	LONG	diff, diff0, diff1 ;

	int	n, i ;


#if	CF_SAFE
	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;
#endif /* CF_SAFE */

	if (f_se) {

	    hdb_cursorinit(&op->tts,&cur) ;

	    while (hdb_enum(&op->tts,&cur,&key,&value) >= 0) {

	        tpep = (struct memstats_tpe *) value.buf ;
	        tep = tpep->pp ;
	        n = op->pagesize / sizeof(struct memstats_te) ;
	        for (i = 0 ; i < n ; i += 1) {

	            if (tep[i].write > 0) {

	                diff = (tep[i].read - tep[i].write) ;
	                if (diff > 0) {

/* 2^0 table */

				diff0 = diff ;
			    op->sumlife += diff0 ;
	                    if (diff0 >= op->lenden)
	                        diff0 = op->lenden - 1 ;

	                    op->denlife[diff0] += 1 ;

/* 2^10 table */

				diff1 = diff >> 10 ;
			    op->sumlife1 += diff1 ;
	                    if (diff1 >= op->lenden)
	                        diff1 = op->lenden - 1 ;

	                    op->denlife1[diff1] += 1 ;

	                } /* end if (we had some extra reads) */

	            } /* end if (variable was written) */

	        } /* end for (looping through entries) */

	    } /* end while (looping through pages) */

	    hdb_cursorfree(&op->tts,&cur) ;

	} /* end if (selection in effect) */

	return 0 ;
}
/* end subroutine (memstats_writedone) */


int memstats_free(op)
MEMSTATS	*op ;
{
	struct memstats_group	*gp ;

	HDB_DATUM	key, value ;

	HDB_CURSOR	cur ;

	int	rs, i ;


	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;

/* free up the page entries */

	hdb_cursorinit(&op->tts,&cur) ;

	while (hdb_enum(&op->tts,&cur,&key,&value) >= 0) {

	    free(value.buf) ;

	} /* end while */

	hdb_cursorfree(&op->tts,&cur) ;

	hdb_free(&op->tts) ;

/* free up the groups */

	for (i = 0 ; vecitem_get(&op->groups,i,&gp) >= 0 ; i += 1) {

	    u_munmap(gp->pa,op->groupsize) ;

	} /* end for */

	rs = vecitem_free(&op->groups) ;

/* finish up */

	if (op->den != NULL)
	    free(op->den) ;

	op->magic = 0 ;
	return rs ;
}
/* end subroutine (memstats_free) */


/* return some statistics to the user */
int memstats_stats(op,sp)
MEMSTATS	*op ;
MEMSTATS_STATS	*sp ;
{
	struct memstats_tpe	*tpep ;

	struct memstats_te	*tep ;

	HDB_DATUM	key, value ;

	HDB_CURSOR	cur ;

	ULONG	c ;
	ULONG	npages = 0 ;

	uint	mult = (1 << 10) ;

	int	rs, i ;
	int	n ;

	double	percents[100 + 1] ;
	double	mean, var, ov ;


	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;

	if (sp == NULL)
	    return SR_FAULT ;

	(void) memset(sp,0,sizeof(MEMSTATS_STATS)) ;

/* instructions */

	if (op->f.started) {
	    sp->in_start = op->in_start ;
	    sp->ins = op->in - op->in_start ;
	}

/* the tough data */

	sp->flen = op->flen ;
	sp->lenden = op->lenden ;

	sp->reads = op->c_read ;
	sp->writes = op->c_write ;

	sp->readvars = op->c_readnew ;
	sp->writevars = op->c_writenew ;

/* test */

#if	CF_DEBUGS2
	{

/* writes */

	    c = 0 ;
	    for (i = 0 ; i < op->lenden ; i += 1)
	        c += op->denlife[i] ;

	    if (c != (op->c_write + op->c_writenew)) {
	        eprintf("memstats_stats: writes=%llu c=%llu\n",op->c_write,c) ;
	        eprintf("memstats_stats: new writes=%llu\n",op->c_writenew) ;
	    }

/* reads */

	    c = 0 ;
	    for (i = 0 ; i < op->lenden ; i += 1)
	        c += op->denuse[i] ;

	    if (c != op->c_read)
	        eprintf("memstats_stats: reads=%llu c=%llu\n",op->c_read,c) ;

	}
#endif /* CF_DEBUGS2 */

/* find the number of tracking entries that were used */

	sp->tes = 0 ;
	hdb_cursorinit(&op->tts,&cur) ;

	while (hdb_enum(&op->tts,&cur,&key,&value) >= 0) {

	    tpep = (struct memstats_tpe *) value.buf ;
	    tep = tpep->pp ;
	    n = op->pagesize / sizeof(struct memstats_te) ;
	    for (i = 0 ; i < n ; i += 1) {

	        if ((tep[i].read != 0) || (tep[i].write != 0))
	            sp->tes += 1 ;

	    } /* end for */

	    npages += 1 ;

	} /* end while */

	hdb_cursorfree(&op->tts,&cur) ;

/* continue */

#if	CF_DEBUGS2
	eprintf("memstats_stats: test\n") ;
	if (npages != op->c_page) {
	    eprintf("memstats_stats: npages=%u pages=%llu\n",
	        npages,op->c_page) ;
	    sleep(2) ;
	}
#endif

#if	CF_ASSERTS
	assert(npages == op->c_page) ;
#endif

	sp->pages = op->c_page ;
	sp->groups = op->c_group ;

#if	CF_DEBUGS
	eprintf("memstats_stats: groups=%u pages=%u\n",
	    op->c_group,op->c_page) ;
#endif

/* the 2^0 table */

	mult = (1 << 0) ;
	sp->rint_amean = 0.0 ;
	sp->use_amean = 0.0 ;
	sp->life_amean = 0.0 ;
	sp->rint_mean = sp->rint_var = sp->rint_ov = 0.0 ;
	sp->life_mean = sp->life_var = sp->life_ov = 0.0 ;
	sp->use_mean = sp->use_var = sp->use_ov = 0.0 ;

/* read intervals */

	rs = fmeanvaral(op->denrint,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denrint,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denrint[op->lenden - 1]) / c ;
	    sp->rint_amean = op->sumrint * mult / c ;
	}

	sp->rint_ov = ov ;
	sp->rint_mean = mean * mult ;
	sp->rint_var = var * mult ;

	rs = denpercentsl(op->denrint,op->lenden,percents) ;

	sp->rint_p7 = percents[70] ;
	sp->rint_p8 = percents[80] ;
	sp->rint_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: rint rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->rint_p7,sp->rint_p8,sp->rint_p9) ;
#endif

/* def-uses */

	rs = fmeanvaral(op->denuse,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denuse,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denuse[op->lenden - 1]) / c ;
	    sp->use_amean = op->sumuse * mult / c ;
	}

	sp->use_ov = ov ;
	sp->use_mean = mean * mult ;
	sp->use_var = var * mult ;

	rs = denpercentsl(op->denuse,op->lenden,percents) ;

	sp->use_p7 = percents[70] ;
	sp->use_p8 = percents[80] ;
	sp->use_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: use rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->use_p7,sp->use_p8,sp->use_p9) ;
#endif

/* lifetimes */

	rs = fmeanvaral(op->denlife,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denlife,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denlife[op->lenden - 1]) / c ;
	    sp->life_amean = op->sumlife * mult / c ;
	}

	sp->life_ov = ov ;
	sp->life_mean = mean * mult ;
	sp->life_var = var * mult ;

	rs = denpercentsl(op->denlife,op->lenden,percents) ;

	sp->life_p7 = percents[70] ;
	sp->life_p8 = percents[80] ;
	sp->life_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: life rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->life_p7,sp->life_p8,sp->life_p9) ;
#endif

/* the 2^10 table */

	mult = (1 << 10) ;
	sp->rint1_amean = 0.0 ;
	sp->use1_amean = 0.0 ;
	sp->life1_amean = 0.0 ;
	sp->rint1_mean = sp->rint1_var = sp->rint1_ov = 0.0 ;
	sp->life1_mean = sp->life1_var = sp->life1_ov = 0.0 ;
	sp->use1_mean = sp->use1_var = sp->use1_ov = 0.0 ;

/* read intervals */

	rs = fmeanvaral(op->denrint1,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denrint1,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denrint1[op->lenden - 1]) / c ;
	    sp->rint1_amean = op->sumrint1 * mult / c ;
	}

	sp->rint1_ov = ov ;
	sp->rint1_mean = mean * mult ;
	sp->rint1_var = var * mult ;

	rs = denpercentsl(op->denrint1,op->lenden,percents) ;

	sp->rint1_p7 = percents[70] ;
	sp->rint1_p8 = percents[80] ;
	sp->rint1_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: rint1 rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->rint1_p7,sp->rint1_p8,sp->rint1_p9) ;
#endif

/* def-uses */

	rs = fmeanvaral(op->denuse1,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denuse1,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denuse1[op->lenden - 1]) / c ;
	    sp->use1_amean = op->sumuse1 * mult / c ;
	}

	sp->use1_ov = ov ;
	sp->use1_mean = mean * mult ;
	sp->use1_var = var * mult ;

	rs = denpercentsl(op->denuse1,op->lenden,percents) ;

	sp->use1_p7 = percents[70] ;
	sp->use1_p8 = percents[80] ;
	sp->use1_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: use1 rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->use1_p7,sp->use1_p8,sp->use1_p9) ;
#endif

/* lifetimes */

	rs = fmeanvaral(op->denlife1,op->lenden,&mean,&var) ;

	if (rs < 0)
	    goto ret0 ;

	c = countden(op->denlife1,op->lenden) ;

	ov = 0.0 ;
	if (c > 0) {
		ov = 100.0 * ((double) op->denlife1[op->lenden - 1]) / c ;
		sp->life1_amean = op->sumlife1 * mult / c ;
	}

	sp->life1_ov = ov ;
	sp->life1_mean = mean * mult ;
	sp->life1_var = var * mult ;

	rs = denpercentsl(op->denlife1,op->lenden,percents) ;

	sp->life1_p7 = percents[70] ;
	sp->life1_p8 = percents[80] ;
	sp->life1_p9 = percents[90] ;

#if	CF_DEBUGS2
	eprintf("memstats_stats: life1 rs=%d p7=%12.4f p8=%12.4f p9=%12.4f\n",
		rs,sp->life1_p7,sp->life1_p8,sp->life1_p9) ;
#endif

/* we-re out of here */
ret0:
	return rs ;
}
/* end subroutine (memstats_stats) */


int memstats_storefiles(op,rintfname,lifefname,usefname)
MEMSTATS	*op ;
char		rintfname[] ;
char		lifefname[] ;
char		usefname[] ;
{
	bfile	outfile ;

	uint	mult = (1 << 10) ;
	int	rs = SR_OK, i ;

	char	fname[MAXPATHLEN + 1] ;


	if (op == NULL)
	    return SR_FAULT ;

	if (op->magic != MEMSTATS_MAGIC)
	    return SR_NOTOPEN ;

/* the 2^0 table */

/* read-interval density */

#if	CF_DEBUGS
	eprintf("memstats_storefiles: rintfname=%s\n",rintfname) ;
#endif

	if ((rs >= 0) && (rintfname != NULL) && (rintfname[0] != '\0'))
	    rs = writearray(rintfname,op->denrint,op->lenden) ;

/* life density */

#if	CF_DEBUGS
	eprintf("memstats_storefiles: lifefname=%s\n",lifefname) ;
#endif

	if ((rs >= 0) && (lifefname != NULL) && (lifefname[0] != '\0'))
	    rs = writearray(lifefname,op->denlife,op->lenden) ;

/* use density */

	if ((rs >= 0) && (usefname != NULL) && (usefname[0] != '\0'))
	    rs = writearray(usefname,op->denuse,op->lenden) ;

/* the 2^10 table */

	mkfname2(fname,rintfname,"1") ;

	if (rs >= 0)
	    rs = writearray(fname,op->denrint1,op->lenden) ;

	mkfname2(fname,lifefname,"1") ;

	if (rs >= 0)
	    rs = writearray(fname,op->denlife1,op->lenden) ;

	mkfname2(fname,usefname,"1") ;

	if (rs >= 0)
	    rs = writearray(fname,op->denuse1,op->lenden) ;

/* we're done */

#if	CF_DEBUGS
	eprintf("memstats_storefiles: 4 rs=%d\n",rs) ;
#endif

	return rs ;
}
/* end subroutine (memstats_storefiles) */



/* PRIVATE SUBROUTINES */



/* get a tracking entry for the given address */
static int memstats_lookup(op,a,tepp)
MEMSTATS		*op ;
ULONG			a ;
struct memstats_te	**tepp ;
{
	struct memstats_tpe	*tpep ;

	HDB_DATUM	key, value ;

	ULONG		page, offset ;

	int	rs = SR_OK ;


	a = (a >> op->elemoff) ;

	offset = a & op->offmask ;
	page = (a >> op->offbits) & op->pagemask ;

#if	CF_DEBUGS
	eprintf("memstats_lookup: a=\\x%016llx page=%llu offset=%llu\n",
	    a,page,offset) ;
#endif

/* search for the page of tracking data with this page number */

/* do the lookup */

	key.buf = &page ;
	key.len = sizeof(ULONG) ;

	rs = hdb_fetch(&op->tts,key,NULL,&value) ;

#if	CF_DEBUGS
	eprintf("memstats_lookup: hdb_fetch() rs=%d\n",rs) ;
#endif

	if (rs == SR_NOTFOUND) {

	    struct memstats_te	*pp ;

	    int	size ;


/* allocate a page if possible */

	    rs = memstats_allocpage(op,&pp) ;

#if	CF_DEBUGS
	    eprintf("memstats_lookup: memstats_allocpage() rs=%d pp=%p\n",
	        rs,pp) ;
#endif

	    if (rs < 0)
	        goto ret0 ;

	    size = sizeof(struct memstats_tpe) ;
	    rs = uc_malloc(size,&tpep) ;

	    if (rs < 0)
	        goto ret0 ;

/* fill in the information for this entry */

	    tpep->pp = pp ;
	    tpep->page = page ;

/* store it away (in the page table) */

#if	CF_DEBUGS
	    eprintf("memstats_lookup: hdb_store() pp=%p page=%u\n",
	        pp,page) ;
#endif

	    key.buf = &tpep->page ;
	    key.len = sizeof(ULONG) ;

	    value.buf = tpep ;
	    value.len = size ;

	    rs = hdb_store(&op->tts,key,value) ;

#if	CF_DEBUGS
	    eprintf("memstats_lookup: hdb_store() rs=%d\n", rs) ;
#endif

	    if (rs < 0) {

	        free(tpep) ;

	        goto ret0 ;
	    }

	    op->c_page += 1 ;

	} else if (rs >= 0) {

	    tpep = (struct memstats_tpe *) value.buf ;

#if	CF_DEBUGS
	    eprintf("memstats_lookup: hit tpep=%p pp=%p page=%d\n",
	        tpep,tpep->pp,tpep->page) ;
#endif

	} /* end if */

#if	CF_DEBUGS
	eprintf("memstats_lookup: rs=%d tpep=%p offset=%llu (+\\x%016llx)\n",
	    rs,tpep,offset,(offset << 3)) ;
#endif

	if (rs >= 0) {

#if	CF_DEBUGS
	    eprintf("memstats_lookup: pp=%p\n",tpep->pp) ;
#endif

	    *tepp = tpep->pp + offset ;

#if	CF_DEBUGS
	    eprintf("memstats_lookup: tep=%p\n",*tepp) ;
#endif

	}

ret0:
	return rs ;
}
/* end subroutine (memstats_lookup) */


/* allocate a new page */
static int memstats_allocpage(op,ppp)
MEMSTATS		*op ;
struct memstats_te	**ppp ;
{
	int	rs = SR_OK ;


	if ((op->cg.pa == NULL) || (op->cg.e >= op->npages)) {

	    struct memstats_te	*gp ;


	    rs = memstats_allocgroup(op,&gp) ;

#if	CF_DEBUGS
	    eprintf("memstats_allocpage: new group=%p\n",gp) ;
#endif

	    if (rs < 0)
	        goto ret0 ;

	    op->c_group += 1 ;

	    op->cg.pa = (caddr_t) gp ;
	    op->cg.e = 0 ;

	} /* end if (allocated a new group) */

	*ppp = (struct memstats_te *)
	    (((char *) op->cg.pa) + (op->cg.e * op->pagesize)) ;
	op->cg.e += 1 ;

#if	CF_DEBUGS
	eprintf("memstats_allocpage: new page=%p\n",*ppp) ;
#endif

ret0:
	return rs ;
}
/* end subroutine (memstats_allocpage) */


/* allocate a new group of pages */
static int memstats_allocgroup(op,gpp)
MEMSTATS		*op ;
struct memstats_te	**gpp ;
{
	struct memstats_group	ge ;

	int	rs ;
	int	prot, flags ;


	*gpp = NULL ;

#if	CF_DEBUGS
	eprintf("memstats_allocgroup: file offset=%08x\n",op->flen) ;
#endif

/* extend the file */

	{
	    int	wlen ;
	    int	lenleft ;

	    char	buf[PAGESIZE] ;


	    memset(buf,0,PAGESIZE) ;

	    u_seek(op->fd,op->flen,SEEK_SET) ;

	    lenleft = op->groupsize ;
	    while (lenleft > 0) {

	        wlen = MIN(lenleft,PAGESIZE) ;
	        rs = u_write(op->fd,buf,wlen) ;

	        lenleft -= wlen ;

	    } /* end while */

	} /* end block */

/* map the new region */

	prot = PROT_READ | PROT_WRITE ;
	flags = MAP_PRIVATE ;

	*gpp = NULL ;
	rs = u_mapfile(NULL,(size_t) op->groupsize,prot,flags,op->fd,
	    (off_t) op->flen,&ge.pa) ;

	if (rs < 0)
	    return rs ;

	op->flen += op->groupsize ;

#if	CF_DEBUGS
	eprintf("memstats_allocgroup: new group=%p\n",ge.pa) ;
#endif

	rs = vecitem_add(&op->groups,&ge,sizeof(struct memstats_group)) ;

	if (rs < 0)
	    return rs ;

	*gpp = (struct memstats_te *) ge.pa ;

	return rs ;
}
/* end subroutine (memstats_allocgroup) */


static int writearray(fname,a,alen)
const char	fname[] ;
ULONG		a[] ;
int		alen ;
{
	bfile	outfile ;

	int	rs ;
	int	i ;


	if (fname == NULL)
		return SR_FAULT ;

	if (a == NULL)
		return SR_FAULT ;

	rs = bopen(&outfile,fname,"wct",0666) ;

	if (rs >= 0) {

	    for (i = 0 ; i < alen ; i += 1)
	        bprintf(&outfile,"%12d %12llu\n",i,a[i]) ;

	    bclose(&outfile) ;

	} /* end if */

	return rs ;
}
/* end subroutine (writearray) */


static ULONG countden(density,lenden)
ULONG	density[] ;
int	lenden ;
{
	ULONG	c = 0 ;

	int	i ;


	for (i = 0 ; i < lenden ; i += 1)
		c += density[i] ;

	return c ;
}
/* end subroutine (countden) */


#ifdef	COMMENT

static uint ourhash(keyp)
HDB_DATUM	*keyp ;
{
	uint	*pp ;

	int	hv ;


	pp = (uint *) keyp->buf ;

#if	CF_DEBUGS
	eprintf("memstats/ourhash: page=%d\n", *pp) ;
#endif

	hv = elfhash((char *) pp,sizeof(ULONG)) ;

#if	CF_DEBUGS
	eprintf("memstats/ourhash: hv=%08x\n",hv) ;
#endif

	return hv ;
}
/* end subroutine (ourhash) */

#endif /* COMMENT */



