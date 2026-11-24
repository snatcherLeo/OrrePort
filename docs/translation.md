# Translating OrrePort

Thank you for wanting to translate OrrePort. This page explains the language files, how to write a translation, and
how to check it. You don't need to know any code: a text editor and (for the checker) Python are enough.

## What you translate, and what you don't

- **You translate OrrePort's own words:** the hub, the Settings pages, the controller screen, the save list, and the
  buttons and messages OrrePort adds inside the game's screens. OrrePort Studio has its own words in its own files.
- **You don't translate the game's own text.** The game's dialogue, menus and Pokémon names come from the player's own
  disc while they play; they are not in these files. A player who wants the game in German, French, Italian, Spanish
  or Japanese adds their own European or Japanese disc, and OrrePort reads that language's text from it. Nothing from
  any disc ships with OrrePort.

## The files

| Program | Folder in the repo | Source file |
|---|---|---|
| OrrePort (the hub and its screens inside the game) | `port/ui/lang/` | `port/ui/lang/en.txt` |
| OrrePort Studio | `studio/lang/` | `studio/lang/en.txt` |

- **One file per language,** named with the language's code: `de.txt` (German), `fr.txt` (French), `ja.txt`
  (Japanese), `pt-BR.txt` (Brazilian Portuguese), and so on.
- **English is the source.** Every other file is a translation of `en.txt` in the same folder. The header of
  `port/ui/lang/en.txt` repeats the steps below for translators.
- **Some lines are shared.** Studio reads a few groups of lines from OrrePort's own file, so both programs say the same
  thing: every line starting `disc.`, `discs.`, `check.`, `update.` or `hub.lang.`, and a few `report.` and
  `settings.logs` lines. Translate those in `port/ui/lang/`; Studio picks them up from there.

## How a line looks

```
# the big button (capitals, very short)
hub.play = PLAY
```

- **The key** is the part before `=`, with dots (`hub.play`). Never change it: the program looks the text up by it.
- **The text** is the part after `=`. Change only this.
- **The note** is the `#` line (or lines) just above. It says where the text shows, and says **short** when the space
  is tight. Keep those texts about as short as the English. A note sometimes covers the few lines under it.
- **A new line** inside a text is written `\n`.
- **Please don't use `<` or `&`** in a text: the screens read them as layout.
- **Button names printed on controllers** (A, B, X, Y, L, R, START) are not in the files; they stay as they are.

## Whole sentences, with placeholders

A text never gets glued together from pieces. Each message is one whole sentence, with **placeholders** in curly
braces where OrrePort puts a value:

```
# problems; {file} the file's name
saves.err.read = Couldn't read {file}.
```

- **Keep every `{name}` exactly as written,** braces included.
- **Move it anywhere** in the sentence your language needs. That is why sentences are whole: any word order works.
- **Don't add new ones:** OrrePort only fills the names the English line has.

## Counts and dates

- **Counts** ("1 picture", "5 pictures") have one line per form: `key.one`, `key.few`, `key.many`, `key.other`. The
  `plural` line at the top of your file picks which forms your language uses:

  | `plural` | Forms | Languages (examples) |
  |---|---|---|
  | `one-other` | one, other | English, German, Spanish, Italian, Dutch, Swedish |
  | `french` | one (0 and 1), other | French, Brazilian Portuguese |
  | `east-slavic` | one, few, many | Russian, Ukrainian, Belarusian |
  | `polish` | one, few, many | Polish |
  | `czech` | one, few, other | Czech, Slovak |
  | `none` | other only | Japanese, Chinese, Korean |

  A missing form uses `other`.
- **Dates** are patterns with placeholders (`date.time = {month} {day}, {year} {time}`) plus the month names, so
  you can put day, month and year in your own order. The time line can use a 12-hour clock (`{h}` with `{ampm}`) or a
  24-hour one (`{hh}`).

## Adding a new language

1. **Copy `en.txt`** in the program's folder and name the copy with your language's code (`de.txt`, `pt-BR.txt`).
2. **Set the two lines at the top:** `language.name` (your language's name as its speakers write it: Deutsch,
   Français, 日本語) and `plural` (from the table above).
3. **Translate the text after each `=`.** A half-done file is fine: any line you leave out shows in English.
4. **Save it as UTF-8.** Most editors do; in Notepad, choose UTF-8 when saving.
5. **Try it:** start OrrePort. With your computer set to that language, OrrePort picks the file by itself; or set
   `language = <your code>` in OrrePort's `settings.ini`. No rebuild, nothing to install. Studio follows the same
   setting.
6. **Run the checker** (below), and fix anything it lists.

**A file for the game's screens only.** A file can start with `language.scope = game`. It then holds only OrrePort's
words inside the game's screens; the hub doesn't offer the language and stays in English, so players never see half a
language there. Remove the line once the file has every line.

## Testing your layout: the `pseudo` language

Set `language = pseudo`. Every text then shows accented and made longer (by about a third to 40%), inside brackets:
`[Šåvé fîlé ~~~~]`. It is not a real language; it shows you, before a real translation does:

- **text that gets cut off:** a missing closing bracket means the box was too small;
- **text not yet on a key:** plain English among the accented text means it was written into the code by mistake.

Placeholders and layout marks stay untouched, so screens still work.

## The checker

`tools/local/lang_check.py` compares a translation with its English source. Plain Python 3, no packages, the same on
Windows, Linux and macOS.

```
python tools/local/lang_check.py de              # OrrePort's port/ui/lang/de.txt
python tools/local/lang_check.py --studio de     # Studio's studio/lang/de.txt
python tools/local/lang_check.py path/to/de.txt  # any file, checked against the en.txt beside it
python tools/local/lang_check.py --all           # every translation in both folders
```

For each file it prints how many lines are translated, then:

- **`FIX`**: something to correct.
  - A line English doesn't have (a typo in the key, or a line OrrePort no longer uses).
  - A `{name}` missing, renamed or added.
  - A count missing a form your `plural` rule needs.
  - A `<` or `&` in a text.
  - A `plural` value that isn't one of the six.
- **`LOOK`**: worth a look on screen.
  - A text much longer than English where the note says "short".
  - No `language.name` line.
- **still in English:** the lines not translated yet (fine for a half-done file).

It ends with "nothing to fix" when there is nothing to correct. Its exit code is 0 then (missing lines alone are
fine) and 1 otherwise, so it can run in a check before a pull request.

## The official languages

OrrePort's official languages are the six the game itself was released in: **English, German, French, Italian,
Spanish and Japanese.**

- **English** is complete in both programs.
- **German, French, Italian, Spanish and Japanese** have files in both folders. OrrePort's hold, so far, only its words
  inside the game's screens (`language.scope = game`), written to match each language's own release of the game, and
  they need a native speaker's review. Studio's cover most of its lines. Corrections from native speakers are very
  welcome.
- **Other languages** are welcome as community translations, using the same files and the same checker.

## Fonts and scripts

OrrePort ships open fonts (the Noto family) for every living script, so the letters of any language can be drawn.
Text shaping (Arabic and Hebrew joined and read from the right, Indic and Thai marks placed correctly) works on
OrrePort's own pages. A right-to-left translation of the pages hasn't been tried yet (likely to need small layout
changes).
