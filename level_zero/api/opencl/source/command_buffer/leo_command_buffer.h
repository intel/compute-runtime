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
namespace LEO {

class CommandQueue;

template <>
struct OpenCLObjectMapper<_cl_command_buffer_khr> {
    typedef class CommandBuffer DerivedType;
};

// An OpenCL command buffer is a record-once, replay-many container, which maps onto a
// regular (non-immediate) L0 command list. Replay appends that list onto the immediate
// command list backing the target CommandQueue.
class CommandBuffer : public BaseObject<_cl_command_buffer_khr> {
  public:
    static const cl_ulong objectMagic = 0x7C31A5D0428E9B16LL;

    CommandBuffer(Context *context, CommandQueue *commandQueue, ze_command_list_handle_t cmdListHandle, const cl_command_buffer_properties_khr *properties);
    CommandBuffer() = delete;
    ~CommandBuffer() override;

    // cl_khr_command_buffer is incomplete and therefore not advertised unless the
    // EnableClKhrCommandBuffer debug variable is explicitly set to 1.
    static bool isSupported();

    cl_int finalize();
    cl_int enqueue(CommandQueue *commandQueue, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event);

    cl_int getInfo(cl_command_buffer_info_khr paramName, size_t paramValueSize,
                   void *paramValue, size_t *paramValueSizeRet);

    bool isFinalized() const { return this->state == CL_COMMAND_BUFFER_STATE_EXECUTABLE_KHR; };

    CommandQueue *getCommandQueue() const { return this->commandQueue; };
    ze_command_list_handle_t getL0Handle() const { return this->cmdListHandle; };

  protected:
    void storeProperties(const cl_command_buffer_properties_khr *properties);

    std::vector<cl_command_buffer_properties_khr> bufferProperties{};
    Context *context = nullptr;
    CommandQueue *commandQueue = nullptr;
    ze_command_list_handle_t cmdListHandle = nullptr;
    cl_command_buffer_state_khr state = CL_COMMAND_BUFFER_STATE_RECORDING_KHR;
};

static_assert(NEO::NonCopyableAndNonMovable<CommandBuffer>);

} // namespace LEO
} // namespace NEO
