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

#pragma once
#include "runtime/utilities/idlist.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <utility>

namespace OCLRT {
class DeferrableDeletion;
class Thread;
class DeferredDeleter {
  public:
    DeferredDeleter();
    virtual ~DeferredDeleter();

    DeferredDeleter(const DeferredDeleter &) = delete;
    DeferredDeleter &operator=(const DeferredDeleter &) = delete;

    MOCKABLE_VIRTUAL void deferDeletion(DeferrableDeletion *deletion);

    MOCKABLE_VIRTUAL void addClient();

    MOCKABLE_VIRTUAL void removeClient();

    MOCKABLE_VIRTUAL void drain(bool blocking);

  protected:
    void stop();
    void safeStop();
    void ensureThread();
    MOCKABLE_VIRTUAL void clearQueue();
    MOCKABLE_VIRTUAL bool areElementsReleased();
    MOCKABLE_VIRTUAL bool shouldStop();

    static void *run(void *);

    std::atomic<bool> doWorkInBackground;
    std::atomic<int> elementsToRelease;
    std::unique_ptr<Thread> worker;
    int32_t numClients = 0;
    IDList<DeferrableDeletion, true> queue;
    std::mutex queueMutex;
    std::mutex threadMutex;
    std::condition_variable condition;
};
} // namespace OCLRT
