/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"
#include "zello_common.h"

#include <cstring>
#include <string>

constexpr std::string_view blackBoxName = "Zello OpenCL Interop";

std::tuple<cl_platform_id, cl_device_id, cl_context> initOCL(ze_context_handle_t context) {
    cl_uint numPlatforms{};
    clGetPlatformIDs(0, nullptr, &numPlatforms);
    if (numPlatforms == 0) {
        printf("No OpenCL platform found, check OCL_ICD_FILENAMES\n");
        return {nullptr, nullptr, nullptr};
    }
    std::vector<cl_platform_id> platforms(numPlatforms);
    clGetPlatformIDs(numPlatforms, platforms.data(), &numPlatforms);
    auto platform = platforms[0];

    cl_uint numDevices{};
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);
    if (numDevices == 0) {
        printf("No OpenCL GPU device found\n");
        return {nullptr, nullptr, nullptr};
    }
    std::vector<cl_device_id> devices(numDevices);
    clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numDevices, devices.data(), &numDevices);
    auto device = devices[0];

    cl_context_properties properties[3] = {CL_CONTEXT_L0_HANDLE_INTEL, reinterpret_cast<intptr_t>(context), 0};
    auto clContext = clCreateContext(properties, 1, &device, nullptr, nullptr, nullptr);
    if (clContext == nullptr) {
        printf("Failed to create OpenCL context from L0 context\n");
        return {nullptr, nullptr, nullptr};
    }

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
    cl_queue_properties properties[5] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL, reinterpret_cast<cl_properties>(cmdList), CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    return clCreateCommandQueueWithProperties(context, device, properties, nullptr);
}

std::string getInfoString(cl_platform_id platform, cl_platform_info paramName) {
    size_t size{};
    clGetPlatformInfo(platform, paramName, 0, nullptr, &size);
    std::string value(size, '\0');
    clGetPlatformInfo(platform, paramName, size, value.data(), nullptr);
    return value;
}

std::string getInfoString(cl_device_id device, cl_device_info paramName) {
    size_t size{};
    clGetDeviceInfo(device, paramName, 0, nullptr, &size);
    std::string value(size, '\0');
    clGetDeviceInfo(device, paramName, size, value.data(), nullptr);
    return value;
}

bool verifyExtensionAdvertised(cl_platform_id platform, cl_device_id device) {
    const std::string extensionName{"cl_intel_level_zero_interop"};
    const bool onPlatform = getInfoString(platform, CL_PLATFORM_EXTENSIONS).find(extensionName) != std::string::npos;
    const bool onDevice = getInfoString(device, CL_DEVICE_EXTENSIONS).find(extensionName) != std::string::npos;

    if (onPlatform && onDevice) {
        printf("CL L0 INTEROP EXTENSION ADVERTISED CORRECT\n");
    } else {
        printf("CL L0 INTEROP EXTENSION ADVERTISED ERROR\n");
        std::cout << "platform: " << onPlatform << ", device: " << onDevice << "\n";
    }
    return onPlatform && onDevice;
}

bool verifyImportedCmdQIgnoresProperties(ze_command_list_handle_t cmdList, cl_device_id device, cl_context context) {
    cl_queue_properties properties[] = {CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL, reinterpret_cast<cl_properties>(cmdList),
                                        CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                        CL_QUEUE_FAMILY_INTEL, 0x7,
                                        CL_QUEUE_INDEX_INTEL, 0x7,
                                        CL_QUEUE_THROTTLE_KHR, CL_QUEUE_THROTTLE_LOW_KHR,
                                        0};

    cl_int errcode = CL_SUCCESS;
    auto queue = clCreateCommandQueueWithProperties(context, device, properties, &errcode);

    cl_command_queue_properties reportedProperties = 0;
    ze_command_list_handle_t reportedCmdList{};
    if (queue != nullptr) {
        clGetCommandQueueInfo(queue, CL_QUEUE_PROPERTIES, sizeof(reportedProperties), &reportedProperties, nullptr);
        clGetCommandQueueInfo(queue, CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL, sizeof(reportedCmdList), &reportedCmdList, nullptr);
    }

    const bool propertiesCorrect = errcode == CL_SUCCESS && queue != nullptr && reportedCmdList == cmdList &&
                                   reportedProperties == static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE);
    if (propertiesCorrect) {
        printf("CL L0 IMPORTED CMDQ PROPERTIES CORRECT\n");
    } else {
        printf("CL L0 IMPORTED CMDQ PROPERTIES ERROR\n");
        std::cout << "errcode: " << errcode << ", properties: " << reportedProperties << "\n";
    }

    if (queue != nullptr) {
        clReleaseCommandQueue(queue);
    }
    return propertiesCorrect;
}

