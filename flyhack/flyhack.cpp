// Defining NOMINMAX to prevent Windows headers from defining min and max macros,
// which can conflict with the min and max functions in the C++ Standard Library.
#define NOMINMAX
#include <Windows.h>
#include <conio.h>
#include <TlHelp32.h>
#include <string>
#include <limits>
#include <iostream>



// Structure to store information during window enumeration.
struct EnumData {
    DWORD processID;
    HWND hwnd;
};

// Callback to locate the window of our target process.
BOOL CALLBACK EnumWindowsCallback(HWND hWnd, LPARAM lParam) {
    EnumData* data = reinterpret_cast<EnumData*>(lParam);
    DWORD winProcId;
    GetWindowThreadProcessId(hWnd, &winProcId);
    if (winProcId == data->processID) {
        data->hwnd = hWnd;
        return FALSE;
    }
    return TRUE;
}

// Get the main window of the target process.
HWND GetProcessWindow(DWORD processId) {
    EnumData data{ processId, NULL };
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&data));
    return data.hwnd;
}

// Get dimensions and position of a window.
RECT GetWindowRectFromHandle(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);
    return rect;
}

// Get process ID from its name.
DWORD GetProcessID(const char* processName) {
    DWORD processID = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe32)) {
            do {
                if (_wcsicmp(pe32.szExeFile, L"gtutorial-x86_64.exe") == 0) {
                    processID = pe32.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pe32));
        }
        CloseHandle(hSnap);
    }
    return processID;
}

// Get the base address of a module within a process.
uintptr_t GetBaseAddress(DWORD processId, const wchar_t* moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processId);
    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(hSnapshot, &moduleEntry)) {
        do {
            if (_wcsicmp(moduleEntry.szModule, moduleName) == 0) {
                baseAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                break;
            }
        } while (Module32Next(hSnapshot, &moduleEntry));
    }
    CloseHandle(hSnapshot);
    return baseAddress;
}

// Traverse the multi-level pointer to get the final address.
uintptr_t GetFinalAddress(HANDLE processHandle, uintptr_t baseAddress) {
    uintptr_t currentAddress = baseAddress;
    uintptr_t temp;
    uintptr_t offsets[] = { 0x003CCE00, 0x60, 0xC98, 0xc58, 0xd8, 0x628, 0x200 };                // Note: This pointer was checked on 2 windows 10 systems runnin cheat engine 7.5 and it worked fine 10/10, even after the re-creation of the player entity in memory upon "death"
    int offsetsLength = sizeof(offsets) / sizeof(offsets[0]);

    // Display the initial base address
    std::cout << "[\"gtutorial-x86_64.exe\"] -> " << std::hex << "0x" << baseAddress << std::dec << std::endl;

    for (int i = 0; i < offsetsLength; i++) {
        currentAddress += offsets[i];

        // Read the memory at the current address
        if (!ReadProcessMemory(processHandle, reinterpret_cast<LPVOID>(currentAddress), &temp, sizeof(temp), nullptr)) {
            return 0;
        }

        // Display the pointer step
        if (i < offsetsLength) {
            std::cout << "[" << std::hex << "0x" << currentAddress - offsets[i] << std::dec << " + " << std::hex << "0x" << offsets[i] << std::dec << "] -> " << std::hex << "0x" << temp << std::dec << std::endl;
        }

        currentAddress = temp;
    }

    return currentAddress;
}


// Get mouse position relative to game window.
void GetGameWindowNormalizedMouseCoords(HWND gameWindow, float& x, float& y) {
    POINT p;
    RECT windowRect;
    if (GetCursorPos(&p) && GetWindowRect(gameWindow, &windowRect)) {
        float gameWidth = static_cast<float>(windowRect.right - windowRect.left);
        float gameHeight = static_cast<float>(windowRect.bottom - windowRect.top);
        x = (p.x - windowRect.left) / gameWidth * 2 - 1;
        y = (p.y - windowRect.top) / gameHeight * 2 - 1;
    }
    else {
        x = y = 0.0f;
    }
}

// Clear the console screen
void ClearConsole() {
    system("cls");
}

// Proper error message display function with clearConsole() to keep the console clean and readable.
void DisplayErrorAndWait(const std::string& errorMsg) {
    std::cerr << errorMsg << std::endl;
    std::cerr << "Press any key to restart..." << std::endl;
    _getch();  // Wait for any key to be pressed so the user can read the error message.
    ClearConsole();
}

