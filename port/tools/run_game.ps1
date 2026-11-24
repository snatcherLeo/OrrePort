# Starts snatcherLeo's play build from his portable OrrePort folder, with the debug toolkit on (F1), and saves the console
# output to a fresh log file. Phase 4 step 4.1 (2026-11-21): the play folder is decomp\OrrePort\ (snatcherLeo's choice,
# 2026-11-21), beside the checkout and outside build-pc, so a build clean can never reach his saves; builds refresh only
# its app\ (port/tools/stage_bin.py, called by firstlink.py; bin\ until layout A, 2026-11-22). Kept in the repo (build-pc\run_game.ps1 is git-ignored and
# was never backed up); build-pc\run_game.ps1 forwards here once the migration is done.
#   & .\port\tools\run_game.ps1              normal (movies play)
#   & .\port\tools\run_game.ps1 -SkipMovies  skip the logo, opening and other movies
#   & .\port\tools\run_game.ps1 -Old         the old flat folder (build-pc\firstlink, disc from orig\), as before 4.1
# What changed from the old script, and why:
# - the exe is <OrrePort>\app\orrecolo.exe (not build-pc\firstlink), started directly rather than through the
#   OrrePort.exe launcher, so the log redirect and the debug variables below still reach it and the exit code is the game's;
# - no --disc: the disc is in <OrrePort>\Pokemon Colosseum\ROM\ (snatcherLeo: start the way testers will);
# - no MSYS2 on PATH: app\ carries every DLL the exe needs (the step 2 check);
# - the console copy goes to <OrrePort>\data\system\logs\player_run_<stamp>.log (the game's own log is beside it;
#   until 2026-11-24 the name started with the project lead's name, changed so the public copy carries none).
# Windows PowerShell 5.1 (snatcherLeo's terminal). Linux / macOS would use a shell script with the same steps (likely).
param([switch]$SkipMovies, [switch]$Trace, [switch]$KeepTests, [switch]$Old)
$ErrorActionPreference = "Continue"
# A normal start clears every ORRE_ test setting left in this PowerShell window by an earlier test command (
# 2026-11-15: snatcherLeo saw Wes named "WWWWWWWWWWWW" in normal play; an ORRE_TEST_HERO_NAME from a re-wrapping hand check
# had stayed in his window). -KeepTests keeps them, for a test command that sets one on purpose.
if (-not $KeepTests) {
    $left = @(Get-ChildItem Env: | Where-Object { $_.Name -like 'ORRE_*' })
    foreach ($v in $left) { Remove-Item "Env:$($v.Name)" }
    if ($left.Count -gt 0) { Write-Host "Cleared left-over test settings: $(($left | ForEach-Object { $_.Name }) -join ', ')" }
}
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # the checkout (port\tools -> port -> checkout)
$decomp = Split-Path -Parent $root
$play = Join-Path $decomp "OrrePort"
if ($Old) {
    $exe = Join-Path $root "build-pc\firstlink\orrecolo.exe"
    $gameArgs = @("--disc", (Join-Path $root "orig\Pokemon Colosseum (USA).iso"))
    $env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
    $logDir = Join-Path $root "build-pc"
    $cwd = Join-Path $root "build-pc\firstlink"
} else {
    $cwd = $play
    $exe = Join-Path $play "app\orrecolo.exe"
    $gameArgs = @()
    $logDir = Join-Path $play "data\system\logs"
    if (-not (Test-Path $exe)) {
        Write-Host "No play folder yet: $exe is missing. Use -Old for build-pc\firstlink, or ask Main to stage it."
        return
    }
}
$env:ORRE_DEBUG = "1"
$env:ORRE_CRASH_ANY_MODULE = "1"   # crash reports with a call stack even when the fault is outside the exe
$env:ORRE_WATCHDOG = "20"        # stack report if the game stops drawing for 20 s; also enables the F1 "stack" samples
$env:ORRE_HEAP_LOG = "1"         # B's heap recorder: failed allocations, and use / peak per heap at exit
if ($Trace) { $env:ORRE_TRACE_NATIVES = "1" } else { Remove-Item Env:ORRE_TRACE_NATIVES -ErrorAction SilentlyContinue }
if ($SkipMovies) { $env:ORRE_SKIP_MOVIES = "1" } else { Remove-Item Env:ORRE_SKIP_MOVIES -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$log = Join-Path $logDir "player_run_$stamp.log"
Write-Host "Starting the game from $exe (skip movies: $SkipMovies); log: $log"
# "snatcherLeo is playing" marker: tools/local/saferun.py makes every session's heavy run wait while it names a live process
$playing = Join-Path $decomp ".orre_player_playing"
Set-Content -Path $playing -Value "$PID playing since $(Get-Date -Format 'HH:mm:ss')"
# What else is running (the run board): new runs now wait for you; a test boot already running finishes within its
# time limit (usually 1-2 minutes), and its window may appear next to yours
Write-Host "--- other runs on this PC ---"
python -I (Join-Path $root "tools\local\whatsrunning.py")
Write-Host "-----------------------------"
try {
    # the working folder is the OrrePort folder, as the launcher sets it (test-only files never land in app\)
    Push-Location $cwd
    & $exe @gameArgs *> $log
} finally {
    Pop-Location
    Remove-Item $playing -ErrorAction SilentlyContinue
}
Write-Host "The game exited (code $LASTEXITCODE). Log: $log"
