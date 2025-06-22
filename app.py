# Python3 script to display
# random image quotes from a folder

# Importing modules
from tkinter import *
from PIL import ImageTk, Image
import os
import random

# Getting the file name of
# random selected image
name = random.choice(os.listdir(
    "C:\\Users\\NEERAJ RANA\\Desktop\\quotes_folder\\"))

# Appending the rest of the path
# to the filename
file = "C:\\Users\\NEERAJ RANA\\Desktop\\quotes_folder\\" + name

# Displaying the image
root = Tk()
canvas = Canvas(root, width=1300, height=750)
canvas.pack()
img = ImageTk.PhotoImage(Image.open(file))
canvas.create_image(20, 20, anchor=NW, image=img)
root.mainloop()