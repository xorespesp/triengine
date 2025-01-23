param(
    [Parameter(Mandatory=$true)]
    [string] $projectBinarySubPath,

    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]] $launchArguments = @() # Optional
)

function PauseConsole {
    param(
        [string] $message
    )
    Write-Host $message
    [Console]::ReadKey($true) | Out-Null
}

function PauseConsoleTimeout {
    param(
        [int] $waitSeconds = 5
    )

    Write-Host "Press [P] to pause, [C] to continue. [C] will be selected after $waitSeconds secs:"

    $stopwatch = [Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.Elapsed.TotalSeconds -lt $waitSeconds) {
        # Check if a key is available
        if ([Console]::KeyAvailable) {
            # Read the key without echoing to the console
            $key = ([Console]::ReadKey($true).KeyChar).ToString().ToUpper()
            if ($key -eq 'P') {
                PauseConsole -message "Press any key to continue.."
                return
            }
            elseif ($key -eq 'C') {
                return
            }
            else {
                # If some other key, ignore and keep waiting
                # (do not reset time, just continue waiting until we get P/C or time runs out)
            }
        }
        Start-Sleep -Milliseconds 100
    }

    # If we reach here, time has expired without a valid key press ('P' or 'C'),
    # so default to 'C'.
    return
}

try {
    # Move to the directory where the script is located.
    Set-Location -LiteralPath $PSScriptRoot

    $projectBinaryFullPath = Join-Path $PSScriptRoot "..\bin\$projectBinarySubPath"
    $launchArguments = $launchArguments | Where-Object {
        $_ -ne $null -and $_.Trim() -ne '' # trim null or empty strings
    }

    Write-Host "Executable Path  : `"$projectBinaryFullPath`""
    Write-Host "Launch Arguments : $($launchArguments -join ' ')"
    Write-Host ('=' * 80)
    Write-Host ""

    try {
        if ($launchArguments.Count -eq 0) {
            # Start process without extra arguments
            $process = Start-Process -FilePath $projectBinaryFullPath `
                                    -NoNewWindow `
                                    -PassThru `
                                    -Wait
        } else {
            # Start process with extra arguments
            $process = Start-Process -FilePath $projectBinaryFullPath `
                                    -ArgumentList $launchArguments `
                                    -NoNewWindow `
                                    -PassThru `
                                    -Wait
        }
        Write-Host ""
        Write-Host ('=' * 80)
        if ($process.ExitCode -eq 0) {
            Write-Host "Program exited successfully!"
            PauseConsoleTimeout -waitSeconds 5
        } else {
            Write-Host "Program terminated unexpectedly. (code: $exitCode)"
            PauseConsole -message "Press any key to quit.."
        }
        exit $process.ExitCode
    } catch {
        Write-Host "Failed to start process: $($_.Exception.Message)"
    }
} catch {
    Write-Host "Exception: $($_.Exception.Message)"
}

Write-Host ""
Write-Host ('=' * 80)
PauseConsole -message "Press any key to quit.."
exit 1