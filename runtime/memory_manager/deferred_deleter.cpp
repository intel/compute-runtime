/*
* Copyright (c) 2017, Intel Corporation
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

#include "runtime/memory_manager/deferred_deleter.h"
#include "runtime/memory_manager/deferrable_deletion.h"

namespace OCLRT {
DeferredDeleter::DeferredDeleter() {
    doWorkInBackground = false;
    threadLoaded = false;
    elementsToRelease = 0;
}

void DeferredDeleter::stop() {
    if (worker) {
        while (!threadLoaded)
            ;
        threadLoaded = false;
        doWorkInBackground = false;
        queueMutex.lock();
        queueMutex.unlock();
        condition.notify_one();
        worker->join();
        delete worker;
        worker = nullptr;
    }
    drain(false);
}

DeferredDeleter::~DeferredDeleter() {
    stop();
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
    if (worker)
        return;
    worker = new std::thread(run, this);
}

bool DeferredDeleter::areElementsReleased() {
    return elementsToRelease == 0;
}

bool DeferredDeleter::shouldStop() {
    return !doWorkInBackground;
}

void DeferredDeleter::run(DeferredDeleter *self) {
    std::unique_lock<std::mutex> lock(self->queueMutex);
    self->doWorkInBackground = true;
    self->threadLoaded = true;
    do {
        if (self->queue.peekIsEmpty()) {
            self->condition.wait(lock);
        }
        lock.unlock();
        self->clearQueue();
        lock.lock();
    } while (!self->shouldStop());
    lock.unlock();
}

void DeferredDeleter::drain(bool blocking) {
    clearQueue();
    if (blocking) {
        while (!areElementsReleased())
            ;
    }
}

void DeferredDeleter::clearQueue() {
    std::unique_ptr<DeferrableDeletion> deletion(nullptr);
    do {
        deletion.reset(queue.removeFrontOne().release());
        if (deletion) {
            deletion->apply();
            elementsToRelease--;
        }
    } while (deletion);
}
} // namespace OCLRT
