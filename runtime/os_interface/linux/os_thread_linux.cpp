/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/os_thread_linux.h"

namespace OCLRT {
ThreadLinux::ThreadLinux(pthread_t threadId) : threadId(threadId){};

std::unique_ptr<Thread> Thread::create(void *(*func)(void *), void *arg) {
    pthread_t threadId;
    pthread_create(&threadId, nullptr, func, arg);
    return std::unique_ptr<Thread>(new ThreadLinux(threadId));
}

void ThreadLinux::join() {
    pthread_join(threadId, nullptr);
}
} // namespace OCLRT
