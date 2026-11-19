# CLR 2-compatible readers drain both pipes while the controller is running.
# This prevents a completed long regression from blocking on a full stdout pipe.
if(-not ('VT7PtyInputCapture' -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Threading;
public sealed class VT7PtyInputCapture {
    private readonly StringBuilder buffer = new StringBuilder();
    private readonly object gate = new object();
    private readonly Thread worker;
    public bool Truncated;
    public string Error = "";
    public VT7PtyInputCapture(StreamReader reader) {
        worker = new Thread(new ThreadStart(delegate {
            try {
                char[] chunk = new char[4096];
                int count;
                while ((count = reader.Read(chunk, 0, chunk.Length)) != 0) {
                    lock(gate) {
                        int available = 4194304 - buffer.Length;
                        if (count > available) Truncated = true;
                        if (available > 0) buffer.Append(chunk, 0, Math.Min(count, available));
                    }
                }
            } catch(Exception error) { Error = error.Message; }
        }));
        worker.IsBackground = true;
        worker.Start();
    }
    public bool Finish(int milliseconds) { return worker.Join(milliseconds); }
    public string Text { get { lock(gate) { return buffer.ToString(); } } }
}
"@
}

function Invoke-InputProcess([string]$Executable,[string]$Arguments,[string]$WorkingDirectory,[int]$TimeoutMs,[hashtable]$Environment) {
    $taskStart=New-Object Diagnostics.ProcessStartInfo
    $taskStart.FileName=$Executable;$taskStart.Arguments=$Arguments;$taskStart.WorkingDirectory=$WorkingDirectory
    $taskStart.UseShellExecute=$false;$taskStart.CreateNoWindow=$true;$taskStart.RedirectStandardOutput=$true;$taskStart.RedirectStandardError=$true
    foreach($taskKey in $Environment.Keys){
        if([string]$Environment[$taskKey] -eq ''){$taskStart.EnvironmentVariables.Remove($taskKey)}
        else {$taskStart.EnvironmentVariables[$taskKey]=[string]$Environment[$taskKey]}
    }
    $taskProcess=New-Object Diagnostics.Process;$taskProcess.StartInfo=$taskStart
    $taskClock=[Diagnostics.Stopwatch]::StartNew()
    try {
        if(-not $taskProcess.Start()){throw 'Cannot start the created test controller.'}
        $taskOut=New-Object VT7PtyInputCapture -ArgumentList $taskProcess.StandardOutput
        $taskErr=New-Object VT7PtyInputCapture -ArgumentList $taskProcess.StandardError
        $taskLastProgress=0
        while(-not $taskProcess.WaitForExit(1000) -and $taskClock.ElapsedMilliseconds -lt $TimeoutMs){
            if($taskClock.ElapsedMilliseconds-$taskLastProgress -ge 60000){
                Write-Host ('Input controller active: '+[int]($taskClock.ElapsedMilliseconds/60000)+' minutes')
                $taskLastProgress=$taskClock.ElapsedMilliseconds
            }
        }
        $taskTimedOut=-not $taskProcess.HasExited
        if($taskTimedOut) {
            # Only this created controller. Session-bound monitors retire its
            # owned consoles; the runner never opens or terminates a conhost.
            $taskProcess.Kill();[void]$taskProcess.WaitForExit(10000)
        }
        if(-not $taskOut.Finish(10000) -or -not $taskErr.Finish(10000)){throw 'Controller output drain deadline.'}
        if($taskOut.Truncated -or $taskErr.Truncated -or $taskOut.Error -or $taskErr.Error){throw 'Controller output was truncated or could not be collected.'}
        $taskStdout=$taskOut.Text.Trim()
        $taskStderr=$taskErr.Text.Trim()
        return @{ExitCode=$taskProcess.ExitCode;TimedOut=$taskTimedOut;StandardOutput=$taskStdout;StandardError=$taskStderr;DurationMilliseconds=$taskClock.ElapsedMilliseconds}
    } finally {$taskClock.Stop();$taskProcess.Close()}
}
