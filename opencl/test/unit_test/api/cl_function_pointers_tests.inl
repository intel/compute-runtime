/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetDeviceGlobalVariablePointer = ApiTests;
using clGetDeviceFunctionPointer = ApiTests;

TEST_F(clGetDeviceGlobalVariablePointer, GivenNullMandatoryArgumentsThenReturnInvalidArgError) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::globalVariables;

    void *globalRet = 0;
    auto ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", nullptr, &globalRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, reinterpret_cast<uintptr_t>(globalRet));

    ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);

    ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), nullptr, "A", nullptr, &globalRet);
    EXPECT_EQ(CL_INVALID_PROGRAM, ret);

    ret = clGetDeviceGlobalVariablePointerINTEL(nullptr, this->pProgram, "A", nullptr, &globalRet);
    EXPECT_EQ(CL_INVALID_DEVICE, ret);
}

TEST_F(clGetDeviceGlobalVariablePointer, GivenValidSymbolNameThenReturnProperAddressAndSize) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::globalVariables;

    void *globalRet = 0;
    size_t sizeRet = 0;
    auto ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &sizeRet, &globalRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, reinterpret_cast<uintptr_t>(globalRet));
    EXPECT_EQ(64U, sizeRet);
}

TEST_F(clGetDeviceGlobalVariablePointer, GivenFunctionSymbolNameThenReturnInvalidArgError) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::instructions;

    void *globalRet = 0;
    auto ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", nullptr, &globalRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}

TEST_F(clGetDeviceGlobalVariablePointer, GivenUnknownSymbolNameThenReturnInvalidArgError) {
    void *globalRet = 0;
    auto ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", nullptr, &globalRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}

TEST_F(clGetDeviceFunctionPointer, GivenNullMandatoryArgumentsThenReturnInvalidArgError) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::instructions;

    cl_ulong fptrRet = 0;
    auto ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, fptrRet);

    ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);

    ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), nullptr, "A", &fptrRet);
    EXPECT_EQ(CL_INVALID_PROGRAM, ret);

    ret = clGetDeviceFunctionPointerINTEL(nullptr, this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_INVALID_DEVICE, ret);
}

TEST_F(clGetDeviceFunctionPointer, GivenValidSymbolNameThenReturnProperAddress) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::instructions;

    cl_ulong fptrRet = 0;
    auto ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, fptrRet);
}

TEST_F(clGetDeviceFunctionPointer, GivenGlobalSymbolNameThenReturnInvalidArgError) {
    auto &symbols = pProgram->buildInfos[testedRootDeviceIndex].symbols;
    symbols["A"].gpuAddress = 7U;
    symbols["A"].symbol.size = 64U;
    symbols["A"].symbol.segment = NEO::SegmentType::globalVariables;
    symbols["B"].gpuAddress = 7U;
    symbols["B"].symbol.size = 64U;
    symbols["B"].symbol.segment = NEO::SegmentType::globalConstants;

    cl_ulong fptrRet = 0;
    auto ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
    ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "B", &fptrRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}
TEST_F(clGetDeviceFunctionPointer, GivenUnknownSymbolNameThenReturnInvalidArgError) {
    cl_ulong fptrRet = 0;
    auto ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, ret);
}
