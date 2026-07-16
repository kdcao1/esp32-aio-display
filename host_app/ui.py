import customtkinter as ctk
from tkinter import filedialog
import os

class SettingsUI(ctk.CTk):
    def __init__(self, comms_instance):
        super().__init__()
        self.comms = comms_instance
        
        self.title("Cooler Screen Settings")
        self.geometry("400x300")
        self.resizable(False, False)
        
        # Configure grid
        self.grid_columnconfigure(0, weight=1)
        
        # Title Label
        self.title_label = ctk.CTkLabel(self, text="Screen Mode Settings", font=ctk.CTkFont(size=20, weight="bold"))
        self.title_label.grid(row=0, column=0, padx=20, pady=(20, 10))
        
        # Mode selection
        self.mode_var = ctk.StringVar(value="TEMP")
        
        self.radio_temp = ctk.CTkRadioButton(self, text="Temperature Mode", variable=self.mode_var, value="TEMP", command=self.mode_changed)
        self.radio_temp.grid(row=1, column=0, padx=20, pady=10, sticky="w")
        
        self.radio_gif = ctk.CTkRadioButton(self, text="GIF Mode", variable=self.mode_var, value="GIF", command=self.mode_changed)
        self.radio_gif.grid(row=2, column=0, padx=20, pady=10, sticky="w")
        
        # GIF Upload Button
        self.btn_upload = ctk.CTkButton(self, text="Upload GIF", command=self.upload_gif, state="disabled")
        self.btn_upload.grid(row=3, column=0, padx=20, pady=20)
        
        # Status Label
        self.status_label = ctk.CTkLabel(self, text="Connected" if self.comms.connected else "Not Connected", text_color="green" if self.comms.connected else "red")
        self.status_label.grid(row=4, column=0, padx=20, pady=10)

    def mode_changed(self):
        mode = self.mode_var.get()
        if mode == "TEMP":
            self.btn_upload.configure(state="disabled")
            self.comms.set_temp_mode()
        else:
            self.btn_upload.configure(state="normal")
            self.comms.set_gif_mode()

    def upload_gif(self):
        file_path = filedialog.askopenfilename(
            title="Select a GIF file",
            filetypes=[("GIF Files", "*.gif")]
        )
        if file_path:
            self.status_label.configure(text=f"Uploading {os.path.basename(file_path)}...", text_color="orange")
            self.update() # Force update UI
            success = self.comms.upload_gif(file_path)
            if success:
                self.status_label.configure(text="Upload Complete", text_color="green")
            else:
                self.status_label.configure(text="Upload Failed", text_color="red")

def show_settings(comms_instance):
    ctk.set_appearance_mode("dark")
    ctk.set_default_color_theme("blue")
    app = SettingsUI(comms_instance)
    app.mainloop()

if __name__ == "__main__":
    from comms import ESP32Comms
    dummy_comms = ESP32Comms()
    show_settings(dummy_comms)
