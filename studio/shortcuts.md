# OrrePort Studio: keyboard and mouse shortcuts (Windows)

snatcherLeo, 2026-11-18: "we'll make a "shortcuts.md" or some file where we keep track of all shortcuts we are using in
OrrePort Studio, for this Windows version. This'll help for later OS development."

**The rule:** every new shortcut gets a row here in the same commit that adds it, with where its code is and what
Linux and macOS would use. The "Linux / macOS" column follows the platforms' own conventions: on macOS, Cmd takes Ctrl's
place for app commands, and a few Windows-only inputs (the mouse's side buttons as `WM_APPCOMMAND`) come through other
events. Rows marked *likely* are not yet checked on those systems.

Code: `studio/win32/app.cpp` (`key()`, `search_proc`, `wnd_proc`) and `studio/win32/viewer.cpp` (the 3D view's own
window). Last checked against the code: 2026-11-18.

## Tabs (browser-style; snatcherLeo, 2026-11-18: "Basic tab navigation that you'd find on Chrome")

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+click a Pokémon (list, evolution card, anywhere it links) | open it in a new tab | `click()`, `select(.., ctrl)` | Linux: Ctrl+click; macOS: Cmd+click (likely) |
| Ctrl+click an editor's icon on the rail, or a link to an editor (a type, an ability, an evolution's item method) | open that editor in a new tab (the Pokémon icon: the last Pokémon shown, in a new tab); without Ctrl it opens in the shown tab. *Added 2026-11-22* (snatcherLeo: "if I hold the "CTRL" button and click on one of the left-hand icons, it should open it in a new tab") | `click()`, `open_editor(.., ctrl)` | Linux: Ctrl+click; macOS: Cmd+click (likely) |
| Ctrl+Tab / Ctrl+Shift+Tab | next / previous tab, round the ends | `key()` | the same on both (macOS also Cmd+Shift+] / [, likely) |
| Ctrl+1 … Ctrl+8 | that tab | `key()` | Linux: Ctrl / Alt+number; macOS: Cmd+number (likely) |
| Ctrl+9 | the last tab | `key()` | as above |
| Ctrl+T | a new tab, as "+" makes one (a Home tab at the end, shown); works while typing in a search box too. *Added 2026-11-22* (snatcherLeo: "can "CTRL+T" open a new tab?"). Ctrl+Shift+T is left free (Chrome's "reopen closed tab", for later) | `key()`, `search_proc`, the global search's proc | Linux: Ctrl+T; macOS: Cmd+T |
| Ctrl+W | close the shown tab | `key()` | Linux: Ctrl+W; macOS: Cmd+W |
| Middle-click a tab | close it | `WM_MBUTTONDOWN` | the same (middle button) |
| Wheel up / down over the tab strip | scroll the strip sideways (when the tabs don't all fit), one whole tab a notch (*since 2026-11-22*: the strip shows only whole tabs) | `WM_MOUSEWHEEL` | the same |
| Wheel tilted left / right (anywhere) | scroll the strip | `WM_MOUSEHWHEEL` | horizontal scroll events (and trackpad swipes on macOS, likely) |
| "‹" / "›" at the strip's ends | jump to the first / last tabs | `HIT_TAB_FIRST` / `HIT_TAB_LAST` | the same (drawn by Studio) |
| "▾" | a menu of every open tab | `HIT_TAB_MENU` (a native menu) | the toolkit's popup menu |
| "+" | a new Home tab at the end (as Chrome and Edge open a new tab; until 2026-11-19 it copied the shown Pokémon's tab; the search field is not focused, snatcherLeo found a waiting cursor there confusing) | `HIT_PTAB_ADD`, `open_page(kHome, true)` | the same |
| Right-click a tab | its menu: Move to new window, Duplicate, Close, Close other tabs, Close tabs to the right (Move and Close act on every selected tab when the clicked tab is one of them) | `WM_RBUTTONUP`, `tab_menu()` | the toolkit's context menu (macOS: Ctrl+click too) |
| Ctrl+click a tab | add it to the selection, or take it out | `tab_press()` | Linux: Ctrl+click; macOS: Cmd+click (likely) |
| Shift+click a tab | select every tab from the shown one to this one | `tab_press()` | the same on both (likely) |
| Click a tab (plain) | show it and clear the selection | `tab_press()` | the same |
| Ctrl+W with tabs selected | close every selected tab | `key()`, `sel_list()` | Linux: Ctrl+W; macOS: Cmd+W |
| Drag a tab (or the selection) along the strip | reorder; the other tabs slide aside to show where they'll land. The dragged tabs stop at the strip's start and before the "+"; a window's only tab doesn't slide (drag it up or down to move it out) | `drag_move()`, `drag_drop()`, `draw_tab_bar()` | the same (drawn by Studio) |
| Drag tabs out of the strip, drop on empty space | they open in a new window there (every tab of a window: the window itself moves) | `drag_move()`, `drag_drop()`, `open_window()` | the same; the preview is a toolkit drag window (likely) |
| Drag tabs onto another Studio window's tab strip | they move into that window at the marked place | `find_dock()`, `put_tabs()` | the same |
| Escape during a tab drag | cancel it, nothing moves | `key()`, `drag_end()` | the same |

## Report a problem (4.3 part 3)

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| F8 | open Report a problem, anywhere in Studio (also while typing in a search box). *Added 2026-11-22* (the report flow snatcherLeo approved; OrrePort's key) | `key()`, `search_proc`, the global search's proc, `open_report()` | the same key (Fn+F8 on Mac laptops, likely) |
| Escape (while the panel is open) | close it without saving; no other shortcut acts behind it | `key()`, `report_note_proc`, `close_report()` | the same |

## Windows (the multi-window plan, `studio/multi-window-plan.md`)

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+Shift+N | a new, empty window (showing Home) | `key()`, `open_window()` | Linux: Ctrl+Shift+N; macOS: Cmd+Shift+N (Cmd+N is the usual "new window" there; to decide then) |
| Closing a window's last tab | closes that window (the last window stays open, on Home) | the tab close in `click()` | the same |
| Closing the last window | quits Studio | `WM_DESTROY` | macOS apps often stay running with no window: to decide then |

## Going back and forward (per tab)

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Alt+Left / Alt+Right | back / forward in the tab's history | `WM_SYSKEYDOWN` | Linux: Alt+Left / Right; macOS: Cmd+[ / Cmd+] |
| Mouse side buttons (back / forward) | the same | `WM_APPCOMMAND` (Windows turns the side buttons into it) | Linux: buttons 8 / 9; macOS: `NSEvent` buttons 3 / 4 |
| "←" / "→" in the tab bar | the same | `HIT_BACK` / `HIT_FORWARD` | the same |
| Click the O logo (top of the rail) | the shown tab becomes Home (a plain page, no list); Back returns to the Pokémon | `HIT_HOME`, `open_page(kHome)` | the same |
| Click the Pokémon icon on the rail, on Home | back to the Pokémon the window showed last, in that tab | `open_editor(0)` | the same |

## The Pokémon list and search

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+F | put the cursor in the search box (selects its text) | `key()` | Linux: Ctrl+F; macOS: Cmd+F |
| Up / Down | the previous / next Pokémon in the list (also while typing in the search box) | `key()`, `search_proc` | the same |
| Page Up / Page Down | a page up / down the list | `key()` | the same (macOS: Fn+Up / Down on laptops) |
| Home / End (not while typing) | the first / last Pokémon | `key()` | macOS: Fn+Left / Right or Cmd+Up / Down (likely) |
| Escape (in the search box) | clear the search | `search_proc` | the same |
| Enter (in the search box) | leave the search box (keys go back to the list) | `search_proc` | the same |
| Wheel over the list | scroll the list | `WM_MOUSEWHEEL` | the same |
| Wheel over the page | scroll the page | `WM_MOUSEWHEEL` | the same |

## The 3D view (the battle model card)

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Drag with the left button | turn the model | `viewer.cpp` `WM_LBUTTONDOWN` / `WM_MOUSEMOVE` | the same |
| Wheel | zoom in / out | `viewer.cpp` `WM_MOUSEWHEEL` | the same (trackpad pinch on macOS, likely) |
| Double-click | reset the view | `viewer.cpp` `WM_LBUTTONDBLCLK` | the same |
| Space (not while typing) | spin the model on / off (on the Moves tab: Play / Pause, below) | `key()` | the same |

## The Moves tab's preview (version 1, 2026-11-19)

The same 3D view, so the drag, wheel and double-click above work there too, also while a move plays or is held on a
frame (snatcherLeo, 2026-11-19: "are we able to navigate the preview/rotate so we can better see how the Pokemon uses the
move?"). On Plain stage the drag turns around the Pokémon starting from the game's own move-viewer angle, and the
double-click goes back to that angle.

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Space (not while typing) | Play the chosen move; while it plays, Pause (holds the frame); again, carry on | `key()` → `HIT_PLAY` | the same |
| Comma / period | one frame back / forward (holds the move; the first press shows its first frame) | `key()` → `HIT_STEP_BACK` / `HIT_STEP_FWD`, `Viewer::step` | the same keys (likely) |
| Click a learnset row | choose that move (the camera also frames its animation) | `HIT_LROW` | the same |

## Settings

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+, (also while typing in the list's search box) | Settings: the window's Settings tab, or a new one | `key()`, `open_page(kSettings)` | Linux: Ctrl+,; macOS: Cmd+, (the system's own Settings shortcut) |
| The gear on Home | this tab becomes Settings (Back returns to Home) | `HIT_SETTINGS` | the same |

## For developers

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+Shift+D | the developer view (memory, timings) | `key()` | Linux: Ctrl+Shift+D; macOS: Cmd+Shift+D |

## The global search (snatcherLeo, 2026-11-18: "both, but both do the same exact thing - fuzzy searching, just like Notion")

The top bar's field (with its drop-down) and the Ctrl+K palette run one forgiving search: accents, spaces and
punctuation don't matter, the letters may come with gaps (charzrd), and a typo or two is forgiven (pikchu, charmandr);
digits find National Dex numbers.

| Shortcut | What it does | Code | Linux / macOS |
|---|---|---|---|
| Ctrl+K (anywhere, also while typing) | open the palette | `key()`, the search boxes' subclasses | Linux: Ctrl+K; macOS: Cmd+K |
| Click the top bar's field | type there; matches drop down under it | `HIT_GSEARCH`, `gsearch_proc` | the same |
| Up / Down (in either field) | move through the matches | `gsearch_proc` | the same |
| Enter | open the selected match | `gsearch_proc` | the same (Return) |
| Ctrl+Enter, or Ctrl+click a match | open it in a new tab | `gsearch_proc`, `choose_result` | macOS: Cmd+Return / Cmd+click (likely) |
| Escape | close (the top field also clears) | `gsearch_proc`, `key()` | the same |
| Click outside the palette | close it | `HIT_PAL_BG` | the same |

## Planned, not built

- **Commands in the palette** (switching the theme, opening settings…): the palette searches Pokémon and editors now;
  commands, moves, items and trainers join it as they arrive.

## Retired

- Ctrl+I: the search box still passed it on, but nothing handled it any more (likely the v2 page's inspector toggle;
  the v3 page has no inspector, hover cards instead). Removed from the code 2026-11-18.
