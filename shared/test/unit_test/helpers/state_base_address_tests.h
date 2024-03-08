/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

struct SbaFixture : public NEO::DeviceFixture {
    void setUp() {
        NEO::DeviceFixture::setUp();
        size_t sizeStream = 512;
        size_t alignmentStream = 0x1000;

        sshBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, sshBuffer);
        ssh.replaceBuffer(sshBuffer, sizeStream);
        auto graphicsAllocation = new MockGraphicsAllocation(sshBuffer, sizeStream);
        ssh.replaceGraphicsAllocation(graphicsAllocation);

        dshBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, dshBuffer);
        dsh.replaceBuffer(dshBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(dshBuffer, sizeStream);
        dsh.replaceGraphicsAllocation(graphicsAllocation);

        iohBuffer = alignedMalloc(sizeStream, alignmentStream);
        ASSERT_NE(nullptr, iohBuffer);
        ioh.replaceBuffer(iohBuffer, sizeStream);
        graphicsAllocation = new MockGraphicsAllocation(iohBuffer, sizeStream);
        ioh.replaceGraphicsAllocation(graphicsAllocation);

        linearStreamBuffer = alignedMalloc(sizeStream, alignmentStream);
        commandStream.replaceBuffer(linearStreamBuffer, alignmentStream);

        auto &productHelper = getHelper<ProductHelper>();
        l1CachePolicyData.init(productHelper);
    }

    void tearDown() {
        alignedFree(linearStreamBuffer);

        delete ssh.getGraphicsAllocation();
        alignedFree(sshBuffer);

        delete dsh.getGraphicsAllocation();
        alignedFree(dshBuffer);

        delete ioh.getGraphicsAllocation();
        alignedFree(iohBuffer);

        NEO::DeviceFixture::tearDown();
    }

    template <typename FamilyType>
    StateBaseAddressHelperArgs<FamilyType> createSbaHelperArgs(typename FamilyType::STATE_BASE_ADDRESS *sbaCommand, GmmHelper *gmmHelper, IndirectHeap *ssh, IndirectHeap *dsh, IndirectHeap *ioh, StateBaseAddressProperties *sbaProperties) {
        StateBaseAddressHelperArgs<FamilyType> sbaArgs = {
            0,                                        // generalStateBaseAddress
            0,                                        // indirectObjectHeapBaseAddress
            0,                                        // instructionHeapBaseAddress
            0,                                        // globalHeapsBaseAddress
            0,                                        // surfaceStateBaseAddress
            0,                                        // bindlessSurfaceStateBaseAddress
            sbaCommand,                               // stateBaseAddressCmd
            sbaProperties,                            // sbaProperties
            dsh,                                      // dsh
            ioh,                                      // ioh
            ssh,                                      // ssh
            gmmHelper,                                // gmmHelper
            0,                                        // statelessMocsIndex
            l1CachePolicyData.getL1CacheValue(false), // l1CachePolicy
            l1CachePolicyData.getL1CacheValue(true),  // l1CachePolicyDebuggerActive
            MemoryCompressionState::notApplicable,    // memoryCompressionState
            false,                                    // setInstructionStateBaseAddress
            false,                                    // setGeneralStateBaseAddress
            false,                                    // useGlobalHeapsBaseAddress
            false,                                    // isMultiOsContextCapable
            false,                                    // areMultipleSubDevicesInContext
            false,                                    // overrideSurfaceStateBaseAddress
            false                                     // doubleSbaWa
        };
        return sbaArgs;
    }

    template <typename FamilyType>
    StateBaseAddressHelperArgs<FamilyType> createSbaHelperArgs(typename FamilyType::STATE_BASE_ADDRESS *sbaCommand, GmmHelper *gmmHelper) {
        return createSbaHelperArgs<FamilyType>(sbaCommand, gmmHelper, nullptr, nullptr, nullptr, nullptr);
    }

    template <typename FamilyType>
    StateBaseAddressHelperArgs<FamilyType> createSbaHelperArgs(typename FamilyType::STATE_BASE_ADDRESS *sbaCommand, GmmHelper *gmmHelper, IndirectHeap *ssh, IndirectHeap *dsh, IndirectHeap *ioh) {
        return createSbaHelperArgs<FamilyType>(sbaCommand, gmmHelper, ssh, dsh, ioh, nullptr);
    }

    template <typename FamilyType>
    StateBaseAddressHelperArgs<FamilyType> createSbaHelperArgs(typename FamilyType::STATE_BASE_ADDRESS *sbaCommand, GmmHelper *gmmHelper, StateBaseAddressProperties *sbaProperties) {
        return createSbaHelperArgs<FamilyType>(sbaCommand, gmmHelper, nullptr, nullptr, nullptr, sbaProperties);
    }

    template <typename FamilyType>
    void updateSbaHelperArgsL1CachePolicy(StateBaseAddressHelperArgs<FamilyType> &args, ProductHelper &productHelper) {
        l1CachePolicyData.init(productHelper);
        args.l1CachePolicy = l1CachePolicyData.getL1CacheValue(false);
        args.l1CachePolicyDebuggerActive = l1CachePolicyData.getL1CacheValue(true);
    }

    IndirectHeap ssh = {nullptr};
    IndirectHeap dsh = {nullptr};
    IndirectHeap ioh = {nullptr};

    DebugManagerStateRestore restorer;
    LinearStream commandStream;
    L1CachePolicy l1CachePolicyData;

    void *sshBuffer = nullptr;
    void *dshBuffer = nullptr;
    void *iohBuffer = nullptr;
    void *linearStreamBuffer = nullptr;
};

using SbaTest = Test<SbaFixture>;
