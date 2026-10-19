/* Build 26.10.19.2 01:12PM */
/*****************************************************************************
 *   MMEDIT.C                                 Menu Magician - The editor      *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1993-1996 High Velocity Software, Inc.         *
 *                                                                           *
 *   The online menu tree editor: the "edit which page?" entry prompt, the   *
 *   page editor, the menu selection editor, and the page operations         *
 *   (create, rename, delete, change type).  Ported from the original        *
 *   HVSMM.C with its logic kept; see CHANGELOG.MD for what changed.         *
 *                                                                           *
 *   The editor runs inside a session (MMFIBER.C), so a call such as         *
 *   ses_cinput() really does wait for the user's answer.  Two rules follow  *
 *   from that.  Anything that touches the BBS's shared state - the menu     *
 *   file, the current message file - is set up and put back between waits, *
 *   never across one.  And any page read before a wait is read again after *
 *   it, because other users (and the BBS) used the menu file meanwhile.     *
 *                                                                           *
 *   The pages live in the BBS's own menu file, WGSMENU2.DAT, which the      *
 *   engine keeps open as mnubb.  Each record is a struct mnupag (SDK         *
 *   MAJORBBS.H); key 0 is the page type followed by the page name.  Menu    *
 *   Magician edits only terminal pages (type 'A').  The Major BBS v10 has  *
 *   no client/server mode, so any Worldgroup client/server pages (type 'C') *
 *   still in a menu file are left alone.                                    *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <time.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "HVSMMDEF.H"

INT UseChan = -1;                    /* channel using the editor (one at a   */
                                     /*   time), or -1                       */
INT TurboChan = -1;                  /* channel that just typed the global   */
                                     /*   edit command, or -1                */
struct mnupag Work;                  /* the page being edited                */
struct mnupag Scratch;               /* a second page, for side trips        */

static GBOOL ShouldSave = FALSE;     /* has Work changed since it was saved? */

static const CHAR *DisplayOpts[] = { "Hide option", "Dim option", "Show option" };

static VOID EditPage(VOID);
static VOID ShowPage(VOID);
static GBOOL HandleModuleInput(CHAR InChar);
static GBOOL HandleFileInput(CHAR InChar);
static GBOOL HandleMenuInput(CHAR InChar);
static GBOOL Save(GBOOL Ask);
static VOID ShowMenuChoice(INT Index);
static VOID EditMenuChoice(INT Index);
static GBOOL SideEffects(VOID);
static VOID KillPage(VOID);
static VOID RenamePage(const CHAR *NewName);
static VOID CreateOrphan(VOID);

/* ------------------------------------------------------------------------ *
 * Small helpers.                                                           *
 * ------------------------------------------------------------------------ */

static VOID
say(                                 /* print one of our messages            */
INT msgnum)
{
     /* The current message file is shared by every module and changes      */
     /* between our waits, so select ours for each message and put the      */
     /* previous one back.  setmbk()/rstmbk() are a stack: always paired.   */
     setmbk(mmmb);
     prfmsg(msgnum);
     rstmbk();
}

static const CHAR *
orNone(const CHAR *text)             /* "(None)" for an empty field          */
{
     return *text == '\0' ? "(None)" : text;
}

static const CHAR *
yesNo(GBOOL flag)
{
     return flag ? "Yes" : "No";
}

static const CHAR *
dispOpt(CHAR optdsp)                 /* "Hide/Dim/Show option"               */
{
     return DisplayOpts[(optdsp >= 0 && optdsp <= 2) ? optdsp : 2];
}

static GBOOL
IsSelection(                         /* is this character used on Work?      */
CHAR Char)
{
     INT Index;

     for (Index = 0 ; Index < MAXSEL ; Index++) {
          if (Work.page[Index].selchr == Char) {
               return TRUE;
          }
     }
     return FALSE;
}

static INT                           /*   selection index, or -1             */
FindPage(                            /* find the selection leading to a page */
struct mnupag *pPage,                /*   a menu page                        */
const CHAR *pName)                   /*   destination page name              */
{
     INT Index;

     for (Index = 0 ; Index < MAXSEL ; Index++) {
          if (sameas(pPage->page[Index].destpage, (CHAR *)pName)) {
               return Index;
          }
     }
     return -1;
}

static GBOOL
ValidateAAPage(                      /* is this a usable new page name?      */
const CHAR *pProposed)               /*   (sets HVSMM_REASON if not)         */
{
     INT Length = 0;

     while (*pProposed != '\0' && Length < PNMSIZ) {
          if (!istxvc(toupper(*pProposed))) {
               tv_set(MTV_REASON, "Invalid character in page name.");
               return FALSE;
          }
          pProposed++, Length++;
     }
     if (Length < 2) {
          tv_set(MTV_REASON, "Page name too short.");
          return FALSE;
     }
     if (Length == PNMSIZ) {
          tv_set(MTV_REASON, "Page name too long.");
          return FALSE;
     }
     return TRUE;
}

/* ------------------------------------------------------------------------ *
 * The menu file.  Every routine selects mnubb and puts the previous file   *
 * back before returning (dfaSetBlk/dfaRstBlk are a shared stack), so      *
 * nothing is left selected across a wait.                                  *
 * ------------------------------------------------------------------------ */

