/* opclass */


/* revision history:

	= 1998-11-01, David Morano

	Originally written for Audix Database Processor (DBP) work.


*/

/* Copyright � 1998 David A�D� Morano.  All rights reserved. */

#ifndef	OPCLASS_INCLUDE
#define	OPCLASS_INCLUDE		1


/*    Class of instructions */

#define OPCLASS_UNUSED	0
#define OPCLASS_BREL	1
#define OPCLASS_BIND	2		/* there are none in R3000 or R4000 */
#define OPCLASS_LOAD	3	
#define OPCLASS_STORE	4
#define OPCLASS_ALU	5
#define OPCLASS_FPALU	6
#define OPCLASS_SPECIAL	7
#define OPCLASS_JREL	8
#define OPCLASS_JIND	9
#define OPCLASS_EXCEP	10
#define	OPCLASS_UNIMPL	11
#define	OPCLASS_JUNC	12


#endif /* OPCLASS_INCLUDE */


