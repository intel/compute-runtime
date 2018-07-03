/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "gtest/gtest.h"
#include "runtime/program/kernel_info.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_program.h"

using namespace OCLRT;
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
        pDevice = MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr);
    }

    void TearDown() override {
        if (pKernelInfo->kernelAllocation) {
            pDevice->getMemoryManager()->freeGraphicsMemory(pKernelInfo->kernelAllocation);
            const_cast<KernelInfo *>(pKernelInfo)->kernelAllocation = nullptr;
        }
        delete pDevice;
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

    MockProgram program;
    MockDevice *pDevice;
    const KernelInfo *pKernelInfo;
};
