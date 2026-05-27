#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cryptopp/filters.h>
#include <cryptopp/hex.h>
#include <cryptopp/rsa.h>

using namespace std;
using namespace CryptoPP;

#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define RESET "\033[0m"

const string DLL_PATH = "path/to/your/dll.dll";
const string SEM_PHRASE = "Minecraft";
const int ENCRYPTION_KEY = 0xDEADBEEF;
const int WAIT_TIME = 10000; 

bool isMinecraftRunning() {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        cout << RED << "Error: CreateToolhelp32Snapshot failed!" RESET << endl;
        return false;
    }
    bool running = false;
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (strstr(pe32.szExeFile, "Minecraft") || strstr(pe32.szExeFile, "javaw")) {
                running = true;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);
    if (running) {
        cout << GREEN << "Minecraft is running." RESET << endl;
    }
    else {
        cout << YELLOW << "Warning: Minecraft is not running!" RESET << endl;
    }
    return running;
}

DWORD getMinecraftPID() {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        cout << RED << "Error: CreateToolhelp32Snapshot failed!" RESET << endl;
        return 0;
    }
    DWORD pid = 0;
    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (strstr(pe32.szExeFile, "Minecraft") || strstr(pe32.szExeFile, "javaw")) {
                pid = pe32.th32ProcessID;
                break;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }
    CloseHandle(hProcessSnap);
    if (pid) {
        cout << GREEN << "Minecraft PID: " << pid RESET << endl;
    }
    else {
        cout << YELLOW << "Warning: Could not retrieve Minecraft PID!" RESET << endl;
    }
    return pid;
}

void encryptDLL(const string& path) {
    ifstream dllFile(path, ios::binary);
    if (!dllFile) {
        cout << RED << "Error: Could not open DLL file for encryption!" RESET << endl;
        return;
    }
    vector<uint8_t> dllData((istreambuf_iterator<char>(dllFile)), (istreambuf_iterator<char>()));
    dllFile.close();

    for (auto& byte : dllData) {
        byte ^= ENCRYPTION_KEY;
    }

    ofstream encryptedFile(path, ios::binary);
    if (!encryptedFile) {
        cout << RED << "Error: Could not open encrypted DLL file!" RESET << endl;
        return;
    }
    encryptedFile.write(reinterpret_cast<const char*>(dllData.data()), dllData.size());
    encryptedFile.close();
    cout << GREEN << "DLL encrypted successfully." RESET << endl;
}

void decryptDLL(const string& path) {
    ifstream dllFile(path, ios::binary);
    if (!dllFile) {
        cout << RED << "Error: Could not open DLL file for decryption!" RESET << endl;
        return;
    }
    vector<uint8_t> dllData((istreambuf_iterator<char>(dllFile)), (istreambuf_iterator<char>()));
    dllFile.close();

    for (auto& byte : dllData) {
        byte ^= ENCRYPTION_KEY;
    }

    ofstream decryptedFile(path, ios::binary);
    if (!decryptedFile) {
        cout << RED << "Error: Could not open decrypted DLL file!" RESET << endl;
        return;
    }
    decryptedFile.write(reinterpret_cast<const char*>(dllData.data()), dllData.size());
    decryptedFile.close();
    cout << GREEN << "DLL decrypted successfully." RESET << endl;
}

bool injectDLL(DWORD pid, const string& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        cout << RED << "Error: OpenProcess failed!" RESET << endl;
        return false;
    }

    LPVOID dllPathAddress = VirtualAllocEx(hProcess, NULL, dllPath.length() + 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!dllPathAddress) {
        CloseHandle(hProcess);
        cout << RED << "Error: VirtualAllocEx failed!" RESET << endl;
        return false;
    }

    if (!WriteProcessMemory(hProcess, dllPathAddress, dllPath.c_str(), dllPath.length() + 1, NULL)) {
        VirtualFreeEx(hProcess, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        cout << RED << "Error: WriteProcessMemory failed!" RESET << endl;
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        VirtualFreeEx(hProcess, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        cout << RED << "Error: GetModuleHandleA failed!" RESET << endl;
        return false;
    }

    FARPROC pLoadLibraryA = GetProcAddress(hKernel32, "LoadLibraryA");
    if (!pLoadLibraryA) {
        VirtualFreeEx(hProcess, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        cout << RED << "Error: GetProcAddress failed!" RESET << endl;
        return false;
    }

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryA, dllPathAddress, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, dllPathAddress, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        cout << RED << "Error: CreateRemoteThread failed!" RESET << endl;
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    DWORD exitCode;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, dllPathAddress, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exitCode == 0) {
        cout << RED << "Error: DLL injection failed!" RESET << endl;
        return false;
    }

    cout << GREEN << "DLL injected successfully!" RESET << endl;
    return true;
}

void addAntiCheatBypassMethods(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        cout << RED << "Error: OpenProcess failed for anti-cheat bypass!" RESET << endl;
        return;
    }

    const unsigned long ANTI_CHEAT_FLAG_ADDRESS = 0x00400000; 
    const unsigned long ANTI_CHEAT_FLAG_VALUE = 0xDEADBEEF; // Example value

    if (WriteProcessMemory(hProcess, (LPVOID)ANTI_CHEAT_FLAG_ADDRESS, &ANTI_CHEAT_FLAG_VALUE, sizeof(ANTI_CHEAT_FLAG_VALUE), NULL)) {
        cout << GREEN << "Anti-cheat bypass method applied successfully." RESET << endl;
    }
    else {
        cout << RED << "Error: Anti-cheat bypass method failed!" RESET << endl;
    }

    const unsigned long PATCH_ADDRESS = 0x00400004; 
    const unsigned char PATCH_VALUE = 0x90; // NOP instruction
    if (WriteProcessMemory(hProcess, (LPVOID)PATCH_ADDRESS, &PATCH_VALUE, sizeof(PATCH_VALUE), NULL)) {
        cout << GREEN << "Patch memory to disable checks applied successfully." RESET << endl;
    }
    else {
        cout << RED << "Error: Patch memory to disable checks failed!" RESET << endl;
    }

    const unsigned long ASM_CODE_ADDRESS = 0x00400008; 
    const unsigned char ASM_CODE[] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0xC3 }; // this is a basiuc example of the assembly code you can inject
    if (WriteProcessMemory(hProcess, (LPVOID)ASM_CODE_ADDRESS, ASM_CODE, sizeof(ASM_CODE), NULL)) {
        cout << GREEN << "Inject assembly code to manipulate anti-cheat system applied successfully." RESET << endl;
    }
    else {
        cout << RED << "Error: Inject assembly code to manipulate anti-cheat system failed!" RESET << endl;
    }

    CloseHandle(hProcess);
}

int main() {
    while (true) {
        if (isMinecraftRunning()) {
            DWORD pid = getMinecraftPID();
            if (pid) {
                encryptDLL(DLL_PATH);
                addAntiCheatBypassMethods(pid); 
                if (injectDLL(pid, DLL_PATH)) {
                    Sleep(WAIT_TIME);
                    decryptDLL(DLL_PATH);
                }
                else {
                    cout << RED << "Error: DLL injection failed!" RESET << endl;
                }
            }
        }
        Sleep(1000);
    }
    return 0;
}
