/* Build 26.10.20.1 08:06AM */
/*****************************************************************************
 *   TESTS/SESTEST.C                     Menu Magician - session runner test  *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Compiles the real MMFIBER.C against stand-in engine headers (FAKE\) and *
 *   plays the part of the BBS: it hands lines and status codes to the      *
 *   module's input routines the way the engine's main loop does, and       *
 *   checks what the editor routine saw.  The editor routine here is a      *
 *   script, not the real MMEDIT.C.                                          *
 *                                                                           *
 *   The invariants under test:                                              *
 *     - a session waiting for a line is resumed only by a line, never by a *
 *       CYCLE (a stray CYCLE must not be taken for the user's answer);      *
 *     - a session that asked for a pause is resumed by its CYCLE, or by a  *
 *       line if the user presses Enter first;                               *
 *     - a session that ends during status input still takes the user out   *
 *       of the module, at the next line;                                    *
 *     - a session is never handed to a different user on the same channel, *
 *       and channels never share a session;                                 *
 *     - what the editor prints is sent to the user exactly once.            *
 *                                                                           *
 *   Build and run: TESTS\RUNTESTS.BAT                                       *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "GALMMDEF.H"

/* ---- The stand-in engine ------------------------------------------------ */

#define NCHAN 4

INT usrnum, nterms = NCHAN, status, margc;
CHAR input[256];
CHAR *margv[64];
CHAR *nxtcmd = input;
static struct user users[NCHAN];
static struct usracc accs[NCHAN];
struct user *usrptr = &users[0];
struct usracc *usaptr = &accs[0];

static INT injected[NCHAN];          /* last code btuinj()ed, per channel    */
static INT released[NCHAN];          /* ed_release() calls, per channel      */
static INT dfsthncalls;
static CHAR sent[NCHAN][512];        /* what each channel was sent, '|' each */

CHAR prfbuf[4096];
CHAR *prfptr = prfbuf;

VOID  setmem(VOID *p, UINT n, CHAR c) { memset(p, c, n); }
CHAR *stzcpy(CHAR *d, const CHAR *s, INT n) { strncpy(d, s, n - 1); d[n - 1] = '\0'; return d; }
GBOOL sameas(const CHAR *a, const CHAR *b) { return _stricmp(a, b) == 0; }
VOID *alczer(UINT n) { return calloc(1, n); }
VOID  shocst(const CHAR *brief, const CHAR *fmt, ...) { printf("  [audit] %s\n", brief); (void)fmt; }
VOID  rstrin(VOID) { }
VOID  btuclo(INT chan) { (void)chan; }
/* Exactly like the engine's (WG33 PRF.C:87-93): send prfbuf and reset     */
/* prfptr, but leave the text in prfbuf.                                  */
VOID  outprf(INT chan) { strcat(sent[chan], prfbuf); strcat(sent[chan], "|"); prfptr = prfbuf; }
VOID  dfsthn(VOID) { dfsthncalls++; }
INT   btuinj(INT chan, INT code) { injected[chan] = code; return 0; }
INT   endcnc(VOID) { return 1; }
VOID  bgncnc(VOID) { nxtcmd = input; }
ULONG hrtval(VOID) { return GetTickCount(); }
INT   btuoba(INT chan) { (void)chan; return 4096; }

/* Pieces of the module that MMFIBER.C calls. */
INT UseChan = -1, TurboChan = -1;
VOID ed_release(INT chan) { released[chan]++; }

/* ---- The script the "editor" follows ------------------------------------ */

enum { SC_LINES, SC_SLEEP, SC_SLEEPEND, SC_PROMPT };
static INT scenario[NCHAN];
static CHAR seen[NCHAN][512];        /* what each channel's editor saw       */

static VOID
note(const CHAR *fmt, ...)           /* append to this channel's record      */
{
     CHAR buf[128];
     va_list ap;

     va_start(ap, fmt);
     vsprintf(buf, fmt, ap);
     va_end(ap);
     strcat(seen[usrnum], buf);
}

VOID
ed_main(VOID)
{
     note("start(%s)", input);
     switch (scenario[usrnum]) {
     case SC_LINES:                  /* two questions, then done            */
          ses_line();
          note(" line(%s)", input);
          ses_line();
          note(" line(%s)", input);
          break;
     case SC_SLEEP:                  /* a pause, then a question            */
          note(" woke(%d)", ses_sleep());
          ses_line();
          note(" line(%s)", input);
          break;
     case SC_SLEEPEND:               /* a pause, then done                  */
          note(" woke(%d)", ses_sleep());
          break;
     case SC_PROMPT:                 /* two prompts, as the editor asks     */
          strcpy(prfptr, "Q1");
          prfptr += strlen(prfptr);
          ses_cinput();
          note(" line(%s)", input);
          strcpy(prfptr, "Q2");
          prfptr += strlen(prfptr);
          ses_cinput();
          note(" line(%s)", input);
          break;
     }
     note(" end");
}

/* ---- Driving it --------------------------------------------------------- */

