/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/deferred_deleter.h"

#include "shared/source/memory_manager/deferrable_deletion.h"
#include "shared/source/os_interface/os_thread.h"

namespace NEO {
DeferredDeleter::DeferredDeleter() = default;

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
        worker->detach();
        // Wait for the working job to exit main loop
        while (!exitedMainLoop) {
            std::this_thread::yield();
        }
        // Delete working thread
        worker.reset();
    }
    drain(false, false);
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

    this->elementsToRelease++;
    if (deletion->isExternalHostptr()) {
        this->hostptrsToRelease++;
    }

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
    exitedMainLoop = false;
    worker = Thread::createFunc(run, reinterpret_cast<void *>(this));
}

bool DeferredDeleter::areElementsReleased(bool hostptrsOnly) {
    return hostptrsOnly ? this->hostptrsToRelease == 0 : this->elementsToRelease == 0;
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
        self->clearQueue(false);
        lock.lock();
        // Check whether working thread should be stopped
    } while (!self->shouldStop());
    lock.unlock();
    self->exitedMainLoop = true;
    return nullptr;
}

void DeferredDeleter::drain(bool blocking, bool hostptrsOnly) {
    clearQueue(hostptrsOnly);
    if (blocking) {
        while (!areElementsReleased(hostptrsOnly))
            ;
    }
}

void DeferredDeleter::clearQueue(bool hostptrsOnly) {
    do {
        auto deletion = queue.removeFrontOne();
        if (deletion) {
            bool isDeletionHostptr = deletion->isExternalHostptr();
            if ((!hostptrsOnly || isDeletionHostptr) && deletion->apply()) {
                this->elementsToRelease--;
                if (isDeletionHostptr) {
                    this->hostptrsToRelease--;
                }
            } else {
                queue.pushTailOne(*deletion.release());
            }
        }
    } while (hostptrsOnly ? !areElementsReleased(hostptrsOnly) : !queue.peekIsEmpty());
}
} // namespace NEO
