/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"

#include "shared/source/os_interface/windows/os_inc.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace NEO {

namespace SysCalls {

unsigned int getProcessId() {
    return 0xABCEDF;
}

unsigned int getCurrentProcessId() {
    return 0xABCEDF;
}

unsigned long getNumThreads() {
    return 1;
}

DWORD getLastErrorResult = 0u;

BOOL systemPowerStatusRetVal = 1;
BYTE systemPowerStatusACLineStatusOverride = 1;
const wchar_t *currentLibraryPath = L"";
uint32_t regOpenKeySuccessCount = 0u;
uint32_t regQueryValueSuccessCount = 0u;
uint64_t regQueryValueExpectedData = 0ull;
const HKEY validHkey = reinterpret_cast<HKEY>(0);
bool getNumThreadsCalled = false;
bool mmapAllowExtendedPointers = false;
const char *driverStorePath = nullptr;

size_t closeHandleCalled = 0u;

size_t getTempFileNameACalled = 0u;
UINT getTempFileNameAResult = 0u;

size_t lockFileExCalled = 0u;
BOOL lockFileExResult = TRUE;

size_t unlockFileExCalled = 0u;
BOOL unlockFileExResult = TRUE;

size_t createDirectoryACalled = 0u;
BOOL createDirectoryAResult = TRUE;

size_t createFileACalled = 0u;
extern const size_t createFileAResultsCount = 4;
HANDLE createFileAResults[createFileAResultsCount] = {nullptr, nullptr, nullptr, nullptr};

size_t deleteFileACalled = 0u;
const size_t deleteFilesCount = 4;
constexpr size_t deleteFilesMaxLength = 256;
char deleteFiles[deleteFilesCount][deleteFilesMaxLength] = {{0}};

HRESULT shGetKnownFolderPathResult = 0;
extern const size_t shGetKnownFolderSetPathSize = 50;
wchar_t shGetKnownFolderSetPath[shGetKnownFolderSetPathSize];

bool callBaseReadFile = true;
BOOL readFileResult = TRUE;
size_t readFileCalled = 0u;
size_t readFileBufferData = 0u;

size_t writeFileCalled = 0u;
BOOL writeFileResult = true;
extern const size_t writeFileBufferSize = 10;
char writeFileBuffer[writeFileBufferSize];
DWORD writeFileNumberOfBytesWritten = 0u;

HANDLE findFirstFileAResult = nullptr;

size_t findNextFileACalled = 0u;
extern const size_t findNextFileAFileDataCount = 4;
WIN32_FIND_DATAA findNextFileAFileData[findNextFileAFileDataCount];

size_t findCloseCalled = 0u;

size_t getFileAttributesCalled = 0u;
DWORD getFileAttributesResult = INVALID_FILE_ATTRIBUTES;
std::unordered_map<std::string, DWORD> pathAttributes;

size_t setFilePointerCalled = 0u;
DWORD setFilePointerResult = 0;

size_t setProcessPowerThrottlingStateCalled = 0u;
ProcessPowerThrottlingState setProcessPowerThrottlingStateLastValue{};

size_t setThreadPriorityCalled = 0u;
ThreadPriority setThreadPriorityLastValue{};

HANDLE(*sysCallsCreateFile)
(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) = nullptr;

BOOL(*sysCallsDeviceIoControl)
(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) = nullptr;

CONFIGRET(*sysCallsCmGetDeviceInterfaceListSize)
(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) = nullptr;

CONFIGRET(*sysCallsCmGetDeviceInterfaceList)
(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) = nullptr;

LPVOID(*sysCallsHeapAlloc)
(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) = nullptr;

BOOL(*sysCallsHeapFree)
(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) = nullptr;

void exit(int code) {
}

DWORD getLastError() {
    return getLastErrorResult;
}

HANDLE createEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName) {
    if (mockCreateEventClb) {
        return mockCreateEventClb(lpEventAttributes, bManualReset, bInitialState, lpName, mockCreateEventClbData);
    }
    return reinterpret_cast<HANDLE>(dummyHandle);
}

BOOL closeHandle(HANDLE hObject) {
    closeHandleCalled++;
    if (mockCloseHandleClb) {
        return mockCloseHandleClb(hObject, mockCloseHandleClbData);
    }
    return (reinterpret_cast<HANDLE>(dummyHandle) == hObject) ? TRUE : FALSE;
}

BOOL getSystemPowerStatus(LPSYSTEM_POWER_STATUS systemPowerStatusPtr) {
    systemPowerStatusPtr->ACLineStatus = systemPowerStatusACLineStatusOverride;
    return systemPowerStatusRetVal;
}

BOOL getModuleHandle(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    constexpr auto expectedFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    if (dwFlags != expectedFlags) {
        return FALSE;
    }
    *phModule = handleValue;
    return TRUE;
}

DWORD getModuleFileName(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    lstrcpyW(lpFilename, currentLibraryPath);
    return TRUE;
}

UINT getTempFileNameA(LPCSTR lpPathName, LPCSTR lpPrefixString, UINT uUnique, LPSTR lpTempFileName) {
    getTempFileNameACalled++;
    return getTempFileNameAResult;
}

