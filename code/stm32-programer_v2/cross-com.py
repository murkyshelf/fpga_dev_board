import tkinter as tk
from tkinter import messagebox
import paramiko

def append_remote():
    host = entry_host.get()
    port = entry_port.get()
    user = entry_user.get()
    password = entry_pass.get()
    remote_file = entry_file.get()
    text = text_box.get("1.0", tk.END).strip()

    if not all([host, port, user, password, remote_file, text]):
        messagebox.showerror("Error", "All fields are required!")
        return

    try:
        ssh = paramiko.SSHClient()
        ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        ssh.connect(
            hostname=host,
            port=int(port),
            username=user,
            password=password
        )

        # Better multiline-safe append
        stdin, stdout, stderr = ssh.exec_command(f"cat >> {remote_file}")
        stdin.write(text + "\n")
        stdin.channel.shutdown_write()

        error = stderr.read().decode()
        ssh.close()

        if error:
            messagebox.showerror("Remote Error", error)
        else:
            messagebox.showinfo("Success", "Text appended successfully!")

    except Exception as e:
        messagebox.showerror("Error", str(e))


root = tk.Tk()
root.title("SSH File Appender")

tk.Label(root, text="Host").grid(row=0, column=0)
entry_host = tk.Entry(root, width=40)
entry_host.grid(row=0, column=1)

tk.Label(root, text="Port").grid(row=1, column=0)
entry_port = tk.Entry(root, width=40)
entry_port.insert(0, "22")
entry_port.grid(row=1, column=1)

tk.Label(root, text="Username").grid(row=2, column=0)
entry_user = tk.Entry(root, width=40)
entry_user.grid(row=2, column=1)

tk.Label(root, text="Password").grid(row=3, column=0)
entry_pass = tk.Entry(root, width=40, show="*")
entry_pass.grid(row=3, column=1)

tk.Label(root, text="Remote File Path").grid(row=4, column=0)
entry_file = tk.Entry(root, width=40)
entry_file.grid(row=4, column=1)

tk.Label(root, text="Text to Append").grid(row=5, column=0)
text_box = tk.Text(root, height=10, width=40)
text_box.grid(row=5, column=1)

btn = tk.Button(root, text="Append via SSH", command=append_remote)
btn.grid(row=6, column=1)

root.mainloop()
