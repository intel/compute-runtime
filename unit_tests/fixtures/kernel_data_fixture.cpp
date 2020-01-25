/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/kernel_data_fixture.h"

#include "core/device_binary_format/patchtokens_decoder.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/string.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/program/program_info_from_patchtokens.h"

void KernelDataTest::buildAndDecode() {
    cl_int error = CL_SUCCESS;
    kernelBinaryHeader.CheckSum = checkSum;
    kernelBinaryHeader.DynamicStateHeapSize = dshSize;
    kernelBinaryHeader.GeneralStateHeapSize = gshSize;
    kernelBinaryHeader.KernelHeapSize = kernelHeapSize;
    kernelBinaryHeader.KernelNameSize = kernelNameSize;
    kernelBinaryHeader.KernelUnpaddedSize = kernelUnpaddedSize;
    kernelBinaryHeader.PatchListSize = patchListSize + sizeof(SPatchDataParameterStream);

    kernelBinaryHeader.ShaderHashCode = shaderHashCode;
    kernelBinaryHeader.SurfaceStateHeapSize = sshSize;

    kernelDataSize = sizeof(SKernelBinaryHeaderCommon) +
                     kernelNameSize + sshSize + dshSize + gshSize + kernelHeapSize + patchListSize;

    kernelDataSize += sizeof(SPatchDataParameterStream);

    pKernelData = static_cast<char *>(alignedMalloc(kernelDataSize, MemoryConstants::cacheLineSize));
    ASSERT_NE(nullptr, pKernelData);

    // kernel blob
    pCurPtr = pKernelData;

    // kernel header
    // first clear it because sizeof() > sum of sizeof(fields). this is due to packing
    memset(pCurPtr, 0, sizeof(SKernelBinaryHeaderCommon));
    *(SKernelBinaryHeaderCommon *)pCurPtr = kernelBinaryHeader;
    pCurPtr += sizeof(SKernelBinaryHeaderCommon);

    // kernel name
    memset(pCurPtr, 0, kernelNameSize);
    strcpy_s(pCurPtr, strlen(kernelName.c_str()) + 1, kernelName.c_str());
    pCurPtr += kernelNameSize;

    // kernel heap
    memcpy_s(pCurPtr, kernelHeapSize, pKernelHeap, kernelHeapSize);
    pCurPtr += kernelHeapSize;

    // general state heap
    memcpy_s(pCurPtr, gshSize, pGsh, gshSize);
    pCurPtr += gshSize;

    // dynamic state heap
    memcpy_s(pCurPtr, dshSize, pDsh, dshSize);
    pCurPtr += dshSize;

    // surface state heap
    memcpy_s(pCurPtr, sshSize, pSsh, sshSize);
    pCurPtr += sshSize;

    // patch list
    memcpy_s(pCurPtr, patchListSize, pPatchList, patchListSize);
    pCurPtr += patchListSize;

    // add a data stream member
    iOpenCL::SPatchDataParameterStream dataParameterStream;
    dataParameterStream.Token = PATCH_TOKEN_DATA_PARAMETER_STREAM;
    dataParameterStream.Size = sizeof(SPatchDataParameterStream);
    dataParameterStream.DataParameterStreamSize = 0x40;
    memcpy_s(pCurPtr, sizeof(SPatchDataParameterStream),
             &dataParameterStream, sizeof(SPatchDataParameterStream));

    pCurPtr += sizeof(SPatchDataParameterStream);

    // now build a program with this kernel data
    iOpenCL::SProgramBinaryHeader header = {};
    NEO::PatchTokenBinary::ProgramFromPatchtokens programFromPatchtokens;
    programFromPatchtokens.decodeStatus = PatchTokenBinary::DecoderError::Success;
    programFromPatchtokens.header = &header;
    programFromPatchtokens.kernels.resize(1);
    auto &kernelFromPatchtokens = *programFromPatchtokens.kernels.rbegin();
    auto kernelBlob = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(pKernelData), kernelDataSize);
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelBlob, kernelFromPatchtokens);
    EXPECT_TRUE(decodeSuccess);

    ProgramInfo programInfo;
    NEO::populateProgramInfo(programInfo, programFromPatchtokens, DeviceInfoKernelPayloadConstants());
    error = program->processProgramInfo(programInfo);
    EXPECT_EQ(CL_SUCCESS, error);

    // extract the kernel info
    pKernelInfo = program->Program::getKernelInfo(kernelName.c_str());

    // validate header
    EXPECT_EQ(0, memcmp(pKernelInfo->heapInfo.pKernelHeader, &kernelBinaryHeader, sizeof(SKernelBinaryHeaderCommon)));

    // validate name
    EXPECT_STREQ(pKernelInfo->name.c_str(), kernelName.c_str());

    // validate each heap
    if (pKernelHeap != nullptr) {
        EXPECT_EQ(0, memcmp(pKernelInfo->heapInfo.pKernelHeap, pKernelHeap, kernelHeapSize));
    }
    if (pGsh != nullptr) {
        EXPECT_EQ(0, memcmp(pKernelInfo->heapInfo.pGsh, pGsh, gshSize));
    }
    if (pDsh != nullptr) {
        EXPECT_EQ(0, memcmp(pKernelInfo->heapInfo.pDsh, pDsh, dshSize));
    }
    if (pSsh != nullptr) {
        EXPECT_EQ(0, memcmp(pKernelInfo->heapInfo.pSsh, pSsh, sshSize));
    }
    if (kernelHeapSize) {
        auto kernelAllocation = pKernelInfo->getGraphicsAllocation();
        UNRECOVERABLE_IF(kernelAllocation == nullptr);
        EXPECT_EQ(kernelAllocation->getUnderlyingBufferSize(), kernelHeapSize);
        auto kernelIsa = kernelAllocation->getUnderlyingBuffer();
        EXPECT_EQ(0, memcmp(kernelIsa, pKernelInfo->heapInfo.pKernelHeap, kernelHeapSize));
    } else {
        EXPECT_EQ(nullptr, pKernelInfo->getGraphicsAllocation());
    }
}
