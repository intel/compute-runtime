/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/sys_calls_common.h"
#include "shared/source/os_interface/windows/windows_wrapper.h"

namespace NEO {

namespace SysCalls {

enum class ProcessPowerThrottlingState {
    Eco,
    High
};

enum class ThreadPriority {
    Normal,
    AboveNormal
};

DWORD getLastError();
HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
BOOL closeHandle(HANDLE hObject);
BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr);
BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule);
DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
UINT getTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName);
BOOL moveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags);
BOOL lockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);
BOOL unlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped);
BOOL getOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait);
BOOL createDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes);
HANDLE createFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL deleteFileA(LPCSTR lpFileName);
HRESULT shGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPat);
BOOL readFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL writeFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
HANDLE findFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
BOOL findNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
BOOL findClose(HANDLE hFindFile);
DWORD getFileAttributesA(LPCSTR lpFileName);
DWORD setFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL duplicateHandle(HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwOptions);
HANDLE openProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);

void setProcessPowerThrottlingState(ProcessPowerThrottlingState state);
void setThreadPriority(ThreadPriority priority);
void coTaskMemFree(LPVOID pv);

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
HANDLE createFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
BOOL deviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
CONFIGRET cmGetDeviceInterfaceListSize(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags);
CONFIGRET cmGetDeviceInterfaceList(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags);
LPVOID heapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
BOOL heapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
SIZE_T virtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
BOOL getModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule);
DWORD getModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
DWORD getFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle);
BOOL getFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
BOOL verQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen);
DWORD getLastError();
extern LPWCH mockEnvStringsW;
extern BOOL mockFreeEnvStringsWResult;
LPWCH getEnvironmentStringsW();
BOOL freeEnvironmentStringsW(LPWCH);
} // namespace SysCalls

} // namespace NEO
