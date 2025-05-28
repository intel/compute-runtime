/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "gtest/gtest.h"

namespace NEO {

struct KernelInfo;

struct HardwareParse : NEO::NonCopyableAndNonMovableClass {
    HardwareParse() {
        itorMediaInterfaceDescriptorLoad = cmdList.end();
        itorMediaVfeState = cmdList.end();
        itorPipelineSelect = cmdList.end();
        itorStateBaseAddress = cmdList.end();
        itorWalker = cmdList.end();
        itorGpgpuCsrBaseAddress = cmdList.end();
        itorBindingTableBaseAddress = cmdList.end();
    }

    void setUp() {
    }

    void tearDown() {
        cmdList.clear();
        lriList.clear();
        pipeControlList.clear();
    }

    template <typename CmdType>
    GenCmdList getCommandsList() {
        GenCmdList list;
        for (auto it = cmdList.begin(); it != cmdList.end(); it++) {
            auto cmd = genCmdCast<CmdType *>(*it);
            if (cmd) {
                list.push_back(*it);
            }
        }
        return list;
    }

    template <typename FamilyType>
    void findCsrBaseAddress();

    template <typename FamilyType>
    bool requiresPipelineSelectBeforeMediaState() {
        return true;
    }

    template <typename FamilyType>
    bool isStallingBarrier(GenCmdList::iterator &iter);

    template <typename FamilyType>
    void findHardwareCommands();

    template <typename FamilyType>
    void findHardwareCommands(IndirectHeap *dsh);

