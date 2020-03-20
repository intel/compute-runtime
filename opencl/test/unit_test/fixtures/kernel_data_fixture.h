/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/program/kernel_info.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

using namespace NEO;
using namespace iOpenCL;

class KernelDataTest : public testing::Test {
  public:
    KernelDataTest() {
        memset(&kernelBinaryHeader, 0x00, sizeof(SKernelBinaryHeaderCommon));
        pCurPtr = nullptr;
        pKernelData = nullptr;
        kernelName = "test";
        pDsh = nullptr;
        pGsh = nullptr;
        pKernelHeap = nullptr;
        pSsh = nullptr;
        pPatchList = nullptr;

        kernelDataSize = 0;
        kernelNameSize = (uint32_t)alignUp(strlen(kernelName.c_str()) + 1, sizeof(uint32_t));
        dshSize = 0;
        gshSize = 0;
        kernelHeapSize = 0;
        sshSize = 0;
        patchListSize = 0;

        checkSum = 0;
        shaderHashCode = 0;
        kernelUnpaddedSize = 0;

        pKernelInfo = nullptr;
    }

    void buildAndDecode();

  protected:
    void SetUp() override {
        kernelBinaryHeader.KernelNameSize = kernelNameSize;
        pContext = new MockContext;
        program = std::make_unique<MockProgram>(*pContext->getDevice(0)->getExecutionEnvironment(), pContext, false, nullptr);
    }

    void TearDown() override {
        if (pKernelInfo->kernelAllocation) {
            pContext->getDevice(0)->getMemoryManager()->freeGraphicsMemory(pKernelInfo->kernelAllocation);
            const_cast<KernelInfo *>(pKernelInfo)->kernelAllocation = nullptr;
        }
        program.reset();
        delete pContext;
        alignedFree(pKernelData);
    }

    char *pCurPtr;
    char *pKernelData;
    SKernelBinaryHeaderCommon kernelBinaryHeader;
    std::string kernelName;
    void *pDsh;
    void *pGsh;
    void *pKernelHeap;
    void *pSsh;
    void *pPatchList;

    uint32_t kernelDataSize;
    uint32_t kernelNameSize;
    uint32_t dshSize;
    uint32_t gshSize;
    uint32_t kernelHeapSize;
    uint32_t sshSize;
    uint32_t patchListSize;

    uint32_t checkSum;
    uint64_t shaderHashCode;
    uint32_t kernelUnpaddedSize;

    std::unique_ptr<MockProgram> program;
    MockContext *pContext;
    const KernelInfo *pKernelInfo;
};
