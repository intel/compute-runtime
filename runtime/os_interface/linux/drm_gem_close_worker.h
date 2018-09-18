/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <map>
#include <set>
#include <queue>
#include <cstdint>

namespace OCLRT {
class DrmMemoryManager;
class BufferObject;
class Thread;

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
    static void *worker(void *arg);
    bool active = true;

    std::unique_ptr<Thread> thread;

    std::queue<BufferObject *> queue;
    std::atomic<uint32_t> workCount{0};

    DrmMemoryManager &memoryManager;

    std::mutex closeWorkerMutex;
    std::condition_variable condition;
    std::atomic<bool> workerDone{false};
};
} // namespace OCLRT