GBOOL
LoadPage(                            /* read a page; it becomes "current"    */
struct mnupag *page,                 /*   where to put it                    */
const CHAR *name)                    /*   page name                          */
{
     /* The key is the page type followed by the page name, exactly as at  */
     /* the start of struct mnupag.  It must be in writable memory: the DFA */
     /* layer copies only as much of the key as it may write to.            */
     CHAR key[1 + PNMSIZ];
     GBOOL found;

     setmem(key, sizeof(key), 0);
     key[0] = MM_TERMINAL;
     stzcpy(key + 1, (CHAR *)name, PNMSIZ);
     dfaSetBlk(mnubb);
     found = dfaAcqEQ(page, key, 0);
     dfaRstBlk();
     if (found) {
          unpack(page);              /* selections back into their slots     */
     }
     return found;
}

VOID
SavePage(                            /* rewrite the page last loaded         */
struct mnupag *page)                 /*   with these contents                */
{
     /* savepage() is the engine's own routine (MNUUTL.C): it packs the    */
     /* selections, writes the variable-length record and unpacks again.   */
     /* An update must directly follow the read that found the record.     */
     dfaSetBlk(mnubb);
     savepage(page, FALSE);
     dfaRstBlk();
}

VOID
InsertPage(                          /* add a new page                       */
struct mnupag *page)
{
     dfaSetBlk(mnubb);
     savepage(page, TRUE);
     dfaRstBlk();
}

VOID
DeletePage(VOID)                     /* delete the page last loaded          */
{
     dfaSetBlk(mnubb);
     dfaDelete();
     dfaRstBlk();
}

LONG
PagePos(VOID)                        /* file position of the page last read  */
{
     LONG pos;

     dfaSetBlk(mnubb);
     pos = dfaAbs();
     dfaRstBlk();
     return pos;
}

VOID
PageAt(                              /* re-read a page by file position      */
struct mnupag *page,
LONG pos)
{
     dfaSetBlk(mnubb);
     dfaGetAbs(page, pos, 0);
     dfaRstBlk();
     unpack(page);
}

/* ------------------------------------------------------------------------ *
 * Entry: the global command and the session routine.                       *
 * ------------------------------------------------------------------------ */

INT
ed_global(VOID)                      /* global command: edit the current page*/
{
     if (usrptr->state != 0 || margc < 1 || !sameas(margv[0], mmcfg.glocmd)
      || haskey(mmcfg.glokey) != 1) {
          return 0;
     }
     if (UseChan == usrnum) {
          /* This user left an editing session without exiting it (with     */
          /* /GO, say).  Drop it rather than lock them out of their own.    */
          ses_discard(usrnum);
     }
     if (UseChan != -1) {
          say(INUSE);
          outprf(usrnum);
          return 1;
     }
     TurboChan = usrnum;

     /* Move the user from the menu into our module.  If we set the state   */
     /* here and return 0, the engine goes on to call the CURRENT module's  */
     /* input routine - now ours (WG33 MAJORBBS.C hdlcri, 3326-3356).       */
     usrptr->state = mmstt;
     usrptr->substt = 0;
     usrptr->flags |= X2MAIN;

     /* "EDIT page" edits that page; plain "EDIT" edits the current one.    */
     Work.menutype[0] = MM_TERMINAL;
     if (margc == 2) {
          stzcpy(Work.pagnam, margv[1], PNMSIZ);
          strupr(Work.pagnam);
     }
     else {
          stzcpy(Work.pagnam, mnuoff(usrnum)->curpag, PNMSIZ);
     }
     input[0] = '\0';
     parsin();
     btuoes(usrnum, 0);
     usrptr->flags &= ~INJOIP;
     setmem(vdaptr, vdasiz, 0);
     clrprf();
     stzcpy(mnuoff(usrnum)->parpag, mnuoff(usrnum)->curpag, PNMSIZ);
     return 0;
}

VOID
ed_release(                          /* a channel's session has ended        */
INT chan)
{
     if (UseChan == chan) {
          UseChan = -1;
     }
     if (TurboChan == chan) {
          TurboChan = -1;
     }
}

VOID
ed_main(VOID)                        /* the session routine                  */
{
     GBOOL turbo = (TurboChan == usrnum);
     CHAR Page[PNMSIZ];
     CHAR *pPage;

     TurboChan = -1;
     bgncnc();                       /* skip the character that chose us     */
     cncchr();

     if (UseChan != -1 && UseChan != usrnum) {
          say(INUSE);
          return;
     }

     if (turbo) {                    /* came in with the global command      */
          UseChan = usrnum;
          if (!LoadPage(&Work, Work.pagnam)) {
               stzcpy(Work.pagnam, mnuoff(usrnum)->curpag, PNMSIZ);
               if (!LoadPage(&Work, Work.pagnam)) {
                    say(NOPAGE);
                    UseChan = -1;
                    return;
               }
          }
          ShouldSave = FALSE;
          cncall();
          EditPage();
          UseChan = -1;
          return;
     }

     say(MENUIN);
     say(BIGENTER);
     UseChan = usrnum;
     ShouldSave = FALSE;

     while (TRUE) {
          say(PICKPAG);
          ses_cinput();
          pPage = cncall();

          if (*pPage != '\0') {
               if (sameas(pPage, "X")) {
                    break;
               }
               if (sameas(pPage, "?")) {
                    say(PICKHLP);
               }
               else if (sameas(pPage, mmcfg.viewcmd)) {
                    ShowTree();
               }
               else if (sameas(pPage, mmcfg.orphcmd)) {
                    CreateOrphan();
               }
               else if (sameas(pPage, "!SEARCH")) {
                    Search();
               }
               else if (sameas(pPage, "!SETUP")) {
                    if (haskey("SYSOP") == 1) {
                         set_editor();
                    }
                    else {
                         say(SETNOKEY);
                    }
               }
               else {
                    stzcpy(Page, pPage, PNMSIZ);
                    strupr(Page);
                    if (LoadPage(&Work, Page)) {
                         ShouldSave = FALSE;
                         EditPage();
                    }
                    else {
                         say(NOPAGE);
                    }
               }
          }
          cncall();
     }
     UseChan = -1;
}

