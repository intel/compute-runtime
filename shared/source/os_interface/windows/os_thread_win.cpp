/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_thread_win.h"
namespace NEO {
ThreadWin::ThreadWin(std::thread *thread) {
    this->thread.reset(thread);
};

decltype(&Thread::create) Thread::createFunc = Thread::create;

std::unique_ptr<Thread> Thread::create(void *(*func)(void *), void *arg) {
    return std::unique_ptr<Thread>(new ThreadWin(new std::thread(func, arg)));
}

void ThreadWin::join() {
    thread->join();
}

void ThreadWin::yield() {
    std::this_thread::yield();
}
} // namespace NEO
