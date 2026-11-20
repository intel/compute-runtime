/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/windows_wrapper.h"
#include "shared/test/common/helpers/ult_output_guard.h"

#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <thread>

namespace UltOutputGuard {
namespace {
std::atomic<bool> active = false;
std::atomic<bool> streamOpen[2] = {false, false};
int savedFds[2] = {-1, -1};
HANDLE serverHandles[2] = {INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
HANDLE readyEvents[2] = {nullptr, nullptr};
HANDLE readEvents[2] = {nullptr, nullptr};
OVERLAPPED zeroByteReads[2] = {};
std::thread teeThread;

DWORD readAvailable(int stream, char *buffer, DWORD size) {
    OVERLAPPED readOverlapped = {};
    readOverlapped.hEvent = readEvents[stream];
    ResetEvent(readEvents[stream]);
    DWORD bytes = 0;
    if (!ReadFile(serverHandles[stream], buffer, size, nullptr, &readOverlapped) && GetLastError() != ERROR_IO_PENDING) {
        return 0;
    }
    if (!GetOverlappedResult(serverHandles[stream], &readOverlapped, &bytes, TRUE)) {
        return 0;
    }
    return bytes;
}

bool hasUnreadData(int stream) {
    DWORD available = 0;
    return PeekNamedPipe(serverHandles[stream], nullptr, 0, nullptr, &available, nullptr) && available > 0;
}

size_t copyAvailable(int stream) {
    char buffer[4096];
    size_t copied = 0;
    DWORD available = 0;
    while (PeekNamedPipe(serverHandles[stream], nullptr, 0, nullptr, &available, nullptr) && available > 0) {
        auto bytes = readAvailable(stream, buffer, available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer)));
        if (bytes == 0) {
            break;
        }
        _write(savedFds[stream], buffer, static_cast<unsigned int>(bytes));
        copied += bytes;
    }
    return copied;
}

bool queueZeroByteRead(int stream) {
    auto &overlapped = zeroByteReads[stream];
    ResetEvent(readyEvents[stream]);
    overlapped = {};
    overlapped.hEvent = readyEvents[stream];
    char unused = 0;
    if (ReadFile(serverHandles[stream], &unused, 0, nullptr, &overlapped)) {
        return true;
    }
    return GetLastError() == ERROR_IO_PENDING;
}

void closePipe(int stream) {
    for (auto handle : {serverHandles[stream], readyEvents[stream], readEvents[stream]}) {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
    serverHandles[stream] = INVALID_HANDLE_VALUE;
    readyEvents[stream] = nullptr;
    readEvents[stream] = nullptr;
}

HANDLE createPipe(int stream) {
    char name[128];
    snprintf(name, sizeof(name), "\\\\.\\pipe\\neo_ult_output_guard_%lu_%d", GetCurrentProcessId(), stream);
    serverHandles[stream] = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, 65536, 0, nullptr);
    readyEvents[stream] = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    readEvents[stream] = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (serverHandles[stream] == INVALID_HANDLE_VALUE || readyEvents[stream] == nullptr || readEvents[stream] == nullptr) {
        closePipe(stream);
        return INVALID_HANDLE_VALUE;
    }
    auto client = CreateFileA(name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (client == INVALID_HANDLE_VALUE || !queueZeroByteRead(stream)) {
        if (client != INVALID_HANDLE_VALUE) {
            CloseHandle(client);
        }
        closePipe(stream);
        return INVALID_HANDLE_VALUE;
    }
    return client;
}

void teeLoop() {
    while (streamOpen[0] || streamOpen[1]) {
        HANDLE waitHandles[2];
        int streams[2];
        DWORD count = 0;
        for (int i = 0; i < 2; i++) {
            if (streamOpen[i]) {
                waitHandles[count] = readyEvents[i];
                streams[count++] = i;
            }
        }
        auto result = WaitForMultipleObjects(count, waitHandles, FALSE, INFINITE);
        if (result >= WAIT_OBJECT_0 + count) {
            return;
        }
        auto stream = streams[result - WAIT_OBJECT_0];
        markBusy();
        DWORD transferred = 0;
        if (GetOverlappedResult(serverHandles[stream], &zeroByteReads[stream], &transferred, FALSE)) {
            capturedBytes += copyAvailable(stream);
            streamOpen[stream] = queueZeroByteRead(stream);
        } else {
            streamOpen[stream] = false;
        }
        markIdle();
    }
}
} // namespace

void start() {
    fflush(stdout);
    fflush(stderr);
    HANDLE clients[2];
    for (int i = 0; i < 2; i++) {
        clients[i] = createPipe(i);
        if (clients[i] == INVALID_HANDLE_VALUE) {
            for (int j = 0; j < i; j++) {
                CloseHandle(clients[j]);
                closePipe(j);
            }
            return;
        }
    }
    for (int i = 0; i < 2; i++) {
        auto clientFd = _open_osfhandle(reinterpret_cast<intptr_t>(clients[i]), _O_BINARY | _O_WRONLY);
        savedFds[i] = _dup(i + 1);
        _dup2(clientFd, i + 1);
        _close(clientFd);
        streamOpen[i] = true;
    }
    active = true;
    teeThread = std::thread(teeLoop);
}

bool pipesHaveUnreadData() {
    if (!active) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        if (streamOpen[i] && hasUnreadData(i)) {
            return true;
        }
    }
    return false;
}

void stop() {
    if (!active) {
        return;
    }
    drain();
    active = false;
    for (int i = 0; i < 2; i++) {
        _dup2(savedFds[i], i + 1);
    }
    teeThread.join();
    for (int i = 0; i < 2; i++) {
        _close(savedFds[i]);
        savedFds[i] = -1;
        closePipe(i);
    }
}

void onFatal() {
    if (!active) {
        return;
    }
    fflush(stdout);
    fflush(stderr);
    for (int i = 0; i < 2; i++) {
        _dup2(savedFds[i], i + 1);
        copyAvailable(i);
    }
}
} // namespace UltOutputGuard
