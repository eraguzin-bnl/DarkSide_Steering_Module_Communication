# -*- coding: utf-8 -*-
"""
@author: Eraguzin
This is a GUI designed to communicate with board IO-1733-1 Rev A, with a HV5523 -> HV3418 SPI control chain
Through an LTC6820 pair of chips to minimize the lines on the cable.  This script uses an FT2323H board:
https://www.adafruit.com/product/2264
To generate the SPI signals for the warm side LTC6820
This requirees the FT232H "libusb" driver to be installed, as well as the Python module "libftdi" as described here:
https://learn.adafruit.com/adafruit-ft232h-breakout/mpsse-setup
"""
import tkinter as tk
import configparser
import os
import sys
import ctypes
from ctypes import c_bool
    
#This is a Tkinter class that must be initialized and run
class GUI_WINDOW(tk.Frame):
    def __init__(self, master=None):
        print("Start")
        self.master = master
        self.config = configparser.ConfigParser()
        #Assume that the settings file is in the same directory
        current_dir = os.path.dirname(os.path.abspath(__file__))
        file_path = os.path.join(current_dir, "settings.ini")
        self.config.read(file_path, encoding='utf-8')
        self.set_up_gui()
        print("GUI set up")
        self.set_up_FT_chip()
        print("FT Chip set up")
        
    def set_up_FT_chip(self):
        # Configure digital inputs and outputs using the setup function.
        # Note that pin numbers 0 to 15 map to pins D0 to D7 then C0 to C7 on the board.
        self.CS_pin = int(self.config["DEFAULT"]["CS"])
        self.CK_pin = int(self.config["DEFAULT"]["SCK"])
        self.DOUT_pin = int(self.config["DEFAULT"]["MOSI"])
        self.DIN_pin = int(self.config["DEFAULT"]["MISO"])
        self.directory = self.config["DEFAULT"]["DIRECTORY"]
        self.DLL_LOCATION = self.config["DEFAULT"]["DLL_LOCATION"]
            
        self.write_array = [None]*96
        self.readback_array = [None]*96
        #Defaults to all channels being off (for HV5523, a "1" is off)
        for i in range(96):
            if (i < 64):
                self.write_array[i] = False
            else:
                self.write_array[i] = True
        self.write_SPI()
        
    def write_SPI(self):
        
        #Load the DLL and hand it back to the main thread
        mydll = ctypes.cdll.LoadLibrary(self.DLL_LOCATION)
        
        #Name of the main function in the DLL
        testFunction = mydll.SPI_Sequence
        
        #There must be a cleaner way to do this, but Ctypes is pretty clunky
        array = (c_bool * 96)(self.write_array[0],self.write_array[1],self.write_array[2],self.write_array[3],
                                self.write_array[4],self.write_array[5],self.write_array[6],self.write_array[7],
                                self.write_array[8],self.write_array[9],self.write_array[10],self.write_array[11],
                                self.write_array[12],self.write_array[13],self.write_array[14],self.write_array[15],
                                self.write_array[16],self.write_array[17],self.write_array[18],self.write_array[19],
                                self.write_array[20],self.write_array[21],self.write_array[22],self.write_array[23],
                                self.write_array[24],self.write_array[25],self.write_array[26],self.write_array[27],
                                self.write_array[28],self.write_array[29],self.write_array[30],self.write_array[31],
                                self.write_array[32],self.write_array[33],self.write_array[34],self.write_array[35],
                                self.write_array[36],self.write_array[37],self.write_array[38],self.write_array[39],
                                self.write_array[40],self.write_array[41],self.write_array[42],self.write_array[43],
                                self.write_array[44],self.write_array[45],self.write_array[46],self.write_array[47],
                                self.write_array[48],self.write_array[49],self.write_array[50],self.write_array[51],
                                self.write_array[52],self.write_array[53],self.write_array[54],self.write_array[55],
                                self.write_array[56],self.write_array[57],self.write_array[58],self.write_array[59],
                                self.write_array[60],self.write_array[61],self.write_array[62],self.write_array[63],
                                self.write_array[64],self.write_array[65],self.write_array[66],self.write_array[67],
                                self.write_array[68],self.write_array[69],self.write_array[70],self.write_array[71],
                                self.write_array[72],self.write_array[73],self.write_array[74],self.write_array[75],
                                self.write_array[76],self.write_array[77],self.write_array[78],self.write_array[79],
                                self.write_array[80],self.write_array[81],self.write_array[82],self.write_array[83],
                                self.write_array[84],self.write_array[85],self.write_array[86],self.write_array[87],
                                self.write_array[88],self.write_array[89],self.write_array[90],self.write_array[91],
                                self.write_array[92],self.write_array[93],self.write_array[94],self.write_array[95])

        #Text being passed in needs to be in unicode wchar format 
        #(caused me a lot of grief, can't use ctypes.create_string_buffer)
        debug_directory = ctypes.create_unicode_buffer(self.directory)
        
        #What we expect as a return value, a 96 bit boolean array
        testFunction.restype = ctypes.POINTER(ctypes.c_bool * 96)
        
        #Calls the DLL and passes all these arguments
        result = testFunction(self.CS_pin,
                              self.CK_pin,
                              self.DOUT_pin,
                              self.DIN_pin,
                              array,
                              debug_directory
                              )
        
        #Turn that return array into a Python boolean array
        for num,i in enumerate(result.contents):
            self.readback_array[num] = bool(i)
            