/* ------------------------------------------------------------------------ *
 * The page editor.                                                         *
 * ------------------------------------------------------------------------ */

static GBOOL                         /*   TRUE = the user backed out         */
Save(                                /* save Work, optionally asking first   */
GBOOL Ask)
{
     CHAR *pRest;

     ShouldSave = FALSE;
     cncall();

     if (Ask) {
          while (TRUE) {
               say(NOTSAVED);
               ses_cinput();
               pRest = cncall();

               if (sameas(pRest, "X")) {
                    ShouldSave = TRUE;         /* still unsaved              */
                    return TRUE;
               }
               if (sameas(pRest, "N")) {
                    return FALSE;
               }
               if (sameas(pRest, "Y")) {
                    tv_set(MTV_PAGE, Work.pagnam);
                    say(SAVED);
                    break;
               }
          }
     }

     /* Read the stored page (into Scratch) so the update has a record to   */
     /* replace, then write Work over it.                                   */
     if (LoadPage(&Scratch, Work.pagnam)) {
          SavePage(&Work);
     }
     return FALSE;
}

static VOID
ShowPage(VOID)                       /* display Work's fields                */
{
     tv_set(MTV_PAGE, Work.pagnam);
     tv_set(MTV_PARENT, orNone(Work.parpag));
     tv_set(MTV_DISABLED, yesNo(Work.flags & DISABL));
     tv_set(MTV_GOTO, yesNo(Work.flags & CNGOTO));
     tv_set(MTV_GOKEY, orNone(Work.golock));
     say(COMMON);

     if (Work.flags & MDLPAG) {
          tv_set(MTV_MODULE, orNone(Work.modnam));
          tv_set(MTV_COMMAND, orNone(Work.cmdstg));
          tv_set(MTV_HEADER, yesNo(!(Work.flags & NOMPHD)));
          say(MODULE);
     }
     else if (Work.flags & FILPAG) {
          tv_set(MTV_FILE, Work.fname);
          tv_set(MTV_HEADER, yesNo(!(Work.flags & NOFPHD)));
          say(FILEP);
     }
     else {
          tv_set(MTV_AUTO, yesNo(Work.flags & AUTPAG));
          tv_set(MTV_DEFDISP, yesNo(Work.flags & DFTDSP));
          tv_set(MTV_FILE, orNone(Work.fname));
          tv_set(MTV_TITLE, orNone(Work.mnuttl));
          say(MENU);
     }
     say(BIGMENU);
}

