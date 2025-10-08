// FIX 1: Define UNICODE and _UNICODE before including windows.h
// This ensures that Windows API functions like OpenService resolve to their
// wide-character (Unicode) versions (e.g., OpenServiceW).
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

// Helper to print a descriptive error message from a Windows error code.
void PrintErrorMessage(const std::wstring& failedFunction, DWORD errorCode) {
    LPWSTR messageBuffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);

    std::wcerr << L"Error: " << failedFunction << L" failed.\n"
               << L"Code: " << errorCode << L" - " << (messageBuffer ? messageBuffer : L"Unable to format message.")
               << std::endl;

    LocalFree(messageBuffer);
}

// Custom deleter for SC_HANDLE to be used with std::unique_ptr for RAII.
struct ServiceHandleDeleter {
    // FIX 2: The deleter must accept the pointer type held by the unique_ptr (void*).
    void operator()(void* handle) const {
        if (handle) {
            CloseServiceHandle(static_cast<SC_HANDLE>(handle));
        }
    }
};

using ScHandlePtr = std::unique_ptr<void, ServiceHandleDeleter>;

// Waits for a service to reach a specified state (e.g., SERVICE_RUNNING or SERVICE_STOPPED).
bool WaitForServiceStatus(SC_HANDLE serviceHandle, DWORD targetStatus, DWORD timeoutMillis = 30000) {
    SERVICE_STATUS_PROCESS ssp;
    DWORD bytesNeeded;
    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        if (!QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded)) {
            PrintErrorMessage(L"QueryServiceStatusEx", GetLastError());
            return false;
        }

        if (ssp.dwCurrentState == targetStatus) {
            return true; // Reached the target state
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        if (elapsed.count() >= timeoutMillis) {
            std::wcerr << L"Error: Timeout waiting for service to reach state " << targetStatus << std::endl;
            return false;
        }

        // Wait a bit before polling again.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

// Stops the specified service and waits for it to stop.
bool StopServiceByName(const wchar_t* serviceName) {
    ScHandlePtr scmHandle(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scmHandle) {
        PrintErrorMessage(L"OpenSCManager", GetLastError());
        return false;
    }

    // FIX 3: Explicitly cast the result of .get() from void* to SC_HANDLE.
    ScHandlePtr serviceHandle(OpenService(static_cast<SC_HANDLE>(scmHandle.get()), serviceName, SERVICE_STOP | SERVICE_QUERY_STATUS));
    if (!serviceHandle) {
        PrintErrorMessage(L"OpenService", GetLastError());
        return false;
    }

    SERVICE_STATUS_PROCESS ssp;
    // Send the stop signal.
    if (!ControlService(static_cast<SC_HANDLE>(serviceHandle.get()), SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
        DWORD err = GetLastError();
        // ERROR_SERVICE_NOT_ACTIVE means it's already stopped, which is a success for us.
        if (err != ERROR_SERVICE_NOT_ACTIVE) {
            PrintErrorMessage(L"ControlService", err);
            return false;
        }
    }
    
    std::wcout << L"Stop request sent. Waiting for service to terminate..." << std::endl;
    return WaitForServiceStatus(static_cast<SC_HANDLE>(serviceHandle.get()), SERVICE_STOPPED);
}

// Starts the specified service and waits for it to be running.
bool StartServiceByName(const wchar_t* serviceName) {
    ScHandlePtr scmHandle(OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!scmHandle) {
        PrintErrorMessage(L"OpenSCManager", GetLastError());
        return false;
    }
    
    // FIX 3: Explicitly cast the result of .get() from void* to SC_HANDLE.
    ScHandlePtr serviceHandle(OpenService(static_cast<SC_HANDLE>(scmHandle.get()), serviceName, SERVICE_START | SERVICE_QUERY_STATUS));
    if (!serviceHandle) {
        PrintErrorMessage(L"OpenService", GetLastError());
        return false;
    }

    if (!StartService(static_cast<SC_HANDLE>(serviceHandle.get()), 0, nullptr)) {
        DWORD err = GetLastError();
        // ERROR_SERVICE_ALREADY_RUNNING is a success condition in this context.
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            PrintErrorMessage(L"StartService", err);
            return false;
        }
        std::wcout << L"Service is already running." << std::endl;
        return true;
    }
    
    std::wcout << L"Start request sent. Waiting for service to run..." << std::endl;
    return WaitForServiceStatus(static_cast<SC_HANDLE>(serviceHandle.get()), SERVICE_RUNNING);
}

int main() {
    const wchar_t* serviceName = L"w32time"; // Windows Time service

    std::wcout << L"--- Attempting to stop the '" << serviceName << L"' service... ---" << std::endl;
    if (StopServiceByName(serviceName)) {
        std::wcout << L"Service successfully stopped.\n" << std::endl;

        std::wcout << L"--- Attempting to start the '" << serviceName << L"' service... ---" << std::endl;
        if (StartServiceByName(serviceName)) {
            std::wcout << L"Service successfully started.\n" << std::endl;

            std::wcout << L"--- Resyncing system time... ---" << std::endl;
            // system() is simple but pops up a cmd window. For a console app, this is fine.
            int resyncResult = system("w32tm /resync /nowait");
            if (resyncResult == 0) {
                 std::wcout << L"Time resync command sent successfully." << std::endl;
            } else {
                 std::wcerr << L"Failed to execute time resync command." << std::endl;
            }
        } else {
            std::wcerr << L"Failed to start the service." << std::endl;
        }
    } else {
        std::wcerr << L"Failed to stop the service. Aborting." << std::endl;
    }

    std::wcout << L"\nDone." << std::endl;
    return 0;
}