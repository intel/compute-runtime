/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/api/leo_cl_types.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"

#include <vector>

namespace NEO {
class PerformanceCounters;
}

namespace NEO {
namespace LEO {

class Event;

template <>
struct OpenCLObjectMapper<_cl_command_queue> {
    typedef class CommandQueue DerivedType;
};

class CommandQueue : public BaseObject<_cl_command_queue> {
  public:
    static const cl_ulong objectMagic = 0x1234567890987654LL;

    CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties, ze_command_list_handle_t cmdListHandle);
    CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties);
    CommandQueue() = delete;
    ~CommandQueue() override;

    cl_int getCmdQInfo(cl_device_info paramName,
                       size_t paramValueSize,
                       void *paramValue,
                       size_t *paramValueSizeRet);

    template <typename ReturnType>
    static ReturnType getCmdQueueProperties(const cl_queue_properties *properties,
                                            cl_queue_properties propertyName = CL_QUEUE_PROPERTIES,
                                            bool *foundValue = nullptr) {
        if (properties != nullptr) {
            while (*properties != 0) {
                if (*properties == propertyName) {
                    if (foundValue) {
                        *foundValue = true;
                    }
                    return static_cast<ReturnType>(*(properties + 1));
                }
                properties += 2;
            }
        }

        if (foundValue) {
            *foundValue = false;
        }
        return 0;
    }

    bool isProfilingEnabled() { return NEO::LEO::CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(this->queueProperties.data()) & static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE); };
    bool isOutOfOrder() const { return !this->getL0Object()->isInOrderExecutionEnabled(); };
    bool isPerfCountersEnabled() const { return perfCountersEnabled; };
    bool setPerfCountersEnabled();
    PerformanceCounters *getPerfCounters();
    Context *getContext() const { return this->context; };
    ClDevice *getDevice() const { return this->clDevice; };

    ze_command_list_handle_t getL0Handle() const { return this->cmdListHandle; };
    L0::CommandList *getL0Object() const { return L0::CommandList::fromHandle(this->cmdListHandle); };

    cl_int enqueueAcquireSharedObjects(cl_uint numObjects,
                                       const cl_mem *memObjects,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *oclEvent,
                                       cl_uint cmdType);

    cl_int enqueueReleaseSharedObjects(cl_uint numObjects,
                                       const cl_mem *memObjects,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *oclEvent,
                                       cl_uint cmdType);

    void storeDependencies(cl_uint numEvents, const cl_event *eventWaitList);
    void clearDependencies();
    bool hasDependencies();
    ze_result_t hostSynchronize(uint64_t timeout);

  protected:
    void storeProperties(const cl_queue_properties *properties);
    cl_queue_properties getCommandQueueProperties() const { return getCmdQueueProperties<cl_queue_properties>(this->queueProperties.data()); };
    cl_uint getQueueFamilyIndex() const { return this->getCmdQueueProperties<cl_uint>(this->queueProperties.data(), CL_QUEUE_FAMILY_INTEL); };
    cl_uint getQueueIndexWithinFamily() const { return this->getCmdQueueProperties<cl_uint>(this->queueProperties.data(), CL_QUEUE_INDEX_INTEL); };

    struct SharingUserData {
        SharingUserData(CommandQueue *cmdQ, cl_uint numObjects, const cl_mem *memObjects) : cmdQ(cmdQ), memObjects(memObjects, memObjects + numObjects) {}

        CommandQueue *cmdQ = nullptr;
        std::vector<cl_mem> memObjects{};
    };
    static void acquireHelper(void *userData);
    static void releaseHelper(void *userData);

    std::vector<cl_queue_properties> queueProperties{};
    std::vector<NEO::LEO::Event *> dependencyEvents{};
    ClDevice *clDevice = nullptr;
    Context *context = nullptr;
    const bool externalHandle = false;
    bool perfCountersEnabled = false;

    ze_command_list_handle_t cmdListHandle = nullptr;
};

static_assert(NEO::NonCopyableAndNonMovable<CommandQueue>);

} // namespace LEO
} // namespace NEO