static VOID
EditPage(VOID)                       /* the page editor's main loop          */
{
     GBOOL AteInput;
     GBOOL Typed;
     INT NewFlags;
     const CHAR *pNewType = "";
     CHAR InChar;
     CHAR *pJunk;
     CHAR Name[PNMSIZ];
     CHAR Choice;
     INT Index;

     ShowPage();

     while (TRUE) {
          tv_set(MTV_PAGE, Work.pagnam);
          say(PROMPT);
          ses_cinput();
          InChar = cncchr();

          if (InChar == 'X') {
               if (!ShouldSave || !Save(TRUE)) {
                    break;
               }
          }

          if (InChar == '\0' && !(usrptr->flags & INJOIP)) {
               InChar = 'V';
          }

          switch (InChar) {
          case 'V':                  /* view the page again                  */
               ShowPage();
               cncall();
               break;

          case '?':                  /* help                                 */
               tv_set(MTV_PAGE, Work.pagnam);
               say(BIGHELP);
               outprf(usrnum);
               ses_line();
               ShowPage();
               cncall();
               break;

          case 'J':                  /* jump to another page                 */
               if (ShouldSave && Save(TRUE)) {
                    break;
               }
               say((Work.flags & (MDLPAG | FILPAG)) ? GOTO : GOTOM);
               ses_cinput();
               pJunk = cncwrd();

               /* On a menu page, a single character may be a selection.    */
               if (strlen(pJunk) == 1 && !(Work.flags & (MDLPAG | FILPAG))) {
                    for (Index = 0 ; Index < MAXSEL ; Index++) {
                         if (toupper(Work.page[Index].selchr) == toupper(*pJunk)) {
                              pJunk = Work.page[Index].destpage;
                              break;
                         }
                    }
               }
               stzcpy(Name, pJunk, PNMSIZ);
               strupr(Name);
               if (LoadPage(&Work, Name)) {
                    ShowPage();
               }
               else {
                    say(NOGOTO);
               }
               cncall();
               break;

          case 'P':                  /* jump to the parent page              */
               if (ShouldSave && Save(TRUE)) {
                    break;
               }
               if (Work.flags & TPOTRE) {
                    say(NOPAR);
                    break;
               }
               stzcpy(Name, Work.parpag, PNMSIZ);
               if (LoadPage(&Work, Name)) {
                    ShowPage();
               }
               else {
                    say(NOGOTO);
               }
               cncall();
               break;

          case 'R':                  /* rename                               */
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               if (sameas(mnuoff(usrnum)->parpag, Work.pagnam)) {
                    say(BADIDEA);
                    break;
               }
               if (!SideEffects()) {
                    break;
               }
               while (TRUE) {
                    say(RENWHAT);
                    outprf(usrnum);
                    ses_line();
                    if (!(usrptr->flags & NOINJO)) {
                         break;
                    }
               }
               if (input[0] == '\0') {
                    break;
               }
               if (!ValidateAAPage(input)) {
                    tv_set(MTV_ACTION, "rename");
                    say(BADPAG);
                    break;
               }
               stzcpy(Name, input, PNMSIZ);
               strupr(Name);
               if (LoadPage(&Scratch, Name)) {
                    say(CORPALR);
                    break;
               }
               RenamePage(Name);
               cncall();
               break;

          case '!':                  /* delete                               */
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               if (sameas(mnuoff(usrnum)->parpag, Work.pagnam)) {
                    say(BADIDEA);
                    break;
               }
               if (!(Work.flags & (FILPAG | MDLPAG))) {
                    say(DELWARN);
                    cncall();
                    ses_cinput();
                    if (cncyesno() != 'Y') {
                         break;
                    }
               }
               if (!SideEffects()) {
                    break;
               }
               KillPage();
               cncall();
               return;

          case 'T':                  /* change the page type                 */
               Typed = FALSE;
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               if (sameas(mnuoff(usrnum)->parpag, Work.pagnam)) {
                    say(BADIDEA);
                    break;
               }
               if (!(Work.flags & (FILPAG | MDLPAG))) {
                    say(TYPWARN);
                    cncall();
                    ses_cinput();
                    if (cncyesno() != 'Y') {
                         break;
                    }
               }
               if (!SideEffects()) {
                    break;
               }
               say(TYPPROM);
               ses_cinput();
               Choice = cncchr();

               NewFlags = Work.flags & TPOTRE;
               if (Choice == 'L' && !(Work.flags & MDLPAG)) {
                    Typed = TRUE;
                    NewFlags |= MDLPAG | CNGOTO;
                    pNewType = "Module page";
               }
               else if (Choice == 'F' && !(Work.flags & FILPAG)) {
                    Typed = TRUE;
                    NewFlags |= FILPAG | CNGOTO;
                    pNewType = "File page";
               }
               else if (Choice == 'M' && (Work.flags & (MDLPAG | FILPAG))) {
                    Typed = TRUE;
                    NewFlags |= DFTDSP | CNGOTO;
                    pNewType = "Menu page";
               }

               if (Typed) {
                    /* A menu page's children lose their parent: orphan them.*/
                    if (!(Work.flags & (FILPAG | MDLPAG))) {
                         for (Index = 0 ; Index < MAXSEL ; Index++) {
                              if (Work.page[Index].destpage[0] != '\0') {
                                   tv_set(MTV_VALUE, Work.page[Index].destpage);
                                   say(MOVING);
                                   if (LoadPage(&Scratch, Work.page[Index].destpage)) {
                                        strcpy(Scratch.parpag, "TOP");
                                        Scratch.flags |= TPOTRE;
                                        SavePage(&Scratch);
                                   }
                              }
                         }
                    }
                    setmem(Work.page, sizeof(Work.page), 0);
                    setmem(Work.fname, sizeof(Work.fname), 0);
                    setmem(Work.modnam, sizeof(Work.modnam), 0);
                    setmem(Work.cmdstg, sizeof(Work.cmdstg), 0);
                    setmem(Work.golock, sizeof(Work.golock), 0);
                    Work.npages = 0;
                    Work.flags = (SHORT)NewFlags;
                    tv_set(MTV_VALUE, pNewType);
                    say(TYPSET);
                    ShouldSave = TRUE;
               }
               cncall();
               break;

          case 'D':                  /* toggle "disabled"                    */
               cncall();
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               Work.flags ^= DISABL;
               tv_set(MTV_VALUE, (Work.flags & DISABL) ? "now" : "no longer");
               say(DISTOG);
               ShouldSave = TRUE;
               break;

          case 'G':                  /* toggle "can /GO here"                */
               cncall();
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               Work.flags ^= CNGOTO;
               tv_set(MTV_VALUE, (Work.flags & CNGOTO) ? "no longer" : "now");
               say(ALGOTOG);
               ShouldSave = TRUE;
               break;

          case 'K':                  /* key needed to /GO here               */
               if (sameas(Work.pagnam, "TOP")) {
                    say(NOTTOP);
                    break;
               }
               say(KEYPROM);
               ses_cinput();
               pJunk = cncwrd();
               if (*pJunk != '\0') {
                    if (sameas(pJunk, "X")) {
                         Work.golock[0] = '\0';
                    }
                    else {
                         stzcpy(Work.golock, pJunk, KEYSIZ);
                         strupr(Work.golock);
                    }
                    tv_set(MTV_VALUE, orNone(Work.golock));
                    say(GOSET);
                    ShouldSave = TRUE;
               }
               cncall();
               break;

          case 'S':                  /* save                                 */
               Save(FALSE);
               tv_set(MTV_PAGE, Work.pagnam);
               say(SAVED);
               cncall();
               break;

          case '\0':
               cncall();
               break;

          default:                   /* fields that depend on the page type  */
               if (Work.flags & MDLPAG) {
                    AteInput = HandleModuleInput(InChar);
               }
               else if (Work.flags & FILPAG) {
                    AteInput = HandleFileInput(InChar);
               }
               else {
                    AteInput = HandleMenuInput(InChar);
               }
               if (!AteInput) {
                    say(UNKCMD);
                    cncall();
               }
               break;
          }
     }
}

/* ------------------------------------------------------------------------ *
 * Fields of module, file and menu pages.                                   *
 * ------------------------------------------------------------------------ */

