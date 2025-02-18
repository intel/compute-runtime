/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

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

enum class GemCloseWorkerMode {
    gemCloseWorkerInactive,
    gemCloseWorkerActive
};

class DrmGemCloseWorker : NEO::NonCopyableAndNonMovableClass {
  public:
    DrmGemCloseWorker(DrmMemoryManager &memoryManager);
    MOCKABLE_VIRTUAL ~DrmGemCloseWorker();

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

static_assert(NEO::NonCopyableAndNonMovable<DrmGemCloseWorker>);

} // namespace NEO
