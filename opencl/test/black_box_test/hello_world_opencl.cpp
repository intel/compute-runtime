/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "CL/cl.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

using namespace std;

int main(int argc, char **argv) {
    int retVal = 0;
    const char *fileName = "kernelOutput.txt";
    cl_int err = 0;
    unique_ptr<cl_platform_id[]> platforms;
    cl_device_id device_id = 0;
    cl_uint platformsCount = 0;
    cl_context context = NULL;
    cl_command_queue queue = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_mem buffer = NULL;
    const size_t bufferSize = sizeof(int) * 1024;

    size_t offset[3] = {0, 0, 0};
    size_t gws[3] = {4, 1, 1};
    size_t lws[3] = {4, 1, 1};
    cl_uint dimension = 1;
    bool validatePrintfOutput = true;

    err = clGetPlatformIDs(0, NULL, &platformsCount);

    if (err != CL_SUCCESS) {
        cout << "Error getting platforms" << endl;
        abort();
    }
    cout << "num_platforms == " << platformsCount << endl;

    platforms.reset(new cl_platform_id[platformsCount]);
    err = clGetPlatformIDs(platformsCount, platforms.get(), NULL);
    if (err != CL_SUCCESS) {
        cout << "Error clGetPlatformIDs failed" << endl;
        abort();
    }

    cl_device_type deviceType = CL_DEVICE_TYPE_GPU;
    err = clGetDeviceIDs(platforms.get()[0], deviceType, 1, &device_id, NULL);
    if (err != CL_SUCCESS) {
        cout << "Error getting device_id" << endl;
        abort();
    }

    context = clCreateContext(0, 1, &device_id, NULL, NULL, &err);
    if (err != CL_SUCCESS) {
        cout << "Error creating context" << endl;
        abort();
    }

    queue = clCreateCommandQueue(context, device_id, 0, &err);
    if (err != CL_SUCCESS || !queue) {
        cout << "Error creating command queue" << endl;
        abort();
    }

    {

        char source[] = R"===(
                                __kernel void hello(__global int* in){
                                    int i = in[0] > 0 ? in[0] : 0;
                                    for( ; i >= 0; i--)
                                    {
                                        printf("%d\n", i);
                                    }
                                    printf("Hello world!\n");
                                    in[1] = 2;
                                }
                        )===";
        const char *strings = source;

        program = clCreateProgramWithSource(context, 1, &strings, 0, &err);

        if (err != CL_SUCCESS) {
            cout << "Error creating program" << endl;
            abort();
        }

        err = clBuildProgram(program, 1, &device_id, nullptr, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            cout << "Error building program" << endl;

            size_t logSize = 0;
            unique_ptr<char[]> buildLog;

            if (clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize) == CL_SUCCESS) {
                if (logSize) {
                    buildLog.reset(new char[logSize]);

                    if (buildLog) {
                        if (clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, logSize, buildLog.get(), NULL) == CL_SUCCESS) {
                            buildLog[logSize - 1] = '\0';
                            cout << "Build log:\n"
                                 << buildLog.get() << endl;
                        }
                    }
                }
            }
            abort();
        }

        kernel = clCreateKernel(program, "hello", &err);
        if (err != CL_SUCCESS) {
            cout << "Error creating kernel" << endl;
            abort();
        }
    }

    {
        cl_mem_flags flags = CL_MEM_READ_WRITE;
        buffer = clCreateBuffer(context, flags, bufferSize, nullptr, &err);

        if (err != CL_SUCCESS) {
            cout << "Error creating buffer" << endl;
            abort();
        }

        void *ptr = clEnqueueMapBuffer(queue, buffer, CL_TRUE, CL_MAP_WRITE, 0, bufferSize, 0, nullptr, nullptr, &err);
        if (err || ptr == nullptr) {
            cout << "Error mapping buffer" << endl;
            abort();
        }
        memset(ptr, 0, bufferSize);
        *(int *)ptr = 4;

        err = clEnqueueUnmapMemObject(queue, buffer, ptr, 0, nullptr, nullptr);
        if (err) {
            cout << "Error unmapping buffer" << endl;
            abort();
        }
    }

    err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &buffer);
    if (err) {
        cout << "Error setting kernel arg" << endl;
        abort();
    }

    if (validatePrintfOutput) {
        auto newFile = freopen(fileName, "w", stdout);
        if (newFile == nullptr) {
            cout << "Failed in freopen()" << endl;
            abort();
        }
    }

    err = clEnqueueNDRangeKernel(queue, kernel, dimension, offset, gws, lws, 0, 0, nullptr);
    if (err) {
        cout << "Error NDRange" << endl;
        abort();
    }

    {
        void *ptr = clEnqueueMapBuffer(queue, buffer, CL_TRUE, CL_MAP_READ, 0, bufferSize, 0, nullptr, nullptr, &err);
        if (err || ptr == nullptr) {
            cout << "Error mapping buffer" << endl;
            abort();
        }

        if (((int *)ptr)[1] != 2) {
            cout << "Invalid value in buffer" << endl;
            retVal = 1;
        }

        err = clEnqueueUnmapMemObject(queue, buffer, ptr, 0, nullptr, nullptr);
        if (err) {
            cout << "Error unmapping buffer" << endl;
            abort();
        }

        err = clFinish(queue);
        if (err) {
            cout << "Error Finish" << endl;
            abort();
        }
    }

    if (validatePrintfOutput) {
        auto kernelOutput = make_unique<char[]>(1024);
        auto kernelOutputFile = fopen(fileName, "r");
        auto result = fread(kernelOutput.get(), sizeof(char), 1024, kernelOutputFile);
        fclose(kernelOutputFile);
        if (result == 0) {
            fclose(stdout);
            abort();
        }
        char *foundString = strstr(kernelOutput.get(), "Hello world!");

        if (foundString == nullptr) {
            retVal = 1;
        }
    }

    clReleaseMemObject(buffer);
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
    return retVal;
}