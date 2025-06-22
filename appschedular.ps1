$action = New-ScheduledTaskAction -Execute "C:\Program Files\Python\Python38\python.exe" -Argument "C:\Scripts\mypScript.py"
$trigger = New-ScheduledTaskTrigger -Daily -At 5:00am
$trigger.StartBoundary = [DateTime]::Parse($trigger.StartBoundary).ToLocalTime().ToString("s")
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit 0

Register-ScheduledTask -Action $action -Trigger $trigger -Settings $settings -TaskName "Full Computer Backup" -Description "Backs up computer"