bool verifyImportedProperties(cl_mem memObj, const std::vector<cl_mem_properties> &expected, const char *label) {
    cl_mem_properties storedProperties[8]{};
    size_t storedSize{};
    auto ret = clGetMemObjectInfo(memObj, CL_MEM_PROPERTIES, sizeof(storedProperties), storedProperties, &storedSize);

    bool matches = (ret == CL_SUCCESS) && (storedSize == expected.size() * sizeof(cl_mem_properties));
    for (size_t i = 0; matches && i < expected.size(); ++i) {
        matches = (storedProperties[i] == expected[i]);
    }

    if (matches) {
        printf("CL L0 %s IMPORTED PROPERTIES CORRECT\n", label);
    } else {
        printf("CL L0 %s IMPORTED PROPERTIES ERROR\n", label);
        std::cout << "ret: " << ret << ", size: " << storedSize << "\n";
    }
    return matches;
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
    ze_event_handle_t e{};
    ze_event_counter_based_desc_t desc{ZE_STRUCTURE_TYPE_EVENT_COUNTER_BASED_DESC};
    desc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    desc.wait = ZE_EVENT_SCOPE_FLAG_DEVICE;
    desc.flags = ZE_EVENT_COUNTER_BASED_FLAG_IMMEDIATE | ZE_EVENT_COUNTER_BASED_FLAG_DEVICE_TIMESTAMP;
    zeEventCounterBasedCreate(context, device, &desc, &e);
    return e;
}

bool runBufferInteropTest(ze_context_handle_t context, ze_driver_handle_t driverHandle, ze_device_handle_t device,
                          cl_device_id clDevice, cl_context clContext,
                          ze_command_list_handle_t cmdList, cl_command_queue clCmdQL0) {
    bool outputValidationSuccessful = true;

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
    size_t bufferHandleSize = 0;
    clGetMemObjectInfo(memA, CL_MEM_L0_HANDLE_INTEL, sizeof(void *), &ptrA, &bufferHandleSize);

    if (bufferHandleSize == sizeof(void *) && ptrA != nullptr) {
        printf("CL L0 BUFFER HANDLE QUERY CORRECT\n");
    } else {
        printf("CL L0 BUFFER HANDLE QUERY ERROR\n");
        std::cout << "size: " << bufferHandleSize << "\n";
        outputValidationSuccessful = false;
    }

    auto memB = clCreateBufferWithProperties(clContext, 0, CL_MEM_COPY_HOST_PTR, gws * sizeof(int), dataB.data(), nullptr);
    void *ptrB = nullptr;
    clGetMemObjectInfo(memB, CL_MEM_L0_HANDLE_INTEL, sizeof(void *), &ptrB, nullptr);

    ze_device_mem_alloc_desc_t deviceDescC{ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
    void *ptrC = nullptr;
    zeMemAllocDevice(context, &deviceDescC, gws * sizeof(int), 0u, device, &ptrC);
    cl_mem_properties memObjHandleProperties[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(ptrC), 0};
    auto memC = clCreateBufferWithProperties(clContext, memObjHandleProperties, 0, gws * sizeof(int), nullptr, nullptr);

    outputValidationSuccessful &= verifyImportedProperties(memC, {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(ptrC), 0}, "BUFFER");

    cl_mem_properties wrongKeyProperties[] = {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(ptrC), CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    cl_int wrongKeyErrcode = CL_SUCCESS;
    auto rejectedBuffer = clCreateBufferWithProperties(clContext, wrongKeyProperties, 0, gws * sizeof(int), nullptr, &wrongKeyErrcode);
    if (rejectedBuffer == nullptr && wrongKeyErrcode == CL_INVALID_VALUE) {
        printf("CL L0 BUFFER IMPORT KEY SPLIT CORRECT\n");
    } else {
        printf("CL L0 BUFFER IMPORT KEY SPLIT ERROR\n");
        std::cout << "errcode: " << wrongKeyErrcode << "\n";
        outputValidationSuccessful = false;
    }

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
        outputValidationSuccessful &= !errorFound;

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
            outputValidationSuccessful = false;
        }

        zeEventDestroy(event);
        clReleaseEvent(clEvent);
    }

    zeKernelDestroy(kernel);
    clReleaseKernel(clKernel);

    zeModuleDestroy(module);
    clReleaseProgram(clProgram);

    return outputValidationSuccessful;
}

