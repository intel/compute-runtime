/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_gem_close_worker.h"

#include "core/helpers/aligned_memory.h"
#include "runtime/os_interface/linux/drm_buffer_object.h"
#include "runtime/os_interface/linux/drm_command_stream.h"
#include "runtime/os_interface/linux/drm_memory_manager.h"
#include "runtime/os_interface/os_thread.h"

#include <atomic>
#include <iostream>
#include <queue>
#include <stdio.h>

namespace NEO {

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
    memoryManager.unreference(bo, false);
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
} // namespace NEO