static GBOOL
HandleModuleInput(CHAR InChar)       /* module page commands                 */
{
     INT Number;
     INT MaxMod;
     CHAR **pList;
     CHAR Temp[MNMSIZ];
     CHAR TheChar;
     CHAR *pTemp;

     switch (InChar) {
     case 'M':                       /* module name                          */
          while (TRUE) {
               say(MODPROM);
               ses_cinput();
               pTemp = cncall();
               if (*pTemp == '\0') {
                    if (!(usrptr->flags & INJOIP)) {
                         break;
                    }
                    continue;
               }
               if (sameas(pTemp, "?")) {
                    DumpModules();
                    continue;
               }
               stzcpy(Temp, pTemp, MNMSIZ);
               Number = atoi(Temp);
               if (Number == 0 && Temp[0] != '0') {     /* not a number      */
                    Number = -1;
               }
               pList = ppTerminal;
               MaxMod = nTerminal;
               if (Number >= 0 && Number < MaxMod) {
                    stzcpy(Temp, pList[Number], MNMSIZ);
               }
               else if (!CheckList(Temp, pList, MaxMod)) {
                    say(NOMOD);
                    ses_cinput();
                    if (cncyesno() != 'Y') {
                         break;
                    }
               }
               stzcpy(Work.modnam, Temp, MNMSIZ);
               tv_set(MTV_VALUE, Work.modnam);
               say(SETMOD);
               ShouldSave = TRUE;
               cncall();
               break;
          }
          return TRUE;

     case 'C':                       /* command string                       */
          say(CMDSTG);
          /* Let the sysop type a global command as the command string.     */
          usrptr->flags |= NOGLOB;
          ses_cinput();
          usrptr->flags &= ~NOGLOB;
          pTemp = cncall();
          if (*pTemp != '\0') {
               if (sameas(pTemp, "X")) {
                    Work.cmdstg[0] = '\0';
               }
               else {
                    stzcpy(Work.cmdstg, pTemp, CMDSIZ);
               }
               ShouldSave = TRUE;
               tv_set(MTV_VALUE, orNone(Work.cmdstg));
               say(CMDSET);
          }
          return TRUE;

     case 'H':                       /* show the module page header?         */
          say(MPHPROM);
          ses_cinput();
          TheChar = cncchr();
          if (TheChar == 'Y') {
               Work.flags &= ~NOMPHD;
          }
          else if (TheChar == 'N') {
               Work.flags |= NOMPHD;
          }
          tv_set(MTV_VALUE, yesNo(!(Work.flags & NOMPHD)));
          say(MPHSET);
          cncall();
          ShouldSave = TRUE;
          return TRUE;
     }
     return FALSE;
}

static GBOOL
HandleFileInput(CHAR InChar)         /* file page commands                   */
{
     CHAR TheChar;
     CHAR *pTemp;

     switch (InChar) {
     case 'F':                       /* file to display                      */
          say(FILPROM);
          ses_cinput();
          pTemp = cncwrd();
          if (*pTemp != '\0') {
               stzcpy(Work.fname, pTemp, FNMSIZ);
               strupr(Work.fname);
               tv_set(MTV_VALUE, Work.fname);
               say(FILSET);
               ShouldSave = TRUE;
          }
          cncall();
          return TRUE;

     case 'H':                       /* show the file page header?           */
          say(FPHPROM);
          ses_cinput();
          TheChar = cncchr();
          if (TheChar == 'Y') {
               Work.flags &= ~NOFPHD;
          }
          else if (TheChar == 'N') {
               Work.flags |= NOFPHD;
          }
          tv_set(MTV_VALUE, yesNo(!(Work.flags & NOFPHD)));
          say(FPHSET);
          cncall();
          ShouldSave = TRUE;
          return TRUE;
     }
     return FALSE;
}

static VOID
ListLine(                            /* one line of a selections listing     */
INT msgnum,                          /*   MENULIN or MENUEND                 */
INT Left,                            /*   left-hand selection index          */
INT Right,                           /*   right-hand index, or -1            */
GBOOL Pages)                         /*   TRUE: show page names, not titles  */
{
     struct pglink *l = &Work.page[Left];

     /* A slot is in use if it has a selection character.                   */
     tv_setn(MTV_NUM, Left + 1);
     tv_setc(MTV_SEL, l->selchr == '\0' ? ' ' : l->selchr);
     tv_set(MTV_TEXT, l->selchr == '\0' ? "(Not used)"
                                        : (Pages ? l->destpage : l->shortd));
     if (Right >= 0) {
          struct pglink *r = &Work.page[Right];

          tv_setn(MTV_NUM2, Right + 1);
          tv_setc(MTV_SEL2, r->selchr == '\0' ? ' ' : r->selchr);
          tv_set(MTV_TEXT2, r->selchr == '\0' ? "(Not used)"
                                              : (Pages ? r->destpage : r->shortd));
     }
     say(msgnum);
}

static VOID
ListSelections(                      /* list a menu page's 25 selections     */
GBOOL Pages)                         /*   TRUE: page names, FALSE: titles    */
{
     INT Index;

     say(Pages ? MENUPHDR : MENUHDR);
     /* Two columns: 1-12 on the left, 14-25 on the right, then 13.         */
     for (Index = 0 ; Index < MAXSEL / 2 ; Index++) {
          ListLine(MENULIN, Index, Index + 1 + MAXSEL / 2, Pages);
     }
     ListLine(MENUEND, MAXSEL / 2, -1, Pages);
}

