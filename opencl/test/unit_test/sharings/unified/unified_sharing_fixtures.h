/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/unified_sharing_types.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

namespace NEO {

template <bool validContext>
struct UnifiedSharingContextFixture : ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        cl_device_id deviceId = device.get();
        deviceVector = std::make_unique<ClDeviceVector>(&deviceId, 1);
        if (validContext) {
            context = createValidContext();
        } else {
            context = createInvalidContext();
        }
    }

    std::unique_ptr<MockContext> createContext(const cl_context_properties *contextProperties) {
        cl_int retVal{};
        auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(contextProperties, *deviceVector,
                                                                                 nullptr, nullptr, retVal));
        EXPECT_EQ(CL_SUCCESS, retVal);
        return context;
    }

    std::unique_ptr<MockContext> createValidContext() {
        const cl_context_properties contextProperties[] = {
            static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle), 0,
            CL_CONTEXT_INTEROP_USER_SYNC, 1,
            0};
        return createContext(contextProperties);
    }

    std::unique_ptr<MockContext> createInvalidContext() {
        return createContext(nullptr);
    }

    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<ClDeviceVector> deviceVector;
    std::unique_ptr<MockContext> context;
};

template <bool validMemoryManager>
struct UnifiedSharingMockMemoryManager : MockMemoryManager {
    GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex) override {
        if (!validMemoryManager) {
            return nullptr;
        }

        auto graphicsAllocation = createMemoryAllocation(GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY, nullptr, reinterpret_cast<void *>(1), 1,
                                                         4096u, reinterpret_cast<uint64_t>(handle), MemoryPool::SystemCpuInaccessible,
                                                         rootDeviceIndex, false, false, false);
        graphicsAllocation->setSharedHandle(static_cast<osHandle>(reinterpret_cast<uint64_t>(handle)));
        graphicsAllocation->set32BitAllocation(false);
        graphicsAllocation->setDefaultGmm(new MockGmm());
        return graphicsAllocation;
    }
};

template <bool validContext, bool validMemoryManager>
struct UnifiedSharingFixture : UnifiedSharingContextFixture<validContext> {
    void SetUp() override {
        UnifiedSharingContextFixture<validContext>::SetUp();
        this->memoryManager = std::make_unique<UnifiedSharingMockMemoryManager<validMemoryManager>>();
        this->memoryManagerBackup = std::make_unique<VariableBackup<MemoryManager *>>(&this->context->memoryManager, this->memoryManager.get());
    }

    void TearDown() override {
        UnifiedSharingContextFixture<validContext>::TearDown();
    }

    std::unique_ptr<UnifiedSharingMockMemoryManager<validMemoryManager>> memoryManager;
    std::unique_ptr<VariableBackup<MemoryManager *>> memoryManagerBackup;
};

} // namespace NEO
