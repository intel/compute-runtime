/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/extensions/public/cl_ext_private.h"
#include "level_zero/core/source/module/defines_ext.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"
#include "zello_common.h"

#include <cstring>
#include <string>

namespace Test {
decltype(&zexCounterBasedEventCreate2) zexCounterBasedEventCreate2Func = nullptr;
} // namespace Test

std::tuple<cl_platform_id, cl_device_id, cl_context> initOCL(ze_context_handle_t context) {
    cl_uint numPlatforms{};
    clGetPlatformIDs(0, nullptr, &numPlatforms);
    std::vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs(numPlatforms, platforms.data(), &numPlatforms);
    auto platform = platforms[0];

    cl_uint numDevices{};
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    std::vector<cl_device_id> devices(numDevices);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numDevices, devices.data(), &numDevices);
    auto device = devices[0];

    cl_context_properties properties[3] = {CL_L0_CONTEXT_HANDLE, reinterpret_cast<intptr_t>(context), 0};
    auto clContext = clCreateContext(properties, 1, &device, nullptr, nullptr, nullptr);

    return {platform, device, clContext};
}

ze_command_list_handle_t createImmIoqCmdList(ze_context_handle_t context, ze_device_handle_t device) {
    ze_command_list_handle_t commandListHandle{};
    ze_command_queue_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                           nullptr,
                                           0,
                                           0,
                                           ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT | ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                           ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                           ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    zeCommandListCreateImmediate(context, device, &cmdListDesc, &commandListHandle);
    return commandListHandle;
}

cl_command_queue createOclCmdQFromL0(ze_command_list_handle_t cmdList, cl_device_id device, cl_context context) {
    cl_queue_properties properties[5] = {CL_L0_IMMEDIATE_CMD_LIST_HANDLE, reinterpret_cast<cl_properties>(cmdList), CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    return clCreateCommandQueueWithProperties(context, device, properties, nullptr);
}

const char *kernelSource =
    "__kernel void increment_and_sum(                     \n"
    "    __global int* a,                                 \n"
    "    __global int* b,                                 \n"
    "    __global int* c)                                 \n"
    "{                                                     \n"
    "    int gid = get_global_id(0);                      \n"
    "                                                      \n"
    "    int a_val = a[gid] + 1;                          \n"
    "    int b_val = b[gid] + 1;                          \n"
    "                                                      \n"
    "    a[gid] = a_val;                                  \n"
    "    b[gid] = b_val;                                  \n"
    "                                                      \n"
    "    c[gid] = a_val + b_val;                          \n"
    "}                                                     \n";

const char *imageKernelSource =
    "#pragma OPENCL EXTENSION __opencl_c_read_write_images : enable \n"
    "\n"
    "__kernel void increment_and_sum_image(                \n"
    "    __read_write image2d_t a,                         \n"
    "    __read_write image2d_t b,                         \n"
    "    __read_write image2d_t c)                         \n"
    "{                                                      \n"
    "    int2 coord = (int2)(get_global_id(0),             \n"
    "                        get_global_id(1));            \n"
    "                                                      \n"
    "    int4 a_val = read_imagei(a, coord) + (int4)(1);  \n"
    "    int4 b_val = read_imagei(b, coord) + (int4)(1);  \n"
    "                                                      \n"
    "    write_imagei(a, coord, a_val);                    \n"
    "    write_imagei(b, coord, b_val);                    \n"
    "                                                      \n"
    "    write_imagei(c, coord, a_val + b_val);            \n"
    "}                                                     \n";

std::tuple<cl_program, cl_kernel> createOCLKernel(cl_context context, cl_device_id device) {
    auto program = clCreateProgramWithSource(context, 1, &kernelSource, nullptr, nullptr);
    auto buildResult = clBuildProgram(program, 1, &device, "-cl-std=CL3.0", nullptr, nullptr);
    if (buildResult != CL_SUCCESS) {
        size_t logSize{};
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::string buildLog(logSize, '\0');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog.data(), nullptr);
        printf("CL build failed for increment_and_sum: %s\n", buildLog.c_str());
    }
    auto kernel = clCreateKernel(program, "increment_and_sum", nullptr);
    return {program, kernel};
}

