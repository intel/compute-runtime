/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/kernel/kernel.h"

#include "gtest/gtest.h"

namespace NEO {

struct HardwareParse {
    HardwareParse() {
        itorMediaInterfaceDescriptorLoad = cmdList.end();
        itorMediaVfeState = cmdList.end();
        itorPipelineSelect = cmdList.end();
        itorStateBaseAddress = cmdList.end();
        itorWalker = cmdList.end();
        itorGpgpuCsrBaseAddress = cmdList.end();
    }

    void SetUp() {
    }

    void TearDown() {
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
    void findHardwareCommands();

    template <typename FamilyType>
    void findHardwareCommands(IndirectHeap *dsh);

    template <typename FamilyType>
    void parseCommands(NEO::LinearStream &commandStream, size_t startOffset = 0) {
        ASSERT_LE(startOffset, commandStream.getUsed());
        auto sizeToParse = commandStream.getUsed() - startOffset;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandStream.getCpuBase(), startOffset),
            sizeToParse));
    }

    template <typename FamilyType>
    void parseCommands(NEO::CommandQueue &commandQueue) {
        auto &commandStreamReceiver = commandQueue.getGpgpuCommandStreamReceiver();
        auto &commandStreamCSR = commandStreamReceiver.getCS();

        parseCommands<FamilyType>(commandStreamCSR, startCSRCS);
        startCSRCS = commandStreamCSR.getUsed();

        auto &commandStream = commandQueue.getCS(1024);
        if (previousCS != &commandStream) {
            startCS = 0;
        }
        parseCommands<FamilyType>(commandStream, startCS);
        startCS = commandStream.getUsed();
        previousCS = &commandStream;

        sizeUsed = commandStream.getUsed();
        findHardwareCommands<FamilyType>(&commandStreamReceiver.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0));
    }

    template <typename FamilyType>
    const typename FamilyType::RENDER_SURFACE_STATE &getSurfaceState(IndirectHeap *ssh, uint32_t index) {
        typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
        typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
        typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

        const auto &interfaceDescriptorData = *(INTERFACE_DESCRIPTOR_DATA *)cmdInterfaceDescriptorData;

        auto cmdSBA = (STATE_BASE_ADDRESS *)cmdStateBaseAddress;
        auto surfaceStateHeap = cmdSBA->getSurfaceStateBaseAddress();
        if (ssh && (ssh->getHeapGpuBase() == surfaceStateHeap)) {
            surfaceStateHeap = reinterpret_cast<uint64_t>(ssh->getCpuBase());
        }
        EXPECT_NE(0u, surfaceStateHeap);

        auto bindingTablePointer = interfaceDescriptorData.getBindingTablePointer();

        const auto &bindingTableState = reinterpret_cast<BINDING_TABLE_STATE *>(surfaceStateHeap + bindingTablePointer)[index];
        auto surfaceStatePointer = bindingTableState.getSurfaceStatePointer();

        return *(RENDER_SURFACE_STATE *)(surfaceStateHeap + surfaceStatePointer);
    }

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
    const void *getStatelessArgumentPointer(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh);

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

    template <typename FamilyType>
    int getNumberOfPipelineSelectsThatEnablePipelineSelect() {
        typedef typename FamilyType::PIPELINE_SELECT PIPELINE_SELECT;
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
        auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
        if (hwHelper.is3DPipelineSelectWARequired(*defaultHwInfo)) {
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
    static const char *getCommandName(void *cmd) {
        return FamilyType::PARSE::getCommandName(cmd);
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

    void *cmdInterfaceDescriptorData = nullptr;
    void *cmdMediaInterfaceDescriptorLoad = nullptr;
    void *cmdMediaVfeState = nullptr;
    void *cmdPipelineSelect = nullptr;
    void *cmdStateBaseAddress = nullptr;
    void *cmdWalker = nullptr;
    void *cmdBBStartAfterWalker = nullptr;
    void *cmdGpgpuCsrBaseAddress = nullptr;

    bool parsePipeControl = false;
};

} // namespace NEO
