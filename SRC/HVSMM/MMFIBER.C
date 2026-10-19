/* Build 26.10.19.3 01:18PM */
/*****************************************************************************
 *   MMFIBER.C                               Menu Magician - User sessions    *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *   Based on the HyperSpace layer (MHYPER.C / H2NT.C), (C) Copyright        *
 *   1992-1996 High Velocity Software, Inc.                                  *
 *                                                                           *
 *   The BBS talks to a module one line at a time: it calls our input        *
 *   routine with the line the user typed, and we must return promptly so   *
 *   every other user gets a turn.  The editor, however, was written as one  *
 *   long routine that prints a prompt and waits for the answer, dozens of   *
 *   times over, inside nested menus.  The original framework made that work *
 *   by copying stacks around with setjmp/longjmp.                           *
 *                                                                           *
 *   This file does the same job with Windows fibers.  A fiber is a second   *
 *   stack that runs on the same thread: we switch to it, the editor runs    *
 *   until it needs input, and it switches back to us.  Next time the user   *
 *   types a line we switch to it again and the editor carries on exactly    *
 *   where it stopped.  Only one thing runs at a time, so the BBS's single-  *
 *   thread rules still hold - but NOTHING the editor holds across a wait    *
 *   (a dfaSetBlk(), a position in a Btrieve file, vdatmp) can be trusted    *
 *   afterwards, because other users ran in between.                         *
 *                                                                           *
 *   Each channel has at most one session.  A session waits either for a    *
 *   line (the user's next answer) or for a CYCLE (a short pause we ask for  *
 *   by injecting a CYCLE status, used while printing long listings).        *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include "gcomm.h"
#include "majorbbs.h"
#include <windows.h>
#include "HVSMMDEF.H"

#define SES_COMMIT   (64 * 1024)     /* fiber stack committed up front       */
#define SES_RESERVE  (1024 * 1024)   /* fiber stack it may grow to           */
#define SES_SLICE    0x0000199AUL    /* a tenth of a second in hrtval() units*/

enum { SW_NONE, SW_LINE, SW_CYCLE }; /* what a paused session is waiting for */

struct mmses {                       /* one channel's session                */
     LPVOID fiber;                   /*   its fiber, or NULL for none        */
     LPVOID caller;                  /*   fiber to switch back to            */
     INT wait;                       /*   SW_xxx while paused                */
     GBOOL done;                     /*   the editor routine has returned    */
     GBOOL cycle;                    /*   a CYCLE we injected is pending     */
     GBOOL leave;                    /*   ended during status input: return  */
                                     /*   to the menu at the next line       */
     ULONG mark;                     /*   hrtval() when this turn began      */
     CHAR userid[UIDSIZ];            /*   the user the session belongs to    */
};

static struct mmses *ses = NULL;     /* one per channel (nterms)             */

/* ------------------------------------------------------------------------ *
 * Running sessions (called on the BBS's own stack).                        *
 * ------------------------------------------------------------------------ */

static VOID
flush(VOID)                          /* send whatever the session printed    */
{
     /* Only if there is something new.  outprf() resets prfptr but leaves  */
     /* the text in prfbuf (WG33 PRF.C:87-93), so a second outprf() sends   */
     /* the same text again.  The session usually flushed its own prompt    */
     /* before it waited (ses_cinput), and flushing it again here showed    */
     /* every prompt twice.                                                 */
     if (prfptr != prfbuf) {
          outprf(usrnum);
     }
}

static VOID WINAPI
sesproc(                             /* a session's fiber starts here        */
LPVOID arg)                          /*   the channel number                 */
{
     INT chan = (INT)(INT_PTR)arg;

     ed_main();
     ses[chan].done = TRUE;
     ses[chan].wait = SW_NONE;
     for (;;) {                      /* a fiber routine must never return:   */
          SwitchToFiber(ses[chan].caller);   /* that would end the thread.   */
     }                               /* We are deleted before resuming.      */
}

static GBOOL
fibready(VOID)                       /* can this thread switch fibers?       */
{
     static GBOOL warned = FALSE;

     /* The BBS thread must itself be a fiber before it can switch to ours. */
     /* Another module may have converted it already, which is fine.        */
     if (IsThreadAFiber() || ConvertThreadToFiber(NULL) != NULL) {
          return TRUE;
     }
     if (!warned) {
          warned = TRUE;
          shocst("MENU MAGICIAN ERROR", "Cannot start fibers (error %lu)",
                 GetLastError());
     }
     return FALSE;
}

static VOID
resume(                              /* run a session until it waits again   */
INT chan)
{
     struct mmses *s = &ses[chan];

     s->caller = GetCurrentFiber();
     s->wait = SW_NONE;
     s->mark = hrtval();
     SwitchToFiber(s->fiber);
     if (s->done) {
          ses_discard(chan);
     }
}