std::tuple<cl_program, cl_kernel> createOCLImageKernel(cl_context context, cl_device_id device) {
    auto program = clCreateProgramWithSource(context, 1, &imageKernelSource, nullptr, nullptr);
    auto buildResult = clBuildProgram(program, 1, &device, "-cl-std=CL3.0", nullptr, nullptr);
    if (buildResult != CL_SUCCESS) {
        size_t logSize{};
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::string buildLog(logSize, '\0');
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, buildLog.data(), nullptr);
        printf("CL build failed for increment_and_sum_image: %s\n", buildLog.c_str());
    }
    auto kernel = clCreateKernel(program, "increment_and_sum_image", nullptr);
    return {program, kernel};
}

std::tuple<ze_module_handle_t, ze_kernel_handle_t> createL0Kernel(ze_context_handle_t context, ze_device_handle_t device) {
    ze_module_handle_t module{};
    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_OCLC, strlen(kernelSource) + 1, reinterpret_cast<const uint8_t *>(kernelSource), "-cl-std=CL3.0", nullptr};

    ze_module_build_log_handle_t buildLog{};
    auto result = zeModuleCreate(context, device, &moduleDescription, &module, &buildLog);
    if (result != ZE_RESULT_SUCCESS) {
        size_t logSize{};
        zeModuleBuildLogGetString(buildLog, &logSize, nullptr);
        std::string log(logSize, '\0');
        zeModuleBuildLogGetString(buildLog, &logSize, log.data());
        printf("L0 module build failed for increment_and_sum: %s\n", log.c_str());
    }
    if (buildLog) {
        zeModuleBuildLogDestroy(buildLog);
    }

    ze_kernel_handle_t kernel{};
    ze_kernel_desc_t kernelDescription = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "increment_and_sum"};

    zeKernelCreate(module, &kernelDescription, &kernel);

    return {module, kernel};
}

std::tuple<ze_module_handle_t, ze_kernel_handle_t> createL0ImageKernel(ze_context_handle_t context, ze_device_handle_t device) {
    ze_module_handle_t module{};

    ze_module_desc_t moduleDescription = {ZE_STRUCTURE_TYPE_MODULE_DESC, nullptr, ZE_MODULE_FORMAT_OCLC, strlen(imageKernelSource) + 1, reinterpret_cast<const uint8_t *>(imageKernelSource), "-cl-std=CL3.0", nullptr};

    ze_module_build_log_handle_t buildLog{};
    auto result = zeModuleCreate(context, device, &moduleDescription, &module, &buildLog);
    if (result != ZE_RESULT_SUCCESS) {
        size_t logSize{};
        zeModuleBuildLogGetString(buildLog, &logSize, nullptr);
        std::string log(logSize, '\0');
        zeModuleBuildLogGetString(buildLog, &logSize, log.data());
        printf("L0 module build failed for increment_and_sum_image: %s\n", log.c_str());
    }
    if (buildLog) {
        zeModuleBuildLogDestroy(buildLog);
    }

    ze_kernel_handle_t kernel{};
    ze_kernel_desc_t kernelDescription = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, "increment_and_sum_image"};

    zeKernelCreate(module, &kernelDescription, &kernel);

    return {module, kernel};
}

ze_event_handle_t createCbEvent(ze_driver_handle_t driverHandle, ze_device_handle_t device, ze_context_handle_t context) {
    if (Test::zexCounterBasedEventCreate2Func == nullptr) {
        zeDriverGetExtensionFunctionAddress(driverHandle, "zexCounterBasedEventCreate2", reinterpret_cast<void **>(&Test::zexCounterBasedEventCreate2Func));
    }
    ze_event_handle_t e{};
    zex_counter_based_event_desc_t desc{ZEX_STRUCTURE_COUNTER_BASED_EVENT_DESC};
    desc.signalScope = ZE_EVENT_SCOPE_FLAG_HOST;
    desc.waitScope = ZE_EVENT_SCOPE_FLAG_DEVICE;
    desc.flags = ZEX_COUNTER_BASED_EVENT_FLAG_IMMEDIATE | ZEX_COUNTER_BASED_EVENT_FLAG_KERNEL_TIMESTAMP;
    Test::zexCounterBasedEventCreate2Func(context, device, &desc, &e);
    return e;
}