int main() {
    while (true) { // Restart main loop in case of error.

        std::cout << "Press 'F' to toggle flyhack. Waiting..." << std::endl;
        bool isActive = false;
        bool fKeyPressedPreviously = false;

        const BYTE originalPatchBytes[] = { 0xF3, 0x44, 0x0F, 0x11, 0x48, 0x28 };

        while (true) {
            if (GetAsyncKeyState('F') & 0x8000 && !fKeyPressedPreviously) {
                isActive = !isActive;
                ClearConsole();

                if (isActive) {
                    std::cout << "Flyhack activated! Press 'F' to deactivate." << std::endl;
                }
                else {
                    std::cout << "Flyhack deactivated!" << std::endl;
                }

                fKeyPressedPreviously = true;
            }
            else if (!(GetAsyncKeyState('F') & 0x8000)) {
                fKeyPressedPreviously = false;
            }

            DWORD targetProcessId = GetProcessID("gtutorial-x86_64.exe");
            if (targetProcessId == 0) {
                DisplayErrorAndWait("Could not find the process: gtutorial-x86_64.exe");
                break; // Restart main loop
            }

            if (isActive) {

                // Try to open the target process with necessary permissions.
                HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, targetProcessId);
                if (!hProcess) {
                    DisplayErrorAndWait("Failed to open the target process. Error code: " + std::to_string(GetLastError()));
                    break; // Restart main loop
                }

                // Get the base address of the target application.
                uintptr_t baseAddress = GetBaseAddress(targetProcessId, L"gtutorial-x86_64.exe");
                if (!baseAddress) {
                    std::cout << "Failed to get base address for gtutorial-x86_64.exe" << std::endl;
                    CloseHandle(hProcess);
                    ClearConsole();
                    break; // Restart main loop
                }

                DWORD targetProcessId = GetProcessID("gtutorial-x86_64.exe");
                if (targetProcessId == 0) {
                    DisplayErrorAndWait("Could not find the process: gtutorial-x86_64.exe");
                    break; // Restart main loop
                }

                // Calculate the final address which will be the player entity base address by following the multi-level pointer chain.
                uintptr_t finalAddress = GetFinalAddress(hProcess, baseAddress) + 0xF0;
                std::cout << std::hex << "0x" << finalAddress - 0xF0 << " + 0xF0" << " -> " << std::hex << "0x" << finalAddress << " <--- Player entity base address." << std::endl;

                // NOP instruction bytes to disable the function that updates Y coordinates for the player entity
                BYTE patchBytes[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
                const uintptr_t PATCH_ADDRESS = 0x100040F39;

                // Main loop: Get size of the process window and modify memory values with mouse coordinates.
                while (isActive) {
                    float xValue, yValue;
                    float mouseX, mouseY;
                    
                    // Patch the target memory location related to gravity with NOP.
                    if (!WriteProcessMemory(hProcess, (LPVOID)PATCH_ADDRESS, &patchBytes, sizeof(patchBytes), nullptr)) {
                        std::cerr << "Failed to write memory. Error: " << GetLastError() << std::endl;
                        break;
                    }

                    // Get the window handle of the target process.
                    HWND hWindow = GetProcessWindow(targetProcessId);
                    if (!hWindow) {
                        std::cerr << "Failed to get the window handle for process." << std::endl;
                        break;
                    }

                    // Fetch the mouse coordinates relative to the game window.
                    GetGameWindowNormalizedMouseCoords(hWindow, mouseX, mouseY);

                    // If the cursor is inside the game window, write the adjusted mouse coordinates to memory.
                    if (mouseX >= -1 && mouseX <= 1 && mouseY >= -1 && mouseY <= 1) {
                        WriteProcessMemory(hProcess, (LPVOID)(finalAddress + 0x24), &mouseX, sizeof(float), nullptr);
                        WriteProcessMemory(hProcess, (LPVOID)(finalAddress + 0x28), &mouseY, sizeof(float), nullptr);
                    }

                    // Read the memory values we just wrote.
                    ReadProcessMemory(hProcess, (LPVOID)(finalAddress + 0x24), &xValue, sizeof(float), nullptr);
                    ReadProcessMemory(hProcess, (LPVOID)(finalAddress + 0x28), &yValue, sizeof(float), nullptr);

                    // Use '\r' to return to the beginning of the line, and overwrite previous output.
                    RECT windowRect = GetWindowRectFromHandle(hWindow);

                    // Print out the values, including screen size and location.
                    std::cout << "\rX Value: " << xValue
                        << " | Y Value: " << yValue
                        << " | Window Location: (" << windowRect.left << ", " << windowRect.top << ")" << std::dec
                        << " | Window Size: (" << (windowRect.right - windowRect.left) << "x" << (windowRect.bottom - windowRect.top) << ")";  // Extra spaces to ensure full overwrite if needed

                    // Check again for the 'F' key press within the inner loop
                    if (GetAsyncKeyState('F') & 0x8000 && !fKeyPressedPreviously) {
                        isActive = !isActive;

                        if (!isActive) {
                            // Clear the console
                            ClearConsole();
                            if (!WriteProcessMemory(hProcess, (LPVOID)PATCH_ADDRESS, &originalPatchBytes, sizeof(originalPatchBytes), nullptr)) {
                                std::cerr << "Failed to restore original memory. Error: " << GetLastError() << std::endl;
                                break;
                            }

                            HWND hWindow = GetProcessWindow(targetProcessId);
                            if (!hWindow) {
                                std::cerr << "Failed to get the window handle for process." << std::endl;
                                break;
                            }
                        std::cout << (isActive ? "Flyhack activated! Press 'F' to deactivate." : "Flyhack deactivated!") << std::endl;
                        }
                        fKeyPressedPreviously = true;
                    }
                    else if (!(GetAsyncKeyState('F') & 0x8000)) {
                        fKeyPressedPreviously = false;
                    }
                } // end of isActive while loop
                CloseHandle(hProcess);
            }
            else {
                Sleep(10);
            }
        } // end of GetAsyncKeyState if condition
    } // end of inner while loop
} // end of outermost while loop