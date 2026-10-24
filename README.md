# Windows Process Injection & Virtual Memory Dissection (x64)

[![Language](https://img.shields.io/badge/Language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Platform](https://img.shields.io/badge/Platform-Windows%2011%20%7C%2010%20(x64)-0078d7.svg)](https://microsoft.com/windows)
[![Architecture](https://img.shields.io/badge/Architecture-AMD64-orange.svg)]()
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Channel](https://img.shields.io/badge/YouTube-Shield%20Spectrum-red.svg)](https://youtube.com/@shieldspectrum)

This repository contains the source code, technical documentation, and defensive telemetry analysis for the **Shield Spectrum** engineering breakdown:  
**"How Malware Injects Code into Legitimate Windows Processes (C Lab)"**.

---

## 🔬 Architectural Overview

Modern 64-bit Windows operating systems enforce strict **Process Isolation Boundaries** via the Windows Virtual Memory Manager and Kernel Object Manager. Under normal conditions, Process A cannot directly read, write, or execute code inside the private virtual address space of Process B.

Classic DLL and Shellcode Injection abuses legitimate Win32 process management and debugging APIs to bypass these isolation barriers in 4 discrete architectural stages:

```
+------------------+                                      +-------------------------------+
|   injector.exe   |                                      |   cmd.exe (Target PID: 6196)  |
|  (Process Space) |                                      |       (Virtual Address Space) |
+--------+---------+                                      +---------------+---------------+
         |                                                                |
         |  1. OpenProcess(PROCESS_ALL_ACCESS)                            |
         |--------------------------------------------------------------->|  Kernel Handle Acquired
         |                                                                |
         |  2. VirtualAllocEx(..., PAGE_EXECUTE_READWRITE)                |
         |--------------------------------------------------------------->|  Carve 4KB RWX Page: 0x0000023E9F390000
         |                                                                |
         |  3. WriteProcessMemory(..., [90 90 90 90 CC C3])               |
         |--------------------------------------------------------------->|  Stream Bytes Directly into RAM
         |                                                                |
         |  4. CreateRemoteThread(..., StartAddress = 0x23E9F390000)      |
         |--------------------------------------------------------------->|  Target RIP Hijacked -> Worker Thread Active
         v                                                                v
```

---

## 🛠️ The 4-Stage Injection Sequence

### 1. `OpenProcess` (Handle Acquisition)
```c
HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, targetPID);
```
- Requests a kernel handle to the remote process.
- The `PROCESS_ALL_ACCESS` (`0x1FFFFF`) access mask provides complete rights to modify target virtual memory and thread contexts.

### 2. `VirtualAllocEx` (Memory Carving)
```c
LPVOID pRemoteCode = VirtualAllocEx(
    hProcess, 
    NULL, 
    4096, 
    MEM_COMMIT | MEM_RESERVE, 
    PAGE_EXECUTE_READWRITE
);
```
- Allocates a new 4,096-byte memory page in target RAM.
- **Critical Flag:** `PAGE_EXECUTE_READWRITE` creates memory that is both writable and executable at runtime.

### 3. `WriteProcessMemory` (Payload Streaming)
```c
WriteProcessMemory(hProcess, pRemoteCode, shellcode, sizeof(shellcode), &bytesWritten);
```
- Copies raw machine opcodes directly into the newly allocated remote memory page.
- **Zero Disk Footprint:** No files are created or altered on the physical disk.

### 4. `CreateRemoteThread` (Execution Hijack)
```c
HANDLE hThread = CreateRemoteThread(
    hProcess, 
    NULL, 
    0, 
    (LPTHREAD_START_ROUTINE)pRemoteCode, 
    NULL, 
    0, 
    NULL
);
```
- Spawns a remote worker thread under the execution identity of the legitimate target process.
- Points the CPU instruction pointer (`RIP`) straight to the injected opcode buffer.

---

## 🛡️ Defensive Engineering & Detection

### 1. Sysmon Event ID 8 (CreateRemoteThread)
The primary kernel detection mechanism is Microsoft Sysmon **Event ID 8**. An alert rule can be configured in your Sysmon XML schema:

```xml
<Sysmon schemaversion="4.90">
  <EventFiltering>
    <RuleGroup name="Process Injection Monitoring" groupRelation="or">
      <CreateRemoteThread onmatch="include">
        <TargetImage condition="end with">cmd.exe</TargetImage>
        <TargetImage condition="end with">notepad.exe</TargetImage>
      </CreateRemoteThread>
    </RuleGroup>
  </EventFiltering>
</Sysmon>
```

### 2. Unbacked Memory Scanners (PE-sieve / Moneta)
Legitimate code sections (`.text`) are mapped from authentic, digitally signed executable files on disk (e.g., `kernel32.dll`).  
Injected memory pages are **unbacked** (`MEM_PRIVATE` without a backing PE image). Modern memory scanners detect this by querying `VirtualQueryEx` for pages matching:
- `State == MEM_COMMIT`
- `Type == MEM_PRIVATE`
- `Protect == PAGE_EXECUTE_READWRITE`

---

## 📦 Building & Testing

### Prerequisites
- Windows 10 / 11 (AMD64)
- MinGW-w64 GCC or Microsoft Visual Studio (MSVC)
- System Informer (formerly Process Hacker) for memory inspection

### Compilation (GCC)
```cmd
gcc -O2 src/injector.c -o injector.exe
```

### Running the Lab
```cmd
injector.exe
```
1. Spawns `cmd.exe` in suspended mode.
2. Allocates an RWX page and writes the harmless PoC bytes (`0x90 0x90 0x90 0x90 0xCC 0xC3`).
3. Spawns the thread and holds execution open for interactive memory inspection in System Informer.
4. Press `ENTER` to cleanly close kernel handles and terminate the test process.

---

## ⚠️ Educational & Research Disclaimer

This project is created strictly for **academic computer science, systems programming research, and defensive cybersecurity engineering** education. The bytecode utilized in this lab is a benign proof-of-concept (NOP sled terminating in a debugger trap `INT 3` and procedure return `RET`) and performs no destructive operations. All testing must be conducted within an isolated laboratory or virtual machine environment.

---

## 📺 Connect
- **YouTube:** [@shieldspectrum](https://youtube.com/@shieldspectrum)
- **GitHub:** [@ssp2x](https://github.com/ssp2x)
