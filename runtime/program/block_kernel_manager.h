/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/api/cl_types.h"

#include <vector>

namespace NEO {
class GraphicsAllocation;
class CommandStreamReceiver;
struct KernelInfo;

class BlockKernelManager {
  public:
    BlockKernelManager() = default;
    virtual ~BlockKernelManager();
    void addBlockKernelInfo(KernelInfo *);
    const KernelInfo *getBlockKernelInfo(size_t ordinal);
    size_t getCount() const {
        return blockKernelInfoArray.size();
    }
    bool getIfBlockUsesPrintf() const {
        return blockUsesPrintf;
    }

    void pushPrivateSurface(GraphicsAllocation *allocation, size_t ordinal);
    GraphicsAllocation *getPrivateSurface(size_t ordinal);

    void makeInternalAllocationsResident(CommandStreamReceiver &);

  protected:
    bool blockUsesPrintf = false;
    std::vector<KernelInfo *> blockKernelInfoArray;
    std::vector<GraphicsAllocation *> blockPrivateSurfaceArray;
};
} // namespace NEO