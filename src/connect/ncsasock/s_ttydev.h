/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ttydev.h	7.6 (Berkeley) 11/20/89
*
*
* RCS Modification History:
* $Log$
* Revision 6.0  1997/08/25 18:37:55  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.2  1995/05/17  17:57:40  epstein
 * add RCS log revision history
 *
 */

/*
 * COMPATABILITY HEADER FILE --
 */

/*
 * Terminal definitions related to underlying hardware.
 */
#ifndef _TTYDEV_
#define	_TTYDEV_

#ifdef USE_OLD_TTY
/*
 * Speeds
 */
#define B0	0
#define B50	1
#define B75	2
#define B110	3
#define B134	4
#define B150	5
#define B200	6
#define B300	7
#define B600	8
#define B1200	9
#define	B1800	10
#define B2400	11
#define B4800	12
#define B9600	13
#define EXTA	14
#define EXTB	15
#endif /* USE_OLD_TTY */

#endif /* _TTYDEV_ */
