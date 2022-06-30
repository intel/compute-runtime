/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_thread_linux.h"

#include <sched.h>

namespace NEO {
ThreadLinux::ThreadLinux(pthread_t threadId) : threadId(threadId){};

std::unique_ptr<Thread> Thread::create(void *(*func)(void *), void *arg) {
    pthread_t threadId;
    pthread_create(&threadId, nullptr, func, arg);
    return std::unique_ptr<Thread>(new ThreadLinux(threadId));
}

void ThreadLinux::join() {
    pthread_join(threadId, nullptr);
}

void ThreadLinux::yield() {
    sched_yield();
}

} // namespace NEO