bool runImageInteropTest(ze_context_handle_t context, ze_driver_handle_t driverHandle, ze_device_handle_t device,
                         cl_device_id clDevice, cl_context clContext,
                         ze_command_list_handle_t cmdList, cl_command_queue clCmdQL0) {
    bool outputValidationSuccessful = true;

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
    size_t imageHandleSize = 0;
    clGetImageInfo(imgA, CL_IMAGE_L0_HANDLE_INTEL, sizeof(ze_image_handle_t), &l0ImgA, &imageHandleSize);

    if (imageHandleSize == sizeof(ze_image_handle_t) && l0ImgA != nullptr) {
        printf("CL L0 IMAGE HANDLE QUERY CORRECT\n");
    } else {
        printf("CL L0 IMAGE HANDLE QUERY ERROR\n");
        std::cout << "size: " << imageHandleSize << "\n";
        outputValidationSuccessful = false;
    }

    void *notAnImageHandle = nullptr;
    if (clGetMemObjectInfo(imgA, CL_MEM_L0_HANDLE_INTEL, sizeof(void *), &notAnImageHandle, nullptr) != CL_SUCCESS) {
        printf("CL L0 IMAGE HANDLE QUERY SPLIT CORRECT\n");
    } else {
        printf("CL L0 IMAGE HANDLE QUERY SPLIT ERROR\n");
        outputValidationSuccessful = false;
    }

    auto imgB = clCreateImage(clContext, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, &clFormat, &clImageDesc, dataB.data(), nullptr);
    ze_image_handle_t l0ImgB = nullptr;
    clGetImageInfo(imgB, CL_IMAGE_L0_HANDLE_INTEL, sizeof(ze_image_handle_t), &l0ImgB, nullptr);

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
    cl_mem_properties imgObjHandleProperties[] = {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(l0ImgC), CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0};
    auto imgC = clCreateImageWithProperties(clContext, imgObjHandleProperties, CL_MEM_READ_WRITE, &clFormat, &clImageDesc, nullptr, nullptr);

    outputValidationSuccessful &= verifyImportedProperties(imgC, {CL_IMAGE_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(l0ImgC), CL_IMAGE_L0_HANDLE_LIST_END_INTEL, 0}, "IMAGE");

    cl_mem_properties wrongKeyProperties[] = {CL_MEM_L0_HANDLE_INTEL, reinterpret_cast<cl_mem_properties>(l0ImgC), 0};
    cl_int wrongKeyErrcode = CL_SUCCESS;
    auto rejectedImage = clCreateImageWithProperties(clContext, wrongKeyProperties, CL_MEM_READ_WRITE, &clFormat, &clImageDesc, nullptr, &wrongKeyErrcode);
    if (rejectedImage == nullptr && wrongKeyErrcode == CL_INVALID_PROPERTY) {
        printf("CL L0 IMAGE IMPORT KEY SPLIT CORRECT\n");
    } else {
        printf("CL L0 IMAGE IMPORT KEY SPLIT ERROR\n");
        std::cout << "errcode: " << wrongKeyErrcode << "\n";
        outputValidationSuccessful = false;
    }

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
        outputValidationSuccessful &= !errorFound;

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
            outputValidationSuccessful = false;
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

    return outputValidationSuccessful;
}

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    auto [clPlatform, clDevice, clContext] = initOCL(context);
    if (clContext == nullptr) {
        LevelZeroBlackBoxTests::printResult(false, false, blackBoxName);
        return 1;
    }

    bool outputValidationSuccessful = true;

    auto cmdList = createImmIoqCmdList(context, device);
    auto clCmdQL0 = createOclCmdQFromL0(cmdList, clDevice, clContext);

    ze_context_handle_t contextCheck{};
    clGetContextInfo(clContext, CL_CONTEXT_L0_HANDLE_INTEL, sizeof(ze_context_handle_t), &contextCheck, nullptr);
    if (contextCheck == context) {
        printf("CL L0 CONTEXT HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 CONTEXT HANDLE INTEROP ERROR\n");
        outputValidationSuccessful = false;
    }

    ze_device_handle_t deviceCheck{};
    clGetDeviceInfo(clDevice, CL_DEVICE_L0_HANDLE_INTEL, sizeof(ze_device_handle_t), &deviceCheck, nullptr);
    if (deviceCheck == device) {
        printf("CL L0 DEVICE HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 DEVICE HANDLE INTEROP ERROR\n");
        outputValidationSuccessful = false;
    }

    ze_driver_handle_t driverHandleCheck{};
    clGetPlatformInfo(clPlatform, CL_PLATFORM_L0_DRIVER_HANDLE_INTEL, sizeof(ze_driver_handle_t), &driverHandleCheck, nullptr);
    if (driverHandleCheck == driverHandle) {
        printf("CL L0 DRIVER HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 DRIVER HANDLE INTEROP ERROR\n");
        outputValidationSuccessful = false;
    }

    ze_command_list_handle_t cmdListCheck{};
    clGetCommandQueueInfo(clCmdQL0, CL_QUEUE_L0_IMMEDIATE_CMD_LIST_HANDLE_INTEL, sizeof(ze_command_list_handle_t), &cmdListCheck, nullptr);
    if (cmdListCheck == cmdList) {
        printf("CL L0 CMD LIST HANDLE INTEROP CORRECT\n");
    } else {
        printf("CL L0 CMD LIST HANDLE INTEROP ERROR\n");
        outputValidationSuccessful = false;
    }

    outputValidationSuccessful &= verifyExtensionAdvertised(clPlatform, clDevice);
    outputValidationSuccessful &= verifyImportedCmdQIgnoresProperties(cmdList, clDevice, clContext);

    outputValidationSuccessful &= runBufferInteropTest(context, driverHandle, device, clDevice, clContext, cmdList, clCmdQL0);
    if (LevelZeroBlackBoxTests::checkImageSupport(device, false, true, false, false)) {
        outputValidationSuccessful &= runImageInteropTest(context, driverHandle, device, clDevice, clContext, cmdList, clCmdQL0);
    }

    zeCommandListDestroy(cmdList);
    clReleaseCommandQueue(clCmdQL0);

    clReleaseContext(clContext);
    clReleaseDevice(clDevice);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}
