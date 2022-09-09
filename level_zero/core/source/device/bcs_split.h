/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/sku_info/sku_info_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/event/event.h"

#include <functional>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
}

namespace L0 {
struct CommandQueue;
struct DeviceImp;

struct BcsSplit {
    DeviceImp &device;

    std::vector<CommandQueue *> cmdQs;
    NEO::BcsInfoMask engines = NEO::EngineHelpers::oddLinkedCopyEnginesMask;

    template <GFXCORE_FAMILY gfxCoreFamily>
    ze_result_t appendSplitCall(CommandListCoreFamilyImmediate<gfxCoreFamily> *cmdList,
                                void *dstptr,
                                const void *srcptr,
                                size_t size,
                                ze_event_handle_t hSignalEvent,
                                std::function<ze_result_t(void *, const void *, size_t, ze_event_handle_t)> appendCall) {
        if (hSignalEvent) {
            cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), true);
        }

        auto totalSize = size;
        auto engineCount = this->cmdQs.size();
        for (size_t i = 0; i < this->cmdQs.size(); i++) {
            auto localSize = totalSize / engineCount;
            auto localDstPtr = ptrOffset(dstptr, size - totalSize);
            auto localSrcPtr = ptrOffset(srcptr, size - totalSize);

            appendCall(localDstPtr, localSrcPtr, localSize, nullptr);
            cmdList->executeCommandListImmediateImpl(true, this->cmdQs[i]);

            totalSize -= localSize;
            engineCount--;
        }

        if (hSignalEvent) {
            cmdList->appendEventForProfilingAllWalkers(Event::fromHandle(hSignalEvent), false);
        }

        return ZE_RESULT_SUCCESS;
    }

    bool setupDevice(uint32_t productFamily, bool internalUsage, const ze_command_queue_desc_t *desc, NEO::CommandStreamReceiver *csr);
    void releaseResources();

    BcsSplit(DeviceImp &device) : device(device){};
};

} // namespace L0