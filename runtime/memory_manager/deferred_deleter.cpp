/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/deferred_deleter.h"

#include "runtime/memory_manager/deferrable_deletion.h"
#include "runtime/os_interface/os_thread.h"

namespace NEO {
DeferredDeleter::DeferredDeleter() {
    doWorkInBackground = false;
    elementsToRelease = 0;
}

void DeferredDeleter::stop() {
    // Called with threadMutex acquired
    if (worker != nullptr) {
        // Working thread was created so we can safely stop it
        std::unique_lock<std::mutex> lock(queueMutex);
        // Make sure that working thread really started
        while (!doWorkInBackground) {
            lock.unlock();
            lock.lock();
        }
        // Signal working thread to finish its job
        doWorkInBackground = false;
        lock.unlock();
        condition.notify_one();
        // Wait for the working job to exit
        worker->join();
        // Delete working thread
        worker.reset();
    }
    drain(false);
}

void DeferredDeleter::safeStop() {
    std::lock_guard<std::mutex> lock(threadMutex);
    stop();
}

DeferredDeleter::~DeferredDeleter() {
    safeStop();
}

void DeferredDeleter::deferDeletion(DeferrableDeletion *deletion) {
    std::unique_lock<std::mutex> lock(queueMutex);
    elementsToRelease++;
    queue.pushTailOne(*deletion);
    lock.unlock();
    condition.notify_one();
}

void DeferredDeleter::addClient() {
    std::lock_guard<std::mutex> lock(threadMutex);
    ++numClients;
    ensureThread();
}

void DeferredDeleter::removeClient() {
    std::lock_guard<std::mutex> lock(threadMutex);
    --numClients;
    if (numClients == 0) {
        stop();
    }
}

void DeferredDeleter::ensureThread() {
    if (worker != nullptr) {
        return;
    }
    worker = Thread::create(run, reinterpret_cast<void *>(this));
}

bool DeferredDeleter::areElementsReleased() {
    return elementsToRelease == 0;
}

bool DeferredDeleter::shouldStop() {
    return !doWorkInBackground;
}

void *DeferredDeleter::run(void *arg) {
    auto self = reinterpret_cast<DeferredDeleter *>(arg);
    std::unique_lock<std::mutex> lock(self->queueMutex);
    // Mark that working thread really started
    self->doWorkInBackground = true;
    do {
        if (self->queue.peekIsEmpty()) {
            // Wait for signal that some items are ready to be deleted
            self->condition.wait(lock);
        }
        lock.unlock();
        // Delete items placed into deferred delete queue
        self->clearQueue();
        lock.lock();
        // Check whether working thread should be stopped
    } while (!self->shouldStop());
    lock.unlock();
    return nullptr;
}

void DeferredDeleter::drain(bool blocking) {
    clearQueue();
    if (blocking) {
        while (!areElementsReleased())
            ;
    }
}

void DeferredDeleter::clearQueue() {
    do {
        auto deletion = queue.removeFrontOne();
        if (deletion) {
            if (deletion->apply()) {
                elementsToRelease--;
            } else {
                queue.pushTailOne(*deletion.release());
            }
        }
    } while (!queue.peekIsEmpty());
}
} // namespace NEO