VOID
ses_discard(                         /* drop a channel's session, if any     */
INT chan)
{
     struct mmses *s;

     if (ses == NULL || chan < 0 || chan >= nterms) {
          return;
     }
     s = &ses[chan];
     if (s->fiber != NULL) {
          /* Never called from inside the session itself: a fiber cannot   */
          /* delete itself.  Deleting frees its stack without unwinding it, */
          /* which is safe because the editor holds no open resources       */
          /* across a wait.                                                 */
          DeleteFiber(s->fiber);
     }
     setmem((CHAR *)s, sizeof(*s), 0);
     ed_release(chan);
}

GBOOL
ses_init(VOID)                       /* allocate the session table           */
{
     ses = (struct mmses *)alczer(nterms * sizeof(struct mmses));
     return ses != NULL;
}

VOID
ses_close(VOID)                      /* shutdown: discard every session      */
{
     INT chan;

     if (ses != NULL) {
          for (chan = 0 ; chan < nterms ; chan++) {
               ses_discard(chan);
          }
     }
}

GBOOL                                /*   FALSE = back to the menu           */
ses_input(VOID)                      /* sttrou: a line from the user         */
{
     struct mmses *s = &ses[usrnum];

     if (s->leave) {                 /* the editor ended during status input */
          s->leave = FALSE;
          return FALSE;
     }
     if (!fibready()) {
          return FALSE;
     }
     /* A channel is reused the moment its caller drops.  If a session was  */
     /* somehow left behind, never hand it to the next user.                */
     if (s->fiber != NULL && !sameas(s->userid, usaptr->userid)) {
          ses_discard(usrnum);
     }
     rstrin();                       /* the editor parses the line itself    */
     if (s->fiber == NULL) {
          s->fiber = CreateFiberEx(SES_COMMIT, SES_RESERVE, 0, sesproc,
                                   (LPVOID)(INT_PTR)usrnum);
          if (s->fiber == NULL) {
               shocst("MENU MAGICIAN ERROR", "Cannot create a fiber (error %lu)",
                      GetLastError());
               return FALSE;
          }
          stzcpy(s->userid, usaptr->userid, UIDSIZ);
     }
     else if (usrptr->flags & ABOIP) {
          /* The user aborted output: treat the line as empty.              */
          btuclo(usrnum);
          input[0] = '\0';
          margc = 0;
          margv[0] = nxtcmd = input;
     }
     resume(usrnum);
     flush();
     return s->fiber != NULL;        /* the session ended: leave the module  */
}

VOID
ses_status(VOID)                     /* stsrou: a status code for the user   */
{
     struct mmses *s = &ses[usrnum];

     if (status != CYCLE) {
          dfsthn();                  /* everything else: engine default      */
          return;
     }
     s->cycle = FALSE;
     /* Only a session that asked for a pause is woken by a CYCLE.  One     */
     /* waiting for a line ignores it: a pause the user cut short by        */
     /* pressing Enter leaves its CYCLE behind, and it must not be taken    */
     /* for an answer.                                                      */
     if (s->fiber == NULL || s->wait != SW_CYCLE) {
          return;
     }
     resume(usrnum);
     flush();
     if (s->fiber == NULL) {
          /* It ended, but only line input can take a user out of a module. */
          /* Inject a line and leave when it arrives (as the original did). */
          s->leave = TRUE;
          btuinj(usrnum, CRSTG);
     }
}

/* ------------------------------------------------------------------------ *
 * Waiting (called from inside a session).                                  *
 * ------------------------------------------------------------------------ */

static VOID
yield(                               /* pause until resumed                  */
INT what)                            /*   SW_LINE or SW_CYCLE                */
{
     struct mmses *s = &ses[usrnum];

     /* usrnum is this session's channel both now and when we come back:   */
     /* sessions are only ever resumed from that channel's own input.       */
     s->wait = what;
     SwitchToFiber(s->caller);
}

VOID
ses_line(VOID)                       /* wait for the user's next line        */
{
     do {
          yield(SW_LINE);
     } while (status != CRSTG);
}

VOID
ses_cinput(VOID)                     /* next command, with concatenation     */
{
     /* Users may type several answers at once ("J TOP" or "1 P GAMES").    */
     /* If what they typed is used up, show the prompt and wait for more;   */
     /* otherwise carry on with the rest of the line and drop the prompt.   */
     if (endcnc()) {
          outprf(usrnum);
          ses_line();
     }
     bgncnc();
}

INT
ses_sleep(VOID)                      /* let other users run for a moment     */
{
     struct mmses *s = &ses[usrnum];

     if (!s->cycle) {
          s->cycle = TRUE;
          btuinj(usrnum, CYCLE);
     }
     yield(SW_CYCLE);
     return status;
}

GBOOL
ses_slice(VOID)                      /* has this turn run long enough?       */
{
     return (ULONG)(hrtval() - ses[usrnum].mark) > SES_SLICE;
}

GBOOL                                /*   TRUE if the user pressed Enter     */
ses_drain(                           /* wait for room in the output buffer   */
UINT room)                           /*   bytes of room wanted               */
{
     while ((UINT)btuoba(usrnum) < room) {
          if (ses_sleep() == CRSTG) {
               return TRUE;
          }
     }
     return FALSE;
}