static GBOOL
HandleMenuInput(CHAR InChar)         /* menu page commands                   */
{
     INT Choice;
     CHAR Number[8];
     CHAR *pTemp;

     switch (InChar) {
     case 'L':                       /* short title                          */
          say(MTTLPROM);
          ses_cinput();
          pTemp = cncall();
          if (*pTemp != '\0') {
               if (sameas(pTemp, "X")) {
                    Work.mnuttl[0] = '\0';
               }
               else {
                    stzcpy(Work.mnuttl, pTemp, TITLSZ);
               }
               tv_set(MTV_VALUE, orNone(Work.mnuttl));
               say(TTLSET);
               ShouldSave = TRUE;
          }
          return TRUE;

     case 'A':                       /* auto-select                          */
          Work.flags ^= AUTPAG;
          ShouldSave = TRUE;
          tv_set(MTV_VALUE, (Work.flags & AUTPAG) ? "On" : "Off");
          say(AUTSET);
          return TRUE;

     case 'N':                       /* default display                      */
          Work.flags ^= DFTDSP;
          ShouldSave = TRUE;
          tv_set(MTV_VALUE, (Work.flags & DFTDSP) ? "On" : "Off");
          say(DFTSET);
          return TRUE;

     case 'F':                       /* base name of the menu's files        */
          say(MFILPROM);
          ses_cinput();
          pTemp = cncwrd();
          if (*pTemp != '\0') {
               stzcpy(Work.fname, pTemp, FNSIZE);
               strupr(Work.fname);
               tv_set(MTV_VALUE, Work.fname);
               say(FILSET);
               ShouldSave = TRUE;
          }
          cncall();
          return TRUE;

     case 'M':                       /* list selections by description       */
          ListSelections(FALSE);
          cncall();
          return TRUE;

     case 'C':                       /* list selections by destination page  */
          ListSelections(TRUE);
          cncall();
          return TRUE;

     case '1': case '2': case '3': case '4':
     case '5': case '6': case '7': case '8': case '9':
          /* A selection number: the first digit was already taken.        */
          Number[0] = InChar;
          stzcpy(Number + 1, cncall(), sizeof(Number) - 1);
          Choice = atoi(Number);
          if (Choice > 0 && Choice <= MAXSEL) {
               cncall();
               EditMenuChoice(Choice - 1);
               return TRUE;
          }
          break;
     }
     return FALSE;
}

/* ------------------------------------------------------------------------ *
 * The menu selection editor.                                               *
 * ------------------------------------------------------------------------ */

static VOID
ShowMenuChoice(INT Index)            /* display one menu selection           */
{
     struct pglink *Link = &Work.page[Index];

     tv_setn(MTV_NUM, Index + 1);
     tv_set(MTV_PAGE, Work.pagnam);
     tv_setc(MTV_SEL, Link->selchr == '\0' ? ' ' : Link->selchr);
     tv_set(MTV_SHORT, Link->shortd);
     tv_set(MTV_KEY, orNone(Link->keyreq));
     tv_set(MTV_DISPLAY, dispOpt(Link->optdsp));
     tv_set(MTV_DEST, Link->destpage);
     say(EDTCHO);
}

