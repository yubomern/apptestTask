import tkinter as tk
from tkinter import messagebox
from datetime import datetime

def show_dialog():
    root = tk.Tk()
    root.withdraw() # Hide the main window
    messagebox.showinfo("Script Running", f"Hello!\nScript started at {datetime.now()}")
    root.destroy()

try:
    show_dialog()
    print("Script ran successfully.")
except Exception as e:
    print("Error:", e)
    raise

