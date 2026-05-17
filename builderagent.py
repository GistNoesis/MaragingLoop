#!/usr/bin/env python3
"""
MaragingLoop: Minimal ReAct-style agent loop using NATIVE OpenAI tool calling format.
Connects to llama.cpp server at http://localhost:8080
Runs natively as current user by default, or as 'agentos' via run-agent.sh.
Delegates VirtualBox commands to the active D-Bus session via sudo delegation.
"""
import requests
import struct
import os
import json
import sys
import subprocess
import time
import base64
import signal

# ─── Configuration ───────────────────────────────────────────────
# Environment variables for deployment flexibility
VBOX_DELEGATE_USER = os.environ.get("VBOX_DELEGATE_USER", os.environ.get("USER", "root"))
VBOX_ACL_USER = os.environ.get("VBOX_ACL_USER", "agentos")
VM_NAME = os.environ.get("VBOX_VM_NAME", "agentos")

# Global flag to track interrupt state
_interrupt_flag = False

def _handle_interrupt(signum, frame):
    global _interrupt_flag
    if _interrupt_flag:
        raise KeyboardInterrupt
    _interrupt_flag = True
    print("\n⚠️  Ctrl+C detected. Finishing current step...")

signal.signal(signal.SIGINT, _handle_interrupt)

# ─── Environment & API Setup ─────────────────────────────────────
try:
    with open(".env") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"): continue
            key, value = line.split("=", 1)
            if key in os.environ:
                print(f"⚠️  {key} already set in environment. Skipping .env.")
            else:
                os.environ[key] = value
except FileNotFoundError:
    pass

COMPLETION_API_URL = os.environ.get("COMPLETION_API_URL", "http://localhost:8080/v1/chat/completions")
MODEL_NAME = os.environ.get("MODEL_NAME", "model")

# ─── VirtualBox Delegation Helper ────────────────────────────────
def run_vboxmanage(args, timeout=30):
    """Delegates VBoxManage to the configured delegate user via sudo."""
    cmd = ["sudo", "-u", VBOX_DELEGATE_USER, "VBoxManage"] + args
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)

