# MaragingLoop
## Autonomous Bare-Metal OS Agent | 📝 Code → 🖥️ VM Interact → 👁️ Inspect → 🔨 Harden

[![Python 3.9+](https://img.shields.io/badge/python-3.9+-blue.svg)](https://www.python.org/downloads/)
[![OpenAI Tool Format](https://img.shields.io/badge/format-OpenAI%20Native-lightgrey)](https://platform.openai.org/docs/guides/function-calling)
[![VirtualBox](https://img.shields.io/badge/vm-VirtualBox-1876D2)](https://www.virtualbox.org/)

---

### 🔍 Overview
**MaragingLoop** is a ReAct-style AI agent engineered for autonomous bare-metal OS and kernel development. By orchestrating large language models with automated compilation, robust VirtualBox VM lifecycle control, and vision-based screenshot inspection, it iteratively writes, builds, tests, and refines low-level C code until the target system boots and behaves as specified.

Unlike text-only coding agents, MaragingLoop treats **code generation** and **VM interaction** as equally critical pillars of its development cycle. The LLM doesn’t just write code in isolation—it actively modifies kernel source, compiles it, boots a live virtual machine, injects I/O, captures visual feedback, and then *rewrites* the code based on real-world execution results. This closed-loop process bridges abstract reasoning with bare-metal reality.

---

### 🔄 Core Workflow
The agent operates in a continuous `think → code → interact → observe → refine` cycle. **Code manipulation** and **VM interaction** are the twin engines that drive iteration forward.

1. **Reason**: The LLM plans the next step using the `thinking` tool.
2. **📝 Code Generation & Modification**: The agent writes or updates `.c`/`.h` files in the `os/` directory, implements kernel functions, adjusts linker scripts, or modifies GRUB configs based on the current goal and prior feedback.
3. **🔨 Build & Compile**: Automated toolchain invocation (`gcc -m32`, `ld`, `grub-mkrescue`) transforms source into a bootable `os.iso`.
4. **🖥️ VM Interaction (Critical Step)**: The agent boots a headless VirtualBox VM, waits for the bootloader/kernel to load, and injects precise keyboard/mouse input. This simulates real hardware boundaries and exposes runtime behavior that static analysis cannot reveal.
5. **👁️ Vision Inspection**: A screenshot is captured, base64-encoded, and fed directly into the LLM’s context for visual analysis (boot logs, VGA output, kernel panics, GRUB errors, or unexpected behavior).
6. **Refine**: Based on vision feedback, the agent iterates—rewriting code, fixing compilation errors, adjusting I/O sequences, or recovering VM states—until the kernel meets the specified requirements.

---

### 🛠️ Key Features
| Category | Capabilities |
|----------|--------------|
| **📝 Code Generation & Modification** | Directly creates/edits `.c`/`.h` files, manages references, and writes kernel entry points. The LLM shapes the actual OS logic through precise file I/O tools. |
| **🖥️ Critical VM Interaction** | Full VirtualBox lifecycle: headless boot, ACPI power cycling, precise mouse/keyboard injection, and robust state recovery (`locked`, `crashed`, `running`). |
| **Vision-Driven Feedback** | Captures VM screens, encodes to base64, and feeds them to the LLM for real-time visual debugging and behavior verification. |
| **Bare-Metal Build Pipeline** | Automated 32-bit cross-compilation: `gcc -m32`, `ld -m elf_i386`, `as --32`, and `grub-mkrescue` for ISO generation. |
| **Context Management** | Auto-summarization when token/message limits are reached. Prevents context overflow while preserving session state. |
| **Graceful Interruption** | Custom `SIGINT` handler finishes the current VM/build step before halting. Safe for long-running iterations. |
| **Dual Usage Modes** | Single-query mode for automation, or interactive `chat` mode for step-by-step guidance. |

---

### 📋 Prerequisites
- **Python 3.9+**
- **`llama.cpp` server** running locally:  
  `llama-server --model <path-to-model> --host 0.0.0.0 --port 8080`
- **VirtualBox** with `VBoxManage` in `PATH`
- **32-bit Cross-Compilation Toolchain**:
  - `gcc-multilib` / `gcc -m32`
  - `binutils` (for `ld`, `as`)
  - `grub-mkrescue` (from `grub2-common` or `grub-pc-bin`)
- **`requests`** Python package

---

### 🚀 Installation & Setup
```bash
# 1. Clone the repository
git clone https://github.com/<your-username>/MaragingLoop.git
cd MaragingLoop

# 2. Create a virtual environment
python -m venv .venv
source .venv/bin/activate          # Linux/macOS
# .venv\Scripts\activate           # Windows

# 3. Install dependencies
pip install requests
```

Update the server endpoint in `builderagent.py` if needed:
```python
BASE_URL = "http://localhost:8080/v1/chat/completions"  # Default llama.cpp
```

---

### 💻 Usage
```bash
# Run a single task
python builderagent.py "Make the kernel print 'Hello Maraging' to the VGA console and halt."

# Enter interactive chat mode
python builderagent.py chat
```

**Environment Variables (Optional):**
| Variable | Default | Description |
|----------|---------|-------------|
| `BASE_URL` | `http://megabeast:8080/v1/chat/completions` | LLM server endpoint |
| `VM_NAME` | `agentos` | VirtualBox VM name to control |
| `MAX_ITERATIONS` | `3000` | Safety limit for the ReAct loop |

---

### 🧰 Tool Ecosystem
| Group | Tools |
|-------|-------|
| **📝 Code & Build** | `write_file`, `read_file`, `read_reference_file`, `compile_kernel`, `compile_kernel_files`, `write_kernel` |
| **🖥️ VM Interaction & Lifecycle** | `start_vm`, `stop_vm`, `set_mouse_position`, `send_keyboard_input`, `launch_current_vm` |
| **Vision Feedback** | `take_screenshot`, `take_screenshot_and_inspect`, `inspect_snapshot` |
| **Agent Flow** | `thinking`, `finish`, `calculator` |

---

### 📐 Design Philosophy
The name **MaragingLoop** draws from *maraging steel*—an ultra-high-strength alloy that hardens through controlled precipitation aging. Just as maraging steel gains resilience through repeated thermal and mechanical stress, **MaragingLoop** hardens bare-metal code through an iterative dual-process: **📝 active code generation** and **🖥️ live VM interaction**. 

Code is where the LLM shapes the kernel’s logic, memory layout, and hardware interfaces. VM interaction is where that logic is stress-tested against real execution boundaries, bootloader behavior, and VGA output. Neither step is optional: the agent writes code to solve the problem, boots it to see reality, reads the screen to understand what broke, and rewrites the code to fix it. Each cycle precipitates stability, transforming fragile prototypes into production-ready bare-metal systems.

---

### 📜 License & Contributing
This project is released under the [MIT License](LICENSE). Contributions, bug reports, and feature requests are welcome. Please follow standard fork/pull-request workflows.

---

### 🙏 Acknowledgments
- [llama.cpp](https://github.com/ggerganov/llama.cpp) for the high-performance local LLM server
- [Oracle VM VirtualBox](https://www.virtualbox.org/) for robust VM automation and `VBoxManage` CLI
- OpenAI Function Calling specification for the native tool format
- The bare-metal OS community for relentless inspiration

---
*Built for developers who want AI to actually touch metal, boot it, and fix what breaks.* 🔧🖥️💻