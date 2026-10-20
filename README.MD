# Menu Magician for The Major BBS v10

**Menu Magician** (`GALMM`) is an online menu tree editor for The Major BBS
v10. A sysop can restructure the BBS's menu tree while the board runs: edit
any page's settings and menu selections, create, rename, retype and delete
pages, view the whole tree, and search it, all without taking the system
down.

Menu Magician was originally written by High Velocity Software for
Worldgroup. Version 1.4.1 is its conversion to The Major BBS v10 by Elwynor
Technologies.

## Features

- **Edit any page online.** File, module and menu pages. Changes take
  effect as soon as the page is saved.
- **Menu selections.** Selection character, description, required key,
  display option and destination page for each of a menu page's 25
  selections. Selections can be swapped.
- **Page operations.** Create orphan pages, create pages by pointing a
  selection at a new name, adopt orphans, rename, change type and delete.
  Children and parents are kept consistent.
- **Safety warnings.** Every risky change asks for confirmation first, with
  a stronger warning when other users are online.
- **Tree view and search.** Show the whole menu tree with each page type in
  its own color, or list the pages that mention some text. Both can be
  stopped with Enter and never hold the board up.
- **Global edit command.** A sysop types `EDIT` at any menu prompt to edit
  the page they are on.
- **Online settings.** The edit command, its key and the entry-prompt
  commands are changed online with `!SETUP`. Changes apply immediately and
  are recorded in the Audit Trail.
- **Fully editable text.** All text is in `GALMM.MSG`. Changing values use
  named text variables, so sysops can reword messages freely.
- **No activation code.**

## Installation

1. With the BBS down, copy `GALMM.DLL`, `GALMM.MDF`, `GALMM.MSG` and
   `GALMM.RLN` from the release folder into your BBS directory.
2. Add Menu Magician to your menu with the Menu Tree editor. Protect both
   the menu selection and the module page with the `SYSOP` key or another
   key that only trusted sysops have.
3. Start the BBS. Menu Magician creates `GALMMSET.DAT` on the first start.

**Upgrading from 1.03a (Worldgroup) or the 1.4.0 test build:** the files
were renamed from `HVSMM` to `GALMM` in 1.4.1. Delete the old `HVSMM.DLL`,
`HVSMM.MDF`, `HVSMM.MSG`, `HVSMM.MCV` and `HVSMM.RLN`, then install the files
above. A 1.4.0 install keeps its settings if you rename `HVSMMSET.DAT` to
`GALMMSET.DAT`.

**Upgrading from 1.03a (Worldgroup):** No
activation code is needed. The module name is now `Menu Magician` (it was
`Menu Magician/WG`), so relink the module page that runs it: type `EDIT` on
the menu page above it, jump to the module page and set its module name
again. The former configuration options start at their defaults; set them
again with `!SETUP` if you had changed them. Only the terminal-mode menu
tree is edited, because The Major BBS v10 has no client/server mode.

## Configuration

Type `!SETUP` at Menu Magician's "Edit which page?" prompt (the `SYSOP` key
is required). The settings are:

| Setting | Default |
|---|---|
| Key for the global edit command | `SYSOP` |
| Global edit command | `EDIT` |
| Create-orphan-page command | `!NEW` |
| View-menu-tree command | `!VIEW` |

Settings are changed online only. There are no configuration editor
options. `R` in the settings screen restores the defaults.

## Usage

Enter Menu Magician from the menu, or type `EDIT` at a menu prompt. At the
"Edit which page?" prompt, type a page name to edit it, or one of `!NEW`,
`!VIEW`, `!SEARCH`, `!SETUP` or `X`. `GALMM.RLN`, the sysop manual,
describes every command.

Only one person can use Menu Magician at a time.

## Version Information

Current version: **1.4.1**. See [CHANGELOG.MD](CHANGELOG.MD) for changes and
[BUILDING.MD](BUILDING.MD) to build from source.

## License

Menu Magician is licensed under the
[GNU Affero General Public License v3.0](LICENSE). See
[CONTRIBUTING.MD](CONTRIBUTING.MD) to contribute.

Copyright © Elwynor Technologies. Originally © High Velocity Software, Inc.

## Support

Please report problems and suggestions through the GitHub issue tracker for
this repository.

---

Menu Magician converted for The Major BBS v10 by Mark Laudenbach - https://github.com/laudenbachm
