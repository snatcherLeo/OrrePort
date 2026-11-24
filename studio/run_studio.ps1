# run_studio.ps1: starts OrrePort Studio from its development build (Windows PowerShell 5.1).
#
# Why a launcher: during development Studio's build sits in build-pc\studio, outside the portable folder, so this
# passes OrrePort's folder: snatcherLeo's play folder, decomp\OrrePort\ beside the main checkout (step 4.1's portable layout,
# copied 2026-11-21 with his OK). From it Studio takes the shared Theme and Language (data\settings\settings.ini), the
# disc (Pokemon Colosseum\ROM\, checked for GC6E01) and the fonts (app\Fonts\). (Layout A, 2026-11-22: app\ was bin\,
# data\settings\ and data\system\ were settings\ and system\.) This starts the build of this checkout or
# worktree, so a hand check sees the newest change; Studio's own preferences and logs stay beside that build
# (build-pc\studio\user, \logs). The play folder's own Studio (R\OrrePort Studio.exe) is the project's merged build.
# Before the play folder existed (until 2026-11-21) this passed orig\'s disc, build-pc\firstlink and its Fonts; that
# is still the fallback when the play folder is missing.
# Windows only (PowerShell). Linux and macOS get their own launcher with their shells.
#
#   .\studio\run_studio.ps1                 # normal start (the last session comes back, and is saved on close)
#   .\studio\run_studio.ps1 -Theme light    # force a theme (dark / light; default: OrrePort's setting)
#   .\studio\run_studio.ps1 -Select 25      # start on another Pokémon (internal number; a test start: the last
#                                           # session is neither restored nor saved)
#   .\studio\run_studio.ps1 -FlashWatch 30  # maximized, then reads the 3D view's area on screen for 30 s while you
#                                           # click around (a white-flash recorder; results in build-pc\studio\logs)
param(
    [string]$Theme = "",
    [int]$Select = 1,
    [switch]$LogDetail,
    [int]$FlashWatch = 0
)
$repo = Split-Path -Parent $PSScriptRoot
# the main checkout (its build, disc and play folder): git's shared folder belongs to it, so from a linked worktree
# anywhere this finds the main checkout, and in the main checkout itself it is the same folder. Needs git on PATH;
# without it, this checkout is taken as the main one. (PowerShell, Windows; a Linux / macOS script would ask git the
# same way.)
$main = $repo
$common = git -C $repo rev-parse --path-format=absolute --git-common-dir 2>$null
if ($LASTEXITCODE -eq 0 -and $common) { $main = Split-Path -Parent ($common -replace '/', '\') }
$exe = Join-Path $repo "build-pc\studio\orrestudio.exe"
if (-not (Test-Path $exe)) { Write-Host "Studio isn't built yet: $exe"; exit 1 }
# the play folder: <decomp>\OrrePort, the main checkout's sibling
$play = Join-Path (Split-Path -Parent $main) "OrrePort"
if (Test-Path (Join-Path $play "app")) {
    # the disc and fonts are found inside it (no --disc / --fonts)
    $argsList = @("--orreport", $play)
} else {
    $orreport = Join-Path $main "build-pc\firstlink"
    $argsList = @("--disc", (Join-Path $main "orig\Pokemon Colosseum (USA).iso"), "--orreport", $orreport,
                  "--fonts", (Join-Path $orreport "Fonts"))
}
# --select only when -Select is given: Studio treats a chosen start as a test and neither restores nor saves the last
# session then. *Fixed 2026-11-22* (snatcherLeo: "we need to ensure, the app remembers the exact tab I was last on ... I don't
# think it does"): this launcher always passed --select 1, so every hand check started on Bulbasaur and its close was
# never saved; the Desktop shortcut (no arguments) did restore and save
if ($PSBoundParameters.ContainsKey("Select")) { $argsList += @("--select", "$Select") }
if ($Theme -ne "") { $argsList += @("--theme", $Theme) }
if ($LogDetail) { $argsList += "--log-detail" }
# the white-flash recorder (2026-11-19): the window maximizes, then the 3D view's area is read for that many seconds;
# the variables are set only for Studio's start and cleared again, so this PowerShell window stays as it was
if ($FlashWatch -gt 0) {
    $argsList += @("--flash-watch", "$FlashWatch")
    $env:ORRE_FLASH_STAGE = "1"
    $env:ORRE_FLASH_MAX = "1"
}
Write-Host "Starting OrrePort Studio ($exe)"
# PowerShell 5.1 passes -ArgumentList unquoted, so paths with spaces are quoted here
$joined = ($argsList | ForEach-Object { '"' + $_ + '"' }) -join " "
Start-Process -FilePath $exe -ArgumentList $joined -WorkingDirectory (Split-Path -Parent $exe)
if ($FlashWatch -gt 0) {
    Remove-Item Env:ORRE_FLASH_STAGE -ErrorAction SilentlyContinue
    Remove-Item Env:ORRE_FLASH_MAX -ErrorAction SilentlyContinue
    Write-Host "Recording for $FlashWatch seconds once the window is maximized: click around the Pokemon list now."
}