BOOL moveFileExA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, DWORD dwFlags) {
    return TRUE;
}

BOOL lockFileEx(HANDLE hFile, DWORD dwFlags, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
    lockFileExCalled++;
    return lockFileExResult;
}

BOOL unlockFileEx(HANDLE hFile, DWORD dwReserved, DWORD nNumberOfBytesToLockLow, DWORD nNumberOfBytesToLockHigh, LPOVERLAPPED lpOverlapped) {
    unlockFileExCalled++;
    return unlockFileExResult;
}

BOOL getOverlappedResult(HANDLE hFile, LPOVERLAPPED lpOverlapped, LPDWORD lpNumberOfBytesTransferred, BOOL bWait) {
    return TRUE;
}

BOOL createDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES lpSecurityAttributes) {
    createDirectoryACalled++;
    if (createDirectoryAResult) {
        pathAttributes[lpPathName] = FILE_ATTRIBUTE_DIRECTORY;
    }
    return createDirectoryAResult;
}

HANDLE createFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    HANDLE retVal = nullptr;

    if (createFileACalled < createFileAResultsCount) {
        retVal = createFileAResults[createFileACalled];
    }

    createFileACalled++;
    return retVal;
}

BOOL deleteFileA(LPCSTR lpFileName) {
    if (deleteFileACalled < deleteFilesCount) {
        memcpy_s(deleteFiles[deleteFileACalled], deleteFilesMaxLength, lpFileName, strlen(lpFileName));
    }
    deleteFileACalled++;
    return TRUE;
}

HRESULT shGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPat) {
    *ppszPat = shGetKnownFolderSetPath;
    return shGetKnownFolderPathResult;
}

BOOL readFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    readFileCalled++;
    if (callBaseReadFile) {
        return ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }
    if (lpBuffer) {
        *static_cast<size_t *>(lpBuffer) = readFileBufferData;
    }
    return readFileResult;
}

BOOL writeFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    writeFileCalled++;
    memcpy_s(writeFileBuffer, writeFileBufferSize, lpBuffer, nNumberOfBytesToWrite);
    if (lpNumberOfBytesWritten) {
        *lpNumberOfBytesWritten = writeFileNumberOfBytesWritten;
    }
    return writeFileResult;
}

HANDLE findFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    return findFirstFileAResult;
}

BOOL findNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    BOOL retVal = TRUE;

    if (findNextFileACalled < findNextFileAFileDataCount) {
        *lpFindFileData = findNextFileAFileData[findNextFileACalled];
    } else {
        retVal = FALSE;
    }
    findNextFileACalled++;

    return retVal;
}

BOOL findClose(HANDLE hFindFile) {
    findCloseCalled++;
    return TRUE;
}

DWORD getFileAttributesA(LPCSTR lpFileName) {
    getFileAttributesCalled++;

    std::string tempP1 = lpFileName;
    if (!tempP1.empty() && tempP1.back() == PATH_SEPARATOR) {
        tempP1.pop_back();
    }

    for (const auto &[path, attributes] : pathAttributes) {
        if (path.empty())
            continue;

        std::string tempP2 = path;
        if (tempP2.back() == PATH_SEPARATOR) {
            tempP2.pop_back();
        }

        if (tempP1 == tempP2) {
            return attributes;
        }
    }

    return getFileAttributesResult;
}

DWORD setFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    setFilePointerCalled++;
    return setFilePointerResult;
}

void coTaskMemFree(LPVOID pv) {
    return;
}

void setProcessPowerThrottlingState(ProcessPowerThrottlingState state) {
    setProcessPowerThrottlingStateCalled++;
    setProcessPowerThrottlingStateLastValue = state;
}

void setThreadPriority(ThreadPriority priority) {
    setThreadPriorityCalled++;
    setThreadPriorityLastValue = priority;
}

HANDLE createFile(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    if (sysCallsCreateFile != nullptr) {
        return sysCallsCreateFile(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    }
    return nullptr;
}

BOOL deviceIoControl(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped) {
    if (sysCallsDeviceIoControl != nullptr) {
        return sysCallsDeviceIoControl(hDevice, dwIoControlCode, lpInBuffer, nInBufferSize, lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);
    }
    return false;
}

CONFIGRET cmGetDeviceInterfaceListSize(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags) {
    if (sysCallsCmGetDeviceInterfaceListSize != nullptr) {
        return sysCallsCmGetDeviceInterfaceListSize(pulLen, interfaceClassGuid, pDeviceID, ulFlags);
    }
    return -1;
}

CONFIGRET cmGetDeviceInterfaceList(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags) {
    if (sysCallsCmGetDeviceInterfaceList != nullptr) {
        return sysCallsCmGetDeviceInterfaceList(interfaceClassGuid, pDeviceID, buffer, bufferLen, ulFlags);
    }
    return -1;
}

LPVOID heapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    if (sysCallsHeapAlloc != nullptr) {
        return sysCallsHeapAlloc(hHeap, dwFlags, dwBytes);
    }
    return HeapAlloc(hHeap, dwFlags, dwBytes);
}

