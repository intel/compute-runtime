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
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <map>
#include <set>
#include <queue>
#include <cstdint>

namespace OCLRT {
class DrmMemoryManager;
class BufferObject;

enum gemCloseWorkerMode {
    gemCloseWorkerInactive,
    gemCloseWorkerActive
};

class DrmGemCloseWorker {
  public:
    DrmGemCloseWorker(DrmMemoryManager &memoryManager);
    ~DrmGemCloseWorker();

    DrmGemCloseWorker(const DrmGemCloseWorker &) = delete;
    DrmGemCloseWorker &operator=(const DrmGemCloseWorker &) = delete;

    void push(BufferObject *allocation);
    void close(bool blocking);

    bool isEmpty();

  protected:
    void close(BufferObject *workItem);
    void closeThread();
    void worker();
    bool active;

    std::thread *thread;

    std::queue<BufferObject *> queue;
    std::atomic<uint32_t> workCount;

    DrmMemoryManager &memoryManager;

    std::mutex closeWorkerMutex;
    std::condition_variable condition;
    std::atomic<bool> workerDone;
};
}
