/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/source/device_queue/device_queue_hw.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "test.h"

using namespace NEO;

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
class DeviceHostQueueFixture : public ApiFixture<>,
                               public ::testing::Test {
  public:
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_command_queue createClQueue(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        return create(pContext, devices[testedRootDeviceIndex], retVal, properties);
    }

    T *createQueueObject(cl_queue_properties properties[5] = deviceQueueProperties::noProperties) {
        using BaseType = typename T::BaseType;
        cl_context context = (cl_context)(pContext);
        auto clQueue = create(context, devices[testedRootDeviceIndex], retVal, properties);
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
        device = castToObject<ClDevice>(devices[testedRootDeviceIndex]);
        ASSERT_NE(device, nullptr);
        if (!device->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
            GTEST_SKIP();
        }
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
    ClDevice *device;
};
} // namespace DeviceHostQueue
