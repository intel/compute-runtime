/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"

namespace NEO {

template <bool validContext>
struct UnifiedSharingContextFixture : ::testing::Test {
    void SetUp() override {
        device = std::make_unique<MockDevice>();
        cl_device_id deviceId = static_cast<cl_device_id>(device.get());
        deviceVector = std::make_unique<DeviceVector>(&deviceId, 1);
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

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<DeviceVector> deviceVector;
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