# ─── Tool Definitions (OpenAI standard format) ──────────────────
TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "calculator",
            "description": "Perform arithmetic calculations",
            "parameters": {
                "type": "object",
                "properties": {"expression": {"type": "string", "description": "Math expression"}},
                "required": ["expression"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "write_kernel",
            "description": "Write the full content of kernel.c to the 'os' folder.",
            "parameters": {
                "type": "object",
                "properties": {"text": {"type": "string", "description": "The full content of the code"}},
                "required": ["text"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "launch_current_vm",
            "description": "Launch a headless VM, take a screenshot, and power it off.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "compile_kernel",
            "description": "Compile the kernel by running 'make' in the 'os' directory.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "compile_kernel_files",
            "description": "Compile and link multiple C files in the 'os' folder into kernel.bin, and generate the final os.iso using grub-mkrescue.",
            "parameters": {
                "type": "object",
                "properties": {
                    "filenames": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "List of C filenames to compile (e.g., ['kernel.c'])"
                    }
                },
                "required": ["filenames"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "start_vm",
            "description": "Start the VM in headless mode.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "set_mouse_position",
            "description": "Move the mouse cursor in the running VM to the specified coordinates.",
            "parameters": {
                "type": "object", 
                "properties": {
                    "mouse_pos": {
                        "type": "string", 
                        "description": "Coordinates in 'x,y' format (e.g., '500,300'). VM must be running."
                    }
                },
                "required": ["mouse_pos"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "take_screenshot",
            "description": "Capture a screenshot of the running VM. Saves as vm_snapshot_{timestamp}.png.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "stop_vm",
            "description": "Gracefully stop the VM using ACPI power button.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "send_keyboard_input",
            "description": "Send keyboard input to the running VM.",
            "parameters": {
                "type": "object",
                "properties": {
                    "key": {
                        "type": "string", 
                        "description": "Text or special key. Use 'SPACE' for space, '\\n' for newline, 'TAB' for tab, 'ENTER' for enter."
                    }
                },
                "required": ["key"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "inspect_snapshot",
            "description": "Verify a VM snapshot exists and prepare it for LLM inspection by encoding it to base64.",
            "parameters": {
                "type": "object",
                "properties": {
                    "timestamp": {
                        "type": "string", 
                        "description": "The timestamp from the screenshot filename"
                    }
                },
                "required": ["timestamp"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "take_screenshot_and_inspect",
            "description": "Take a screenshot of the running VM and immediately prepare it for LLM inspection.",
            "parameters": {"type": "object", "properties": {}, "required": []}
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_file",
            "description": "Read numberToRead characters up to 3000 characters from a file inside the 'os' folder starting at a specified character position.",
            "parameters": {
                "type": "object",
                "properties": {
                    "filename": {"type": "string", "description": "The filename relative to the 'os' folder"},
                    "start": {"type": "integer", "description": "The character position to start reading from."},
                    "numberToRead" :  {"type": "integer", "description": "The number of characters to read."}
                },
                "required": ["filename"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_reference_file",
            "description": "Read up to 3000 characters from a file inside the 'os/references' folder starting at a specified character position.",
            "parameters": {
                "type": "object",
                "properties": {
                    "filename": {"type": "string", "description": "The filename relative to the 'os/references' folder"},
                    "start": {"type": "integer", "description": "The character position to start reading from."},
                    "numberToRead": {"type": "integer", "description": "The number of characters to read."}
                },
                "required": ["filename"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "write_file",
            "description": "Write or replace the content of a .c or .h file in the 'os' folder.",
            "parameters": {
                "type": "object",
                "properties": {
                    "filename": {"type": "string", "description": "The filename (must end with .c or .h)"},
                    "text": {"type": "string", "description": "The full content of the file"}
                },
                "required": ["filename", "text"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "thinking",
            "description": "Express your reasoning before taking an action.",
            "parameters": {
                "type": "object",
                "properties": {"justification": {"type": "string", "description": "A short justification (<200 chars)"}},
                "required": ["justification"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "finish",
            "description": "Return the final answer to the user",
            "parameters": {
                "type": "object",
                "properties": {"answer": {"type": "string", "description": "Final answer"}},
                "required": ["answer"]
            }
        }
    }
]

# ─── Tool Implementations ────────────────────────────────────────

def calculator_tool(args):
    expr = args.get("expression", "")
    allowed = set("0123456789+-*/. () ")
    if not all(c in allowed for c in expr):
        raise ValueError("Unsafe expression detected.")
    return str(eval(expr))

def write_kernel_tool(args = {}):
    text = args.get("text", "")
    if not text:
        return "⚠️ Error: No text provided to write."
    file_path = "os/kernel.c"
    try:
        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        with open(file_path, "w", encoding="utf-8") as f:
            f.write(text)
        return f"✅ Successfully wrote {len(text)} characters to kernel.c \n Remember to recompile ! "
    except Exception as e:
        return f"❌ Error writing file: {e}"

def compile_kernel_tool(args):
    os.makedirs("os", exist_ok=True)
    log_output = []
    log_output.append("=== Compilation Log ===\n")
    log_output.append(f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    log_output.append("Tool: compile_kernel_tool\n")
    log_output.append("Inputs: Makefile targets\n")
    log_output.append("="*40 + "\n")
    
    try:
        kernel_bin_path = os.path.join("os", "kernel.bin")
        if os.path.exists(kernel_bin_path): os.remove(kernel_bin_path)
        kernel_o_path = os.path.join("os", "kernel.o")
        if os.path.exists(kernel_o_path): os.remove(kernel_o_path)
        multiboot_o_path = os.path.join("os", "multiboot.o")
        if os.path.exists(multiboot_o_path): os.remove(multiboot_o_path)
        
        result = subprocess.run(["make"], cwd="os", capture_output=True, text=True, timeout=180)
        combined_output = result.stdout + result.stderr
        log_output.append(combined_output)
        log_output.append("="*40 + "\n")

        if result.returncode != 0:
            return f"⚠️ Compilation failed (exit code {result.returncode}).\n{combined_output}"
            
        if len(combined_output) > 5000:
            combined_output = combined_output[:5000] + "\n... [output truncated]"
        msg = f"✅ Compilation successful.\n{combined_output}"
        
        with open("os/os.iso.compilation.log", "w", encoding="utf-8") as f:
            f.write("".join(log_output))
        return msg
    except subprocess.TimeoutExpired:
        return "❌ Compilation timed out after 180 seconds."
    except FileNotFoundError:
        return "❌ 'make' command not found. Is Make installed?"
    except Exception as e:
        return f"❌ Error during compilation: {e}"

def compile_kernel_files_tool(args):
    filenames = args.get("filenames", [])
    if not filenames:
        return "⚠️ Error: No filenames provided."
    
    os_dir = "os"
    iso_dir = "isodir"
    objects = []
    os.makedirs(os_dir, exist_ok=True)
    log_output = []
    log_output.append("=== Compilation Log ===\n")
    log_output.append(f"Timestamp: {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
    log_output.append("Tool: compile_kernel_files\n")
    log_output.append(f"Inputs: {filenames}\n")
    log_output.append("="*40 + "\n")
    
    cc_flags = ["gcc", "-m32", "-ffreestanding", "-nostdlib", "-nostartfiles", "-nodefaultlibs", "-c"]
    ld_flags = ["ld", "-m", "elf_i386", "-T", "linker.ld", "-o", "kernel.bin"]
    as_cmd = ["as", "--32"]
    
    try:
        linker_path = os.path.join(os_dir, "linker.ld")
        if not os.path.exists(linker_path):
            return f"❌ Error: linker.ld not found in '{os_dir}'."
            
        multiboot_s = os.path.join(os_dir, "multiboot.s")
        if os.path.exists(multiboot_s):
            log_output.append("> Assembling multiboot.s -> multiboot.o\n")
            res = subprocess.run(as_cmd + ["-o", "multiboot.o", "multiboot.s"], cwd=os_dir, capture_output=True, text=True)
            log_output.append(res.stdout)
            if res.stderr: log_output.append(res.stderr)
            if res.returncode != 0: return f"❌ Failed to assemble multiboot.s:\n{res.stderr}"
            objects.append("multiboot.o")
        else:
            log_output.append("> Skipping multiboot.s (not found)\n")

        for fname in filenames:
            if ".." in fname or "/" in fname or "\\" in fname:
                return f"❌ Error: Directory traversal not allowed in '{fname}'."
            src_path = os.path.join(os_dir, fname)
            if not os.path.exists(src_path):
                return f"❌ Error: File not found in 'os' folder: '{fname}'."
                
            obj_name = fname.rsplit('.', 1)[0] + ".o"
            objects.append(obj_name)
            log_output.append(f"> Compiling {fname} -> {obj_name}\n")
            res = subprocess.run(cc_flags + ["-o", obj_name, fname], cwd=os_dir, capture_output=True, text=True)
            log_output.append(res.stdout)
            if res.stderr: log_output.append(res.stderr)
            if res.returncode != 0:
                return f"❌ Compilation failed for '{fname}':\n{res.stderr}"
                
        if not objects:
            return "⚠️ No files to link."

        log_output.append(f"> Linking {' '.join(objects)} -> kernel.bin\n")
        res = subprocess.run(ld_flags + objects, cwd=os_dir, capture_output=True, text=True)
        log_output.append(res.stdout)
        if res.stderr: log_output.append(res.stderr)
        if res.returncode != 0:
            return f"❌ Linking failed:\n{res.stderr}"
            
        log_output.append("> Setting up ISO directory structure\n")
        iso_boot_grub = os.path.join(os_dir, iso_dir, "boot", "grub")
        os.makedirs(iso_boot_grub, exist_ok=True)
        subprocess.run(["cp", "kernel.bin", f"{iso_dir}/boot/kernel.bin"], cwd=os_dir, capture_output=True, text=True)
        
        log_output.append("> Creating grub.cfg\n")
        grub_content = 'set timeout=0\nset default=0\nset timeout_style=hidden\nmenuentry "myos" {\n    multiboot /boot/kernel.bin\n}\n'
        grub_cfg_path = os.path.join(iso_boot_grub, "grub.cfg")
        with open(grub_cfg_path, "w") as f:
            f.write(grub_content)
            
        log_output.append("> Building os.iso\n")
        res = subprocess.run(["grub-mkrescue", "-o", "os.iso", iso_dir], cwd=os_dir, capture_output=True, text=True)
        log_output.append(res.stdout)
        if res.stderr: log_output.append(res.stderr)
        if res.returncode != 0:
            return f"⚠️ Kernel compiled & linked successfully, but ISO creation failed.\n{res.stderr}"
            
        final_msg = "✅ Full build successful! kernel.bin and os.iso are ready inside the 'os' folder."
        log_output.append(f"> {final_msg}\n")
        log_output.append("="*40 + "\n")
        
        with open(os.path.join(os_dir, "os.iso.compilation.log"), "w", encoding="utf-8") as f:
            f.write("".join(log_output))
        return final_msg
    except FileNotFoundError as e:
        return f"❌ Command not found: {e}. Ensure gcc, ld, as, and grub-mkrescue are installed."
    except Exception as e:
        return f"❌ Build error: {e}"

def _get_vm_status(vm_name=None):
    vm_name = vm_name or VM_NAME
    try:
        status = run_vboxmanage(["showvminfo", vm_name, "--machinereadable"])
        stdout_lower = status.stdout.lower()
        if 'vmstate="running"' in stdout_lower or 'vmstate="paused"' in stdout_lower: return "running"
        if 'vmstate="locked"' in stdout_lower: return "locked"
        if 'vmstate="gurumeditation"' in stdout_lower: return "crashed"
        if 'vmstate="poweroff"' in stdout_lower: return "stopped"
        return "unknown"
    except Exception:
        return "unknown"

def start_vm_tool(args):
    try:
        current_state = _get_vm_status()
        if current_state == "running": return "⚠️ VM is already running."
        if current_state == "locked":
            run_vboxmanage(["controlvm", VM_NAME, "poweroff"])
            time.sleep(1)
            if _get_vm_status() == "locked": return "❌ VM failed to release lock."
        if current_state == "crashed":
            run_vboxmanage(["controlvm", VM_NAME, "poweroff"])
            time.sleep(1)
            if _get_vm_status() != "stopped": return "❌ VM failed to recover."
        if current_state != "stopped": return f"⚠️ VM is in unexpected state: {current_state}"

        result = run_vboxmanage(["startvm", VM_NAME, "--type", "headless"])
        if result.returncode != 0:
            return f"❌ Failed to start VM. Exit code: {result.returncode}\nSTDERR: {result.stderr.strip()}"
        time.sleep(5)
        return "✅ VM started successfully in headless mode."
    except Exception as e:
        return f"❌ Error starting VM: {e}"

def set_mouse_position_tool(args):
    mouse_pos = args.get("mouse_pos", "0,0")
    try:
        x_str, y_str = mouse_pos.split(",")
        x, y = int(x_str.strip()), int(y_str.strip())
        result = run_vboxmanage(["controlvm", VM_NAME, "mousepointer", str(x), str(y)])
        if result.returncode == 0: return f"✅ Mouse moved to ({x}, {y})."
        return f"⚠️ VBoxManage mousepointer command failed. Error: {result.stderr.strip()}"
    except ValueError:
        return "❌ Invalid mouse position format. Use 'x,y'."
    except Exception as e:
        return f"❌ Error moving mouse: {e}"

def send_keyboard_input_tool(args):
    key_input = args.get("key", "").strip()
    if not key_input: return "❌ Error: No key input provided."
    
    scancode_map = {
        "UP": ("48", "C8"), "DOWN": ("50", "D0"), "LEFT": ("4B", "CB"), "RIGHT": ("4D", "CD"),
        "ENTER": ("1C", "9C"), "TAB": ("0F", "8F"), "SPACE": ("39", "B9"), "ESC": ("01", "81"),
        "BACKSPACE": ("0E", "8E"), "DELETE": ("53", "D3"), "HOME": ("47", "C7"), "END": ("4F", "CF"),
        "PGUP": ("49", "C9"), "PGDN": ("51", "D1"), "INSERT": ("52", "D2"),
        "F1": ("3B", "BB"), "F2": ("3C", "BC"), "F3": ("3D", "BD"), "F4": ("3E", "BE"),
        "F5": ("3F", "BF"), "F6": ("40", "C0"), "F7": ("41", "C1"), "F8": ("42", "C2"),
        "F9": ("43", "C3"), "F10": ("44", "C4"), "F11": ("57", "D7"), "F12": ("58", "D8"),
    }
    
    key_upper = key_input.upper()
    if key_upper in scancode_map:
        down_code, up_code = scancode_map[key_upper]
        cmd = ["controlvm", VM_NAME, "keyboardputscancode", down_code, up_code]
    else:
        clean_input = key_input.replace("\\n", "\n").replace("\\r", "\r").replace("\\t", "\t")
        if clean_input == "\\n": clean_input = "\n"
        cmd = ["controlvm", VM_NAME, "keyboardputstring", clean_input]
        
    result = run_vboxmanage(cmd)
    if result.returncode == 0: return f"✅ Input sent successfully."
    return f"⚠️ Keyboard input failed. Error: {result.stderr.strip()}"

def take_screenshot_tool(args):
    try:
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        screenshot_path = f"screenshots/vm_snapshot_{timestamp}.png"
        os.makedirs("screenshots", exist_ok=True)

        print(f"  📸 Taking screenshot to '{screenshot_path}'...")
        res_vm = subprocess.run(
            ["sudo", "-u", VBOX_DELEGATE_USER, "VBoxManage", "controlvm", VM_NAME, "screenshotpng", screenshot_path],
            capture_output=True, text=True, timeout=30
        )
        
        if res_vm.returncode == 0 and os.path.exists(screenshot_path):
            subprocess.run(
                ["sudo", "-u", VBOX_DELEGATE_USER, "setfacl", "-m", f"u:{VBOX_ACL_USER}:r", screenshot_path],
                capture_output=True, text=True, timeout=5
            )
            size = os.path.getsize(screenshot_path)
            return f"✅ Screenshot saved to vm_snapshot_{timestamp}.png ({size} bytes). Use inspect_snapshot with timestamp '{timestamp}' to view it."
        else:
            stderr_msg = res_vm.stderr.strip() if res_vm.stderr else "(empty)"
            return f"❌ Screenshot failed. Exit code: {res_vm.returncode}\nSTDERR: {stderr_msg}"
    except Exception as e:
        return f"❌ Error taking screenshot: {e}"

def stop_vm_tool(args):
    try:
        print("  🔌 Stopping VM...")
        result = run_vboxmanage(["controlvm", VM_NAME, "acpipowerbutton"])
        time.sleep(1)
        state = _get_vm_status()
        
        if state == "stopped": return "✅ VM stopped successfully."
        if state in ["running", "locked", "crashed"]:
            result = run_vboxmanage(["controlvm", VM_NAME, "poweroff"])
            time.sleep(1)
            state = _get_vm_status()
            if state == "stopped": return f"✅ VM stopped successfully (hard poweroff)."
        
        return f"❌ Failed to stop VM. State: {state}."
    except Exception as e:
        return f"❌ Error stopping VM: {e}"

def launch_current_vm(args):
    try:
        start_result = start_vm_tool(args)
        if start_result.startswith("❌"): return start_result
        screenshot_result = take_screenshot_tool(args)
        stop_result = stop_vm_tool(args)
        return f"{screenshot_result}\n{stop_result}"
    except Exception as e:
        return f"❌ Error in launch_current_vm sequence: {e}"

def _get_png_dimensions(filepath):
    try:
        with open(filepath, "rb") as f:
            f.read(8)
            chunk_len = struct.unpack(">I", f.read(4))[0]
            chunk_type = f.read(4)
            if chunk_type != b"IHDR": raise ValueError("First chunk is not IHDR")
            width = struct.unpack(">I", f.read(4))[0]
            height = struct.unpack(">I", f.read(4))[0]
            return width, height
    except Exception:
        return None, None

def inspect_snapshot(args):
    try:
        timestamp = args.get("timestamp")
        if not timestamp: return "❌ Error: No timestamp provided."
        filename = f"vm_snapshot_{timestamp}.png"
        path = f"screenshots/{filename}"
        if not os.path.exists(path): return f"❌ Error: Snapshot not found."
        
        width, height = _get_png_dimensions(path)
        if width is None or height is None: return f"❌ Error: Could not read resolution."
            
        with open(path, "rb") as f:
            img_bytes = f.read()
        img_b64 = base64.b64encode(img_bytes).decode('utf-8')
        
        return {
            "status": "success",
            "vision_ready": True,
            "image_data": img_b64,
            "resolution": f"{width}x{height}",
            "message": f"Snapshot {timestamp} ready for inspection."
        }
    except Exception as e:
        return f"❌ Error processing snapshot: {e}"

def take_screenshot_and_inspect_tool(args):
    try:
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        screenshot_path = f"screenshots/vm_snapshot_{timestamp}.png"
        os.makedirs("screenshots", exist_ok=True)

        print(f"  📸 Taking and inspecting screenshot...")
        res_vm = subprocess.run(
            ["sudo", "-u", VBOX_DELEGATE_USER, "VBoxManage", "controlvm", VM_NAME, "screenshotpng", screenshot_path],
            capture_output=True, text=True, timeout=30
        )
        
        if res_vm.returncode != 0 or not os.path.exists(screenshot_path):
            stderr_msg = res_vm.stderr.strip() if res_vm.stderr else "(empty)"
            return f"❌ Screenshot failed. Exit code: {res_vm.returncode}\nSTDERR: {stderr_msg}"
            
        subprocess.run(
            ["sudo", "-u", VBOX_DELEGATE_USER, "setfacl", "-m", f"u:{VBOX_ACL_USER}:r", screenshot_path],
            capture_output=True, text=True, timeout=5
        )
        
        width, height = _get_png_dimensions(screenshot_path)
        if width is None or height is None: return f"❌ Error: Could not read resolution."
            
        with open(screenshot_path, "rb") as f:
            img_bytes = f.read()
        img_b64 = base64.b64encode(img_bytes).decode('utf-8')
        
        return {
            "status": "success",
            "vision_ready": True,
            "image_data": img_b64,
            "resolution": f"{width}x{height}",
            "message": f"Screenshot {timestamp} ready for inspection."
        }
    except Exception as e:
        return f"❌ Error during take_screenshot_and_inspect: {e}"

def read_file_tool(args):
    MAX_NUMBER_OF_CHARACTERS_TO_READ = 3000
    filename = args.get("filename", "")
    start = args.get("start", 0)
    numberToRead = args.get("numberToRead", MAX_NUMBER_OF_CHARACTERS_TO_READ)
    numberToRead = min(numberToRead, MAX_NUMBER_OF_CHARACTERS_TO_READ)

    if not filename: return json.dumps({"error": "No filename provided."})
    if not isinstance(start, int) or start < 0: return json.dumps({"error": "Start position must be a non-negative integer."})
    if ".." in filename or "/" in filename or "\\" in filename: return json.dumps({"error": "Directory traversal is not allowed."})

    base_dir = "os"
    file_path = base_dir + "/" + filename
    try:
        with open(file_path, "rb") as f:
            raw_data = f.read()
        content = raw_data.decode("utf-8", errors="replace")
        total_characters = len(content)
        slice_end = min(start + numberToRead, len(content))
        content_slice = content[start:slice_end]
        actual_len = len(content_slice)
        end_pos = start + actual_len - 1 if actual_len > 0 else start

        return json.dumps({
            "start": start, "size": actual_len, "end": end_pos, 
            "total_characters": total_characters, "content": content_slice
        })
    except FileNotFoundError:
        return json.dumps({"error": f"File '{filename}' not found."})
    except Exception as e:
        return json.dumps({"error": f"Error reading file: {e}"})

def read_reference_file_tool(args):
    MAX_NUMBER_OF_CHARACTERS_TO_READ = 3000
    filename = args.get("filename", "")
    start = args.get("start", 0)
    numberToRead = args.get("numberToRead", MAX_NUMBER_OF_CHARACTERS_TO_READ)
    numberToRead = min(numberToRead, MAX_NUMBER_OF_CHARACTERS_TO_READ)

    if not filename: return json.dumps({"error": "No filename provided."})
    if not isinstance(start, int) or start < 0: return json.dumps({"error": "Start position must be a non-negative integer."})
    if ".." in filename or "/" in filename or "\\" in filename: return json.dumps({"error": "Directory traversal is not allowed."})

    base_dir = "os/references"
    file_path = base_dir + "/" + filename
    try:
        with open(file_path, "rb") as f:
            raw_data = f.read()
        content = raw_data.decode("utf-8", errors="replace")
        total_characters = len(content)
        slice_end = min(start + numberToRead, len(content))
        content_slice = content[start:slice_end]
        actual_len = len(content_slice)
        end_pos = start + actual_len - 1 if actual_len > 0 else start

        return json.dumps({
            "start": start, "size": actual_len, "end": end_pos, 
            "total_characters": total_characters, "content": content_slice
        })
    except FileNotFoundError:
        return json.dumps({"error": f"File '{filename}' not found in 'os/references'."})
    except Exception as e:
        return json.dumps({"error": f"Error reading file: {e}"})

def write_file_tool(args):
    filename = args.get("filename", "")
    text = args.get("text", "")
    
    if not filename: return "❌ Error: No filename provided."
    if ".." in filename or "/" in filename or "\\" in filename: return "❌ Error: Directory traversal is not allowed."
    if not (filename.endswith(".c") or filename.endswith(".h")): return "❌ Error: Filename must be a .c or .h file."
        
    file_path = os.path.join("os", filename)
    try:
        os.makedirs("os", exist_ok=True)
        with open(file_path, "w", encoding="utf-8") as f:
            f.write(text)
        return f"✅ Successfully wrote {len(text)} characters to {filename}."
    except Exception as e:
        return f"❌ Error writing file: {e}"

def thinking_tool(args):
    justification = args.get("justification", "")
    if not justification or not justification.strip(): return "❌ Error: Justification cannot be empty."
    if len(justification) >= 200: return "❌ Error: Justification must be less than 200 characters."
    print(f"💭 THINKING: {justification}")
    return "OK"

def finish_tool(args):
    return args.get("answer", "Done.")

TOOL_MAP = {
    "calculator": calculator_tool,
    "write_kernel": write_kernel_tool,
    "read_file": read_file_tool,
    "read_reference_file": read_reference_file_tool,
    "write_file": write_file_tool,
    "compile_kernel": compile_kernel_tool,
    "compile_kernel_files": compile_kernel_files_tool,
    "start_vm": start_vm_tool,
    "set_mouse_position": set_mouse_position_tool,
    "take_screenshot": take_screenshot_tool,
    "take_screenshot_and_inspect": take_screenshot_and_inspect_tool,
    "stop_vm": stop_vm_tool,
    "send_keyboard_input": send_keyboard_input_tool,
    "launch_current_vm": launch_current_vm,
    "inspect_snapshot": inspect_snapshot,
    "thinking": thinking_tool,
    "finish": finish_tool,
}

SYSTEM_PROMPT = """You are a helpful assistant, with access to tools. You must call a tool on every turn. You can use the thinking tool to easily satisfy this constraint.
You can only change '.c' files in the os folder. You can read 'os.iso.compilation.log' if you want to know how the current vm iso was created. The entry point is called 'kernel_main() or  kernel_main(uint32_t magic, uint32_t info_ptr)'"""

def call_llama(messages, temperature=1.0, max_tokens=6000):
    payload = {
        "model": MODEL_NAME,
        "messages": messages,
        "temperature": temperature,
        "max_tokens": max_tokens,
        "tools": TOOLS,
        "tool_choice": "auto",
        "stream": False,
    }
    try:
        resp = requests.post(COMPLETION_API_URL, json=payload, timeout=500)
        resp.raise_for_status()
        return resp.json()
    except requests.exceptions.ConnectionError:
        print("\n❌ Cannot connect to server. Is llama.cpp running?", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        raise

MESSAGE_COUNT_THRESHOLD = 500
TOTAL_TOKEN_SUMMARIZE_THRESHOLD = 100000
SUMMARIZE_PROMPT = "Summarize this session. Describe the current os state, in particular which files were compiled to create the current iso.  State the goal and what should be done next "
KEEP_LAST_N_MESSAGES = 5

def retrieveChatMessageFromConsole( ):
    return input("User: ")

def run_agent(user_input, max_iterations=3000, chatMode=False):
    injectRemindMessage = False
    global _interrupt_flag
    
    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": user_input},
    ]

    totaltokens = -1
    iteration = 0
    while True:
        if chatMode is False and iteration > max_iterations: break 
        iteration = iteration + 1
        
        print(f"\n{'─' * 60}\n🔄  Iteration {iteration}/{max_iterations}\n{'─' * 60}")

        if( len( messages) > MESSAGE_COUNT_THRESHOLD or totaltokens > TOTAL_TOKEN_SUMMARIZE_THRESHOLD):
            print("dropping messages to free context")
            summarymessages = messages + [ {"role": "user", "content": SUMMARIZE_PROMPT} ]
            completion = call_llama(summarymessages)
            if not completion: return
            choice = completion.get("choices", [{}])[0]
            content = choice.get("message", {}).get("content", "")
            messages = messages[0:2] + [{"role": "user", "content": content}] + messages[max(2,len(messages)-KEEP_LAST_N_MESSAGES):]

        print(f"len(messages) : {len(messages)}\ncurrent total token before call_lama : {totaltokens}")

        try:
            response = call_llama(messages)
        except Exception as e:
            print(f"\n❌ Failed to get response from server: {e}")
            return f"Server Error: {e}"
        
        if response is not None:
            usage = response.get("usage",{})
            totaltokens = usage.get("total_tokens",-1)
            print(f"current total token after call_lama : : {totaltokens}")

        assistant_msg = response["choices"][0]["message"]
        print(f"Role:\n{assistant_msg.get('role','')}\n💭 Reasoning:\n{assistant_msg.get('reasoning_content','').strip()}\n💭 Content:\n{assistant_msg.get('content','').strip()}\n")
        
        tool_calls = assistant_msg.get("tool_calls")

        if not tool_calls:
            messages.append({"role": "user", "content": "You must issue a tool call every turn."})
            continue

        for tc in tool_calls:
            tool_name = tc["function"]["name"]
            args_json = tc["function"]["arguments"]
            
            try:
                tool_args = json.loads(args_json)
            except json.JSONDecodeError:
                messages.append({"role": "user", "content": "please retry but be more concise for your next attempt"})
                continue
            except Exception:
                tool_args = {}

            print(f"\n🔧  Calling tool: {tool_name}({json.dumps(tool_args)})")
            try:
                observation = TOOL_MAP[tool_name](tool_args)
            except KeyError:
                observation = f"Unknown tool: {tool_name}"
            except Exception as e:
                observation = f"Tool error: {e}"

            messages.append(assistant_msg)

            if isinstance(observation, dict) and observation.get("vision_ready"):
                b64_data = observation["image_data"]
                resolution = observation.get("resolution", "unknown")
                tool_result_str = f"✅ Snapshot successfully loaded and injected for visual inspection. Resolution: {resolution}."
                tool_result_content = [
                        {"type": "text", "text":tool_result_str },
                        {"type": "image_url", "image_url": {"url": f"data:image/png;base64,{b64_data}", "detail": "high"}}
                    ]
            else:
                tool_result_str = str(observation)
                tool_result_content = tool_result_str

            print(f"\n✅ TOOL Result : {tool_result_str}")
            messages.append({
                    "role": "tool",
                    "tool_call_id": tc["id"],
                    "name": json.dumps(str(tool_name)),
                    "content":tool_result_content
                })
            
            if injectRemindMessage:
                if tool_name == "write_kernel": messages.append( {"role":"reminder","content":"You must recompile now !"})
                if tool_name == "compile_kernel": messages.append( {"role":"reminder","content":"If the compilation was successful try launching the vm"})
                if tool_name == "start_vm": messages.append( {"role":"reminder","content":"Now is a good time to take a screenshot to verify the machine works ok."})
                if tool_name == "set_mouse_position": messages.append( {"role":"reminder","content":"Now take a screenshot to see the result"})
                if tool_name in ["take_screenshot", "take_screenshot_and_inspect"]: messages.append( {"role":"reminder","content":"Now the screenshot is ready for visual inspection."})
                if tool_name == "stop_vm": messages.append( {"role":"reminder","content":"The VM is now powered off. You may finish or start a new session."})

            if tool_name == "finish":
                print(f"\n✅ Agent finished. Answer: {tool_result_str}")
                if chatMode is True:
                    messages.append( {"role":"user","content": retrieveChatMessageFromConsole()} )
                    continue
                return tool_result_str

            if _interrupt_flag:
                print("\n🛑 Session interrupted by user.")
                _interrupt_flag = False
                if chatMode is True:
                    messages.append( {"role":"user","content": retrieveChatMessageFromConsole()} )
                    continue
                return "🛑 Session interrupted."

    print(f"\n⚠️  Max iterations ({max_iterations}) reached.")
    return "I couldn't reach a conclusion within the allowed iterations."

if __name__ == "__main__":
    if len( sys.argv ) == 1:
        print("Usage : builderagent.py \"query\" \n builderagent.py chat interface")
    chatMode = False
    if len( sys.argv ) == 2:
        query = sys.argv[1]
    if len( sys.argv ) == 3:
        query = retrieveChatMessageFromConsole()
        chatMode = True

    print(f"🚀 Agent started. Query: {query}")
    answer = run_agent(query, chatMode = chatMode)
    print(f"\n{'=' * 60}\n🏁  FINAL ANSWER: {answer}\n{'=' * 60}")