void runBufferInteropTest(ze_context_handle_t context, ze_driver_handle_t driverHandle, ze_device_handle_t device,
                          cl_device_id clDevice, cl_context clContext,
                          ze_command_list_handle_t cmdList, cl_command_queue clCmdQL0) {

    auto [module, kernel] = createL0Kernel(context, device);
    auto [clProgram, clKernel] = createOCLKernel(clContext, clDevice);

    size_t wgs = 64;
    size_t wgc = 256;
    ze_group_count_t l0wgc{static_cast<uint32_t>(wgc), 1u, 1u};
    size_t gws = wgc * wgs;
    zeKernelSetGroupSize(kernel, static_cast<uint32_t>(wgs), 1u, 1u);

    std::vector<int> dataA(gws, 1);
    std::vector<int> dataB(gws, 2);

    auto memA = clCreateBufferWithProperties(clContext, 0, CL_MEM_COPY_HOST_PTR, gws * sizeof(int), dataA.data(), nullptr);
    void *ptrA = nullptr;
    clGetMemObjectInfo(memA, CL_L0_MEM_OBJ_HANDLE, sizeof(void *), &ptrA, nullptr);

    auto memB = clCreateBufferWithProperties(clContext, 0, CL_MEM_COPY_HOST_PTR, gws * sizeof(int), dataB.data(), nullptr);
    void *ptrB = nullptr;
    clGetMemObjectInfo(memB, CL_L0_MEM_OBJ_HANDLE, sizeof(void *), &ptrB, nullptr);

    ze_device_mem_alloc_desc_t deviceDescC{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
    void *ptrC = nullptr;
    zeMemAllocDevice(context, &deviceDescC, gws * sizeof(int), 0u, device, &ptrC);
    cl_mem_properties memObjHandleProperties[] = {CL_L0_MEM_OBJ_HANDLE, reinterpret_cast<cl_mem_properties>(ptrC), 0};
    auto memC = clCreateBufferWithProperties(clContext, memObjHandleProperties, 0, gws * sizeof(int), nullptr, nullptr);

    zeKernelSetArgumentValue(kernel, 0, sizeof(void *), &ptrA);
    zeKernelSetArgumentValue(kernel, 1, sizeof(void *), &ptrB);
    zeKernelSetArgumentValue(kernel, 2, sizeof(void *), &ptrC);
    clSetKernelArg(clKernel, 0, sizeof(cl_mem), &memA);
    clSetKernelArg(clKernel, 1, sizeof(cl_mem), &memB);
    clSetKernelArg(clKernel, 2, sizeof(cl_mem), &memC);

    {
        cl_event clEvent{};
        auto event = createCbEvent(driverHandle, device, context);

        zeCommandListAppendLaunchKernel(cmdList, kernel, &l0wgc, event, 0, nullptr);
        clEnqueueNDRangeKernel(clCmdQL0, clKernel, 1, nullptr, &gws, &wgs, 0, nullptr, &clEvent);

        clFinish(clCmdQL0);

        // verify data
        bool errorFound = false;

        int *clIntPtrA = static_cast<int *>(clEnqueueMapBuffer(clCmdQL0, memA, true, 0, 0, gws * sizeof(int), 0, nullptr, nullptr, nullptr));
        int *clIntPtrB = static_cast<int *>(clEnqueueMapBuffer(clCmdQL0, memB, true, 0, 0, gws * sizeof(int), 0, nullptr, nullptr, nullptr));
        int *clIntPtrC = static_cast<int *>(clEnqueueMapBuffer(clCmdQL0, memC, true, 0, 0, gws * sizeof(int), 0, nullptr, nullptr, nullptr));

        for (size_t i = 0; i < gws; ++i) {
            if (clIntPtrA[i] != 3 || clIntPtrB[i] != 4 || clIntPtrC[i] != 7) {
                printf("CL L0 DATA INTEROP ERROR\n");
                errorFound = true;
                break;
            }
        }
        if (!errorFound) {
            printf("CL L0 DATA INTEROP CORRECT\n");
        }

        clEnqueueUnmapMemObject(clCmdQL0, memA, clIntPtrA, 0, nullptr, nullptr);
        clEnqueueUnmapMemObject(clCmdQL0, memB, clIntPtrB, 0, nullptr, nullptr);
        clEnqueueUnmapMemObject(clCmdQL0, memC, clIntPtrC, 0, nullptr, nullptr);

        // verify IOQ
        zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max());

        cl_ulong clStart{};
        clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStart, nullptr);

        ze_kernel_timestamp_result_t timestamps{};
        zeEventQueryKernelTimestamp(event, &timestamps);
        ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        zeDeviceGetProperties(device, &deviceProperties);
        uint64_t l0end = timestamps.global.kernelEnd * deviceProperties.timerResolution;

        if (clStart > l0end) {
            printf("CL L0 IOQ INTEROP CORRECT\n");
        } else {
            printf("CL L0 IOQ INTEROP ERROR\n");
            std::cout << "CL TS: " << clStart << ", L0 TS: " << l0end << "\n";
        }

        // verify TS
        ze_event_handle_t l0ClEventHandle{};
        clGetEventInfo(clEvent, CL_L0_EVENT_HANDLE, sizeof(ze_event_handle_t), &l0ClEventHandle, nullptr);
        zeEventQueryKernelTimestamp(l0ClEventHandle, &timestamps);
        uint64_t l0ClStart = timestamps.global.kernelStart * deviceProperties.timerResolution;

        if (l0ClStart == clStart) {
            printf("CL L0 TS INTEROP CORRECT\n");
        } else {
            printf("CL L0 TS INTEROP ERROR\n");
            std::cout << "CL TS: " << clStart << ", L0 TS: " << l0ClStart << "\n";
        }

        zeEventDestroy(event);
        clReleaseEvent(clEvent);
    }

    zeKernelDestroy(kernel);
    clReleaseKernel(clKernel);

    zeModuleDestroy(module);
    clReleaseProgram(clProgram);
}