static VOID
EditMenuChoice(INT Index)            /* edit one menu selection              */
{
     CHAR Original[PNMSIZ];
     CHAR Temp[PNMSIZ];
     CHAR Used[MAXSEL + 1];
     CHAR *pTemp;
     CHAR Choice;
     INT Dest;
     INT SelTemp;
     INT Chars;
     struct pglink Swap;

     ShowMenuChoice(Index);

     while (TRUE) {
          tv_setn(MTV_NUM, Index + 1);
          tv_set(MTV_PAGE, Work.pagnam);
          say(CHOPROM);
          ses_cinput();
          Choice = cncchr();

          if (Choice == 'X') {
               /* A selection that leads nowhere is not kept.               */
               if (Work.page[Index].destpage[0] == '\0') {
                    setmem(&Work.page[Index], sizeof(struct pglink), 0);
               }
               break;
          }

          switch (Choice) {
          case '?':
               say(CHOHELP);
               break;

          case 'S':                  /* save the page                        */
               Save(FALSE);
               tv_set(MTV_PAGE, Work.pagnam);
               say(SAVED);
               break;

          case 'C':                  /* selection character                  */
               Chars = 0;
               for (SelTemp = 0 ; SelTemp < MAXSEL ; SelTemp++) {
                    if (Work.page[SelTemp].selchr != '\0') {
                         Used[Chars++] = Work.page[SelTemp].selchr;
                    }
               }
               Used[Chars] = '\0';
               tv_set(MTV_VALUE, Used);
               say(SELPROM);
               ses_cinput();
               Choice = cncchr();
               if (Choice != '\0') {
                    Choice = (CHAR)toupper(Choice);
                    if (IsSelection(Choice)) {
                         say(ALRSEL);
                    }
                    else {
                         Work.page[Index].selchr = Choice;
                         tv_setc(MTV_VALUE, Choice);
                         say(SELSET);
                         ShouldSave = TRUE;
                    }
               }
               cncall();
               break;

          case 'H':                  /* short description                    */
               if (Work.page[Index].selchr == '\0') {
                    say(NOCWARN);
               }
               say(CSHOPROM);
               ses_cinput();
               pTemp = cncall();
               if (*pTemp != '\0') {
                    if (sameas(pTemp, "X")) {
                         Work.page[Index].shortd[0] = '\0';
                    }
                    else {
                         stzcpy(Work.page[Index].shortd, pTemp, SHDSIZ);
                    }
                    ShouldSave = TRUE;
                    tv_set(MTV_VALUE, orNone(Work.page[Index].shortd));
                    say(CSHOSET);
               }
               break;

          case 'A':                  /* key needed to use it                 */
               if (Work.page[Index].selchr == '\0') {
                    say(NOCWARN);
               }
               say(CKEYPROM);
               ses_cinput();
               pTemp = cncwrd();
               if (*pTemp != '\0') {
                    if (sameas(pTemp, "X")) {
                         Work.page[Index].keyreq[0] = '\0';
                    }
                    else {
                         stzcpy(Work.page[Index].keyreq, pTemp, KEYSIZ);
                         strupr(Work.page[Index].keyreq);
                    }
                    tv_set(MTV_VALUE, orNone(Work.page[Index].keyreq));
                    say(CKEYSET);
                    ShouldSave = TRUE;
               }
               cncall();
               break;

          case 'D':                  /* how to show it to users without key  */
               if (Work.page[Index].selchr == '\0') {
                    say(NOCWARN);
               }
               say(CDISPROM);
               ses_cinput();
               Choice = cncchr();
               if (Choice == 'H' || Choice == 'D' || Choice == 'S') {
                    Work.page[Index].optdsp = (Choice == 'H') ? 0
                                            : (Choice == 'D') ? 1 : 2;
                    tv_set(MTV_VALUE, dispOpt(Work.page[Index].optdsp));
                    say(CDISSET);
                    ShouldSave = TRUE;
               }
               cncall();
               break;

          case 'P':                  /* destination page                     */
               if (Work.page[Index].selchr == '\0') {
                    say(NOCWARN);
               }
               /* Keep the old destination: it becomes an orphan if the    */
               /* selection is pointed somewhere else.                      */
               stzcpy(Original, Work.page[Index].destpage, PNMSIZ);
               if (Original[0] != '\0') {
                    say(CPAGALR);
               }
               say(CPAGPROM);
               ses_cinput();
               stzcpy(Temp, cncwrd(), PNMSIZ);
               cncall();
               if (Temp[0] == '\0' || sameas(Temp, Original)) {
                    break;
               }
               strupr(Temp);
               if (sameas(Temp, "TOP")) {
                    say(NOTTOP);
                    break;
               }

               if (LoadPage(&Scratch, Temp)) {
                    /* The page exists.  An orphan can be adopted here; a   */
                    /* page that already has a parent cannot.               */
                    if (!(Scratch.flags & TPOTRE)) {
                         tv_set(MTV_VALUE, Temp);
                         say(CPAGNORP);
                         break;
                    }
                    while (TRUE) {
                         say(CPAGELSE);
                         ses_cinput();
                         Choice = (CHAR)toupper(cncchr());
                         cncall();
                         if (Choice == 'Y' || Choice == 'N') {
                              break;
                         }
                    }
                    if (Choice == 'N') {
                         break;
                    }
                    /* Read it again: the menu file was used while we waited.*/
                    if (!LoadPage(&Scratch, Temp)) {
                         say(NOGOTO);
                         break;
                    }
                    Scratch.flags &= ~TPOTRE;
                    stzcpy(Scratch.parpag, Work.pagnam, PNMSIZ);
                    SavePage(&Scratch);
                    ShouldSave = TRUE;
               }
               else {
                    /* A new page.  Ask its type now, so it is created right */
                    /* rather than changed with (T)ype afterwards.          */
                    while (TRUE) {
                         say(CPAGTYP);
                         ses_cinput();
                         Choice = cncchr();
                         cncall();
                         if (Choice == 'M' || Choice == 'L'
                          || Choice == 'X' || Choice == 'F') {
                              break;
                         }
                    }
                    if (Choice == 'X') {
                         break;
                    }
                    if (LoadPage(&Scratch, Temp)) {
                         say(CORPALR);         /* appeared while we waited   */
                         break;
                    }
                    setmem(&Scratch, sizeof(Scratch), 0);
                    stzcpy(Scratch.pagnam, Temp, PNMSIZ);
                    stzcpy(Scratch.parpag, Work.pagnam, PNMSIZ);
                    if (Choice == 'L') {
                         Scratch.flags = MDLPAG | CNGOTO;
                    }
                    else if (Choice == 'F') {
                         Scratch.flags = FILPAG | CNGOTO;
                    }
                    else {
                         Scratch.flags = DFTDSP | CNGOTO;
                    }
                    Scratch.menutype[0] = MM_TERMINAL;
                    InsertPage(&Scratch);
                    tv_set(MTV_VALUE, Temp);
                    say(CMAKED);
               }

               /* The selection now leads to the new page.                  */
               tv_set(MTV_VALUE, Temp);
               say(CPAGSET);
               stzcpy(Work.page[Index].destpage, Temp, PNMSIZ);
               Save(FALSE);

               if (Original[0] != '\0') {
                    /* The old destination is now an orphan.                */
                    if (LoadPage(&Scratch, Original)) {
                         tv_set(MTV_VALUE, Original);
                         say(CORPHED);
                         strcpy(Scratch.parpag, "TOP");
                         Scratch.flags |= TPOTRE;
                         SavePage(&Scratch);
                    }
               }
               break;

          case 'W':                  /* swap with another selection          */
               say(SWAPWHER);
               ses_cinput();
               Dest = cncint();
               if (Dest == 0) {
                    break;
               }
               if (Dest > 0 && Dest <= MAXSEL) {
                    Dest--;
                    Swap = Work.page[Index];
                    Work.page[Index] = Work.page[Dest];
                    Work.page[Dest] = Swap;
                    tv_setn(MTV_NUM, Index + 1);
                    tv_setn(MTV_NUM2, Dest + 1);
                    say(SWAPOK);
               }
               else {
                    say(SWAPBAD);
               }
               Choice = '\0';
               /* fall through: show the selection again                    */

          default:
               if (Choice == '\0' && !(usrptr->flags & INJOIP)) {
                    ShowMenuChoice(Index);
               }
               else if (Choice != '\0') {
                    say(UNKCMD);
               }
               cncall();
               break;
          }
     }
}

