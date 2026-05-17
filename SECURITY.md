# Security Documentation for MaragingLoop

---

## 🔒 1. Overview
`MaragingLoop` is an autonomous AI agent that generates, compiles, boots, and debugs bare-metal C kernels using a local LLM server (`llama.cpp`), VirtualBox VM control, and vision-based feedback. Because the agent executes shell commands, manipulates files, controls virtual machines, and processes untrusted model outputs, security is a foundational design priority. This document outlines the threat model, implemented safeguards, operational guidelines, and known limitations.

---

## 🧭 2. Threat Model & Attack Surfaces
| Surface | Risk Description |
|---------|------------------|
| **LLM-Driven Code Generation** | Prompt injection, model hallucinations, or malicious tool arguments could trigger unsafe file writes or compilation commands. |
| **Shell & Tool Execution** | Arbitrary `subprocess.run` calls could lead to command injection if tool arguments are not validated. |
| **File System Access** | Directory traversal, overwriting critical host files, or exhausting disk space via logs/snapshots. |
| **VM Control Layer** | VM escape, state corruption, or resource exhaustion through repeated boot/power cycles. |
| **Context & Memory Management** | Base64-encoded screenshots and long conversation histories could cause memory bloat or context drift. |
| **Network Exposure** | `llama.cpp` server binding may expose unauthenticated API endpoints if misconfigured. |
| **Privilege Execution** | Running as a privileged user could allow kernel/VM host compromise if safeguards fail. |

---

## 🛡️ 3. Implemented Safeguards

### ✅ Command & Process Safety
- **No `shell=True`**: All `subprocess.run` calls use list-based arguments, preventing shell metacharacter injection.
- **Hardcoded Toolchains**: Commands (`gcc`, `ld`, `grub-mkrescue`, `VBoxManage`) are defined in-code. The LLM only passes structured JSON arguments, not raw commands.
- **Safe Evaluation**: The `calculator` tool restricts `eval()` to `set("0123456789+-*/. () ")`, blocking arbitrary Python execution.
- **Timeout Limits**: `subprocess.run(timeout=180)` prevents hung compilation or VM commands from blocking the loop.

