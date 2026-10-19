/* Build 26.10.19.2 01:12PM */
/*****************************************************************************
 *   HVSMMDEF.H                              Menu Magician - Shared defines   *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1994 High Velocity Software, Inc.              *
 *                                                                           *
 *   Declarations shared by every Menu Magician source file: the version,    *
 *   the settings record, the text variable slots, and each source file's    *
 *   public routines.                                                        *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0 - see the     *
 *   LICENSE file in the project root.                                       *
 *****************************************************************************/

#ifndef HVSMMDEF_H
#define HVSMMDEF_H

#include <stddef.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "HVSMM.H"                   /* message numbers for HVSMM.MSG        */

/* ------------------------------------------------------------------------ *
 * Version.  HVSMM_VERSION is what sysops see (audit trail, help screen);   *
 * the numeric parts feed the DLL's Windows version resource (HVSMM.rc).    *
 * ------------------------------------------------------------------------ */
#define HVSMM_VER_MAJOR 1
#define HVSMM_VER_MINOR 4
#define HVSMM_VER_PATCH 0
#define HVSMM_VERSION   "1.4.0"

/* ------------------------------------------------------------------------ *
 * Live settings (MMSET.C).  They are kept in HVSMMSET.DAT and changed      *
 * online with the !SETUP command; there are no CNF options.  Code reads    *
 * mmcfg at the point of use, never a copy taken at startup.                *
 * ------------------------------------------------------------------------ */
#define MMS_FILE     "HVSMMSET.DAT"
#define MMS_RECID    "SETTINGS"      /* key of the one settings record       */
#define MMS_RECLEN   256             /* fixed record length, room to grow    */
#define MMS_LAYOUT   1               /* bump when the record layout changes  */
#define MMS_CMDSIZ   17              /* a command word, 16 characters + NUL  */

#pragma pack(push, 8)                /* stored record: never depend on /Zp   */
struct mmsetdat {                    /* the settings themselves              */
     SHORT layout;                   /*   MMS_LAYOUT when written            */
     CHAR glokey[KEYSIZ];            /*   key for the global edit command    */
     CHAR glocmd[MMS_CMDSIZ];        /*   the global edit command            */
     CHAR orphcmd[MMS_CMDSIZ];       /*   create-orphan command              */
     CHAR viewcmd[MMS_CMDSIZ];       /*   view-tree command                  */
};

struct mmsetrec {                    /* the record as stored                 */
     CHAR recid[16];                 /*   MMS_RECID (key 0)                  */
     struct mmsetdat d;
     CHAR spare[MMS_RECLEN - 16 - sizeof(struct mmsetdat)];
};
#pragma pack(pop)
typedef CHAR mms_chk_size[(sizeof(struct mmsetrec) == MMS_RECLEN) ? 1 : -1];

extern struct mmsetdat mmcfg;        /* live settings - read at point of use */

VOID  set_open(VOID);                /* open file, load or seed settings     */
VOID  set_close(VOID);
VOID  set_editor(VOID);              /* the !SETUP screen (runs in a session)*/

/* ------------------------------------------------------------------------ *
 * Sessions (MMFIBER.C).  The editor is written as one routine that asks a  *
 * question and waits for the answer, the way the original was.  Each      *
 * editing user runs it on a Windows fiber that pauses while it waits and  *
 * resumes when the BBS hands us the user's next line.                      *
 * ------------------------------------------------------------------------ */
GBOOL ses_init(VOID);                /* allocate per-channel session table   */
VOID  ses_close(VOID);               /* shutdown: discard every session      */
GBOOL ses_input(VOID);               /* sttrou: start or resume a session    */
VOID  ses_status(VOID);              /* stsrou: CYCLE resumes a sleeper      */
VOID  ses_discard(INT chan);         /* drop a channel's session, if any     */

VOID  ses_line(VOID);                /* wait for the user's next line        */
VOID  ses_cinput(VOID);              /* next command, honouring concatenation*/
INT   ses_sleep(VOID);               /* let others run; returns the status   */
                                     /*   that woke us (CYCLE, or CRSTG when */
                                     /*   the user pressed Enter)            */
GBOOL ses_slice(VOID);               /* has this turn run long enough?       */
GBOOL ses_drain(UINT room);          /* wait for output room; TRUE = aborted */

/* ------------------------------------------------------------------------ *
 * The editor (MMEDIT.C) and the menu file helpers.                         *
 * ------------------------------------------------------------------------ */
extern INT mmstt;                    /* our module (state) number            */
extern HMCVFILE mmmb;                /* HVSMM.MCV                            */
extern INT UseChan;                  /* channel using the editor, or -1      */
extern INT TurboChan;                /* channel entering via the global cmd  */
extern struct mnupag Work;           /* the page being edited                */
extern struct mnupag Scratch;        /* a second page, for side trips        */

/* The Major BBS v10 has terminal mode only, so only terminal pages ('A')  */
/* are edited.  Worldgroup's client/server pages ('C') are left alone.      */
#define MM_TERMINAL  'A'

VOID  ed_main(VOID);                 /* session routine: the whole editor    */
VOID  ed_release(INT chan);          /* a session ended: free the editor     */
INT   ed_global(VOID);               /* global command handler               */

GBOOL LoadPage(struct mnupag *page, const CHAR *name);
VOID  SavePage(struct mnupag *page);
VOID  InsertPage(struct mnupag *page);
VOID  DeletePage(VOID);              /* delete the page last loaded          */
LONG  PagePos(VOID);                 /* position of the page last loaded     */
VOID  PageAt(struct mnupag *page, LONG pos);   /* reload by position         */

/* ------------------------------------------------------------------------ *
 * Tree view, search and module list (MMVIEW.C).                            *
 * ------------------------------------------------------------------------ */
VOID  ShowTree(VOID);
VOID  Search(VOID);
VOID  DumpModules(VOID);
VOID  PrepareAppList(VOID);
GBOOL CheckList(const CHAR *name, CHAR **list, INT count);

extern INT nTerminal;                /* modules a page can link to           */
extern CHAR **ppTerminal;

/* ------------------------------------------------------------------------ *
 * Text variables (MMTVARS.C).  Every changing value in HVSMM.MSG is a      *
 * named text variable; set the slot just before printing the message.     *
 * ------------------------------------------------------------------------ */
VOID tv_init(VOID);
VOID tv_set(INT which, const CHAR *value);
VOID tv_setn(INT which, LONG value);
VOID tv_setc(INT which, CHAR value);

enum {                               /* text variable slots                  */
     MTV_PAGE, MTV_PARENT, MTV_DISABLED, MTV_GOTO, MTV_GOKEY, MTV_MODULE,
     MTV_COMMAND, MTV_HEADER, MTV_FILE, MTV_AUTO, MTV_DEFDISP, MTV_TITLE,
     MTV_NUM, MTV_SEL, MTV_TEXT, MTV_NUM2, MTV_SEL2, MTV_TEXT2, MTV_VALUE,
     MTV_SHORT, MTV_KEY, MTV_DISPLAY, MTV_DEST, MTV_ACTION, MTV_REASON,
     MTV_LABEL, MTV_LIMIT,
     MTV_COUNT_OF
};

#endif /* HVSMMDEF_H */
