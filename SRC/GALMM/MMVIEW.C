/* Build 26.10.20.1 08:06AM */
/*****************************************************************************
 *   MMVIEW.C                   Menu Magician - Tree view, search, modules    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Originally (C) Copyright 1994-1996 High Velocity Software, Inc.         *
 *                                                                           *
 *   The long listings: the menu tree display (!VIEW), the text search       *
 *   (!SEARCH), and the list of installed modules offered when a module      *
 *   page is linked to a module (built from the BBS's .MDF files; this was   *
 *   APPS.C in the original).                                                *
 *                                                                           *
 *   A long listing must not hold the BBS up, so these pause (ses_sleep)     *
 *   now and then to let other users run, and stop if the user presses       *
 *   Enter.  After every pause the menu file position is set again, because *
 *   the BBS read other pages meanwhile.                                     *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include "gcomm.h"
#include "majorbbs.h"
#include <windows.h>
#include "GALMMDEF.H"

#define MAXDEPTH 40                  /* deepest tree level drawn with bars   */

static VOID
say(INT msgnum)                      /* print one of our messages            */
{
     setmbk(mmmb);
     prfmsg(msgnum);
     rstmbk();
}

/* ------------------------------------------------------------------------ *
 * The menu tree display.                                                   *
 * ------------------------------------------------------------------------ */

static INT Lines[MAXDEPTH];          /* columns that need a "|" bar drawn    */
static INT Recurse = 0;              /* how deep in the tree we are          */

static GBOOL
HasChildren(VOID)                    /* does Scratch lead to other pages?    */
{
     INT Index;

     if (Scratch.flags & (MDLPAG | FILPAG)) {
          return FALSE;
     }
     for (Index = 0 ; Index < MAXSEL ; Index++) {
          if (Scratch.page[Index].destpage[0] != '\0') {
               return TRUE;
          }
     }
     return FALSE;
}

static INT
GetLastSel(VOID)                     /* Scratch's last used selection        */
{
     INT Index;

     for (Index = MAXSEL - 1 ; Index >= 0 ; Index--) {
          if (Scratch.page[Index].destpage[0] != '\0') {
               return Index;
          }
     }
     return -1;
}

static GBOOL
IsBar(INT Column)                    /* draw a "|" in this column?           */
{
     INT Index;

     if (Column == 0) {
          return FALSE;
     }
     for (Index = 0 ; Index < Recurse ; Index++) {
          if (Lines[Index] == Column) {
               return TRUE;
          }
          if (Lines[Index] > Column) {
               return FALSE;
          }
     }
     return FALSE;
}

static GBOOL                         /*   TRUE = the user stopped the view   */
ShowTopPage(                         /* draw Scratch and everything below it */
INT Level,                           /*   column to draw it in               */
GBOOL IsLast,                        /*   last child of its parent?          */
LONG Pos)                            /*   Scratch's position in the file     */
{
     INT ThisLevel;
     INT Last;
     INT Index;
     INT HowFar;
     CHAR Dest[PNMSIZ];

     /* Let the output drain before adding more.                           */
     if (ses_drain(OUTSIZ - 1)) {
          return TRUE;
     }

     if (Level != 0) {
          if (Recurse != 0) {
               for (HowFar = 0 ; HowFar < Level ; HowFar++) {
                    if (IsBar(HowFar)) {
                         say(PEGBAR);
                    }
                    else {
                         prf(" ");
                    }
               }
          }
          say(IsLast ? LINDENT : INDENT);
     }

     /* Each page type has its own color.                                   */
     if (Scratch.flags & MDLPAG) {
          say(MDLIN);
     }
     else if (Scratch.flags & FILPAG) {
          say(FILIN);
     }
     else if (Scratch.flags & AUTPAG) {
          say(AUTIN);
     }
     else {
          say(MENIN);
     }

     tv_set(MTV_PAGE, Scratch.pagnam);
     if (!HasChildren()) {
          /* A module page with no module set is flagged with a "*".        */
          say(((Scratch.flags & MDLPAG) && Scratch.modnam[0] == '\0') ? NOKIDW : NOKIDS);
          return FALSE;
     }
     say(KIDS);

     ThisLevel = Level + (INT)strlen(Scratch.pagnam) + 1;
     if (Level != 0) {
          ThisLevel++;
     }
     Last = GetLastSel();
     if (Recurse < MAXDEPTH) {
          Lines[Recurse] = ThisLevel;
     }
     Recurse++;

     for (Index = 0 ; Index < MAXSEL ; Index++) {
          if (Scratch.page[Index].destpage[0] == '\0') {
               continue;
          }
          stzcpy(Dest, Scratch.page[Index].destpage, PNMSIZ);
          if (LoadPage(&Scratch, Dest)) {
               if (Index == Last && Recurse > 0 && Recurse <= MAXDEPTH) {
                    Lines[Recurse - 1] = 0;      /* no bar below the last one */
               }
               if (ShowTopPage(ThisLevel, Index == Last, PagePos())) {
                    return TRUE;
               }
               if (ses_sleep() == CRSTG) {
                    return TRUE;
               }
               PageAt(&Scratch, Pos);            /* back to this page        */
          }
     }
     Recurse--;
     return FALSE;
}

