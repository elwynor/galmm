/* Build 26.10.19.2 01:12PM */
/*****************************************************************************
 *   MMSET.C                                  Menu Magician - Live settings   *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Menu Magician's own settings - the global edit command, the key it     *
 *   needs, and the create-orphan and view-tree commands - live in one       *
 *   record in HVSMMSET.DAT.  Up to 1.03a they were CNF options; now a sysop *
 *   changes them online with !SETUP and every change applies at once.      *
 *   A new install starts from the defaults in the table below, which are   *
 *   also what "Reset all" restores.                                         *
 *                                                                           *
 *   Adding a setting: a field at the END of struct mmsetdat (HVSMMDEF.H),   *
 *   taken from the spare space, and a row in setdefs.  An existing record  *
 *   reads the new field as zero, so bump MMS_LAYOUT and default the field  *
 *   in set_open() when zero is not a good value.                            *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "HVSMMDEF.H"

struct mmsetdat mmcfg;               /* the live settings                    */
static DFAFILE *mmsdat;              /* HVSMMSET.DAT                         */

enum { T_KEY, T_CMD };               /* setting types                        */

struct setdef {
     INT type;                       /* T_xxx                                */
     size_t offset;                  /* where it lives in struct mmsetdat    */
     INT size;                       /* buffer size, including the NUL       */
     const CHAR *defval;             /* default value                        */
     const CHAR *label;              /* what the sysop sees                  */
};

#define MMS_OFS(f) offsetof(struct mmsetdat, f)

/* The defaults are those of the 1.03a CNF options.                         */
static const struct setdef setdefs[] = {
     { T_KEY, MMS_OFS(glokey),  KEYSIZ,      "SYSOP", "Key for the global edit command" },
     { T_CMD, MMS_OFS(glocmd),  MMS_CMDSIZ,  "EDIT",  "Global edit command" },
     { T_CMD, MMS_OFS(orphcmd), MMS_CMDSIZ,  "!NEW",  "Create-orphan-page command" },
     { T_CMD, MMS_OFS(viewcmd), MMS_CMDSIZ,  "!VIEW", "View-menu-tree command" },
};
#define NSET ((INT)(sizeof(setdefs) / sizeof(setdefs[0])))

static CHAR *
setptr(INT idx)                      /* where setting idx is stored          */
{
     return (CHAR *)&mmcfg + setdefs[idx].offset;
}

/* ------------------------------------------------------------------------ *
 * File handling.                                                           *
 * ------------------------------------------------------------------------ */

static VOID
set_save(VOID)                       /* write mmcfg to HVSMMSET.DAT          */
{
     struct mmsetrec rec;
     GBOOL ok;

     /* rec.recid is the lookup key: it must be writable memory, never the  */
     /* MMS_RECID string constant - the DFA layer only copies as much of a  */
     /* key as it could write to, so a read-only key searches for nothing.  */
     setmem(&rec, sizeof(rec), 0);
     stzcpy(rec.recid, MMS_RECID, sizeof(rec.recid));
     mmcfg.layout = MMS_LAYOUT;
     rec.d = mmcfg;
     dfaSetBlk(mmsdat);
     if (dfaAcqEQ(NULL, rec.recid, 0)) {
          ok = dfaUpdateDup(&rec);   /* the ...Dup forms report an error     */
     }                               /* instead of stopping the BBS          */
     else {
          ok = dfaInsertDup(&rec);
     }
     dfaRstBlk();
     if (!ok) {
          shocst("MENU MAGICIAN ERROR", "Could not save settings to %s", MMS_FILE);
     }
}

static VOID
set_sanity(VOID)                     /* keep loaded values usable            */
{
     INT i;

     for (i = 0 ; i < NSET ; i++) {
          CHAR *p = setptr(i);

          p[setdefs[i].size - 1] = '\0';
          if (*p == '\0') {          /* every setting must have a value      */
               stzcpy(p, (CHAR *)setdefs[i].defval, setdefs[i].size);
          }
     }
}

static VOID
set_defaults(VOID)                   /* every setting to its default, saved  */
{
     INT i;

     setmem(&mmcfg, sizeof(mmcfg), 0);
     for (i = 0 ; i < NSET ; i++) {
          stzcpy(setptr(i), (CHAR *)setdefs[i].defval, setdefs[i].size);
     }
     set_save();
}

