# Menu Magician - Technical Notes

How Menu Magician 1.4.1 works inside The Major BBS v10. For building, see
[BUILDING.MD](BUILDING.MD). For using it, see `SRC/GALMM/GALMM.RLN`.

## Source files

| File | Contents |
|---|---|
| `GALMM.C` | Module registration, the module block, startup and shutdown |
| `MMFIBER.C` | Sessions: runs the editor on a fiber per user |
| `MMEDIT.C` | The editor: entry prompt, page editor, selection editor, page operations, menu file access |
| `MMVIEW.C` | Tree view, search, the installed-module list |
| `MMSET.C` | Settings record and the `!SETUP` screen |
| `MMTVARS.C` | Named text variables |
| `GALMMDEF.H` | Shared declarations, version, settings record layout |
| `GALMM.H` | Message numbers (generated from `GALMM.MST`) |

## Sessions

The BBS calls a module's input routine (`sttrou`) once for each line a
user types, and the module must return promptly. The editor, however, is
written as one routine that prints a prompt and waits for the answer, in
nested loops many levels deep. The original framework (High Velocity's
"HyperSpace") made that work by copying stack contents around with
`setjmp`/`longjmp`.

`MMFIBER.C` does the same with Windows fibers:

- The first line a user sends to the module creates a fiber for their
  channel (64 KB committed, 1 MB reserved) that runs `ed_main()`.
- `ses_line()` switches back to the BBS's own fiber, and the input routine
  returns 1. The next line resumes the fiber exactly where it stopped.
- `ses_cinput()` adds the BBS's command concatenation: if the user typed
  several answers on one line, the next one is used without waiting.
- `ses_sleep()` injects a `CYCLE` status and pauses. The status routine
  (`stsrou`) resumes the fiber when the `CYCLE` arrives, or the input
  routine resumes it early if the user presses Enter. Long listings use
  this to let other users run.
- A session waiting for a line is never resumed by a `CYCLE`. A pause the
  user cut short leaves its `CYCLE` behind, and that must not be taken for
  the user's answer.
- When `ed_main()` returns, the fiber is deleted and the input routine
  returns 0, which takes the user back to the menu. If that happens during
  status input, where returning 0 is not possible, a line (`CRSTG`) is
  injected and the user leaves on it.
- A session is discarded on hang-up and at logon, and whenever the user on
  a channel no longer matches the session's owner.

The BBS thread is converted to a fiber the first time it is needed
(`ConvertThreadToFiber`), unless another module already did it. Everything
still runs on the single BBS thread, one thing at a time.

**Rule for code that runs in a session:** nothing shared may be held across
a wait. Every `dfaSetBlk()` is paired with `dfaRstBlk()` and every
`setmbk()` with `rstmbk()` before the next wait, because both are global
stacks. Anything read from the menu file before a wait is read again after
it, because the BBS and other users used the file meanwhile.

`TESTS\SESTEST.C` checks these rules against `MMFIBER.C`.

## The menu file

Menu pages live in the engine's own `WGSMENU2.DAT`, which the engine keeps
open as `mnubb`. Each record is a `struct mnupag` (SDK `MAJORBBS.H`), stored
with variable length: only the used selections are written. Key 0 is the
page type followed by the page name. Menu Magician reads and writes only
terminal pages (`'A'`, `MM_TERMINAL`). The Major BBS v10 has no
client/server mode, so any Worldgroup client/server pages (`'C'`) still in
the file are left alone.

Menu Magician reads pages with `dfaAcqEQ` into its own buffers (`Work`, the
page being edited, and `Scratch`) and calls the engine's `unpack()` to put
each selection back in its slot. It writes them with the engine's
`savepage()` (repack, variable-length update or insert, unpack). An update
always directly follows the read that found the record.

The tree view walks the file in physical order (`dfaStepLO`/`dfaStepNX`)
looking for terminal tree tops (`TPOTRE`), then follows selections by key.
The search walks key 0 with `dfaQueryLO`/`dfaQueryNX` and skips
client/server pages. Both save the file position
before every pause and restore it after.

## Settings

`GALMMSET.DAT` holds one 256-byte record keyed `SETTINGS` (`struct
mmsetrec`, packing pinned at 8). It is created with `dfaCreateSpec` on the
first start and seeded from the defaults table in `MMSET.C`. The lookup key
is copied into the record buffer, because the DFA layer ignores keys in
read-only memory. Code reads the live copy, `mmcfg`, at the point of use.

The `!SETUP` screen runs inside the editor's session. It validates each
value (one word, within length; commands may not shadow the entry prompt's
own commands) and writes every change to the Audit Trail.

## Global edit command

`ed_global()` is registered with `globalcmd()`. When a user on the menu
(`state 0`) with the right key types the edit command, it sets the user's
state to Menu Magician's, prepares `Work` and returns 0. The engine then
calls the current module's input routine for the same line (WG33
`MAJORBBS.C` `hdlcri`, lines 3326-3356), which is now Menu Magician's.
`TurboChan` tells `ed_main()` that this user came in that way.

## Text variables

All `GALMM.MSG` values are named text variables registered in `MMTVARS.C`
with the `GALMM_` prefix. The engine expands them before `prfmsg()` runs
the text through `vsprintf()`, so every value is returned with `%` doubled.
`GALMM_VERSION`, `GALMM_NEWCMD` and `GALMM_VIEWCMD` always return the live
value. The others are set just before each message.

## Installed modules

`PrepareAppList()` reads every `*.MDF` in the BBS directory at startup. A
module is offered for linking unless its MDF says `Internal` or (a
Worldgroup leftover) `C/S Only`.