VOID
ShowTree(VOID)                       /* draw every menu tree                 */
{
     LONG SavePos;
     GBOOL more;

     say(TRHEAD);
     setmem(Lines, sizeof(Lines), 0);
     Recurse = 0;
     usrptr->flags |= NOINJO;

     /* Step through the file in physical order looking for the tops of    */
     /* trees (TOP, and every orphan page) among the terminal pages.        */
     dfaSetBlk(mnubb);
     more = dfaStepLO(&Scratch);
     dfaRstBlk();
     if (more) {
          unpack(&Scratch);
     }
     while (more) {
          if ((Scratch.flags & TPOTRE) && Scratch.menutype[0] == MM_TERMINAL) {
               SavePos = PagePos();
               if (ShowTopPage(0, FALSE, SavePos)) {
                    say(ABVIEW);
                    break;
               }
               PageAt(&Scratch, SavePos);
          }

          dfaSetBlk(mnubb);
          more = dfaStepNX(&Scratch);
          dfaRstBlk();
          if (!more) {
               break;
          }
          if (ses_slice()) {         /* been at it a while: let others run   */
               SavePos = PagePos();
               if (ses_sleep() == CRSTG) {
                    say(ABVIEW);
                    break;
               }
               PageAt(&Scratch, SavePos);
          }
          else {
               unpack(&Scratch);
          }
     }

     say(TRDONE);
     usrptr->flags &= ~NOINJO;
}

/* ------------------------------------------------------------------------ *
 * Search.                                                                  *
 * ------------------------------------------------------------------------ */

static GBOOL
UpperCheck(                          /* does field contain text?             */
CHAR *pBig,                          /*   field (upper-cased in place)       */
const CHAR *pSmall)                  /*   text, already upper case           */
{
     strupr(pBig);
     return strstr(pBig, pSmall) != NULL;
}

static GBOOL
SearchThis(const CHAR *pText)        /* does Scratch mention the text?       */
{
     INT Index;

     if (UpperCheck(Scratch.pagnam, pText) || UpperCheck(Scratch.golock, pText)) {
          return TRUE;
     }
     if (Scratch.flags & MDLPAG) {
          return UpperCheck(Scratch.modnam, pText) || UpperCheck(Scratch.cmdstg, pText);
     }
     if (UpperCheck(Scratch.fname, pText)) {
          return TRUE;
     }
     if (Scratch.flags & FILPAG) {
          return FALSE;
     }
     if (UpperCheck(Scratch.mnuttl, pText)) {
          return TRUE;
     }
     for (Index = 0 ; Index < MAXSEL ; Index++) {
          if (Scratch.page[Index].selchr != '\0'
           && (UpperCheck(Scratch.page[Index].destpage, pText)
            || UpperCheck(Scratch.page[Index].keyreq, pText)
            || UpperCheck(Scratch.page[Index].shortd, pText))) {
               return TRUE;
          }
     }
     return FALSE;
}

VOID
Search(VOID)                         /* list the pages that mention a text   */
{
     CHAR Text[30];
     GBOOL Hit = FALSE;
     GBOOL more;
     LONG pos;

     while (TRUE) {
          say(SEARCHW);
          outprf(usrnum);
          ses_line();
          if (sameas(input, "X")) {
               return;
          }
          if (input[0] != '\0') {
               break;
          }
     }
     stzcpy(Text, input, sizeof(Text));
     tv_set(MTV_VALUE, Text);
     say(SEARCHH);
     strupr(Text);

     dfaSetBlk(mnubb);
     more = dfaQueryLO(0);
     dfaRstBlk();
     while (more) {
          dfaSetBlk(mnubb);
          dfaAbsRec(&Scratch, 0);    /* read the record the query points at  */
          dfaRstBlk();
          unpack(&Scratch);
          if (Scratch.menutype[0] == MM_TERMINAL && SearchThis(Text)) {
               Hit = TRUE;
               tv_set(MTV_PAGE, Scratch.pagnam);
               say(SEARCH1);
          }

          dfaSetBlk(mnubb);
          more = dfaQueryNX();
          pos = more ? dfaAbs() : 0L;
          dfaRstBlk();
          if (more && ses_slice()) { /* been at it a while: let others run   */
               if (ses_sleep() == CRSTG || ses_drain(1024)) {
                    break;           /* the user pressed Enter               */
               }
               dfaSetBlk(mnubb);     /* find our place again                 */
               dfaGetAbs(&Scratch, pos, 0);
               dfaRstBlk();
          }
     }
     if (!Hit) {
          say(SEARCHMT);
     }
}

