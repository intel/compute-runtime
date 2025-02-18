/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/l3_range.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/static_size3.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/resource_barrier.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

template <typename FamilyType>
struct L3ControlPolicy : CmdValidator {
    L3ControlPolicy(typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy, bool isA0Stepping)
        : expectedPolicy(expectedPolicy), isA0Stepping(isA0Stepping) {
    }
    bool operator()(GenCmdList::iterator it, size_t numInScetion, const std::string &member, std::string &outReason) override {
        using L3_CONTROL = typename FamilyType::L3_CONTROL;
        auto l3ControlAddress = genCmdCast<L3_CONTROL *>(*it)->getL3FlushAddressRange();
        if (l3ControlAddress.getL3FlushEvictionPolicy(isA0Stepping) != expectedPolicy) {
            outReason = "Invalid L3_FLUSH_EVICTION_POLICY - expected: " + std::to_string(expectedPolicy) + ", got :" + std::to_string(l3ControlAddress.getL3FlushEvictionPolicy(isA0Stepping));
            return false;
        }
        l3RangesParsed.push_back(L3Range::fromAddressMask(l3ControlAddress.getAddress(isA0Stepping), l3ControlAddress.getAddressMask(isA0Stepping)));
        return true;
    }
    L3RangesVec l3RangesParsed;
    typename FamilyType::L3_FLUSH_ADDRESS_RANGE::L3_FLUSH_EVICTION_POLICY expectedPolicy;
    bool isA0Stepping;
};

using EnqueueKernelFixture = HelloWorldFixture<HelloWorldFixtureFactory>;
using EnqueueKernelTest = Test<EnqueueKernelFixture>;

template <typename FamilyType>
class GivenCacheResourceSurfacesWhenprocessingCacheFlushThenExpectProperCacheFlushCommand : public EnqueueKernelTest {
  public:
    void testBodyImpl() {

        using L3_CONTROL_WITHOUT_POST_SYNC = typename FamilyType::L3_CONTROL;

        MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, 0);
        auto &commandStream = cmdQ.getCS(1024);

        cl_resource_barrier_descriptor_intel descriptor{};
        cl_resource_barrier_descriptor_intel descriptor2{};

        SVMAllocsManager *svmManager = cmdQ.getContext().getSVMAllocsManager();
        void *svm = svmManager->createSVMAlloc(MemoryConstants::pageSize, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());

        auto retVal = CL_INVALID_VALUE;
        size_t bufferSize = MemoryConstants::pageSize;
        std::unique_ptr<Buffer> buffer(Buffer::create(
            context,
            CL_MEM_READ_WRITE,
            bufferSize,
            nullptr,
            retVal));

        descriptor.svmAllocationPointer = svm;

        descriptor2.memObject = buffer.get();

        const cl_resource_barrier_descriptor_intel descriptors[] = {descriptor, descriptor2};
        BarrierCommand bCmd(&cmdQ, descriptors, 2);
        CsrDependencies csrDeps;

        cmdQ.processDispatchForCacheFlush(bCmd.surfacePtrs.begin(), bCmd.numSurfaces, &commandStream, csrDeps);

        std::string err;
        auto cmdBuffOk = expectCmdBuff<FamilyType>(cmdQ.getCS(0), 0,
                                                   std::vector<MatchCmd *>{
                                                       new MatchHwCmd<FamilyType, L3_CONTROL_WITHOUT_POST_SYNC>(atLeastOne)},
                                                   &err);
        EXPECT_TRUE(cmdBuffOk) << err;
        svmManager->freeSVMAlloc(svm);
    }
};
