/* Build 26.10.19.2 01:12PM */
/*****************************************************************************
 *   MMTVARS.C                             Menu Magician - Text variables     *
 *                                                                           *
 *   Copyright (C) 2026 Elwynor Technologies.                                *
 *                                                                           *
 *   Every changing value in HVSMM.MSG (a page name, a module name, a        *
 *   Yes/No) is a named text variable - the byte sequence                    *
 *   0x01 <justify> <width+32> NAME 0x01 - rather than a printf-style %s.    *
 *   That lets sysops reword, reorder or drop values when they edit the      *
 *   text.  The engine replaces each variable with the string returned by    *
 *   the routine registered for its name.  We fill those strings with        *
 *   tv_set() just before printing a message.                                *
 *                                                                           *
 *   One trap: prfmsg() expands variables FIRST and then runs the result     *
 *   through vsprintf() as a format string.  A '%' inside a value (a menu    *
 *   title a sysop typed, say) would be read as a format code and could      *
 *   crash the board.  So values are returned with every '%' doubled.        *
 *                                                                           *
 *   Names are global to the whole BBS, so all of ours start with HVSMM_.    *
 *                                                                           *
 *   Licensed under the GNU Affero General Public License v3.0.              *
 *****************************************************************************/

#include <stdio.h>
#include "gcomm.h"
#include "majorbbs.h"
#include "HVSMMDEF.H"

#define TVBSIZ 80                    /* room for any value we show           */

static CHAR tvval[MTV_COUNT_OF][TVBSIZ];       /* values as set              */
static CHAR tvout[2 * TVBSIZ];                 /* value as returned          */

static CHAR *
tvesc(const CHAR *s)                 /* return s with every '%' doubled      */
{
     CHAR *d = tvout;

     while (*s != '\0' && d < tvout + sizeof(tvout) - 2) {
          if (*s == '%') {
               *d++ = '%';
          }
          *d++ = *s++;
     }
     *d = '\0';
     return tvout;
}

static CHAR *
tvget(INT which)                     /* return a slot, escaped for prfmsg()  */
{
     return tvesc(tvval[which]);
}

/* The engine needs a separate routine for each registered name.            */
static CHAR *tvPAGE(VOID)     { return tvget(MTV_PAGE);     }
static CHAR *tvPARENT(VOID)   { return tvget(MTV_PARENT);   }
static CHAR *tvDISABLED(VOID) { return tvget(MTV_DISABLED); }
static CHAR *tvGOTO(VOID)     { return tvget(MTV_GOTO);     }
static CHAR *tvGOKEY(VOID)    { return tvget(MTV_GOKEY);    }
static CHAR *tvMODULE(VOID)   { return tvget(MTV_MODULE);   }
static CHAR *tvCOMMAND(VOID)  { return tvget(MTV_COMMAND);  }
static CHAR *tvHEADER(VOID)   { return tvget(MTV_HEADER);   }
static CHAR *tvFILE(VOID)     { return tvget(MTV_FILE);     }
static CHAR *tvAUTO(VOID)     { return tvget(MTV_AUTO);     }
static CHAR *tvDEFDISP(VOID)  { return tvget(MTV_DEFDISP);  }
static CHAR *tvTITLE(VOID)    { return tvget(MTV_TITLE);    }
static CHAR *tvNUM(VOID)      { return tvget(MTV_NUM);      }
static CHAR *tvSEL(VOID)      { return tvget(MTV_SEL);      }
static CHAR *tvTEXT(VOID)     { return tvget(MTV_TEXT);     }
static CHAR *tvNUM2(VOID)     { return tvget(MTV_NUM2);     }
static CHAR *tvSEL2(VOID)     { return tvget(MTV_SEL2);     }
static CHAR *tvTEXT2(VOID)    { return tvget(MTV_TEXT2);    }
static CHAR *tvVALUE(VOID)    { return tvget(MTV_VALUE);    }
static CHAR *tvSHORT(VOID)    { return tvget(MTV_SHORT);    }
static CHAR *tvKEY(VOID)      { return tvget(MTV_KEY);      }
static CHAR *tvDISPLAY(VOID)  { return tvget(MTV_DISPLAY);  }
static CHAR *tvDEST(VOID)     { return tvget(MTV_DEST);     }
static CHAR *tvACTION(VOID)   { return tvget(MTV_ACTION);   }
static CHAR *tvREASON(VOID)   { return tvget(MTV_REASON);   }
static CHAR *tvLABEL(VOID)    { return tvget(MTV_LABEL);    }
static CHAR *tvLIMIT(VOID)    { return tvget(MTV_LIMIT);    }

/* These always show the live value, so they never need setting.            */
static CHAR *tvVERSION(VOID)  { return HVSMM_VERSION;            }
static CHAR *tvNEWCMD(VOID)   { return tvesc(mmcfg.orphcmd);     }
static CHAR *tvVIEWCMD(VOID)  { return tvesc(mmcfg.viewcmd);     }

VOID
tv_init(VOID)                        /* register all of our text variables   */
{
     register_textvar("HVSMM_PAGE",     tvPAGE);
     register_textvar("HVSMM_PARENT",   tvPARENT);
     register_textvar("HVSMM_DISABLED", tvDISABLED);
     register_textvar("HVSMM_GOTO",     tvGOTO);
     register_textvar("HVSMM_GOKEY",    tvGOKEY);
     register_textvar("HVSMM_MODULE",   tvMODULE);
     register_textvar("HVSMM_COMMAND",  tvCOMMAND);
     register_textvar("HVSMM_HEADER",   tvHEADER);
     register_textvar("HVSMM_FILE",     tvFILE);
     register_textvar("HVSMM_AUTO",     tvAUTO);
     register_textvar("HVSMM_DEFDISP",  tvDEFDISP);
     register_textvar("HVSMM_TITLE",    tvTITLE);
     register_textvar("HVSMM_NUM",      tvNUM);
     register_textvar("HVSMM_SEL",      tvSEL);
     register_textvar("HVSMM_TEXT",     tvTEXT);
     register_textvar("HVSMM_NUM2",     tvNUM2);
     register_textvar("HVSMM_SEL2",     tvSEL2);
     register_textvar("HVSMM_TEXT2",    tvTEXT2);
     register_textvar("HVSMM_VALUE",    tvVALUE);
     register_textvar("HVSMM_SHORT",    tvSHORT);
     register_textvar("HVSMM_KEY",      tvKEY);
     register_textvar("HVSMM_DISPLAY",  tvDISPLAY);
     register_textvar("HVSMM_DEST",     tvDEST);
     register_textvar("HVSMM_ACTION",   tvACTION);
     register_textvar("HVSMM_REASON",   tvREASON);
     register_textvar("HVSMM_LABEL",    tvLABEL);
     register_textvar("HVSMM_LIMIT",    tvLIMIT);
     register_textvar("HVSMM_VERSION",  tvVERSION);
     register_textvar("HVSMM_NEWCMD",   tvNEWCMD);
     register_textvar("HVSMM_VIEWCMD",  tvVIEWCMD);
}

VOID
tv_set(INT which, const CHAR *value)           /* set a text value           */
{
     stzcpy(tvval[which], (CHAR *)value, TVBSIZ);
}

VOID
tv_setn(INT which, LONG value)                 /* set a numeric value        */
{
     sprintf(tvval[which], "%ld", value);
}

VOID
tv_setc(INT which, CHAR value)                 /* set a single character     */
{
     tvval[which][0] = value;
     tvval[which][1] = '\0';
}