void runImageInteropTest(ze_context_handle_t context, ze_driver_handle_t driverHandle, ze_device_handle_t device,
                         cl_device_id clDevice, cl_context clContext,
                         ze_command_list_handle_t cmdList, cl_command_queue clCmdQL0) {

    auto [module, kernel] = createL0ImageKernel(context, device);
    auto [clProgram, clKernel] = createOCLImageKernel(clContext, clDevice);

    constexpr uint32_t imageWidth = 256;
    constexpr uint32_t imageHeight = 64;

    size_t gwsImage[2] = {imageWidth, imageHeight};
    size_t lwsImage[2] = {16, 4};
    ze_group_count_t l0wgcImage{imageWidth / 16, imageHeight / 4, 1u};
    zeKernelSetGroupSize(kernel, 16u, 4u, 1u);

    cl_image_format clFormat{};
    clFormat.image_channel_order = CL_RGBA;
    clFormat.image_channel_data_type = CL_SIGNED_INT32;

    cl_image_desc clImageDesc{};
    clImageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    clImageDesc.image_width = imageWidth;
    clImageDesc.image_height = imageHeight;

    std::vector<int> dataA(imageWidth * imageHeight * 4, 1);
    std::vector<int> dataB(imageWidth * imageHeight * 4, 2);

    auto imgA = clCreateImage(clContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &clFormat, &clImageDesc, dataA.data(), nullptr);
    ze_image_handle_t l0ImgA = nullptr;
    clGetMemObjectInfo(imgA, CL_L0_MEM_OBJ_HANDLE, sizeof(ze_image_handle_t), &l0ImgA, nullptr);

    auto imgB = clCreateImage(clContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &clFormat, &clImageDesc, dataB.data(), nullptr);
    ze_image_handle_t l0ImgB = nullptr;
    clGetMemObjectInfo(imgB, CL_L0_MEM_OBJ_HANDLE, sizeof(ze_image_handle_t), &l0ImgB, nullptr);

    ze_image_desc_t l0ImageDesc{ZE_STRUCTURE_TYPE_IMAGE_DESC};
    l0ImageDesc.type = ZE_IMAGE_TYPE_2D;
    l0ImageDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32;
    l0ImageDesc.format.type = ZE_IMAGE_FORMAT_TYPE_SINT;
    l0ImageDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    l0ImageDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    l0ImageDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    l0ImageDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    l0ImageDesc.width = imageWidth;
    l0ImageDesc.height = imageHeight;
    l0ImageDesc.depth = 1;
    ze_image_handle_t l0ImgC = nullptr;
    zeImageCreate(context, device, &l0ImageDesc, &l0ImgC);
    cl_mem_properties imgObjHandleProperties[] = {CL_L0_MEM_OBJ_HANDLE, reinterpret_cast<cl_mem_properties>(l0ImgC), 0};
    auto imgC = clCreateImageWithProperties(clContext, imgObjHandleProperties, CL_MEM_READ_WRITE, &clFormat, &clImageDesc, nullptr, nullptr);

    zeKernelSetArgumentValue(kernel, 0, sizeof(ze_image_handle_t), &l0ImgA);
    zeKernelSetArgumentValue(kernel, 1, sizeof(ze_image_handle_t), &l0ImgB);
    zeKernelSetArgumentValue(kernel, 2, sizeof(ze_image_handle_t), &l0ImgC);
    clSetKernelArg(clKernel, 0, sizeof(cl_mem), &imgA);
    clSetKernelArg(clKernel, 1, sizeof(cl_mem), &imgB);
    clSetKernelArg(clKernel, 2, sizeof(cl_mem), &imgC);

    {
        cl_event clEvent{};
        auto event = createCbEvent(driverHandle, device, context);

        zeCommandListAppendLaunchKernel(cmdList, kernel, &l0wgcImage, event, 0, nullptr);
        clEnqueueNDRangeKernel(clCmdQL0, clKernel, 2, nullptr, gwsImage, lwsImage, 0, nullptr, &clEvent);

        clFinish(clCmdQL0);

        // verify data
        bool errorFound = false;

        size_t origin[3] = {0, 0, 0};
        size_t region[3] = {imageWidth, imageHeight, 1};
        std::vector<int> resultA(imageWidth * imageHeight * 4);
        std::vector<int> resultB(imageWidth * imageHeight * 4);
        std::vector<int> resultC(imageWidth * imageHeight * 4);

        clEnqueueReadImage(clCmdQL0, imgA, true, origin, region, 0, 0, resultA.data(), 0, nullptr, nullptr);
        clEnqueueReadImage(clCmdQL0, imgB, true, origin, region, 0, 0, resultB.data(), 0, nullptr, nullptr);
        clEnqueueReadImage(clCmdQL0, imgC, true, origin, region, 0, 0, resultC.data(), 0, nullptr, nullptr);

        for (size_t i = 0; i < imageWidth * imageHeight * 4; ++i) {
            if (resultA[i] != 3 || resultB[i] != 4 || resultC[i] != 7) {
                printf("CL L0 IMAGE DATA INTEROP ERROR\n");
                errorFound = true;
                break;
            }
        }
        if (!errorFound) {
            printf("CL L0 IMAGE DATA INTEROP CORRECT\n");
        }

        // verify IOQ
        zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max());

        cl_ulong clStart{};
        clGetEventProfilingInfo(clEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &clStart, nullptr);

        ze_kernel_timestamp_result_t timestamps{};
        zeEventQueryKernelTimestamp(event, &timestamps);
        ze_device_properties_t deviceProperties{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        zeDeviceGetProperties(device, &deviceProperties);
        uint64_t l0end = timestamps.global.kernelEnd * deviceProperties.timerResolution;

        if (clStart > l0end) {
            printf("CL L0 IMAGE IOQ INTEROP CORRECT\n");
        } else {
            printf("CL L0 IMAGE IOQ INTEROP ERROR\n");
            std::cout << "CL TS: " << clStart << ", L0 TS: " << l0end << "\n";
        }

        // verify TS
        ze_event_handle_t l0ClEventHandle{};
        clGetEventInfo(clEvent, CL_L0_EVENT_HANDLE, sizeof(ze_event_handle_t), &l0ClEventHandle, nullptr);
        zeEventQueryKernelTimestamp(l0ClEventHandle, &timestamps);
        uint64_t l0ClStart = timestamps.global.kernelStart * deviceProperties.timerResolution;

        if (l0ClStart == clStart) {
            printf("CL L0 IMAGE TS INTEROP CORRECT\n");
        } else {
            printf("CL L0 IMAGE TS INTEROP ERROR\n");
            std::cout << "CL TS: " << clStart << ", L0 TS: " << l0ClStart << "\n";
        }

        zeEventDestroy(event);
        clReleaseEvent(clEvent);
    }

    zeKernelDestroy(kernel);
    clReleaseKernel(clKernel);

    zeModuleDestroy(module);
    clReleaseProgram(clProgram);

    clReleaseMemObject(imgA);
    clReleaseMemObject(imgB);
    clReleaseMemObject(imgC);
}

