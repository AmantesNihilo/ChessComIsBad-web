param(
    [int]$BackendPort = 8000,
    [int]$FrontendPort = 5173,
    [switch]$NoOpen
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$FrontendDir = Join-Path $Root "frontend"
$BackendReq = Join-Path $Root "backend\requirements.txt"
$EngineExe = Join-Path $Root "engine\bin\chessviz.exe"
$LogDir = Join-Path $Root ".logs"

function Test-Command($Name) {
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-Step($Title, [scriptblock]$Body) {
    Write-Host "[TRPO] $Title" -ForegroundColor Cyan
    & $Body
}

function Wait-Http($Url, $Name) {
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        try {
            $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
            if ($response.StatusCode -ge 200 -and $response.StatusCode -lt 500) {
                Write-Host "[TRPO] $Name ready: $Url" -ForegroundColor Green
                return
            }
        } catch {
            Start-Sleep -Milliseconds 500
        }
    }
    throw "$Name did not start in time: $Url"
}

function Test-Http($Url) {
    try {
        $response = Invoke-WebRequest -Uri $Url -UseBasicParsing -TimeoutSec 2
        return $response.StatusCode -ge 200 -and $response.StatusCode -lt 500
    } catch {
        return $false
    }
}

function ConvertTo-ProcessArgument([string]$Argument) {
    if ($null -eq $Argument) {
        return '""'
    }
    if ($Argument -notmatch '[\s"]') {
        return $Argument
    }

    $builder = New-Object System.Text.StringBuilder
    [void]$builder.Append('"')
    $backslashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq '\') {
            $backslashes += 1
        } elseif ($character -eq '"') {
            [void]$builder.Append('\' * (($backslashes * 2) + 1))
            [void]$builder.Append('"')
            $backslashes = 0
        } else {
            if ($backslashes -gt 0) {
                [void]$builder.Append('\' * $backslashes)
                $backslashes = 0
            }
            [void]$builder.Append($character)
        }
    }
    if ($backslashes -gt 0) {
        [void]$builder.Append('\' * ($backslashes * 2))
    }
    [void]$builder.Append('"')
    return $builder.ToString()
}

function Stop-ProcessTree($Process) {
    if (-not $Process -or $Process.HasExited) {
        return
    }

    try {
        $children = Get-CimInstance Win32_Process -Filter "ParentProcessId = $($Process.Id)" -ErrorAction SilentlyContinue
        foreach ($child in $children) {
            try {
                $childProcess = Get-Process -Id $child.ProcessId -ErrorAction Stop
                Stop-ProcessTree $childProcess
            } catch {
                Write-Host "[TRPO] Could not stop child process $($child.ProcessId): $($_.Exception.Message)" -ForegroundColor DarkYellow
            }
        }
    } catch {
        Write-Host "[TRPO] Could not inspect child processes for $($Process.Id): $($_.Exception.Message)" -ForegroundColor DarkYellow
    }

    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force -ErrorAction Stop
        [void]$Process.WaitForExit(3000)
    }
}

function Start-LoggedProcess($FileName, [string[]]$Arguments, $WorkingDirectory, $OutLog, $ErrLog) {
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FileName
    if ($null -ne $startInfo.ArgumentList) {
        foreach ($argument in $Arguments) {
            [void]$startInfo.ArgumentList.Add($argument)
        }
    } else {
        $startInfo.Arguments = ($Arguments | ForEach-Object { ConvertTo-ProcessArgument $_ }) -join " "
    }
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()

    Register-ObjectEvent -InputObject $process -EventName OutputDataReceived -Action {
        if ($EventArgs.Data) { Add-Content -LiteralPath $Event.MessageData -Value $EventArgs.Data }
    } -MessageData $OutLog | Out-Null
    Register-ObjectEvent -InputObject $process -EventName ErrorDataReceived -Action {
        if ($EventArgs.Data) { Add-Content -LiteralPath $Event.MessageData -Value $EventArgs.Data }
    } -MessageData $ErrLog | Out-Null
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()

    return $process
}

if (-not (Test-Command "python")) {
    throw "Python was not found in PATH"
}
if (-not (Test-Command "node")) {
    throw "Node.js was not found in PATH"
}
if (-not (Test-Command "npm.cmd")) {
    throw "npm.cmd was not found in PATH"
}

$PythonCommand = (Get-Command "python").Source
$NpmCommand = (Get-Command "npm.cmd").Source

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

Invoke-Step "Checking Python backend dependencies" {
    $check = & $PythonCommand -c "import fastapi, uvicorn, pydantic" 2>$null
    if ($LASTEXITCODE -ne 0) {
        & $PythonCommand -m ensurepip --upgrade
        & $PythonCommand -m pip install -r $BackendReq
    }
}

Invoke-Step "Checking frontend dependencies" {
    if (-not (Test-Path (Join-Path $FrontendDir "node_modules"))) {
        Push-Location $FrontendDir
        try {
            & $NpmCommand install
        } finally {
            Pop-Location
        }
    }
}

Invoke-Step "Checking C engine" {
    if (-not (Test-Path $EngineExe)) {
        if (-not (Test-Command "make")) {
            throw "Engine executable is missing and make was not found in PATH"
        }
        & make -C (Join-Path $Root "engine")
    }
}

$backendOut = Join-Path $LogDir "backend.out.log"
$backendErr = Join-Path $LogDir "backend.err.log"
$frontendOut = Join-Path $LogDir "frontend.out.log"
$frontendErr = Join-Path $LogDir "frontend.err.log"
Remove-Item -LiteralPath $backendOut, $backendErr, $frontendOut, $frontendErr -Force -ErrorAction SilentlyContinue

$backend = $null
$frontend = $null

try {
    if (Test-Http "http://127.0.0.1:$BackendPort/api/health") {
        Write-Host "[TRPO] Backend already running on port $BackendPort" -ForegroundColor Green
    } else {
        Invoke-Step "Starting backend on port $BackendPort" {
            $script:backend = Start-LoggedProcess $PythonCommand @(
                "-m", "uvicorn", "backend.app.main:app",
                "--host", "127.0.0.1",
                "--port", "$BackendPort"
            ) $Root $backendOut $backendErr
        }
    }

    Wait-Http "http://127.0.0.1:$BackendPort/api/health" "Backend"

    if (Test-Http "http://127.0.0.1:$FrontendPort") {
        Write-Host "[TRPO] Frontend already running on port $FrontendPort" -ForegroundColor Green
    } else {
        Invoke-Step "Starting frontend on port $FrontendPort" {
            $script:frontend = Start-LoggedProcess $NpmCommand @(
                "run", "dev", "--", "--host", "127.0.0.1", "--port", "$FrontendPort"
            ) $FrontendDir $frontendOut $frontendErr
        }
    }

    Wait-Http "http://127.0.0.1:$FrontendPort" "Frontend"

    $url = "http://127.0.0.1:$FrontendPort"
    Write-Host ""
    Write-Host "[TRPO] App is running: $url" -ForegroundColor Green
    Write-Host "[TRPO] Logs: $LogDir"
    Write-Host "[TRPO] Press Ctrl+C to stop backend and frontend."
    Write-Host ""

    if (-not $NoOpen) {
        Start-Process $url
    }

    while ($true) {
        if ($backend -and $backend.HasExited) {
            throw "Backend stopped unexpectedly. See $backendErr"
        }
        if ($frontend -and $frontend.HasExited) {
            throw "Frontend stopped unexpectedly. See $frontendErr"
        }
        Start-Sleep -Seconds 1
    }
} finally {
    Write-Host ""
    Write-Host "[TRPO] Stopping services..." -ForegroundColor Yellow
    foreach ($process in @($frontend, $backend)) {
        if ($process -and -not $process.HasExited) {
            try {
                Stop-ProcessTree $process
            } catch {
                Write-Host "[TRPO] Could not stop process $($process.Id): $($_.Exception.Message)" -ForegroundColor DarkYellow
            }
        }
    }
}
