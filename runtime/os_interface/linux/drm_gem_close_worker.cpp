/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <atomic>
#include <iostream>
#include <queue>
#include <stdio.h>
#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_gem_close_worker.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/os_thread.h"

namespace OCLRT {

DrmGemCloseWorker::DrmGemCloseWorker(DrmMemoryManager &memoryManager) : memoryManager(memoryManager) {
    thread = Thread::create(worker, reinterpret_cast<void *>(this));
}

void DrmGemCloseWorker::closeThread() {
    if (thread) {
        while (!workerDone.load()) {
            condition.notify_all();
        }

        thread->join();
        thread.reset();
    }
}

DrmGemCloseWorker::~DrmGemCloseWorker() {
    active = false;
    closeThread();
}

void DrmGemCloseWorker::push(BufferObject *bo) {
    std::unique_lock<std::mutex> lock(closeWorkerMutex);
    workCount++;
    queue.push(bo);
    lock.unlock();
    condition.notify_one();
}

void DrmGemCloseWorker::close(bool blocking) {
    active = false;
    condition.notify_all();
    if (blocking) {
        closeThread();
    }
}

bool DrmGemCloseWorker::isEmpty() {
    return workCount.load() == 0;
}

inline void DrmGemCloseWorker::close(BufferObject *bo) {
    bo->wait(-1);
    memoryManager.unreference(bo);
    workCount--;
}

void *DrmGemCloseWorker::worker(void *arg) {
    DrmGemCloseWorker *self = reinterpret_cast<DrmGemCloseWorker *>(arg);
    BufferObject *workItem = nullptr;
    std::queue<BufferObject *> localQueue;
    std::unique_lock<std::mutex> lock(self->closeWorkerMutex);
    lock.unlock();

    while (self->active) {
        lock.lock();
        workItem = nullptr;

        while (self->queue.empty() && self->active) {
            self->condition.wait(lock);
        }

        if (!self->queue.empty()) {
            localQueue.swap(self->queue);
        }

        lock.unlock();
        while (!localQueue.empty()) {
            workItem = localQueue.front();
            localQueue.pop();
            self->close(workItem);
        }
    }

    lock.lock();
    while (!self->queue.empty()) {
        workItem = self->queue.front();
        self->queue.pop();
        self->close(workItem);
    }

    lock.unlock();
    self->workerDone.store(true);
    return nullptr;
}
}
