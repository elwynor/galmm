# Changelog

All notable changes to Menu Magician are documented here. Versions follow
[Semantic Versioning](https://semver.org/).

## [1.4.0] - Unreleased

First version for The Major BBS v10, converted from High Velocity Software's
Worldgroup release 1.03a. The editor's commands, prompts and menu file
handling are unchanged.

### Changed
- Built with Visual Studio against the MBBS v10 module SDK, as a Win32 DLL
  with version information.
- Rewritten in C. The editor logic is carried over from the original C++
  source function by function.
- The original framework ran each user's editor on a stack it copied with
  `setjmp`/`longjmp`. The editor now runs on a Windows fiber per user, which
  pauses while it waits for input. See ADR 0001.
- The four configuration options (`GLOKEY`, `GLOCMD`, `ORPHCMD`,
  `VIEWCMD`) are gone from the configuration editor. They are settings in
  `HVSMMSET.DAT`, changed online with the new `!SETUP` command (sysops
  only). Changes apply immediately and are written to the Audit Trail.
  Defaults are those of 1.03a. See ADR 0002.
- Every value in `HVSMM.MSG` text is a named text variable (`HVSMM_PAGE`,
  `HVSMM_VALUE` and others) instead of a positional `%s`/`%d`. Message
  numbers were regenerated, so a 1.03a `HVSMM.MSG` cannot be used with this
  DLL.
- The help text at the entry prompt mentions `!SETUP`.
- The module name in `HVSMM.MDF` is now `Menu Magician` (it was
  `Menu Magician/WG`). Menu pages that link to the old name must be
  relinked once.
- The menu file is read and written with the engine's own routines
  (`savepage`, `unpack`), with each access selected and restored on its own.
- The installed-module list is read with the Windows file search functions.

### Removed
- The activation code and copy protection, including the `ACTCODE`
  configuration option.
- Worldgroup's client/server mode. The Major BBS v10 has terminal mode
  only, so Menu Magician edits only terminal menu pages. The `!MODE` and
  `O` commands are gone, as are the client/server-only fields (module page
  type, launch in Windows, show as icons, long description, icon name) and
  their messages. Client/server pages left in a menu file are not touched,
  and the tree view and search skip them.
- The Metakeys integration. It had been disabled since 1.03 and could never
  be switched on. Its messages (`COMMONK`, `EDTCHOK`, `EDTCHOKC`) are gone.
- The page-name substring check, which always allowed the name, and its
  `SUBBER` message.
- Touching `HVSMM.MDF` after module-page changes. On Windows, opening the
  file for append without writing does not change its date, so it did
  nothing. Menu changes already cause the module list to be rebuilt at the
  next start.

### Fixed
- A second user entering from the menu while the first was using the global
  edit command could be given the first user's page.
- Answering `X` ("back up") to "Save it?" lost the unsaved flag, so the next
  exit discarded the changes without asking.
- `EDIT <page>` with a very long page name, and some page-name prompts,
  could write past a buffer. Names are now always cut to the page-name size.
- A page that could not be loaded on global entry stopped the whole BBS.
  It now shows "There is no page by that name".
- Page names typed at the entry prompt and in rename are upper-cased, like
  everywhere else.
- A user who left the editor with `/GO` and then typed `EDIT` was told the
  editor was in use, by themselves.
- The tree view could lose its place when other users used the menu file
  during a pause, and drew bars wrongly after a view was stopped. The search
  could lose its place the same way.
- Before a new page is created, Menu Magician checks again that the name is
  still free after its last question.
- A session left on a channel is discarded when a new user logs on to that
  channel, so it is never resumed for someone else.
