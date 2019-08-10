/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_program.h"

#include "cl_api_tests.h"

using namespace NEO;

using clGetDeviceGlobalVariablePointer = api_tests;
using clGetDeviceFunctionPointer = api_tests;

TEST_F(clGetDeviceGlobalVariablePointer, GivenNullMandatoryArgumentsThenReturnInvalidArgError) {
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::GlobalVariable;

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
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::GlobalVariable;

    void *globalRet = 0;
    size_t sizeRet = 0;
    auto ret = clGetDeviceGlobalVariablePointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &sizeRet, &globalRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, reinterpret_cast<uintptr_t>(globalRet));
    EXPECT_EQ(64U, sizeRet);
}

TEST_F(clGetDeviceGlobalVariablePointer, GivenFunctionSymbolNameThenReturnInvalidArgError) {
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::Function;

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
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::Function;

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
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::Function;

    cl_ulong fptrRet = 0;
    auto ret = clGetDeviceFunctionPointerINTEL(this->pContext->getDevice(0), this->pProgram, "A", &fptrRet);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(7U, fptrRet);
}

TEST_F(clGetDeviceFunctionPointer, GivenGlobalSymbolNameThenReturnInvalidArgError) {
    this->pProgram->symbols["A"].gpuAddress = 7U;
    this->pProgram->symbols["A"].symbol.size = 64U;
    this->pProgram->symbols["A"].symbol.type = NEO::SymbolInfo::GlobalVariable;
    this->pProgram->symbols["B"].gpuAddress = 7U;
    this->pProgram->symbols["B"].symbol.size = 64U;
    this->pProgram->symbols["B"].symbol.type = NEO::SymbolInfo::GlobalConstant;

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
