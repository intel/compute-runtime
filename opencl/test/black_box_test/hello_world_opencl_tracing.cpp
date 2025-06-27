/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tracing/tracing_api.h"

#include "CL/cl.h"

#include <iostream>
#include <memory>
#include <sstream>

// Sample of OCL tracing interface usage.
// This test case covers using OCL tracing interface, user-definied callback,
// and a case which may lead to infinite recursive loop (nested API call in callback).
// Traced API call: clCreateContext

cl_int err = 0;
std::stringstream callbackOutput{};

#define SUCCESS_OR_ABORT_ERROR_PRINT(call, ...)                       \
    err = call(__VA_ARGS__);                                          \
    if (err != CL_SUCCESS) {                                          \
        std::cout << "Error on " << #call << "() call." << std::endl; \
        std::abort();                                                 \
    }

auto callback = [](ClFunctionId fid,
                   cl_callback_data *callbackData,
                   void *userData) {
    callbackOutput << "Function " << callbackData->functionName << " is called (" << (callbackData->site == CL_CALLBACK_SITE_ENTER ? "enter" : "exit") << ")" << std::endl;
};

auto contextNestedCallback = [](ClFunctionId fid,
                                cl_callback_data *callbackData,
                                void *userData) {
    static int numCalled = 0;
    numCalled++;

    if (numCalled > 2) { // desired two calls for enter and exit
        std::cout << "Error: possible infinite recursion on clCreateContext callback!" << std::endl;
        std::abort();
    }

    auto deviceId = reinterpret_cast<cl_device_id *>(userData);
    cl_context anotherContext = clCreateContext(0, 1, deviceId, NULL, NULL, &err);
    clReleaseContext(anotherContext);
};

bool validateOutput(const std::stringstream &out) {
    auto stringData = out.str();
    std::cout << "Captured callback output:\n"
              << stringData << std::endl;
    if (std::string::npos == stringData.find("Function clCreateContext is called (enter)\n")) {
        return false;
    }
    if (std::string::npos == stringData.find("Function clCreateContext is called (exit)\n")) {
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    std::unique_ptr<cl_platform_id[]> platforms;
    cl_device_id deviceId = 0;
    cl_uint platformsCount = 0;
    cl_context context = nullptr;
    cl_device_type deviceType = CL_DEVICE_TYPE_GPU;

    cl_tracing_handle tracingHandle = nullptr;
    cl_tracing_handle tracingHandleNestedCallback = nullptr;

    SUCCESS_OR_ABORT_ERROR_PRINT(clGetPlatformIDs, 0, NULL, &platformsCount)

    platforms.reset(new cl_platform_id[platformsCount]);
    SUCCESS_OR_ABORT_ERROR_PRINT(clGetPlatformIDs, platformsCount, platforms.get(), NULL)
    SUCCESS_OR_ABORT_ERROR_PRINT(clGetDeviceIDs, platforms.get()[0], deviceType, 1, &deviceId, NULL)

    decltype(clCreateTracingHandleINTEL) *clCreateTracingHandleINTELFunction = reinterpret_cast<decltype(clCreateTracingHandleINTEL) *>(clGetExtensionFunctionAddressForPlatform(platforms.get()[0], "clCreateTracingHandleINTEL"));

    SUCCESS_OR_ABORT_ERROR_PRINT(clCreateTracingHandleINTELFunction, deviceId, callback, nullptr, &tracingHandle)
    SUCCESS_OR_ABORT_ERROR_PRINT(clCreateTracingHandleINTELFunction, deviceId, contextNestedCallback, nullptr, &tracingHandleNestedCallback)

    decltype(clSetTracingPointINTEL) *clSetTracingPointINTELFunction = reinterpret_cast<decltype(clSetTracingPointINTEL) *>(clGetExtensionFunctionAddressForPlatform(platforms.get()[0], "clSetTracingPointINTEL"));
    SUCCESS_OR_ABORT_ERROR_PRINT(clSetTracingPointINTELFunction, tracingHandle, CL_FUNCTION_clCreateContext, CL_TRUE)
    SUCCESS_OR_ABORT_ERROR_PRINT(clSetTracingPointINTELFunction, tracingHandleNestedCallback, CL_FUNCTION_clCreateContext, CL_TRUE)

    decltype(clEnableTracingINTEL) *clEnableTracingINTELFunction = reinterpret_cast<decltype(clEnableTracingINTEL) *>(clGetExtensionFunctionAddressForPlatform(platforms.get()[0], "clEnableTracingINTEL"));
    SUCCESS_OR_ABORT_ERROR_PRINT(clEnableTracingINTELFunction, tracingHandle)
    SUCCESS_OR_ABORT_ERROR_PRINT(clEnableTracingINTELFunction, tracingHandleNestedCallback)

    context = clCreateContext(0, 1, &deviceId, NULL, NULL, &err);

    decltype(clDisableTracingINTEL) *clDisableTracingINTELFunction = reinterpret_cast<decltype(clDisableTracingINTEL) *>(clGetExtensionFunctionAddressForPlatform(platforms.get()[0], "clDisableTracingINTEL"));
    SUCCESS_OR_ABORT_ERROR_PRINT(clDisableTracingINTELFunction, tracingHandle)
    SUCCESS_OR_ABORT_ERROR_PRINT(clDisableTracingINTELFunction, tracingHandleNestedCallback)

    decltype(clDestroyTracingHandleINTEL) *clDestroyTracingHandleINTELFunction = reinterpret_cast<decltype(clDestroyTracingHandleINTEL) *>(clGetExtensionFunctionAddressForPlatform(platforms.get()[0], "clDestroyTracingHandleINTEL"));
    SUCCESS_OR_ABORT_ERROR_PRINT(clDestroyTracingHandleINTELFunction, tracingHandle)
    SUCCESS_OR_ABORT_ERROR_PRINT(clDestroyTracingHandleINTELFunction, tracingHandleNestedCallback)

    clReleaseContext(context);
    bool validationResult = validateOutput(callbackOutput);

    if (validationResult) {
        std::cout << "Hello World OpenCL Tracing Result: PASSED" << std::endl;
        return 0;
    }
    std::cout << "Hello World OpenCL Tracing Result: FAILED" << std::endl;
    return 1;
}