int main(int argc, char *argv[]) {
    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    auto [clPlatform, clDevice, clContext] = initOCL(context);
    auto cmdList = createImmIoqCmdList(context, device);
    auto clCmdQL0 = createOclCmdQFromL0(cmdList, clDevice, clContext);

    ze_context_handle_t contextCheck{};
    clGetContextInfo(clContext, CL_L0_CONTEXT_HANDLE, sizeof(ze_context_handle_t), &contextCheck, nullptr);
    if (contextCheck == context) {
        printf("CL L0 CONTEXT HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 CONTEXT HANDLE INTEROP ERROR\n");
    }

    ze_device_handle_t deviceCheck{};
    clGetDeviceInfo(clDevice, CL_L0_DEVICE_HANDLE, sizeof(ze_device_handle_t), &deviceCheck, nullptr);
    if (deviceCheck == device) {
        printf("CL L0 DEVICE HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 DEVICE HANDLE INTEROP ERROR\n");
    }

    ze_driver_handle_t driverHandleCheck{};
    clGetPlatformInfo(clPlatform, CL_L0_DRIVER_HANDLE, sizeof(ze_driver_handle_t), &driverHandleCheck, nullptr);
    if (driverHandleCheck == driverHandle) {
        printf("CL L0 DRIVER HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 DRIVER HANDLE INTEROP ERROR\n");
    }

    ze_command_list_handle_t cmdListCheck{};
    clGetCommandQueueInfo(clCmdQL0, CL_L0_IMMEDIATE_CMD_LIST_HANDLE, sizeof(ze_command_list_handle_t), &cmdListCheck, nullptr);
    if (cmdListCheck == cmdList) {
        printf("CL L0 CMD LIST HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 CMD LIST HANDLE INTEROP ERROR\n");
    }

    runBufferInteropTest(context, driverHandle, device, clDevice, clContext, cmdList, clCmdQL0);
    runImageInteropTest(context, driverHandle, device, clDevice, clContext, cmdList, clCmdQL0);

    zeCommandListDestroy(cmdList);
    clReleaseCommandQueue(clCmdQL0);

    clReleaseContext(clContext);
    clReleaseDevice(clDevice);

    return 0;
}
