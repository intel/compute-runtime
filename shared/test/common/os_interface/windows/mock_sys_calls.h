/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/sys_calls.h"

using mockCreateEventClbT = HANDLE (*)(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName, void *data);
inline mockCreateEventClbT mockCreateEventClb = nullptr;
inline void *mockCreateEventClbData = nullptr;

using mockCloseHandleClbT = BOOL (*)(HANDLE hObject, void *data);
inline mockCloseHandleClbT mockCloseHandleClb = nullptr;
inline void *mockCloseHandleClbData = nullptr;

constexpr uintptr_t dummyHandle = static_cast<uintptr_t>(0x7);
inline HMODULE handleValue = reinterpret_cast<HMODULE>(dummyHandle);

template <typename CallbackT>
struct MockGlobalSysCallRestorer {
    MockGlobalSysCallRestorer(CallbackT &globalClb, void *&globalClbData)
        : globalClb(globalClb), globalClbData(globalClbData) {
        callbackPrev = globalClb;
        callbackData = globalClbData;
    }
    ~MockGlobalSysCallRestorer() {
        if (restoreOnDtor) {
            globalClb = callbackPrev;
            globalClbData = callbackData;
        }
    }
    MockGlobalSysCallRestorer(const MockGlobalSysCallRestorer &rhs) = delete;
    MockGlobalSysCallRestorer &operator=(const MockGlobalSysCallRestorer &rhs) = delete;
    MockGlobalSysCallRestorer(MockGlobalSysCallRestorer &&rhs) noexcept
        : globalClb(rhs.globalClb), globalClbData(rhs.globalClbData) {
        callbackPrev = rhs.callbackPrev;
        callbackData = rhs.callbackData;
        rhs.restoreOnDtor = false;
    }
    MockGlobalSysCallRestorer &operator=(MockGlobalSysCallRestorer &&rhs) = delete;

    CallbackT callbackPrev, &globalClb;
    void *callbackData, *&globalClbData;
    bool restoreOnDtor = true;
};

template <typename CallbackT>
MockGlobalSysCallRestorer<CallbackT> changeSysCallMock(CallbackT &globalClb, void *&globalClbData,
                                                       CallbackT mockCallback, void *mockCallbackData) {
    MockGlobalSysCallRestorer<CallbackT> ret{globalClb, globalClbData};
    globalClb = mockCallback;
    globalClbData = mockCallbackData;
    return ret;
}

namespace NEO {

namespace SysCalls {

extern HANDLE (*sysCallsCreateFile)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
extern BOOL (*sysCallsDeviceIoControl)(HANDLE hDevice, DWORD dwIoControlCode, LPVOID lpInBuffer, DWORD nInBufferSize, LPVOID lpOutBuffer, DWORD nOutBufferSize, LPDWORD lpBytesReturned, LPOVERLAPPED lpOverlapped);
extern CONFIGRET (*sysCallsCmGetDeviceInterfaceListSize)(PULONG pulLen, LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, ULONG ulFlags);
extern CONFIGRET (*sysCallsCmGetDeviceInterfaceList)(LPGUID interfaceClassGuid, DEVINSTID_W pDeviceID, PZZWSTR buffer, ULONG bufferLen, ULONG ulFlags);
extern LPVOID (*sysCallsHeapAlloc)(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
extern BOOL (*sysCallsHeapFree)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

extern BOOL (*sysCallsGetModuleHandleExW)(DWORD dwFlags, LPCWSTR lpModuleName, HMODULE *phModule);
extern DWORD (*sysCallsGetModuleFileNameW)(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
extern DWORD (*sysCallsGetFileVersionInfoSizeW)(LPCWSTR lptstrFilename, LPDWORD lpdwHandle);
extern BOOL (*sysCallsGetFileVersionInfoW)(LPCWSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
extern BOOL (*sysCallsVerQueryValueW)(LPCVOID pBlock, LPCWSTR lpSubBlock, LPVOID *lplpBuffer, PUINT puLen);
extern DWORD (*sysCallsGetLastError)();
} // namespace SysCalls
} // namespace NEO