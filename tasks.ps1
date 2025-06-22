# Define variables
$action = New-ScheduledTaskAction -Execute "python.exe" -Argument "C:\path\to\your\script.py"
$trigger = New-ScheduledTaskTrigger -Daily -At "09:00AM"
$principal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -LogonType ServiceAccount
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable

# Register the scheduled task
Register-ScheduledTask -Action $action -Trigger $trigger -Principal $principal -Settings $settings -TaskName "MyPythonScriptTask" -Description "Runs my Python script daily at 9 AM"