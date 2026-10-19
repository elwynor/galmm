/* Build 26.10.19.1 12:41PM */
/*****************************************************************************
 *   TESTS/FAKE/gcomm.h            Menu Magician - stand-in engine header     *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Just enough of the Major BBS SDK's types and names to compile           *
 *   MMFIBER.C into a standalone test (SESTEST.C), which plays the part of   *
 *   the BBS.  Values match the v10 SDK headers where they matter (CRSTG,    *
 *   CYCLE, sizes).                                                          *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#ifndef FAKE_GCOMM_H
#define FAKE_GCOMM_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <string.h>

typedef int GBOOL;
typedef void *HMCVFILE;
typedef struct dfablk DFAFILE;

#define UIDSIZ  30
#define KEYSIZ  16

VOID  setmem(VOID *p, UINT n, CHAR c);
CHAR *stzcpy(CHAR *d, const CHAR *s, INT n);
GBOOL sameas(const CHAR *a, const CHAR *b);
VOID *alczer(UINT n);
VOID  shocst(const CHAR *brief, const CHAR *fmt, ...);

extern CHAR prfbuf[];                /* the output buffer, as in GCOMM.H     */
extern CHAR *prfptr;                 /* where the next prf() text goes       */

#endif