### ✅ File System Confinement
- **Directory Traversal Prevention**: All file operations reject paths containing `..`, `/`, or `\`.
- **Extension Enforcement**: `write_file` only accepts `.c` and `.h` extensions.
- **Base Directory Isolation**: All I/O is scoped to `os/` or `os/references`. No arbitrary host paths are accessible.
- **Atomic Overwrites**: Files are opened in write mode with explicit encoding, preventing partial writes or permission bypasses.

### ✅ VM & Virtualization Security
- **Headless-Only Execution**: VMs run in `--type headless` mode, disabling graphical input and reducing attack surface.
- **Graceful Power Control**: VM shutdown uses `acpipowerbutton` with fallback to `poweroff`, preventing disk corruption or snapshot inconsistency.
- **State Recovery**: `_get_vm_status()` handles `locked`, `crashed`, and `running` states safely before attempting operations.
- **No Shared Folders/Networking**: Default VM configuration isolates the guest from host network/storage shares.

### ✅ LLM & Agent Guardrails
- **Strict Tool Schema**: OpenAI-native tool definitions enforce JSON structure. Malformed arguments trigger retry cycles instead of silent failures.
- **Mandatory `thinking` Tool**: Ensures the model reasons before acting, reducing reckless tool calls.
- **Iteration Limit**: `max_iterations=3000` prevents infinite loops and resource exhaustion.
- **Context Compaction**: Auto-summarization at `MESSAGE_COUNT_THRESHOLD` or `TOTAL_TOKEN_SUMMARIZE_THRESHOLD` prevents context overflow and keeps base64 images bounded.
- **Graceful Interruption**: Custom `SIGINT` handler finishes the current step before exiting, avoiding orphaned processes or locked VMs.

### ✅ Data & Privacy
- **Local-Only Inference**: `llama.cpp` runs on localhost by default. No telemetry, logging, or external API calls are made.
- **Snapshot Isolation**: Screenshots are saved to `screenshots/` with timestamps and automatically ignored by file watchers. No PII or host data is embedded.
- **No Credential Handling**: The agent does not request, store, or transmit passwords, API keys, or certificates.

---

## 🧱 4. Configuration & Deployment Guidelines

| Recommendation | Reason |
|----------------|--------|
| **Run as a non-root user** | Prevents accidental host-level privilege escalation if tool calls or file writes fail. |
| **Bind `llama.cpp` to `127.0.0.1`** | Restricts API access to localhost. Never expose port `8080` to untrusted networks. |
| **Use dedicated VM storage & snapshots** | Isolate `agentos` VM files to prevent host disk exhaustion or cross-contamination. |
| **Enable host firewall rules** | Allow only localhost traffic to `8080`. Block inbound connections. |
| **Rotate/update toolchains regularly** | Keep `gcc-multilib`, `binutils`, `grub-mkrescue`, and VirtualBox up to date to patch known CVEs. |
| **Audit `os/` directory permissions** | Ensure the host user owns `os/`, `screenshots/`, and `isodir/` with `755`/`644` permissions. |

---

## ⚠️ 5. Known Limitations & Risk Acceptance
| Limitation | Impact | Mitigation Strategy |
|------------|--------|---------------------|
| **No sandboxing for shell commands** | A hallucinated argument could theoretically invoke unexpected host utilities. | Hardcoded command lists + strict JSON validation + non-root execution. |
| **LLM prompt injection/jailbreaking** | Model may attempt to bypass tool constraints or output malformed JSON. | Schema enforcement + retry loops + `thinking` tool + iteration cap. |
| **Base64 image memory bloat** | Repeated high-res screenshots increase RAM usage. | `take_screenshot_and_inspect` uses timestamp-based rotation + context summarization drops old messages. |
| **VirtualBox host integration risks** | VM escape vulnerabilities exist in older VBox versions. | Keep VirtualBox updated; never run VMs with host filesystem sharing or network bridging enabled. |
| **No formal security audit** | Agent architecture is experimental by nature. | Treat as a research prototype; validate outputs before deploying to production environments. |

---

## 📦 6. Security Update & Patch Policy
- **Releases** are tagged with semantic versioning. Breaking changes to tool schemas or security mitigations will be clearly documented in release notes.
- **Vulnerabilities** in the agent loop (e.g., path traversal bypass, subprocess injection, context overflow) will be patched in minor releases within 14 days of verification.
- **Dependencies** (`requests`, `llama.cpp`, VirtualBox SDKs) are tracked. Critical upstream CVEs will be evaluated for backport or pinning.

---

## 📬 7. Reporting Security Issues
If you discover a vulnerability in `MaragingLoop`, please report it responsibly:
1. **Do not** disclose details publicly until a fix is available.
2. Email: `[your-secure-contact@domain.com]` or open a private GitHub issue with `[SECURITY]` prefix.
3. Include: reproduction steps, environment details, affected versions, and potential impact.
4. Expected response: acknowledgment within 72 hours, patch timeline within 14 days.

---

## 📜 8. Disclaimer
`MaragingLoop` is an experimental AI-driven development agent intended for research, education, and hobbyist bare-metal OS development. While security mitigations are implemented by design, autonomous agents interacting with shell commands and virtual machines carry inherent risks. Users are responsible for:
- Running the agent in isolated, non-production environments
- Validating generated code and ISO artifacts before deployment
- Maintaining host VM, toolchain, and OS security updates
- Complying with local laws regarding AI automation and virtualization

By using `MaragingLoop`, you acknowledge that the maintainers are not liable for host system modifications, VM state corruption, or security incidents arising from agent operations.

---
*Security is a continuous process. This document will be updated as the agent evolves.* 🔐🐧🖥️