    template <typename FamilyType>
    void parseCommands(NEO::LinearStream &commandStream, size_t startOffset = 0) {
        ASSERT_LE(startOffset, commandStream.getUsed());
        auto sizeToParse = commandStream.getUsed() - startOffset;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(commandStream.getCpuBase(), startOffset),
            sizeToParse));
    }

    template <typename FamilyType>
    void parseCommands(NEO::CommandStreamReceiver &commandStreamReceiver, NEO::LinearStream &commandStream) {
        auto &commandStreamCSR = commandStreamReceiver.getCS();

        parseCommands<FamilyType>(commandStreamCSR, startCSRCS);
        startCSRCS = commandStreamCSR.getUsed();

        if (previousCS != &commandStream) {
            startCS = 0;
        }
        parseCommands<FamilyType>(commandStream, startCS);
        startCS = commandStream.getUsed();
        previousCS = &commandStream;

        sizeUsed = commandStream.getUsed();
        findHardwareCommands<FamilyType>(&commandStreamReceiver.getIndirectHeap(IndirectHeap::Type::dynamicState, 0));
    }

    template <typename FamilyType>
    const typename FamilyType::RENDER_SURFACE_STATE *getSurfaceState(IndirectHeap *ssh, uint32_t index);

    template <typename FamilyType>
    const typename FamilyType::SAMPLER_STATE &getSamplerState(uint32_t index) {
        typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
        typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
        typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

        const auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

        auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
        auto dynamicStateHeap = cmdSBA->getDynamicStateBaseAddress();
        EXPECT_NE(0, dynamicStateHeap);

        const auto samplerState = reinterpret_cast<SAMPLER_STATE *>(dynamicStateHeap + interfaceDescriptorData.getSamplerStatePointer());
        return samplerState[index];
    }

    template <typename FamilyType>
    const void *getStatelessArgumentPointer(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex);

    template <typename CmdType>
    CmdType *getCommand(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        auto itorCmd = find<CmdType *>(itorStart, itorEnd);
        return itorCmd != cmdList.end()
                   ? genCmdCast<CmdType *>(*itorCmd)
                   : nullptr;
    }

    template <typename CmdType>
    CmdType *getCommand() {
        return getCommand<CmdType>(cmdList.begin(), cmdList.end());
    }

    template <typename CmdType>
    GenCmdList::iterator getCommandItor(GenCmdList::iterator itorStart, GenCmdList::iterator itorEnd) {
        auto itorCmd = find<CmdType *>(itorStart, itorEnd);
        return itorCmd != cmdList.end()
                   ? itorCmd
                   : cmdList.end();
    }

    template <typename CmdType>
    GenCmdList::iterator getCommandItor() {
        return getCommandItor<CmdType>(cmdList.begin(), cmdList.end());
    }

    template <typename FamilyType>
    int getNumberOfPipelineSelectsThatEnablePipelineSelect() {
        using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
        int numberOfGpgpuSelects = 0;
        int numberOf3dSelects = 0;
        auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        while (itorCmd != cmdList.end()) {
            auto cmd = getCommand<PIPELINE_SELECT>(itorCmd, cmdList.end());
            if (cmd->getPipelineSelection() == PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU &&
                pipelineSelectEnablePipelineSelectMaskBits == (pipelineSelectEnablePipelineSelectMaskBits & cmd->getMaskBits())) {
                numberOfGpgpuSelects++;
            }
            if (cmd->getPipelineSelection() == PIPELINE_SELECT::PIPELINE_SELECTION_3D &&
                pipelineSelectEnablePipelineSelectMaskBits == (pipelineSelectEnablePipelineSelectMaskBits & cmd->getMaskBits())) {
                numberOf3dSelects++;
            }
            itorCmd = find<PIPELINE_SELECT *>(++itorCmd, cmdList.end());
        }
        MockExecutionEnvironment mockExecutionEnvironment{};
        const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();
        if (productHelper.is3DPipelineSelectWARequired()) {
            auto maximalNumberOf3dSelectsRequired = 2;
            EXPECT_LE(numberOf3dSelects, maximalNumberOf3dSelectsRequired);
            EXPECT_EQ(numberOf3dSelects, numberOfGpgpuSelects);
            auto numberOfGpgpuSelectsAddedByWa = numberOf3dSelects - 1;
            numberOfGpgpuSelects -= numberOfGpgpuSelectsAddedByWa;
        } else {
            EXPECT_EQ(0, numberOf3dSelects);
        }
        return numberOfGpgpuSelects;
    }

    template <typename CmdType>
    uint32_t getCommandCount() {
        GenCmdList::iterator cmdItor = cmdList.begin();
        uint32_t cmdCount = 0;

        do {
            cmdItor = find<CmdType *>(cmdItor, cmdList.end());
            if (cmdItor != cmdList.end()) {
                ++cmdCount;
                ++cmdItor;
            }
        } while (cmdItor != cmdList.end());

        return cmdCount;
    }

    template <typename FamilyType>
    uint32_t getCommandWalkerCount() {

        GenCmdList::iterator cmdItor = cmdList.begin();
        uint32_t cmdCount = 0;

        do {
            cmdItor = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdItor, cmdList.end());
            if (cmdItor != cmdList.end()) {
                ++cmdCount;
                ++cmdItor;
            }
        } while (cmdItor != cmdList.end());

        return cmdCount;
    }

    template <typename FamilyType>
    static const char *getCommandName(void *cmd) {
        return FamilyType::Parse::getCommandName(cmd);
    }

    // The starting point of parsing commandBuffers.  This is important
    // because as buffers get reused, we only want to parse the deltas.
    LinearStream *previousCS = nullptr;
    size_t startCS = 0u;
    size_t startCSRCS = 0u;

    size_t sizeUsed = 0u;
    GenCmdList cmdList;
    GenCmdList lriList;
    GenCmdList pipeControlList;
    GenCmdList::iterator itorMediaInterfaceDescriptorLoad;
    GenCmdList::iterator itorMediaVfeState;
    GenCmdList::iterator itorPipelineSelect;
    GenCmdList::iterator itorStateBaseAddress;
    GenCmdList::iterator itorWalker;
    GenCmdList::iterator itorBBStartAfterWalker;
    GenCmdList::iterator itorGpgpuCsrBaseAddress;
    GenCmdList::iterator itorBindingTableBaseAddress;

    void *cmdInterfaceDescriptorData = nullptr;
    void *cmdMediaInterfaceDescriptorLoad = nullptr;
    void *cmdMediaVfeState = nullptr;
    void *cmdPipelineSelect = nullptr;
    void *cmdStateBaseAddress = nullptr;
    void *cmdWalker = nullptr;
    void *cmdBBStartAfterWalker = nullptr;
    void *cmdGpgpuCsrBaseAddress = nullptr;
    void *cmdBindingTableBaseAddress = nullptr;

    bool parsePipeControl = false;
};

static_assert(NEO::NonCopyableAndNonMovable<HardwareParse>);

} // namespace NEO
