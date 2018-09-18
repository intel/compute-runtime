/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/api/cl_api_tests.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue.h"
#include "test.h"

using namespace OCLRT;

namespace DeviceHostQueue {
struct deviceQueueProperties {
    static cl_queue_properties minimumProperties[5];
    static cl_queue_properties minimumPropertiesWithProfiling[5];
    static cl_queue_properties noProperties[5];
    static cl_queue_properties allProperties[5];
};

IGIL_CommandQueue getExpectedInitIgilCmdQueue(DeviceQueue *deviceQueue);
IGIL_CommandQueue getExpectedgilCmdQueueAfterReset(DeviceQueue *deviceQueue);

template <typename T>
class DeviceHostQueueFixture : public api_fixture,
                               public ::testing::Test {
  public:
    void SetUp() override {
        api_fixture::SetUp();
    }
    void TearDown() override {
        api_fixture::TearDown();
    }

    cl_command_queue createClQueue(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        return create(pContext, devices[0], retVal, properties);
    }

    T *createQueueObject(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        using BaseType = typename T::BaseType;
        cl_context context = (cl_context)(pContext);
        auto clQueue = create(context, devices[0], retVal, properties);
        return castToObject<T>(static_cast<BaseType *>(clQueue));
    }

    cl_command_queue create(cl_context ctx, cl_device_id device, cl_int &retVal,
                            cl_queue_properties properties[5] = deviceQueueProperties::noProperties);
};

class DeviceQueueHwTest : public DeviceHostQueueFixture<DeviceQueue> {
  public:
    using BaseClass = DeviceHostQueueFixture<DeviceQueue>;
    void SetUp() override {
        BaseClass::SetUp();
        device = castToObject<Device>(devices[0]);
        ASSERT_NE(device, nullptr);
    }

    void TearDown() override {
        BaseClass::TearDown();
    }

    template <typename GfxFamily>
    DeviceQueueHw<GfxFamily> *castToHwType(DeviceQueue *deviceQueue) {
        return reinterpret_cast<DeviceQueueHw<GfxFamily> *>(deviceQueue);
    }

    template <typename GfxFamily>
    size_t getMinimumSlbSize() {
        return sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
               sizeof(typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD) +
               sizeof(typename GfxFamily::PIPE_CONTROL) +
               sizeof(typename GfxFamily::GPGPU_WALKER) +
               sizeof(typename GfxFamily::MEDIA_STATE_FLUSH) +
               sizeof(typename GfxFamily::PIPE_CONTROL) +
               DeviceQueueHw<GfxFamily>::getCSPrefetchSize(); // prefetch size
    }

    DeviceQueue *deviceQueue;
    Device *device;
};
} // namespace DeviceHostQueue
