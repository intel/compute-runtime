/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/os_thread_win.h"
namespace OCLRT {
ThreadWin::ThreadWin(std::thread *thread) {
    this->thread.reset(thread);
};

std::unique_ptr<Thread> Thread::create(void *(*func)(void *), void *arg) {
    return std::unique_ptr<Thread>(new ThreadWin(new std::thread(func, arg)));
}

void ThreadWin::join() {
    thread->join();
}
} // namespace OCLRT
