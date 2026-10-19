/* Build 26.10.19.1 12:41PM */
/*****************************************************************************
 *   TESTS/FAKE/majorbbs.h         Menu Magician - stand-in engine header     *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   See gcomm.h in this folder.                                             *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#ifndef FAKE_MAJORBBS_H
#define FAKE_MAJORBBS_H

#define PNMSIZ  16
#define MNMSIZ  25

#define CRSTG   3                    /* BRKTHU.H: a line is ready            */
#define CYCLE   240                  /* MAJORBBS.H: cycle pseudo-status      */
#define ABOIP   0x00008000L          /* MAJORBBS.H: output abort in progress */

struct user  { LONG flags; };
struct usracc { CHAR userid[UIDSIZ]; };

extern INT usrnum;
extern INT nterms;
extern INT status;
extern INT margc;
extern CHAR *margv[];
extern CHAR *nxtcmd;
extern CHAR input[];
extern struct user *usrptr;
extern struct usracc *usaptr;

VOID  rstrin(VOID);
VOID  btuclo(INT chan);
VOID  outprf(INT chan);
VOID  dfsthn(VOID);
INT   btuinj(INT chan, INT code);
INT   endcnc(VOID);
VOID  bgncnc(VOID);
ULONG hrtval(VOID);
INT   btuoba(INT chan);

#endif
