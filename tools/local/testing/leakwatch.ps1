# 2026-11-05 (port-gaps 67, the P09 crash; snatcherLeo: "check for CPU and RAM leaks and stuff like that too");
# moved from the session's scratch folder into the repo 2026-11-06.
# Samples one running orrecolo every N seconds: its own dedicated GPU memory (the Windows "GPU Process Memory"
# counter), the whole card's use (nvidia-smi, -1 when it isn't there), RAM (private bytes, working set), CPU seconds,
# handles and threads. Read-only: it never touches the game. Judge the result with leakcheck.py.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\local\testing\leakwatch.ps1 <out file> [interval s] [max s] [pid]
# Without a pid it waits up to 120 s for the first orrecolo to start and follows that one.
param([string]$Out, [int]$Every = 10, [int]$Max = 3000, [int]$TargetPid = 0)
"# t_s gpu_proc_MiB gpu_card_MiB private_MiB workingset_MiB cpu_s handles threads" | Out-File -Encoding ascii $Out
$p = $null
if ($TargetPid -ne 0) {
    $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
} else {
    $t = 0
    while ($t -lt 120 -and -not (Get-Process orrecolo -ErrorAction SilentlyContinue)) { Start-Sleep 5; $t += 5 }
    $p = Get-Process orrecolo -ErrorAction SilentlyContinue | Select-Object -First 1
}
if ($p -eq $null) { "# no orrecolo found" | Out-File -Append -Encoding ascii $Out; exit 1 }
$procId = $p.Id
"# pid $procId" | Out-File -Append -Encoding ascii $Out
$haveSmi = [bool](Get-Command nvidia-smi -ErrorAction SilentlyContinue)
$t = 0
while ($t -lt $Max) {
    $p = Get-Process -Id $procId -ErrorAction SilentlyContinue
    if ($p -eq $null) { break }
    $gpu = 0
    try {
        $c = Get-Counter "\GPU Process Memory(pid_${procId}_*)\Dedicated Usage" -ErrorAction Stop
        $gpu = ($c.CounterSamples | Measure-Object -Property CookedValue -Sum).Sum / 1MB
    } catch { $gpu = -1 }
    $card = -1
    if ($haveSmi) {
        try { $card = ((& nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits) | Select-Object -First 1).Trim() }
        catch { $card = -1 }
    }
    "{0} {1:F0} {2} {3:F0} {4:F0} {5:F1} {6} {7}" -f $t, $gpu, $card, ($p.PrivateMemorySize64 / 1MB),
        ($p.WorkingSet64 / 1MB), $p.TotalProcessorTime.TotalSeconds, $p.HandleCount, $p.Threads.Count |
        Out-File -Append -Encoding ascii $Out
    Start-Sleep $Every
    $t += $Every
}
"# done at $t s" | Out-File -Append -Encoding ascii $Out
