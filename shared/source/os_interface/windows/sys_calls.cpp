/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"

#include <cstdlib>
#include <string>

namespace NEO {

bool isShutdownInProgress() {
    auto handle = GetModuleHandleA("ntdll.dll");

    if (!handle) {
        return true;
    }

    auto rtlDllShutdownInProgress = reinterpret_cast<BOOLEAN(WINAPI *)()>(GetProcAddress(handle, "RtlDllShutdownInProgress"));
    return rtlDllShutdownInProgress();
}

namespace SysCalls {
void exit(int code) {
    std::exit(code);
}

unsigned int getProcessId() {
    return GetCurrentProcessId();
}

unsigned int getCurrentProcessId() {
    return GetCurrentProcessId();
}

BOOL duplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions) {
    return DuplicateHandle(hSourceProcessHandle, hSourceHandle, hTargetProcessHandle, lpTargetHandle, dwDesiredAccess, bInheritHandle, dwOptions);
}

HANDLE openProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    return OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
}

unsigned long getNumThreads() {
    return 1;
}

DWORD getLastError() {
    return GetLastError();
}

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName);
}

BOOL closeHandle(HANDLE hObject) {
    return CloseHandle(hObject);
}

BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr) {
    return GetSystemPowerStatus(systemPowerStatusPtr);
}
BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    return GetModuleHandleEx(dwFlags, lpModuleName, phModule);
}
DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    return GetModuleFileName(hModule, lpFilename, nSize);
}

UINT getTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName) {
    return GetTempFileNameA(lpPathName, lpPrefixString, uUnique, lpTempFileName);
}

BOOL moveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
    return MoveFileExA(lpExistingFileName, lpNewFileName, dwFlags);
}

BOOL lockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
    return LockFileEx(hFile, dwFlags, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

BOOL unlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
    return UnlockFileEx(hFile, dwReserved, nNumberOfBytesToLockLow, nNumberOfBytesToLockHigh, lpOverlapped);
}

BOOL getOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait) {
    return GetOverlappedResult(hFile, lpOverlapped, lpNumberOfBytesTransferred, bWait);
}

BOOL createDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    return CreateDirectoryA(lpPathName, lpSecurityAttributes);
}

HANDLE createFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    return CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL deleteFileA(LPCSTR lpFileName) {
    return DeleteFileA(lpFileName);
}

HRESULT shGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPat) {
    return SHGetKnownFolderPath(rfid, dwFlags, hToken, ppszPat);
}

BOOL readFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
}

BOOL writeFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    return WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

HANDLE findFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    return FindFirstFileA(lpFileName, lpFindFileData);
}

BOOL findNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    return FindNextFileA(hFindFile, lpFindFileData);
}

BOOL findClose(HANDLE hFindFile) {
    return FindClose(hFindFile);
}

DWORD getFileAttributesA(LPCSTR lpFileName) {
    return GetFileAttributesA(lpFileName);
}

DWORD setFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    return SetFilePointer(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
}

void coTaskMemFree(LPVOID pv) {
    CoTaskMemFree(pv);
}

void setProcessPowerThrottlingState(ProcessPowerThrottlingState state) {
    PROCESS_POWER_THROTTLING_STATE prio;
    prio.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    prio.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

    prio.StateMask = state == ProcessPowerThrottlingState::Eco ? PROCESS_POWER_THROTTLING_EXECUTION_SPEED : 0;
    SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &prio, sizeof(prio));
}

void setThreadPriority(ThreadPriority priority) {
    if (ThreadPriority::AboveNormal == priority) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    }
}

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return RegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

HANDLE createFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    return CreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL deviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    return DeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
}

CONFIGRET cmGetDeviceInterfaceListSize(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) {
    return CM_Get_Device_Interface_List_Size(pulLen, interfaceClassGuid, pDeviceID, ulFlags);
}

CONFIGRET cmGetDeviceInterfaceList(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) {
    return CM_Get_Device_Interface_List(interfaceClassGuid, pDeviceID, buffer, bufferLen, ulFlags);
}

LPVOID heapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    return HeapAlloc(hHeap, dwFlags, dwBytes);
}

BOOL heapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    return HeapFree(hHeap, dwFlags, lpMem);
}

SIZE_T virtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    return VirtualQuery(lpAddress, lpBuffer, dwLength);
}

BOOL getModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    return ::GetModuleHandleExW(dwFlags, lpModuleName, phModule);
}
DWORD getModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    return ::GetModuleFileNameW(hModule, lpFilename, nSize);
}
DWORD getFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    return ::GetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
}
BOOL getFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    return ::GetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
}
BOOL verQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lpBuffer, PUINT puLen) {
    return ::VerQueryValueW(pBlock, lpSubBlock, lpBuffer, puLen);
}
LPWCH getEnvironmentStringsW() {
    return ::GetEnvironmentStringsW();
}

BOOL freeEnvironmentStringsW(LPWCH env) {
    return ::FreeEnvironmentStringsW(env);
}
} // namespace SysCalls
} // namespace NEO
