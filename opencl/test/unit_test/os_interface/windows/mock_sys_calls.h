/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "sys_calls.h"

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
    MockGlobalSysCallRestorer(MockGlobalSysCallRestorer &&rhs)
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
