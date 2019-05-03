/*
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** version.h
**
** Program name / version definitions
** $Id: version.h,v 1.2 2001/05/05 16:50:49 neil Exp $
*/

#ifndef _VERSION_H_
#define _VERSION_H_

#ifdef NSF_PLAYER
#define  APP_STRING     "Nosefart"
#else
#define  APP_STRING     "Nofrendo"
#endif /* NSF_PLAYER */

#define  APP_VERSION    "2.0"

#endif /* _VERSION_H_ */

/*
** $Log: version.h,v $
** Revision 1.2  2001/05/05 16:50:49  neil
** preparing for distribution
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.9  2000/07/31 04:28:47  matt
** one million cleanups
**
** Revision 1.8  2000/07/17 01:52:28  matt
** made sure last line of all source files is a newline
**
** Revision 1.7  2000/07/04 04:46:55  matt
** updated version number
**
** Revision 1.6  2000/06/20 00:03:39  matt
** updated for 1.91
**
** Revision 1.5  2000/06/09 17:01:56  matt
** changed version to 1.90
**
** Revision 1.4  2000/06/09 15:12:25  matt
** initial revision
**
*/