BOOL heapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    if (sysCallsHeapFree != nullptr) {
        return sysCallsHeapFree(hHeap, dwFlags, lpMem);
    }
    return HeapFree(hHeap, dwFlags, lpMem);
}

LSTATUS regOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
    if (regOpenKeySuccessCount > 0) {
        regOpenKeySuccessCount--;
        if (phkResult) {
            *phkResult = validHkey;
        }
        return ERROR_SUCCESS;
    }
    return ERROR_FILE_NOT_FOUND;
};

LSTATUS regQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
    if (hKey == validHkey && regQueryValueSuccessCount > 0) {
        regQueryValueSuccessCount--;

        if (lpcbData) {
            if (strcmp(lpValueName, "settingSourceString") == 0) {
                const auto settingSource = "registry";
                if (lpData) {
                    strcpy_s(reinterpret_cast<char *>(lpData), strlen(settingSource) + 1u, settingSource);
                } else {
                    *lpcbData = static_cast<DWORD>(strlen(settingSource) + 1u);
                    if (lpType) {
                        *lpType = REG_SZ;
                    }
                }
            } else if (strcmp(lpValueName, "settingSourceInt") == 0) {
                if (lpData) {
                    *reinterpret_cast<DWORD *>(lpData) = 1;
                } else {
                    *lpcbData = sizeof(DWORD);
                }
            } else if (strcmp(lpValueName, "settingSourceInt64") == 0) {
                if (lpData) {
                    *reinterpret_cast<INT64 *>(lpData) = 0xffffffffeeeeeeee;
                } else {
                    *lpcbData = sizeof(INT64);
                }
            } else if (strcmp(lpValueName, "settingSourceBinary") == 0) {
                const auto settingSource = L"registry";
                auto size = wcslen(settingSource) * sizeof(wchar_t);
                if (lpData) {
                    memcpy(reinterpret_cast<wchar_t *>(lpData), settingSource, size);
                } else {
                    *lpcbData = static_cast<DWORD>(size);
                    if (lpType) {
                        *lpType = REG_BINARY;
                    }
                }
            } else if (strcmp(lpValueName, "boolRegistryKey") == 0) {
                if (*lpcbData == sizeof(int64_t)) {
                    if (lpData) {
                        *reinterpret_cast<uint64_t *>(lpData) = regQueryValueExpectedData;
                    }
                }
            } else if (driverStorePath && (strcmp(lpValueName, "DriverStorePathForComputeRuntime") == 0)) {
                if (lpData) {
                    strcpy_s(reinterpret_cast<char *>(lpData), strlen(driverStorePath) + 1u, driverStorePath);
                }
                if (lpcbData) {
                    *lpcbData = static_cast<DWORD>(strlen(driverStorePath) + 1u);
                    if (lpType) {
                        *lpType = REG_SZ;
                    }
                }
            }
        }

        return ERROR_SUCCESS;
    }
    return ERROR_FILE_NOT_FOUND;
};

MEMORY_BASIC_INFORMATION virtualQueryMemoryBasicInformation = {};
SIZE_T virtualQuery(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    memcpy_s(lpBuffer, sizeof(MEMORY_BASIC_INFORMATION), &virtualQueryMemoryBasicInformation, sizeof(MEMORY_BASIC_INFORMATION));
    return sizeof(MEMORY_BASIC_INFORMATION);
}

BOOL(*sysCallsGetModuleHandleExW)
(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) = nullptr;
DWORD(*sysCallsGetModuleFileNameW)
(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) = nullptr;
DWORD(*sysCallsGetFileVersionInfoSizeW)
(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) = nullptr;
BOOL(*sysCallsGetFileVersionInfoW)
(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) = nullptr;
BOOL(*sysCallsVerQueryValueW)
(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) = nullptr;
DWORD(*sysCallsGetLastError)
() = nullptr;
BOOL getModuleHandleExW(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule) {
    if (sysCallsGetModuleHandleExW) {
        return sysCallsGetModuleHandleExW(dwFlags, lpModuleName, phModule);
    }
    return FALSE;
}
DWORD getModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize) {
    if (sysCallsGetModuleFileNameW) {
        return sysCallsGetModuleFileNameW(hModule, lpFilename, nSize);
    }
    return 0;
}
DWORD getFileVersionInfoSizeW(LPCWSTR lptstrFilename, LPDWORD lpdwHandle) {
    if (sysCallsGetFileVersionInfoSizeW) {
        return sysCallsGetFileVersionInfoSizeW(lptstrFilename, lpdwHandle);
    }
    return 0;
}
BOOL getFileVersionInfoW(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData) {
    if (sysCallsGetFileVersionInfoW) {
        return sysCallsGetFileVersionInfoW(lptstrFilename, dwHandle, dwLen, lpData);
    }
    return FALSE;
}
BOOL verQueryValueW(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen) {
    if (sysCallsVerQueryValueW) {
        return sysCallsVerQueryValueW(pBlock, lpSubBlock, lplpBuffer, puLen);
    }
    return FALSE;
}

} // namespace SysCalls

bool isShutdownInProgress() {
    return false;
}

unsigned int readEnablePreemptionRegKey() {
    return 1;
}

} // namespace NEO
