/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"

#include <cstdlib>
#include <string>

namespace NEO {

unsigned int getPid() {
    return GetCurrentProcessId();
}

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

unsigned long getNumThreads() {
    return 1;
}

DWORD getLastError() {
    return GetLastError();
}

bool pathExists(const std::string &path) {
    DWORD ret = GetFileAttributesA(path.c_str());

    return ret == FILE_ATTRIBUTE_DIRECTORY;
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

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    return RegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    return RegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}
} // namespace SysCalls

} // namespace NEO