#        print(self.readback_array)
#        print(self.readback_array == self.write_array)
        self.fill_readback_circles(self.adjust_return_array(self.readback_array))
        
        #The DLL has to be released from memory in a convoluted way becaues it's for 64-bit systems
        libHandle = mydll._handle
        del mydll
        ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.wintypes.HMODULE]
        ctypes.windll.kernel32.FreeLibrary(libHandle)
        self.update_status("Wrote to SPI", "black")
        #print ("Done")
            
    #Takes the raw readback and makes it GUI-friendly 
    #There are 96 bits, but the GUI only cares about 50 of them, and they're not all consecutive
    def adjust_return_array(self, array):
        final_array = [None]*50
        for num,i in enumerate(range(95,70,-1)):
            final_array[num] = array[i]
            
        for num,i in enumerate(range(63,38,-1)):
            final_array[num+25] = array[i]
            
#        print(final_array)
        return(final_array)
        
    #Finds the associated circle on the GUI for each readback bit and colors it
    def fill_readback_circles(self, array):
        for i in range(25):
            value = array[i]
            bubble = self.w.find_withtag("rb0-{}".format(i))
            if (value == True):
                self.w.itemconfig(bubble, fill=self.config["DEFAULT"]["READBACK_OFF"])
            elif (value == False):
                self.w.itemconfig(bubble, fill=self.config["DEFAULT"]["READBACK_ON"])
            else:
                self.update_status("ERROR: Something wrong with the readback truth array", "red") 
                
        for i in range(25, 50, 1):
            value = array[i]
            bubble = self.w.find_withtag("rb1-{}".format(i-25))
            if (value == True):
                self.w.itemconfig(bubble, fill=self.config["DEFAULT"]["READBACK_ON"])
            elif (value == False):
                self.w.itemconfig(bubble, fill=self.config["DEFAULT"]["READBACK_OFF"])
            else:
                self.update_status("ERROR: Something wrong with the readback truth array", "red") 

    #This changes the master array of what to send over SPI.  Called when a button is pushed
    def change_channel_val(self, hvlv, chn, val):
        if (chn < 1 or chn > 25):
            self.update_status("Error: LV channel must be between 1 and 25, you tried {}".format(chn), "red")
            return
        
        if (hvlv == "lv"):
            index = 96-chn
        elif (hvlv == "hv"):
            index = 64-chn
        else:
            self.update_status("Error: Must tell function whether you're talking about HV or LV channels, you said {}".format(hvlv), "red")
            
        if (val == True):
            self.write_array[index] = True
        elif (val == False):
            self.write_array[index] = False
        else:
            self.update_status("Error: Must tell function the new value as either True or False, you said {}".format(val), "red")
    
    #Designs the structure of the GUI
    def set_up_gui(self):
        #Master object is a frame that encompasses the whole window
        tk.Frame.__init__(self,self.master, padx = self.config["DEFAULT"]["PADX"], pady = self.config["DEFAULT"]["PADY"])
        self.master.title(self.config["DEFAULT"]["TITLE"])
        self.pack()
        
        label = tk.Label(self, text="Channel Selector")
        label.grid(row=0,column=0, columnspan=25)

        #Iteratively creates the buttons that you can push, row 0 for LV and row 1 for HV
        #When you click it, it will send its name through to the gui_callback command
        #Lambda statement is the only way to send in arguments through called command
        for i in range(2):
            for j in range(1,26,1):
                button = tk.Button(self, name = "{}-{}".format(i,j), text ="Ch {}".format(j), width = self.config["DEFAULT"]["BUTTON_WIDTH"], 
                                   bg = self.config["DEFAULT"]["BUTTON_OFF"], fg = self.config["DEFAULT"]["BUTTON_TEXT"],
                                   command = lambda arg="{}-{}".format(i,j): self.gui_callback(arg))
                button.grid(row = i+2, column = j+1)
                
        #Functions to change all of a type
        button = tk.Button(self, name = "1-on", text ="All HV ON", 
                               bg = self.config["DEFAULT"]["BUTTON_ON"], fg = self.config["DEFAULT"]["BUTTON_TEXT"],
                               command = lambda arg="1-on": self.gui_callback(arg))
        button.grid(row = 0, column = 1, columnspan = 5, pady = 10)
        
        button = tk.Button(self, name = "1-off", text ="All HV OFF", 
                               bg = self.config["DEFAULT"]["BUTTON_OFF"], fg = self.config["DEFAULT"]["BUTTON_TEXT"],
                               command = lambda arg="1-off": self.gui_callback(arg))
        button.grid(row = 0, column = 7, columnspan = 5, pady = 10)
        
        button = tk.Button(self, name = "0-on", text ="All LV ON", 
                               bg = self.config["DEFAULT"]["BUTTON_ON"], fg = self.config["DEFAULT"]["BUTTON_TEXT"],
                               command = lambda arg="0-on": self.gui_callback(arg))
        button.grid(row = 0, column = 13, columnspan = 5, pady = 10)
        
        button = tk.Button(self, name = "0-off", text ="All LV OFF", 
                               bg = self.config["DEFAULT"]["BUTTON_OFF"], fg = self.config["DEFAULT"]["BUTTON_TEXT"],
                               command = lambda arg="0-off": self.gui_callback(arg))
        button.grid(row = 0, column = 19, columnspan = 5, pady = 10)
            
        #Text GUI widgets
        label = tk.Label(self, text="Low Voltage")
        label.grid(row=2,column=0, columnspan=1)
        
        label = tk.Label(self, text="High Voltage")
        label.grid(row=3,column=0, columnspan=1)
        
        self.status_label = tk.Label(self, text="Status")
        self.status_label.grid(row=4,column=0, columnspan=25)
        
        label = tk.Label(self, text="Readback")
        label.grid(row=5,column=0, columnspan=25)
        
        #Determine how to space the Readback circles so that they line up with the buttons
        #Found empirically, there has to be a better way of doing this
        #But the top part is much easier to make as a grid, and you need the bottom part to be a canvas to draw colors
        width = float(self.config["DEFAULT"]["BUTTON_WIDTH"]) * 230
        x_spacing = float(self.config["DEFAULT"]["BUTTON_WIDTH"]) * 9.2
        x_pad = float(self.config["DEFAULT"]["BUTTON_WIDTH"]) * 3
        y_spacing = float(self.config["DEFAULT"]["READBACK_BUTTON_SPACING_Y"])
        y_pad = float(self.config["DEFAULT"]["READBACK_PADDING_Y"])
        circle_diameter = float(self.config["DEFAULT"]["READBACK_BUTTON_DIAMETER"])
        
        #Create a "canvas" object as the last row, because that's the only GUI widget you can draw shapes on
        self.w = tk.Canvas(self, width=width, height=85)
        self.w.grid(row = 6, column = 1, columnspan = 26)
        
        #Each shape is given a unique tag so it can be found and updated
        for i in range(2):
            for j in range(25):
                self.w.create_oval((j*x_spacing)+x_pad,
                              (i*y_spacing)+y_pad,
                              (j*x_spacing)+x_pad+circle_diameter,
                              (i*y_spacing)+y_pad+circle_diameter, 
                              tags=("rb{}-{}".format(i,j)),
                              dash = (1,1))
        
    #Whenever a button is pushed, this function is called with the buttons ID tag as an argument
    def gui_callback(self,*args,**kwargs):
        name = args[0]
        button = self.nametowidget(name)
        row = int(name[0])
        cmd = (name[2:])
        
        #Made this way so that the loop can apply to individual channels being pushed and the "ALL" buttons
        if (cmd == "on"):
            state = False
            if (row == 0):
                chn = range(1,33,1)
            elif (row == 1):
                chn = range(1,65,1)
        elif (cmd == "off"):
            state = True
            if (row == 0):
                chn = range(1,33,1)
            elif (row == 1):
                chn = range(1,65,1)
        else:  
            chn = [int(cmd)]
            state = self.button_status(button)
            
        for i in chn:
            if (state == True):
                new_color = self.config["DEFAULT"]["BUTTON_OFF"]
                if (row == 1):
                    name = "{}-{}".format(row,i)
                    try:
                        button = self.nametowidget(name)
                        button["bg"] = new_color
                    except KeyError:
                        pass
                    self.change_channel_val("hv", i, False)
                elif (row == 0):
                    name = "{}-{}".format(row,i)
                    try:
                        button = self.nametowidget(name)
                        button["bg"] = new_color
                    except KeyError:
                        pass
                    self.change_channel_val("lv", i, True)
                else:
                    self.update_status("ERROR: Button name was set up incorrectly", "red")
                    
            elif (state == False):
                new_color = self.config["DEFAULT"]["BUTTON_ON"]
                if (row == 1):
                    name = "{}-{}".format(row,i)
                    try:
                        button = self.nametowidget(name)
                        button["bg"] = new_color
                    except KeyError:
                        pass
                    self.change_channel_val("hv", i, True)
                elif (row == 0):
                    name = "{}-{}".format(row,i)
                    try:
                        button = self.nametowidget(name)
                        button["bg"] = new_color
                    except KeyError:
                        pass
                    self.change_channel_val("lv", i, False)
                else:
                    self.update_status("ERROR: Button name was set up incorrectly", "red")
                
        self.update_status("Writing to SPI...", "magenta")
        self.write_SPI()
        
    #Change the status label according to incoming text and chosen color and force it to update
    def update_status(self, text, color):
        self.status_label["text"] = text
        self.status_label["fg"] = color
        self.master.update_idletasks()
        
    #Take a GUI widget object and determine it's state based on its color
    def button_status(self, button):
        color = button["bg"]
        if (color == self.config["DEFAULT"]["BUTTON_ON"]):
            state = True
        elif (color == self.config["DEFAULT"]["BUTTON_OFF"]):
            state = False
        else:
            state = None
        return state
        
#Create GUI object
def main():
    root = tk.Tk()
    root.resizable(0, 0) #Don't allow resizing in the x or y direction
    GUI_WINDOW(root)
    root.mainloop()
    
if __name__ == "__main__":
    main()
