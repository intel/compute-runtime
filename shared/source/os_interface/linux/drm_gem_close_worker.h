/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <set>

namespace NEO {
class DrmMemoryManager;
class BufferObject;
class Thread;

enum class gemCloseWorkerMode {
    gemCloseWorkerInactive,
    gemCloseWorkerActive
};

class DrmGemCloseWorker {
  public:
    DrmGemCloseWorker(DrmMemoryManager &memoryManager);
    MOCKABLE_VIRTUAL ~DrmGemCloseWorker();

    DrmGemCloseWorker(const DrmGemCloseWorker &) = delete;
    DrmGemCloseWorker &operator=(const DrmGemCloseWorker &) = delete;

    void push(BufferObject *allocation);
    MOCKABLE_VIRTUAL void close(bool blocking);

    bool isEmpty();

  protected:
    void close(BufferObject *workItem);
    void closeThread();
    void processQueue(std::queue<BufferObject *> &inputQueue);
    static void *worker(void *arg);
    std::atomic<bool> active{true};

    std::unique_ptr<Thread> thread;

    std::queue<BufferObject *> queue;
    std::atomic<uint32_t> workCount{0};

    DrmMemoryManager &memoryManager;

    std::mutex closeWorkerMutex;
    std::condition_variable condition;
    std::atomic<bool> workerDone{false};
};
} // namespace NEO
