$ErrorActionPreference = "Continue"
$configPath = ".\config.json"
$errorLog = ".\error_log.txt"

# Parse JSON configuration
$config = Get-Content $configPath | ConvertFrom-Json

function Log-Error {
    param ($msg)
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Add-Content -Path $errorLog -Value "$timestamp - $msg"
}

foreach ($entry in $config) {
    $scheduledTime = Get-Date -Format "HH:mm"
    if ($entry.time -eq $scheduledTime) {
        try {
            Write-Host "Launching: $($entry.path)"
            & python "$($entry.path)"
        }
        catch {
            Log-Error "Failed: $($entry.path) - $_"
        }
    }
}