VOID
set_open(VOID)                       /* open the file; load or seed settings */
{
     struct mmsetrec rec;

     if (!isfile(MMS_FILE)) {
          struct dfaSegSpec seg;
          struct dfaKeySpec key;

          setmem(&seg, sizeof(seg), 0);
          seg.position = offsetof(struct mmsetrec, recid);
          seg.length = sizeof(rec.recid);
          seg.type = DFAST_ZSTRING;
          setmem(&key, sizeof(key), 0);
          key.flags = 0;
          key.nSegments = 1;
          key.segs = &seg;
          dfaCreateSpec(MMS_FILE, FALSE, sizeof(struct mmsetrec), 1024,
                        0, 0, 1, &key, NULL);
     }
     mmsdat = dfaOpen(MMS_FILE, sizeof(struct mmsetrec), NULL);

     setmem(&rec, sizeof(rec), 0);
     stzcpy(rec.recid, MMS_RECID, sizeof(rec.recid));
     dfaSetBlk(mmsdat);
     if (dfaAcqEQ(&rec, rec.recid, 0)) {
          dfaRstBlk();
          mmcfg = rec.d;
          set_sanity();
          return;
     }
     dfaRstBlk();
     set_defaults();                 /* a new install: the defaults          */
     shocst("MENU MAGICIAN SETTINGS", "New settings file, default values");
}

VOID
set_close(VOID)
{
     dfaClose(mmsdat);
}

/* ------------------------------------------------------------------------ *
 * The !SETUP screen.  It runs inside the editor's session, so it can ask  *
 * a question and wait for the answer.                                      *
 * ------------------------------------------------------------------------ */

static VOID
say(INT msgnum)
{
     setmbk(mmmb);
     prfmsg(msgnum);
     rstmbk();
}

static const CHAR *
set_limit(INT idx)                   /* describe the allowed values          */
{
     static CHAR buf[80];

     if (setdefs[idx].type == T_KEY) {
          sprintf(buf, "a key name, up to %d characters, no spaces",
                  setdefs[idx].size - 1);
     }
     else {
          sprintf(buf, "one word, up to %d characters", setdefs[idx].size - 1);
     }
     return buf;
}

static GBOOL
set_check(                           /* is this value allowed?               */
INT idx,
const CHAR *text)
{
     const CHAR *p;

     if (text[0] == '\0' || (INT)strlen(text) >= setdefs[idx].size) {
          return FALSE;
     }
     for (p = text ; *p != '\0' ; p++) {
          if (!isgraph((UCHAR)*p)) {           /* one word: no spaces      */
               return FALSE;
          }
     }
     if (setdefs[idx].type == T_CMD) {
          /* A command the entry prompt already uses would never be seen.   */
          if (sameas((CHAR *)text, "X") || sameas((CHAR *)text, "?")
           || sameas((CHAR *)text, "!SEARCH") || sameas((CHAR *)text, "!SETUP")) {
               return FALSE;
          }
     }
     return TRUE;
}

static VOID
set_list(VOID)                       /* show every setting                   */
{
     INT i;

     say(SETHDR);
     for (i = 0 ; i < NSET ; i++) {
          tv_setn(MTV_NUM, i + 1);
          tv_set(MTV_LABEL, setdefs[i].label);
          tv_set(MTV_VALUE, setptr(i));
          say(SETLINE);
     }
}

VOID
set_editor(VOID)                     /* change settings online               */
{
     CHAR *pIn;
     CHAR Value[MMS_CMDSIZ > KEYSIZ ? MMS_CMDSIZ : KEYSIZ];
     INT idx;

     set_list();
     while (TRUE) {
          tv_setn(MTV_NUM, NSET);
          say(SETPROM);
          ses_cinput();
          pIn = cncwrd();
          if (sameas(pIn, "X")) {
               cncall();
               return;
          }
          if (pIn[0] == '\0') {
               set_list();
               continue;
          }
          if (sameas(pIn, "R")) {
               say(SETRST);
               ses_cinput();
               if (cncyesno() == 'Y') {
                    set_defaults();
                    say(SETRSTD);
                    shocst("MENU MAGICIAN SETTINGS", "%s reset all settings",
                           usaptr->userid);
                    set_list();
               }
               cncall();
               continue;
          }
          idx = atoi(pIn) - 1;
          if (idx < 0 || idx >= NSET) {
               say(UNKCMD);
               cncall();
               continue;
          }

          /* Ask for the new value, showing the current one and the rules.  */
          tv_set(MTV_LABEL, setdefs[idx].label);
          tv_set(MTV_VALUE, setptr(idx));
          tv_set(MTV_LIMIT, set_limit(idx));
          say(SETVAL);
          ses_cinput();
          pIn = cncall();
          if (pIn[0] == '\0') {
               continue;             /* keep the current value               */
          }
          if (!set_check(idx, pIn)) {
               tv_set(MTV_LIMIT, set_limit(idx));
               say(SETBAD);
               continue;
          }
          stzcpy(Value, pIn, setdefs[idx].size);
          strupr(Value);
          stzcpy(setptr(idx), Value, setdefs[idx].size);
          set_save();
          tv_set(MTV_LABEL, setdefs[idx].label);
          tv_set(MTV_VALUE, setptr(idx));
          say(SETOK);

          /* Keep the audit line well under the 128-byte detail limit.      */
          shocst("MENU MAGICIAN SETTINGS", "%.30s set setting %d to %.16s",
                 usaptr->userid, idx + 1, setptr(idx));
     }
}
