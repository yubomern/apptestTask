🧰 Add to Windows Task Scheduler

Steps:

1. Open Task Scheduler


2. Create Task → Trigger: Daily/Hourly


3. Action:

Program: powershell.exe

Args: -ExecutionPolicy Bypass -File "C:\Path\To\schedule_runner.ps1"




Now your Python scripts will run based on config time.


## 🚀 How to Run

### 1. Configure `config.json`
```json
[
  { "path": "C:\\YourScripts\\main.py", "time": "09:00" }
]

2. Run Once

powershell -ExecutionPolicy Bypass -File .\runner.ps1

3. Auto Schedule in Windows

powershell -ExecutionPolicy Bypass -File .\setup_task.ps1

4. Docker (for testing)

docker build -t tkinter-scheduler .
docker run --rm tkinter-scheduler

🛠 Tech Stack

PowerShell

Python 3 (Tkinter)

Docker (xvfb)

Jenkins

GitHub Actions


📝 Logs

All errors saved to error_log.txt.

📂 Branch Strategy

test: Dev testing

prod: Production-ready