/* ------------------------------------------------------------------------ *
 * Page operations.                                                         *
 * ------------------------------------------------------------------------ */

static GBOOL                         /*   TRUE = go ahead                    */
SideEffects(VOID)                    /* warn before a risky change           */
{
     INT Index;
     INT Users = 0;
     GBOOL Continue = TRUE;

     /* Changing a page under users who are on it, or who will return to   */
     /* it, can confuse the menu system.  Warn harder if others are on.     */
     for (Index = 0 ; Index < nterms ; Index++) {
          if (usroff(Index)->usrcls >= SUPLON) {
               Users++;
          }
     }
     say(Users != 1 ? SIDEBAD : SIDEWARN);
     cncall();
     ses_cinput();
     if (cncyesno() != 'Y') {
          Continue = FALSE;
     }
     cncall();
     return Continue;
}

static VOID
KillPage(VOID)                       /* delete Work                          */
{
     INT Index;

     /* Clear the selection on the parent page that leads here.             */
     if (!(Work.flags & TPOTRE)) {
          if (LoadPage(&Scratch, Work.parpag)) {
               Index = FindPage(&Scratch, Work.pagnam);
               if (Index != -1) {
                    setmem(&Scratch.page[Index], sizeof(struct pglink), 0);
                    SavePage(&Scratch);
               }
          }
     }

     if (LoadPage(&Scratch, Work.pagnam)) {          /* still there?         */
          DeletePage();
          tv_set(MTV_VALUE, Work.pagnam);
          say(DELING);

          /* A deleted menu page's children become orphans.                 */
          if (!(Work.flags & (FILPAG | MDLPAG))) {
               for (Index = 0 ; Index < MAXSEL ; Index++) {
                    if (Work.page[Index].selchr != '\0') {
                         tv_set(MTV_VALUE, Work.page[Index].destpage);
                         say(MOVING);
                         if (LoadPage(&Scratch, Work.page[Index].destpage)) {
                              strcpy(Scratch.parpag, "TOP");
                              Scratch.flags |= TPOTRE;
                              SavePage(&Scratch);
                         }
                    }
               }
          }
     }
     say(DELEND);
}

static VOID
RenamePage(                          /* rename Work                          */
const CHAR *NewName)                 /*   new name, upper case, checked      */
{
     INT Index;

     /* Point the parent's selection at the new name.                       */
     if (!(Work.flags & TPOTRE)) {
          if (LoadPage(&Scratch, Work.parpag)) {
               Index = FindPage(&Scratch, Work.pagnam);
               if (Index != -1) {
                    stzcpy(Scratch.page[Index].destpage, (CHAR *)NewName, PNMSIZ);
                    SavePage(&Scratch);
               }
          }
     }

     if (LoadPage(&Scratch, Work.pagnam)) {          /* still there?         */
          /* Rewrite the record under its new name (the name is part of     */
          /* the key), then save Work again so it matches.                  */
          stzcpy(Work.pagnam, (CHAR *)NewName, PNMSIZ);
          SavePage(&Work);
          tv_set(MTV_VALUE, Work.pagnam);
          say(REND);
          tv_set(MTV_PAGE, Work.pagnam);
          say(SAVED);
          Save(FALSE);

          /* Its children name it as their parent: update them.             */
          if (!(Work.flags & (FILPAG | MDLPAG))) {
               for (Index = 0 ; Index < MAXSEL ; Index++) {
                    if (Work.page[Index].selchr != '\0') {
                         tv_set(MTV_VALUE, Work.page[Index].destpage);
                         say(ADJING);
                         if (LoadPage(&Scratch, Work.page[Index].destpage)) {
                              stzcpy(Scratch.parpag, Work.pagnam, PNMSIZ);
                              SavePage(&Scratch);
                         }
                    }
               }
          }
     }
}

static VOID
CreateOrphan(VOID)                   /* create a page with no parent         */
{
     CHAR NewName[PNMSIZ];
     CHAR Word[UIDSIZ];
     INT NewFlags = 0;
     CHAR Choice;

     while (TRUE) {
          say(CORPNAM);
          ses_cinput();
          stzcpy(Word, cncwrd(), sizeof(Word));
          if (Word[0] == '\0') {
               return;
          }
          stzcpy(NewName, Word, PNMSIZ);
          strupr(NewName);
          if (LoadPage(&Scratch, NewName)) {
               say(CORPALR);
          }
          else if (ValidateAAPage(Word)) {
               break;
          }
          else {
               tv_set(MTV_ACTION, "create");
               say(BADPAG);
          }
     }

     cncall();
     while (TRUE) {
          say(CORPTYP);
          ses_cinput();
          Choice = cncchr();
          if (Choice == 'X') {
               return;
          }
          if (Choice == 'L') {
               NewFlags = MDLPAG | CNGOTO | TPOTRE;
               break;
          }
          if (Choice == 'F') {
               NewFlags = FILPAG | CNGOTO | TPOTRE;
               break;
          }
          if (Choice == 'M') {
               NewFlags = DFTDSP | CNGOTO | TPOTRE;
               break;
          }
     }
     if (LoadPage(&Scratch, NewName)) {
          say(CORPALR);              /* appeared while we waited             */
          return;
     }

     setmem(&Work, sizeof(Work), 0);
     strcpy(Work.parpag, "TOP");
     stzcpy(Work.pagnam, NewName, PNMSIZ);
     Work.flags = (SHORT)NewFlags;
     Work.menutype[0] = MM_TERMINAL;
     InsertPage(&Work);
     ShouldSave = FALSE;
     cncall();
     EditPage();
}
