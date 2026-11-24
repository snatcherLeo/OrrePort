# OrrePort Studio: multi-tab, multi-window plan

**Status:** steps 1-5 built (1 verified; 2-5 waiting for snatcherLeo's hand-check); step 6 later. Approved by snatcherLeo as a plan, 2026-11-18 ("Write up a plan for this, sounds good to
me"). Each step below is built, checked by hand and committed before the next one starts.

## What snatcherLeo asked for

snatcherLeo, 2026-11-18: "can we do a thing (huge idea I think), where, just like Chrome, you can drag a tab or multiple tabs
multiselected (either by CTRL or SHIFT + click) out of a window, and then it can open up in its own window? But in its
own window, it still has access to.. basically, this is how an ideal editor would act. You can like have a separate
window process and you could maybe, in a separate window, open another editor, or stay in the existing editor.
multi-tab workflow + multi window workflow."

So:

- **Tear tabs off:** drag one tab, or several tabs selected with Ctrl+click or Shift+click, out of the tab strip, and
  they open in a new window where they're dropped.
- **Dock them back:** drop dragged tabs onto another window's tab strip, and they move into it.
- **Full windows:** every window is a full Studio window, with its own rail, list, editor, tabs, search and 3D view.
  One window can show the Pokémon editor while another shows a different editor, or the same one.

## The main decision: one program, many windows

All windows run in **one Studio process**, not one process per window. Why:

- **Shared data:** the disc, the species list, the decoded pictures, the text and the fonts are read once and shared.
  A second process would read and hold everything again (Studio's budget is 600 MB).
- **Instant moves:** a tab moving between windows is a few numbers moving in memory. Across processes it would have
  to be sent and rebuilt.
- **Precedent:** browsers and code editors that look multi-process still keep one process for the windows' own state.

Linux / macOS: the same decision (one process, several toolkit windows).

## Where things stand today (2026-11-18)

Studio keeps all its state in one global, `App g` (`studio/win32/app.cpp`), built for exactly one window:

| Shared by every window (stays global) | One per window (moves into a `Window`) |
|---|---|
| `disc`, `all`, `by_index`: the disc and the species list | `hwnd`, `rt` (its Direct2D target), `dc`, `scale` (each window can sit on a monitor with its own DPI) |
| `dw`, `fonts`, `fallback`, `ui_family`, `formats`: DirectWrite and the fonts | `page_tabs`, `cur_tab`, `tab_hist`, `tab_pos`, `tab_scroll`, …: the tabs and their history |
| `pal`, `theme_*`, `lang_*`, the settings | `selected`, `editor`, `tab` (the page's tab), `shiny`, `form`, `forms`, `mugs`, `pcs`, `pscroll`, `scroll`, `shown` |
| the search vocabulary (`g_sdocs`, `g_vocab`) | `search`, `gsearch`, `psearch` (the edit controls), `palette_open`, `drop_open`, `sres`, `ssel`, `snote` |
| `d2d` (the Direct2D factory) | `viewer` (its 3D view), `hits`, `hover`, `mouse_*`, `toast` |

**Two places that need care:**

- **The pictures:** mug shots and other pictures are Direct2D bitmaps made on the window's render target
  (`make_bitmap` on `g.rt`). A bitmap from one `ID2D1HwndRenderTarget` can't be drawn on another. Two options:
  - **(a)** one Direct2D device shared by all windows, each window drawing through a device context into its own
    swap chain. Bitmaps from one device work in every window. This is the better design, and what modern Direct2D
    apps do.
  - **(b)** a picture cache per window, so each window decodes its own copies.
  
  The plan is **(a)**, done in step 1. Linux / macOS: the toolkit's images are shared anyway (likely).
- **The 3D view:** `Viewer` holds its own Dawn instance, adapter, device and surface. With several windows there
  will be one Dawn device for all of them, and one surface plus swap chain per window's 3D view, so textures and
  pipelines are made once. Models already upload per view and stay so.

## The steps

Each step leaves Studio working, is checked by hand, and is committed on its own.

### Step 0: hand-check the tabs as they are now (snatcherLeo, pending)

The multi-window work builds on these, so they're checked first (snatcherLeo: "didn't do these yet, but I will at some
point"). Open about ten Pokémon with Ctrl+click first.

1. Ctrl+Tab / Ctrl+Shift+Tab: next and previous tab, wrapping around at the ends.
2. Ctrl+1 … Ctrl+8: jump to that tab. Ctrl+9: the last tab.
3. Ctrl+W: closes the tab you're on.
4. Middle-click a tab: closes it.
5. Wheel tilt left / right, anywhere in the window: scrolls the tabs sideways. The normal wheel does the same over
   the tab strip.
6. "‹" / "›" at the strip's ends, when scrolled: jump to the first or last tabs. "▾" lists every open tab.
7. Alt+← / Alt+→, or the mouse's side buttons: back and forward inside the tab you're on.
8. Ctrl+K, also while typing in a search box: opens the palette.
9. While typing in the list's search box, Ctrl+Tab and Ctrl+W still work.

### Step 1: split the state, shared and per window (large, invisible) — *done 2026-11-18*

*Done* (snatcherLeo: "Start step 1"), in three commits:
- **1a / 1b:** `WinState`, a window list, and `activate()`.
- **1c:** one shared Direct3D 11 / Direct2D device, and each window's own device context and flip-model swap chain.
  It adds about 15 MB.
- **1d:** one shared WebGPU instance, adapter, device and queue for every 3D view.

How it was checked: four screenshots taken before step 1, re-taken after each part, came out byte-identical:
- Espeon, dark;
- Wurmple, light;
- the palette searching "fire";
- Unown in German.

The golden tests passed. As built, App derives from WinState, so the existing code reads `g.field` unchanged.
`activate()` swaps a window's WinState into `g` at the start of each of its messages (one UI thread). Each window
gets its own Viewer with its own child window and surface; the GPU device behind them is shared.

- **The split itself:** move the per-window fields of `App` into a `struct Window`, and keep a list of windows.
  Code that today reads `g.selected` reads the current window's field instead. The window procedure sets "the current
  window" from the `HWND` at the start of each message, so most drawing code keeps its shape.
- **Drawing:** switch from `ID2D1HwndRenderTarget` to one shared `ID2D1Device` with a device context and a DXGI swap
  chain per window (option (a) above). Bitmaps and brushes are then shared.
- **3D:** the `Viewer`'s Dawn instance and device become shared, with a surface per view.
- **Timers and messages:** the timers (toast, shot, watch), the worker threads' completion messages (list loaded,
  model loaded) and the screenshot tests go to the right window: a model load carries the window it was asked for.
- **Done when:** Studio looks and behaves exactly as before, with one window. The golden tests and every screenshot
  test match, and the memory budget still holds.

### Step 2: new windows, the simple way — *built 2026-11-18, waiting for snatcherLeo's hand-check*

*Built* (snatcherLeo: "Start step 2"):
- **New windows:** Ctrl+Shift+N, and the tab menu's "Move to new window" (the tab keeps its history). A new window
  opens a little down and to the right of the one it came from, at the same size.
- **The tab menu:** Duplicate, Close, Close other tabs, Close tabs to the right.
- **Closing:** a second window closes with its last tab; the last window quits Studio.

**How it works:**
- `open_window()` hands the tabs to the new window's `WM_CREATE` through `g_new_window`. `WM_CREATE` parks the
  active window's state and starts the new one fresh.
- **The search boxes:** their subclass procedures activate their own window first, since their messages bypass the
  window procedure.
- **The 3D view:** each Viewer can move now. Its child window keeps the Viewer's never-moving Impl, which points back
  at whichever Viewer owns it.
- **Closing a window:** `WM_DESTROY` releases that window's drawing context, swap chain, pictures and model, then
  makes another window the active one.

How it was checked:
- **A screenshot test with two windows:** `--second-window 196` opens Espeon in a second window and captures it. Its
  list, pictures and 3D view work, and memory with both open is about 268 MB.
- **Regression:** the single-window screenshots are still byte-identical.

Known limits:
- **Fonts across monitors:** the search boxes' fonts are made at the first window's DPI. A window on a monitor with
  another DPI keeps them until its own DPI change remakes them for all.
- **A lost graphics device** is handled only for the window that notices it.
- **Theme changes** reach the window that gets the system message; the others follow on their next repaint.

The original plan for this step:

- **Ctrl+Shift+N:** a new, empty window, with Home shown.
- **Right-click a tab** for a menu: Move to new window, Close, Close other tabs, Close tabs to the right, Duplicate.
- **Closing:** closing the last tab of a window closes that window (as in Chrome); closing the last window quits
  Studio.
- **Window placement:** each window remembers its size and position while open. The first window's placement is
  saved as today.
- **Done when:** two windows can be open, each with its own tabs, editor and 3D view, and a tab moves to a new window
  from the menu.

### Steps 3-5: selecting, dragging, tearing off, docking — *built 2026-11-18, waiting for snatcherLeo's hand-check*

*Built* together (snatcherLeo: "Can we click and drag a tab into another window ... move tab order around, be able to CTRL
click to selectively multiselect tabs, and SHIFT click to just multi-select from a range, then drag out those
selections into a new window as well. Just like Chrome").

**How it works** (`studio/win32/app.cpp`):
- **Selection:** each window keeps `tab_sel` (one flag per tab) and `tab_anchor`. `tab_press()` follows Chrome:
  Ctrl+click toggles, Shift+click selects from the anchor, and a plain click on an already-selected tab keeps the
  selection until the button comes up (`clear_on_up`), so a selection can be dragged.
- **Moving tabs:** `take_tabs()` lifts tabs out of a window with their history (`TabRec`); `put_tabs()` inserts them
  at a place and shows the first. Reorder, dock and the menu's "Move to new window" all go through these two.
- **The drag:** a press on a tab captures the mouse (`g_drag`); after 6 points of movement it starts. Inside the strip
  (and a little above and below it) the tabs float with the mouse and the others leave a gap where they'll land.
  Further out, a small see-through popup (`WS_EX_LAYERED`, "OrrePortStudioDragPreview") follows the mouse with the tab
  names. Over another Studio window's strip (`find_dock()`, using `WindowFromPoint` and that window's parked strip
  layout) an accent bar marks the place.
- **The drop:** in the strip, reorder; on another window, dock (a window emptied this way closes); elsewhere, a new
  window opens at the mouse (`open_window()` with a position), or, if every tab of the window was dragged, the window
  itself moves there. Escape or losing the mouse capture cancels.

How it was checked:
- **Screenshot tests:** `--tabs "1,4,7,25,133" --tab-select "1,3"` shows the tint; `--tab-move "0,1>2"` moves two tabs
  together (Bulbasaur and Charmander land after Pikachu).
- **Regression:** the four step-1 screenshots are still byte-identical (one retake needed: a mouse pointer left over
  an evolution card gave it its hover border). The golden tests pass.
- **Not checked by a test:** the live drag itself (tear-off, docking, the preview). That needs snatcherLeo's hand-check.

**Polish after snatcherLeo's first hand-check (2026-11-19).** Dragging, tearing off and docking worked; he asked for:
- **A limit on sliding:** "I can't drag it too far to the right, only past the second tab ... Maybe it stops right
  before the "+" button". The dragged tabs are now held between the strip's start and the last tab's place, so they
  never cover the "+", and a window's only tab can't slide at all (as in Chrome); it can still be dragged up or down
  to tear it off.
- **A clearer selection:** "make it look more refined/clear that I multi-selected the tabs". Selected tabs now take
  the shown tab's own shape, open into the page, in a softer fill with an accent line along the top and a brighter
  name, so the set reads as raised tabs, as Chrome and Edge draw it. The old look was a flat tint.
- **A new tab as Chrome makes one:** "+" opens a Home tab at the end, with the cursor in the top search field. It
  used to copy the shown Pokémon's tab.
- **Home as a tab:** "if I click HOME button ... I'm stuck", "just have clicking the home button not show Pokémon,
  and just show a blank screen with some text for now", "The tab would change too, to just showing our app logo and
  saying "Home"".
  - **How Home works now:** Home is a page a tab can show (`kHome` in `page_tabs` and the histories). The O turns the
    shown tab into Home, as a browser's Home button does, so Back returns to the Pokémon.
  - **What Home shows:** a plain page with no list, the logo and a short text. snatcherLeo: "It'll eventually be where we
    put all our icons for all the editors and stuff, but not yet".
  - **Leaving Home:** the rail's Pokémon icon on Home goes back to the last Pokémon shown, which fixes the dead end.
  - **New windows:** a new empty window (Ctrl+Shift+N) starts with a Home tab. Closing the last window's last tab
    leaves a Home tab.
- **Checks:** the four step-1 screenshots are byte-identical. A test shot shows a Home tab: `--tabs` takes 0 for Home,
  since the disc's slot 0 is never a Pokémon. The golden tests and the language check pass.
- **A flaky capture:** one retake was needed because the native search boxes came out white when captured before they
  had painted. It passed on retake.

**Second round (snatcherLeo, 2026-11-19):**
- **No cursor from "+":** "the search field at the top has a cursor where it shouldn't". "+" no longer focuses the
  search field.
- **The selection line:** "Shouldn't it be one more line above the 3rd tab, and shouldn't that green line be covering
  the entire top portion of each tab selected?" The accent line now covers each selected tab's whole top edge,
  following its rounded corners (the tab's shape, clipped to its top 2.5 DIPs). The shown tab gets it too when it is
  part of a multi-selection.
- **The 3D view during a drag:** "there can be a black flicker that happens on the model ... we could freeze the
  movement of the idle pose ... and then it resumes the loop when we let go".
  - **Pausing:** every window's 3D view pauses when a drag starts (`Viewer::set_paused`) and carries on from the same
    frame when it ends. The loop runs on the clock, so its start moves forward by the time paused.
  - **Before this:** a drag already starved the view's timer, since Windows sends `WM_TIMER` only when nothing else
    waits.
  - **The flicker, two causes (both *likely*, from the code; snatcherLeo's hand-check confirms or not):** after a reorder,
    `take_tabs` briefly showed (and loaded) a neighbouring tab's Pokémon, and `put_tabs` forced the dropped one to
    load again even when it was already shown. A reorder now does neither.

Known limits:
- The strip doesn't yet scroll on its own while a drag reaches its edge.
- The preview shows the tab names, not a snapshot of the page (planned in step 5; a later polish).

### Step 3: selecting several tabs (the original plan)

- **Selecting:** Ctrl+click a tab adds it to the selection or removes it; Shift+click selects the range from the shown
  tab. Selected tabs get a light accent tint. A plain click clears the selection.
- **Acting on the selection:** the right-click menu and Ctrl+W act on every selected tab.
- **Done when:** several tabs can be selected, and moved to a new window or closed together.

### Step 4: dragging inside the tab strip

- **Reordering:** press on a tab and move a few pixels, and it lifts and follows the mouse along the strip; the other
  tabs slide aside to show where it will land. Several selected tabs move together.
- **Edges:** the strip scrolls when the drag reaches its edge.
- **Done when:** tabs reorder smoothly, and the tab you're on and the history of each tab survive the move.

### Step 5: tearing off and docking (large)

- **Tearing off:** dragging a tab, or the selection, more than a short distance above or below the strip tears it
  off. A floating preview follows the mouse: a small see-through window (`WS_EX_LAYERED`) showing the tabs and a
  snapshot of the page.
- **Dropping on empty space:** a new window opens there with those tabs, at the size of the window they came from.
- **Dropping on another window's tab strip:** the tabs move into it at the place shown, the same sliding as step 4.
  Windows find each other through the shared window list, by hit-testing the strip under the mouse.
- **Dragging every tab of a window** moves the window itself, as in Chrome.
- **Escape** during a drag cancels it.
- **Done when:** tabs tear off into new windows, dock back, and move between windows, singly and selected together.

### Step 6: polish and later

- **Restoring:** reopen the windows and tabs as they were left (a small file next to Studio's settings).
- **Merging:** "Merge all windows" in the right-click menu.
- **Snapping:** Windows' snap layouts work on their own, since these are real windows.
- **Searching across windows:** the global search opens results in the window it was typed in; a result already open
  in another window could offer "Switch to it" (a design question for later).

### Clearer dragging out and back in (2026-11-22, 4.3 part 2)

snatcherLeo: "I want to firstly review how tabs are dragged out of the app ... and then dragged back in, because that could
use some polish ... In our app, this is possible, but not clear at all". Three animated mockups were shown (A, a live
window following the pointer; B, the tab plus a dashed outline of the new window and a vertical line in the strip it
lands in, lit-up drop zones with a hint). snatcherLeo tried them one by one, each built and checked by the drag test. A was
built first: "I like A's live window, but then I like how in B, you have a clear indicator where you are moving it
back into that window (the vertical line)". Then he picked B: "I take it back I don't like the live window, I'd
prefer B's approach, with the outline". Then he settled on just the tab: "Option mockup C woudl be better. No window,
just the tab" and "but no dropzones, still use the vertical line yeah". What's built:
- **The tab under the pointer:** a tab-shaped chip with the grabbed tab's name, and how many tabs travel when several
  (*changed the same evening*, snatcherLeo: "get rid of the "+" symbol and just have the number of things I am dragging":
  "2" for two, where it read "+1"). It replaces the
  old list box beside the pointer. *Updated the same day (snatcherLeo, with screenshots of the chip beside a real tab: "When I
  drag the tab, I want it to look... idk, better?"):* the first chip was GDI-drawn, with square corners showing, no
  icon and another font. It's now drawn by the strip's own Direct2D helpers: the tab's rounded shape, its mug shot or
  symbol, its font, a soft shadow and an accent "+N" pill. It's drawn into an offscreen bitmap on the window's device
  context, read back, and shown in a layered window with per-pixel alpha (`render_chip`).
- **The vertical line:** over another window's strip, an I-beam (the bar's full height, a cap at each end) where the
  tabs will land. While it shows, the chip hangs below and right of the pointer so it never covers the line.
- **Faded where they were:** the torn-off tabs fade in their own window's strip while they travel.
- **Where windows open:** a new window opens with its first tab under the pointer, where the tab was grabbed. A
  window moved by all its tabs keeps the place it was grabbed at. Before, both used a fixed offset.
- **New tabs grow in** (snatcherLeo, the same day: "we should have the actual tab creation ... have a little animation,
  instead of just instantly and staticaly showing up"; he picked option A of three mockups, the others being a rise-in
  and an accent glow). Every new tab (Ctrl+click, Ctrl+T, "+", Duplicate; all go through `open_page`) is revealed from
  its left edge as its width eases out from 0 to full in 180 ms (ease-out cubic, `kTabGrowMs`). The tabs after it and
  "+" slide along. A timer (`TIMER_TABGROW`, about 60 a second) redraws until it's done. Only in a window already on
  screen: tabs restored at start, or carried into a new window, just appear. Plain Direct2D clipping and a timer;
  Linux / macOS: the same with the toolkit's timer.
- **Many tabs in a narrow window** (snatcherLeo, the same day, with screenshots: the shown tab at the right end cut off under
  "›", a sliver of a tab beside "‹"; "if we are selected on the furthest tab to the right ... It just clips and looks
  awkward"). When the tabs don't all fit, "‹" and "›" have their own room at the strip's ends (faded when there's
  nothing more that way) instead of lying over the tabs. The tabs widen a little so a whole number of them fills the
  room between, and the strip scrolls by whole tabs (a wheel notch is one tab). No tab is ever cut off, and the shown
  tab always shows whole. Checked by screenshots at 1150 × 700 with nine tabs, scrolled to the end and to the start.
- **Windows' own menus in Studio's theme** (snatcherLeo, the same day, with screenshots of the white tab list and Target
  menu: "For dark mode especialy, looks too bright"). Every native popup menu (the tab list, the tab's right-click
  menu, Target, the frame rate) follows Studio's theme through uxtheme's unnamed dark-mode exports (`apply_menu_theme`,
  theme.cpp, Windows 10 1903 and later; older Windows keeps light menus). Checked with `--menu-test 7204`, which
  photographs an open menu. A restyled, Studio-drawn list (mug shots in the tab list) would be a later step if wanted.
- **A closed window's memory** (found after snatcherLeo's multi-window stress test, the same evening: memory stayed at 620 MB
  after he closed two of three windows). A new test, `--window-cycle N`, opens a second window and closes it N times
  with the memory watch after each close. It showed about 12 MB kept per closed window (433 to 582 MB over 12).
  The cause: closing a window replaced its state with a blank one, and `Viewer`'s move-assignment overwrote its `Impl`
  pointer without deleting it, so the whole 3D view (model, textures, frames, particles) was never freed. The page's
  shared-frame textures weren't released either. Now `Viewer::destroy()` (the child window first, then the `Impl`) and
  `release_shared()` run when a window that isn't the last closes. The same test is flat at about 483 MB from the
  second cycle on (verified). Not a destructor on purpose: the last window's view sits in a static destroyed after the
  graphics device.
- **Finding the window under the pointer:** a z-order walk over Studio's windows (`studio_window_at`), skipping the
  dragged window and the chip, instead of WindowFromPoint.
- **The drag test:** `--drag-test T` (with `--tabs`, optionally `--tab-select`) plays a full drag with made-up pointer
  moves and never touches the real mouse. It tears tab T off, drops it into a new window, drags that window's tabs
  back onto the first window's strip and drops them. It writes `drag_1_torn.png` to `drag_4_back.png` into the logs
  folder, and logs the window and tab counts. Checked for one tab and for two selected tabs: 2 windows after the
  tear-off, 1 window and 5 tabs at the end, the line and chip visible.
- **Platforms:** the chip is a layered GDI popup and the walk uses Windows' z-order (Windows-only). Linux and macOS
  would use their toolkit's drag window and stacking order (likely).

## Checks for every step

- The golden tests, the language check and the screenshot tests pass (screenshot tests for two windows get their own
  options, like `--new-window`).
- The memory budget: a second window with a 3D view should add well under 100 MB (likely: one render target and swap
  chain, one 3D surface, no second copy of the data).
- `studio/shortcuts.md` gets every new shortcut (Ctrl+Shift+N, the selection clicks, the drag) in the same commit.
- Every new word goes through `studio/lang/*.txt` in all six languages.

## Platform notes

| Piece | Windows | Linux / macOS |
|---|---|---|
| windows | Win32 `HWND`s | the toolkit's windows (likely the same split) |
| drawing | Direct2D device + DXGI swap chains | the toolkit's drawing; the split of state is the same |
| 3D | one Dawn device, a surface per view (`SurfaceSourceWindowsHWND`) | the same Dawn device; Xlib / Wayland / Metal layer surfaces |
| drag preview | a layered window (`WS_EX_LAYERED`) | X11 / Wayland: a toolkit drag window; macOS: `NSWindow` with a transparent background |
| tab menu | `TrackPopupMenu` | the toolkit's popup menu |

## For XD

The split is Studio's own design, not game data, so it carries over unchanged when XD's data joins Studio. A window
could even show Colosseum in one tab and XD in another, once both discs are read.
