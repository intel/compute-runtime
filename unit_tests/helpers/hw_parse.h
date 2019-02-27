/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/command_queue.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/pipeline_select_helper.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/gen_common/gen_cmd_parse.h"

#include "gtest/gtest.h"

namespace OCLRT {

struct HardwareParse {
    HardwareParse() : previousCS(nullptr),
                      startCS(0),
                      startCSRCS(0),
                      sizeUsed(0),
                      cmdInterfaceDescriptorData(nullptr),
                      cmdMediaInterfaceDescriptorLoad(nullptr),
                      cmdMediaVfeState(nullptr),
                      cmdPipelineSelect(nullptr),
                      cmdStateBaseAddress(nullptr),
                      cmdWalker(nullptr),
                      cmdGpgpuCsrBaseAddress(nullptr) {
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
    void parseCommands(OCLRT::LinearStream &commandStream, size_t startOffset = 0) {
        ASSERT_LE(startOffset, commandStream.getUsed());
        auto sizeToParse = commandStream.getUsed() - startOffset;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList,
            ptrOffset(commandStream.getCpuBase(), startOffset),
            sizeToParse));
    }

    template <typename FamilyType>
    void parseCommands(OCLRT::CommandQueue &commandQueue) {
        auto &commandStreamReceiver = commandQueue.getCommandStreamReceiver();
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
        int numCommands = 0;
        auto itorCmd = find<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
        while (itorCmd != cmdList.end()) {
            auto cmd = getCommand<PIPELINE_SELECT>(itorCmd, cmdList.end());
            if (cmd->getPipelineSelection() == PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU &&
                pipelineSelectEnablePipelineSelectMaskBits == (pipelineSelectEnablePipelineSelectMaskBits & cmd->getMaskBits())) {
                numCommands++;
            }
            itorCmd = find<PIPELINE_SELECT *>(++itorCmd, cmdList.end());
        }
        return numCommands;
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
    LinearStream *previousCS;
    size_t startCS;
    size_t startCSRCS;

    size_t sizeUsed;
    GenCmdList cmdList;
    GenCmdList lriList;
    GenCmdList::iterator itorMediaInterfaceDescriptorLoad;
    GenCmdList::iterator itorMediaVfeState;
    GenCmdList::iterator itorPipelineSelect;
    GenCmdList::iterator itorStateBaseAddress;
    GenCmdList::iterator itorWalker;
    GenCmdList::iterator itorBBStartAfterWalker;
    GenCmdList::iterator itorGpgpuCsrBaseAddress;

    void *cmdInterfaceDescriptorData;
    void *cmdMediaInterfaceDescriptorLoad;
    void *cmdMediaVfeState;
    void *cmdPipelineSelect;
    void *cmdStateBaseAddress;
    void *cmdWalker;
    void *cmdBBStartAfterWalker;
    void *cmdGpgpuCsrBaseAddress;
};

} // namespace OCLRT
