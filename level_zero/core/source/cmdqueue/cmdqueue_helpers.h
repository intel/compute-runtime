/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/utilities/stackvec.h"

#include <level_zero/ze_api.h>

#include <cstdint>
#include <optional>
#include <utility>

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;
struct CommandList;

struct QueueProperties {
    NEO::SynchronizedDispatchMode synchronizedDispatchMode = NEO::SynchronizedDispatchMode::disabled;
    bool interruptHint = false;
    bool copyOffloadHint = false;
    std::optional<int> priorityLevel = std::nullopt;
};

class CommandBufferManager {
  public:
    enum BufferAllocation : uint32_t {
        first = 0,
        second,
        count
    };

    ze_result_t initialize(Device *device, size_t sizeRequested);
    void destroy(Device *device);
    NEO::WaitStatus switchBuffers(NEO::CommandStreamReceiver *csr);

    NEO::GraphicsAllocation *getCurrentBufferAllocation() {
        return buffers[bufferUse];
    }

    void setCurrentFlushStamp(TaskCountType taskCount, NEO::FlushStamp flushStamp) {
        flushId[bufferUse] = std::make_pair(taskCount, flushStamp);
    }
    std::pair<TaskCountType, NEO::FlushStamp> &getCurrentFlushStamp() {
        return flushId[bufferUse];
    }

  private:
    NEO::GraphicsAllocation *buffers[BufferAllocation::count]{};
    std::pair<TaskCountType, NEO::FlushStamp> flushId[BufferAllocation::count];
    BufferAllocation bufferUse = BufferAllocation::first;
};

struct CommandListDirtyFlags {
    bool propertyScmDirty = false;
    bool propertyFeDirty = false;
    bool propertyPsDirty = false;
    bool propertySbaDirty = false;
    bool frontEndReturnPoint = false;
    bool preemptionDirty = false;

    bool isAnyDirty() const {
        return (propertyScmDirty || propertyFeDirty || propertyPsDirty || propertySbaDirty || frontEndReturnPoint || preemptionDirty);
    }
    void cleanDirty() {
        propertyScmDirty = false;
        propertyFeDirty = false;
        propertyPsDirty = false;
        propertySbaDirty = false;
        frontEndReturnPoint = false;
        preemptionDirty = false;
    }
};

struct CommandListRequiredStateChange {
    CommandListRequiredStateChange() = default;
    NEO::StreamProperties requiredState{};
    CommandList *commandList = nullptr;
    CommandListDirtyFlags flags{};
    NEO::PreemptionMode newPreemptionMode = NEO::PreemptionMode::Initial;
    uint32_t cmdListIndex = 0;
};

static constexpr uint32_t defaultCommandListStateChangeListSize = 10;
using CommandListStateChangeList = StackVec<CommandListRequiredStateChange, defaultCommandListStateChangeListSize>;

} // namespace L0