static VOID
on(INT chan, const CHAR *userid)     /* select a channel, as curusr() does   */
{
     usrnum = chan;
     usrptr = &users[chan];
     usaptr = &accs[chan];
     strcpy(accs[chan].userid, userid);
}

static GBOOL
line(INT chan, const CHAR *userid, const CHAR *text)   /* the user typed    */
{
     on(chan, userid);
     status = CRSTG;
     strcpy(input, text);
     margc = text[0] != '\0';
     margv[0] = input;
     return ses_input();
}

static VOID
stat(INT chan, const CHAR *userid, INT code)           /* a status arrives  */
{
     on(chan, userid);
     status = code;
     ses_status();
}

static INT failures = 0;

static VOID
expect(const CHAR *what, GBOOL ok)
{
     printf("%s  %s\n", ok ? "pass" : "FAIL", what);
     if (!ok) {
          failures++;
     }
}

static VOID
expectseen(INT chan, const CHAR *want)
{
     CHAR what[600];

     sprintf(what, "channel %d saw \"%s\"", chan, seen[chan]);
     expect(what, strcmp(seen[chan], want) == 0);
     if (strcmp(seen[chan], want) != 0) {
          printf("        wanted \"%s\"\n", want);
     }
}

int
main(VOID)
{
     ses_init();

     printf("-- a session asks two questions and ends\n");
     scenario[0] = SC_LINES;
     expect("entering starts a session", line(0, "ALICE", "go") == TRUE);
     expect("first answer keeps it going", line(0, "ALICE", "one") == TRUE);
     expect("second answer ends it: back to the menu", line(0, "ALICE", "two") == FALSE);
     expectseen(0, "start(go) line(one) line(two) end");
     expect("the editor was released once", released[0] == 1);

     printf("-- a stray CYCLE never answers a question\n");
     seen[1][0] = '\0';
     scenario[1] = SC_SLEEP;
     line(1, "BOB", "go");
     expect("a pause injects a CYCLE", injected[1] == CYCLE);
     line(1, "BOB", "");             /* user presses Enter during the pause  */
     stat(1, "BOB", CYCLE);          /* the pause's CYCLE arrives late       */
     expectseen(1, "start(go) woke(3)");
     expect("the late CYCLE did not end the question",
            line(1, "BOB", "answer") == FALSE);
     expectseen(1, "start(go) woke(3) line(answer) end");

     printf("-- a CYCLE ends a pause\n");
     seen[1][0] = '\0';
     line(1, "BOB", "go");
     stat(1, "BOB", CYCLE);
     expectseen(1, "start(go) woke(240)");
     line(1, "BOB", "next");
     expectseen(1, "start(go) woke(240) line(next) end");

     printf("-- other status codes go to the engine\n");
     dfsthncalls = 0;
     stat(2, "CAROL", 5);
     expect("non-CYCLE status reached dfsthn()", dfsthncalls == 1);

     printf("-- a session that ends during status input still leaves\n");
     scenario[2] = SC_SLEEPEND;
     injected[2] = 0;
     expect("entering starts it", line(2, "CAROL", "go") == TRUE);
     stat(2, "CAROL", CYCLE);        /* wakes it; it ends inside ses_status  */
     expectseen(2, "start(go) woke(240) end");
     expect("a line was injected to take the user out", injected[2] == CRSTG);
     expect("the injected line returns the user to the menu",
            line(2, "CAROL", "") == FALSE);

     printf("-- a session is never handed to the next caller\n");
     seen[3][0] = '\0';
     scenario[3] = SC_LINES;
     released[3] = 0;
     line(3, "DAVE", "go");          /* DAVE waits for a line, then drops    */
     line(3, "ERIN", "hello");       /* ERIN on the same channel, no hangup  */
     expectseen(3, "start(go)start(hello)");
     expect("DAVE's session was released", released[3] == 1);

     printf("-- hang-up discards a waiting session\n");
     released[3] = 0;
     ses_discard(3);
     expect("released on hang-up", released[3] == 1);
     seen[3][0] = '\0';
     line(3, "ERIN", "again");
     expectseen(3, "start(again)");

     printf("-- channels keep separate sessions\n");
     seen[0][0] = seen[1][0] = '\0';
     scenario[0] = scenario[1] = SC_LINES;
     line(0, "ALICE", "a0");
     line(1, "BOB", "b0");
     line(0, "ALICE", "a1");
     line(1, "BOB", "b1");
     line(1, "BOB", "b2");
     line(0, "ALICE", "a2");
     expectseen(0, "start(a0) line(a1) line(a2) end");
     expectseen(1, "start(b0) line(b1) line(b2) end");

     printf("-- each prompt is sent once\n");
     sent[2][0] = seen[2][0] = '\0';
     scenario[2] = SC_PROMPT;
     line(2, "CAROL", "go");
     line(2, "CAROL", "a");
     line(2, "CAROL", "b");
     expectseen(2, "start(go) line(a) line(b) end");
     {
          CHAR what[600];

          sprintf(what, "channel 2 was sent \"%s\"", sent[2]);
          expect(what, strcmp(sent[2], "Q1|Q2|") == 0);
     }

     ses_close();
     printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
     return failures ? 1 : 0;
}