/* ------------------------------------------------------------------------ *
 * Installed modules (for linking module pages).                            *
 * ------------------------------------------------------------------------ */

INT nTerminal = 0;                   /* modules a page can link to           */
CHAR **ppTerminal = NULL;

static VOID
ClipLine(CHAR *pText)                /* drop the line ending and edge spaces */
{
     CHAR *pEnd;

     pEnd = pText + strlen(pText);
     while (pEnd > pText && (pEnd[-1] == '\r' || pEnd[-1] == '\n' || pEnd[-1] == ' '
                          || pEnd[-1] == '\t')) {
          *--pEnd = '\0';
     }
     while (*pText == ' ' || *pText == '\t') {
          memmove(pText, pText + 1, strlen(pText));
     }
}

static VOID
ProcessMDF(                          /* add one .MDF's module to the lists   */
const CHAR *pName,                   /*   the .MDF file                      */
INT max)                             /*   room in each list                  */
{
     CHAR Scratchy[129];
     CHAR Name[MNMSIZ];
     FILE *fp;
     GBOOL HitName = FALSE;
     GBOOL CSOnly = FALSE;
     GBOOL Internal = FALSE;

     if ((fp = fopen(pName, "rt")) == NULL) {
          return;
     }
     while (fgets(Scratchy, sizeof(Scratchy), fp) != NULL) {
          if (sameto("Module Name:", Scratchy)) {
               CHAR *pValue = strchr(Scratchy, ':') + 1;

               ClipLine(pValue);
               stzcpy(Name, pValue, MNMSIZ);
               HitName = TRUE;
          }
          if (sameto("C/S Only", Scratchy)) {
               CSOnly = TRUE;
          }
          if (sameto("Internal", Scratchy)) {
               Internal = TRUE;
          }
     }
     fclose(fp);

     /* Internal modules and Worldgroup client/server-only ones cannot be  */
     /* linked to a terminal menu page.                                      */
     if (HitName && !Internal && !CSOnly && nTerminal < max) {
          ppTerminal[nTerminal++] = alcdup(Name);
     }
}

VOID
PrepareAppList(VOID)                 /* build the module lists from *.MDF    */
{
     WIN32_FIND_DATAA fd;
     HANDLE h;
     INT Index;
     INT nDesc = 0;

     for (Index = 0 ; Index < nTerminal ; Index++) {
          free(ppTerminal[Index]);
     }
     if (ppTerminal != NULL) {
          free(ppTerminal);
     }
     nTerminal = 0;
     ppTerminal = NULL;

     /* Count the .MDF files in the BBS directory, then read each one.      */
     if ((h = FindFirstFileA("*.MDF", &fd)) != INVALID_HANDLE_VALUE) {
          do {
               nDesc++;
          } while (FindNextFileA(h, &fd));
          FindClose(h);
     }
     if (nDesc == 0) {
          return;
     }
     ppTerminal = (CHAR **)alczer(sizeof(CHAR *) * nDesc);
     if ((h = FindFirstFileA("*.MDF", &fd)) != INVALID_HANDLE_VALUE) {
          do {
               ProcessMDF(fd.cFileName, nDesc);
          } while (FindNextFileA(h, &fd));
          FindClose(h);
     }
}

GBOOL
CheckList(                           /* is this name in a module list?       */
const CHAR *pCheck,
CHAR **pList,
INT Max)
{
     INT Index;

     for (Index = 0 ; Index < Max ; Index++) {
          if (sameas((CHAR *)pCheck, pList[Index])) {
               return TRUE;
          }
     }
     return FALSE;
}

VOID
DumpModules(VOID)                    /* list the modules a page can link to  */
{
     INT Index;

     say(MODSTART);
     for (Index = 0 ; Index < nTerminal ; Index++) {
          tv_setn(MTV_NUM, Index);
          tv_set(MTV_TEXT, ppTerminal[Index]);
          say(ONEMOD);
          if ((Index + 1) % 2 == 0) {          /* two to a line            */
               outprf(usrnum);
               say(MODBRK);
          }
     }
     say(MODEND);
}
