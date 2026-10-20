/* Build 26.10.20.1 08:06AM */
/*****************************************************************************
 *   GALMM.C   v1.4.1                     Menu Magician for The Major BBS v10 *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1993-1996 High Velocity Software, Inc.         *
 *   Menu Magician converted for The Major BBS v10 by Mark Laudenbach -      *
 *   https://github.com/laudenbachm                                          *
 *                                                                           *
 *   Menu Magician lets a sysop edit the BBS's menu tree online, while the   *
 *   board runs: change any page's fields and selections, create, rename,    *
 *   retype and delete pages, view the whole tree, and search it.            *
 *                                                                           *
 *   This file registers the module with the BBS and connects the BBS's     *
 *   calls to the editor's session runner.                                   *
 *                                                                           *
 *   Source map:                                                             *
 *     GALMM.C     this file - startup, shutdown, BBS entry points           *
 *     MMFIBER.C   sessions: runs the editor on a fiber per user             *
 *     MMEDIT.C    the editor: entry prompt, page and selection editing      *
 *     MMVIEW.C    tree view, search, installed-module list                  *
 *     MMSET.C     live settings (GALMMSET.DAT) and the !SETUP screen        *
 *     MMTVARS.C   named text variables used in GALMM.MSG                    *
 *     GALMMDEF.H  shared declarations                                       *
 *     GALMM.H     message numbers (generated from GALMM.MST)                *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0 - see the     *
 *   LICENSE file in the project root.                                       *
 *****************************************************************************/

#include "gcomm.h"
#include "majorbbs.h"
#include "GALMMDEF.H"

static GBOOL mminput(VOID);
static VOID  mmstatus(VOID);
static VOID  mmhangup(VOID);
static GBOOL mmlogon(VOID);
static VOID  mmshutdown(VOID);

static struct module galmm = {       /* our entry in the BBS module table    */
     "",                             /*   name (filled from GALMM.MDF)       */
     mmlogon,                        /*   logon: clear a stale session       */
     mminput,                        /*   a line of input while in module    */
     mmstatus,                       /*   status input (CYCLE wakes a pause) */
     NULL,                           /*   inject-other-user (engine default) */
     NULL,                           /*   logoff                             */
     mmhangup,                       /*   hang-up                            */
     NULL,                           /*   midnight cleanup                   */
     NULL,                           /*   account deleted                    */
     mmshutdown                      /*   system shutdown                    */
};

HMCVFILE mmmb;                       /* GALMM.MCV, our message file          */
INT mmstt;                           /* our module (state) number            */

VOID EXPORT
init__galmm(VOID)                    /* called once by the BBS at startup    */
{
     stzcpy(galmm.descrp, gmdnam("GALMM.MDF"), MNMSIZ);
     mmstt = register_module(&galmm);
     mmmb = opnmsg("GALMM.MCV");
     if (!ses_init()) {
          catastro("MENU MAGICIAN: NO MEMORY FOR SESSIONS");
     }
     set_open();                     /* settings: load, or seed the defaults */
     tv_init();
     globalcmd(ed_global);
     PrepareAppList();

     /* Name the running build in the Audit Trail so a sysop can always    */
     /* tell which version is loaded.                                       */
     shocst("MENU MAGICIAN v" GALMM_VERSION,
            "(C) Elwynor Technologies; orig. High Velocity Software");
}

static GBOOL
mminput(VOID)                        /* a line of input while in the module  */
{
     return ses_input();             /* FALSE returns the user to the menu   */
}

static VOID
mmstatus(VOID)                       /* a status code while in the module    */
{
     ses_status();
}

static VOID
mmhangup(VOID)                       /* the user on usrnum hung up           */
{
     ses_discard(usrnum);
}

static GBOOL
mmlogon(VOID)                        /* a user is logging on to usrnum       */
{
     /* Channels are reused the moment a caller drops.  Make sure nothing   */
     /* of a previous caller's session survives onto the new one, even if   */
     /* its hang-up was never seen.  (Sessions exist only for logged-on     */
     /* users, so logon is the right moment.  The engine's hook_connect()   */
     /* is for client/server logons only and never fires for telnet.)       */
     ses_discard(usrnum);
     return FALSE;                   /* nothing to show: carry on with logon */
}

static VOID
mmshutdown(VOID)                     /* the BBS is shutting down             */
{
     ses_close();
     set_close();
     clsmsg(mmmb);
}

VOID EXPORT
end_galmm(VOID)                      /* marks the end of our code for        */
{                                    /*   exception-dump analysis            */
}
