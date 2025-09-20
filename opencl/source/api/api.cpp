/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "api.h"

#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/debug_env_reader.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

#include "opencl/source/api/additional_extensions.h"
#include "opencl/source/api/api_enter.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/context/driver_diagnostics.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/source/global_teardown/global_platform_teardown.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/helpers/get_info_status_mapper.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/kernel/kernel_info_cl.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/source/tracing/tracing_api.h"
#include "opencl/source/tracing/tracing_notify.h"
#include "opencl/source/utilities/cl_logger.h"

#include "CL/cl.h"
#include "config.h"

#include <algorithm>
#include <cstring>

using namespace NEO;

cl_int CL_API_CALL clGetPlatformIDs(cl_uint numEntries,
                                    cl_platform_id *platforms,
                                    cl_uint *numPlatforms) {
    TRACING_ENTER(ClGetPlatformIDs, &numEntries, &platforms, &numPlatforms);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("numEntries", numEntries,
                   "platforms", platforms,
                   "numPlatforms", numPlatforms);

    do {
        // if platforms is nullptr, we must return the number of valid platforms we
        // support in the num_platforms variable (if it is non-nullptr)
        if ((platforms == nullptr) && (numPlatforms == nullptr)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        // platform != nullptr and num_entries == 0 is defined by spec as invalid
        if (numEntries == 0 && platforms != nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        static std::mutex mutex;
        std::unique_lock<std::mutex> lock(mutex);
        if (platformsImpl->empty()) {
            auto executionEnvironment = new ClExecutionEnvironment();
            executionEnvironment->incRefInternal();

            NEO::EnvironmentVariableReader envReader;
            if (NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.get()) {
                const auto programDebugging = envReader.getSetting("ZET_ENABLE_PROGRAM_DEBUGGING", 0);
                const auto dbgMode = NEO::getDebuggingMode(programDebugging);
                executionEnvironment->setDebuggingMode(dbgMode);
            }
            if (envReader.getSetting("NEO_FP64_EMULATION", false)) {
                executionEnvironment->setFP64EmulationEnabled();
            }
            bool oneApiPvcWa = envReader.getSetting("ONEAPI_PVC_SEND_WAR_WA", true);
            executionEnvironment->setOneApiPvcWaEnv(oneApiPvcWa);

            auto allDevices = DeviceFactory::createDevices(*executionEnvironment);
            executionEnvironment->decRefInternal();
            if (allDevices.empty()) {
                retVal = CL_OUT_OF_HOST_MEMORY;
                break;
            }
            auto groupedDevices = Device::groupDevices(std::move(allDevices));
            for (auto &deviceVector : groupedDevices) {

                auto pPlatform = Platform::createFunc(*executionEnvironment);
                if (!pPlatform || !pPlatform->initialize(std::move(deviceVector))) {
                    retVal = CL_OUT_OF_HOST_MEMORY;
                    break;
                }
                platformsImpl->push_back(std::move(pPlatform));
            }
            if (retVal != CL_SUCCESS) {
                break;
            }
        }
        cl_uint numPlatformsToExpose = std::min(numEntries, static_cast<cl_uint>(platformsImpl->size()));
        if (numEntries == 0) {
            numPlatformsToExpose = static_cast<cl_uint>(platformsImpl->size());
        }
        if (platforms) {
            for (auto i = 0u; i < numPlatformsToExpose; i++) {
                platforms[i] = (*platformsImpl)[i].get();
            }
        }

        if (numPlatforms) {
            *numPlatforms = numPlatformsToExpose;
        }
    } while (false);
    TRACING_EXIT(ClGetPlatformIDs, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clIcdGetPlatformIDsKHR(cl_uint numEntries,
                                                       cl_platform_id *platforms,
                                                       cl_uint *numPlatforms) {
    TRACING_ENTER(ClIcdGetPlatformIDsKHR, &numEntries, &platforms, &numPlatforms);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("numEntries", numEntries,
                   "platforms", platforms,
                   "numPlatforms", numPlatforms);
    retVal = clGetPlatformIDs(numEntries, platforms, numPlatforms);
    TRACING_EXIT(ClIcdGetPlatformIDsKHR, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetPlatformInfo(cl_platform_id platform,
                                     cl_platform_info paramName,
                                     size_t paramValueSize,
                                     void *paramValue,
                                     size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetPlatformInfo, &platform, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_INVALID_PLATFORM;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    auto pPlatform = castToObject<Platform>(platform);
    if (pPlatform) {
        retVal = pPlatform->getInfo(paramName, paramValueSize,
                                    paramValue, paramValueSizeRet);
    }
    TRACING_EXIT(ClGetPlatformInfo, &retVal);
    return retVal;
}

bool checkDeviceTypeAndFillDeviceID(ClDevice &device, cl_device_type deviceType, cl_device_id *devices, cl_uint numEntries, cl_uint &retNum) {
    if (deviceType & device.getDeviceInfo().deviceType) {
        if (devices) {
            if (retNum >= numEntries) {
                return false;
            }
            devices[retNum] = &device;
        }
        retNum++;
    }
    return true;
}

cl_int CL_API_CALL clGetDeviceIDs(cl_platform_id platform,
                                  cl_device_type deviceType,
                                  cl_uint numEntries,
                                  cl_device_id *devices,
                                  cl_uint *numDevices) {
    TRACING_ENTER(ClGetDeviceIDs, &platform, &deviceType, &numEntries, &devices, &numDevices);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform,
                   "deviceType", deviceType,
                   "numEntries", numEntries,
                   "devices", devices,
                   "numDevices", numDevices);
    const cl_device_type validType = CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU |
                                     CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_DEFAULT |
                                     CL_DEVICE_TYPE_CUSTOM;
    Platform *pPlatform = nullptr;

    do {
        /* Check parameter consistency */
        if (devices == nullptr && numDevices == nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (devices && numEntries == 0) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if ((deviceType & validType) == 0) {
            retVal = CL_INVALID_DEVICE_TYPE;
            break;
        }

        if (platform != nullptr) {
            pPlatform = castToObject<Platform>(platform);
            if (pPlatform == nullptr) {
                retVal = CL_INVALID_PLATFORM;
                break;
            }
        } else {
            cl_uint numPlatforms = 0u;
            retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
            if (numPlatforms == 0u) {
                retVal = CL_DEVICE_NOT_FOUND;
                break;
            }
            pPlatform = (*platformsImpl)[0].get();
        }

        DEBUG_BREAK_IF(pPlatform->isInitialized() != true);
        cl_uint numDev = static_cast<cl_uint>(pPlatform->getNumDevices());
        if (numDev == 0) {
            retVal = CL_DEVICE_NOT_FOUND;
            break;
        }

        if (debugManager.flags.LimitAmountOfReturnedDevices.get()) {
            numDev = std::min(static_cast<cl_uint>(debugManager.flags.LimitAmountOfReturnedDevices.get()), numDev);
        }

        if (deviceType == CL_DEVICE_TYPE_ALL) {
            /* According to Spec, set it to all except TYPE_CUSTOM. */
            deviceType = CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_CPU |
                         CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_DEFAULT;
        } else if (deviceType == CL_DEVICE_TYPE_DEFAULT) {
            /* We just set it to GPU now. */
            deviceType = CL_DEVICE_TYPE_GPU;
        }

        cl_uint retNum = 0;
        for (auto platformDeviceIndex = 0u; platformDeviceIndex < numDev; platformDeviceIndex++) {
            bool exposeSubDevices = pPlatform->peekExecutionEnvironment()->getDeviceHierarchyMode() != DeviceHierarchyMode::composite;

            ClDevice *device = pPlatform->getClDevice(platformDeviceIndex);
            UNRECOVERABLE_IF(device == nullptr);

            exposeSubDevices &= device->getNumGenericSubDevices() > 0u;

            if (exposeSubDevices) {
                bool numEntriesReached = false;
                for (uint32_t subDeviceIndex = 0u; subDeviceIndex < device->getNumGenericSubDevices(); subDeviceIndex++) {
                    auto subDevice = device->getSubDevice(subDeviceIndex);
                    numEntriesReached = checkDeviceTypeAndFillDeviceID(*subDevice, deviceType, devices, numEntries, retNum);
                    if (!numEntriesReached) {
                        break;
                    }
                }
                if (!numEntriesReached) {
                    break;
                }
            } else {
                if (!checkDeviceTypeAndFillDeviceID(*device, deviceType, devices, numEntries, retNum)) {
                    break;
                }
            }
        }

        if (numDevices) {
            *numDevices = retNum;
        }

        /* If no suitable device, set a error. */
        if (retNum == 0)
            retVal = CL_DEVICE_NOT_FOUND;
    } while (false);
    TRACING_EXIT(ClGetDeviceIDs, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetDeviceInfo(cl_device_id device,
                                   cl_device_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetDeviceInfo, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_INVALID_DEVICE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clDevice", device, "paramName", paramName, "paramValueSize", paramValueSize, "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize), "paramValueSizeRet", paramValueSizeRet);

    ClDevice *pDevice = castToObject<ClDevice>(device);
    if (pDevice != nullptr) {
        retVal = pDevice->getDeviceInfo(paramName, paramValueSize,
                                        paramValue, paramValueSizeRet);
    }
    TRACING_EXIT(ClGetDeviceInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clCreateSubDevices(cl_device_id inDevice,
                                      const cl_device_partition_property *properties,
                                      cl_uint numDevices,
                                      cl_device_id *outDevices,
                                      cl_uint *numDevicesRet) {
    TRACING_ENTER(ClCreateSubDevices, &inDevice, &properties, &numDevices, &outDevices, &numDevicesRet);
    cl_int retVal = CL_INVALID_DEVICE;
    API_ENTER(&retVal);
    do {
        ClDevice *pInDevice = castToObject<ClDevice>(inDevice);
        if (pInDevice == nullptr) {
            break;
        }
        auto subDevicesCount = pInDevice->getNumSubDevices();

        if (pInDevice->getDevice().getRootDeviceEnvironment().isExposeSingleDeviceMode()) {
            subDevicesCount = 0;
        }
        if (subDevicesCount <= 1) {
            retVal = CL_DEVICE_PARTITION_FAILED;
            break;
        }
        if ((properties == nullptr) ||
            (properties[0] != CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN) ||
            ((properties[1] != CL_DEVICE_AFFINITY_DOMAIN_NUMA) && (properties[1] != CL_DEVICE_AFFINITY_DOMAIN_NEXT_PARTITIONABLE)) ||
            (properties[2] != 0)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (numDevicesRet != nullptr) {
            *numDevicesRet = subDevicesCount;
        }

        if (outDevices == nullptr) {
            retVal = CL_SUCCESS;
            break;
        }

        if (numDevices < subDevicesCount) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        for (uint32_t i = 0; i < subDevicesCount; i++) {
            auto pClDevice = pInDevice->getSubDevice(i);
            pClDevice->retainApi();
            outDevices[i] = pClDevice;
        }
        retVal = CL_SUCCESS;
    } while (false);
    TRACING_EXIT(ClCreateSubDevices, &retVal);
    return retVal;
}

cl_int CL_API_CALL clRetainDevice(cl_device_id device) {
    TRACING_ENTER(ClRetainDevice, &device);
    cl_int retVal = CL_INVALID_DEVICE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device);
    auto pDevice = castToObject<ClDevice>(device);
    if (pDevice) {
        pDevice->retainApi();
        retVal = CL_SUCCESS;
    }

    TRACING_EXIT(ClRetainDevice, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseDevice(cl_device_id device) {
    TRACING_ENTER(ClReleaseDevice, &device);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseDevice, &retVal);
        return CL_SUCCESS;
    }
    retVal = CL_INVALID_DEVICE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device);
    auto pDevice = castToObject<ClDevice>(device);
    if (pDevice) {
        pDevice->releaseApi();
        retVal = CL_SUCCESS;
    }

    TRACING_EXIT(ClReleaseDevice, &retVal);
    return retVal;
}

cl_context CL_API_CALL clCreateContext(const cl_context_properties *properties,
                                       cl_uint numDevices,
                                       const cl_device_id *devices,
                                       void(CL_CALLBACK *funcNotify)(const char *, const void *,
                                                                     size_t, void *),
                                       void *userData,
                                       cl_int *errcodeRet) {
    gtPinTryNotifyInit();
    TRACING_ENTER(ClCreateContext, &properties, &numDevices, &devices, &funcNotify, &userData, &errcodeRet);

    cl_int retVal = CL_SUCCESS;
    cl_context context = nullptr;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("properties", properties, "numDevices", numDevices, "cl_device_id", devices, "funcNotify", reinterpret_cast<void *>(funcNotify), "userData", userData);

    do {
        if (devices == nullptr) {
            /* Must have device. */
            retVal = CL_INVALID_VALUE;
            break;
        }

        /* validateObjects make sure numDevices != 0. */
        retVal = validateObjects(DeviceList(numDevices, devices));
        if (retVal != CL_SUCCESS)
            break;

        if (funcNotify == nullptr && userData != nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        auto pPlatform = Context::getPlatformFromProperties(properties, retVal);
        if (CL_SUCCESS != retVal) {
            break;
        }

        ClDeviceVector allDevs(devices, numDevices);
        if (!pPlatform) {
            pPlatform = allDevs[0]->getPlatform();
        }
        for (auto &pClDevice : allDevs) {
            if (pClDevice->getPlatform() != pPlatform) {
                retVal = CL_INVALID_DEVICE;
                break;
            }
        }
        if (CL_SUCCESS != retVal) {
            break;
        }
        context = Context::create<Context>(properties, allDevs, funcNotify, userData, retVal);
    } while (false);

    if (errcodeRet) {
        *errcodeRet = retVal;
    }
    TRACING_EXIT(ClCreateContext, &context);
    return context;
}

cl_context CL_API_CALL clCreateContextFromType(const cl_context_properties *properties,
                                               cl_device_type deviceType,
                                               void(CL_CALLBACK *funcNotify)(const char *, const void *,
                                                                             size_t, void *),
                                               void *userData,
                                               cl_int *errcodeRet) {
    gtPinTryNotifyInit();
    TRACING_ENTER(ClCreateContextFromType, &properties, &deviceType, &funcNotify, &userData, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("properties", properties, "deviceType", deviceType, "funcNotify", reinterpret_cast<void *>(funcNotify), "userData", userData);
    cl_context context = nullptr;

    do {
        if (funcNotify == nullptr && userData != nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        auto pPlatform = Context::getPlatformFromProperties(properties, retVal);
        if (CL_SUCCESS != retVal) {
            break;
        }
        cl_uint numDevices = 0;
        /* Query the number of device first. */
        retVal = clGetDeviceIDs(pPlatform, deviceType, 0, nullptr, &numDevices);
        if (retVal != CL_SUCCESS) {
            break;
        }

        DEBUG_BREAK_IF(numDevices <= 0);
        std::vector<cl_device_id> devices(numDevices, nullptr);

        retVal = clGetDeviceIDs(pPlatform, deviceType, numDevices, devices.data(), nullptr);
        DEBUG_BREAK_IF(retVal != CL_SUCCESS);

        ClDeviceVector deviceVector(devices.data(), numDevices);
        context = Context::create<Context>(properties, deviceVector, funcNotify, userData, retVal);
    } while (false);

    if (errcodeRet) {
        *errcodeRet = retVal;
    }
    TRACING_EXIT(ClCreateContextFromType, &context);
    return context;
}

cl_int CL_API_CALL clRetainContext(cl_context context) {
    TRACING_ENTER(ClRetainContext, &context);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context);
    Context *pContext = castToObject<Context>(context);
    if (pContext) {
        pContext->retain();
        TRACING_EXIT(ClRetainContext, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_CONTEXT;
    TRACING_EXIT(ClRetainContext, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseContext(cl_context context) {
    TRACING_ENTER(ClReleaseContext, &context);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseContext, &retVal);
        return CL_SUCCESS;
    }
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context);
    Context *pContext = castToObject<Context>(context);
    if (pContext) {
        pContext->release();
        TRACING_EXIT(ClReleaseContext, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_CONTEXT;
    TRACING_EXIT(ClReleaseContext, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetContextInfo(cl_context context,
                                    cl_context_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetContextInfo, &context, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto retVal = CL_INVALID_CONTEXT;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    auto pContext = castToObject<Context>(context);

    if (pContext) {
        retVal = pContext->getInfo(paramName, paramValueSize,
                                   paramValue, paramValueSizeRet);
    }

    TRACING_EXIT(ClGetContextInfo, &retVal);
    return retVal;
}

cl_command_queue CL_API_CALL clCreateCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  const cl_command_queue_properties properties,
                                                  cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateCommandQueue, &context, &device, &properties, &errcodeRet);
    cl_command_queue commandQueue = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "device", device,
                   "properties", properties);

    do {
        if (properties &
            ~(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        Context *pContext = nullptr;
        ClDevice *pDevice = nullptr;

        retVal = validateObjects(
            withCastToInternal(context, &pContext),
            withCastToInternal(device, &pDevice));

        if (retVal != CL_SUCCESS) {
            break;
        }
        if (!pContext->isDeviceAssociated(*pDevice)) {
            retVal = CL_INVALID_DEVICE;
            break;
        }

        cl_queue_properties props[] = {
            CL_QUEUE_PROPERTIES, properties,
            0};

        commandQueue = CommandQueue::create(pContext,
                                            pDevice,
                                            props,
                                            false,
                                            retVal);

        if (pContext->isProvidingPerformanceHints()) {
            pContext->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, DRIVER_CALLS_INTERNAL_CL_FLUSH);
            if (castToObjectOrAbort<CommandQueue>(commandQueue)->isProfilingEnabled()) {
                pContext->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, PROFILING_ENABLED);
            }
        }
    } while (false);

    err.set(retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);
    TRACING_EXIT(ClCreateCommandQueue, &commandQueue);
    return commandQueue;
}

cl_int CL_API_CALL clRetainCommandQueue(cl_command_queue commandQueue) {
    TRACING_ENTER(ClRetainCommandQueue, &commandQueue);
    cl_int retVal = CL_INVALID_COMMAND_QUEUE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);
    retainQueue(commandQueue, retVal);
    if (retVal == CL_SUCCESS) {
        TRACING_EXIT(ClRetainCommandQueue, &retVal);
        return retVal;
    }

    TRACING_EXIT(ClRetainCommandQueue, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue commandQueue) {
    TRACING_ENTER(ClReleaseCommandQueue, &commandQueue);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseCommandQueue, &retVal);
        return CL_SUCCESS;
    }
    retVal = CL_INVALID_COMMAND_QUEUE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);

    releaseQueue(commandQueue, retVal);
    if (retVal == CL_SUCCESS) {
        TRACING_EXIT(ClReleaseCommandQueue, &retVal);
        return retVal;
    }

    TRACING_EXIT(ClReleaseCommandQueue, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetCommandQueueInfo(cl_command_queue commandQueue,
                                         cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetCommandQueueInfo, &commandQueue, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_INVALID_COMMAND_QUEUE;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    getQueueInfo(commandQueue, paramName, paramValueSize, paramValue, paramValueSizeRet, retVal);
    // if host queue not found - try to query device queue
    if (retVal == CL_SUCCESS) {
        TRACING_EXIT(ClGetCommandQueueInfo, &retVal);
        return retVal;
    }

    TRACING_EXIT(ClGetCommandQueueInfo, &retVal);
    return retVal;
}

// deprecated OpenCL 1.0
cl_int CL_API_CALL clSetCommandQueueProperty(cl_command_queue commandQueue,
                                             cl_command_queue_properties properties,
                                             cl_bool enable,
                                             cl_command_queue_properties *oldProperties) {
    TRACING_ENTER(ClSetCommandQueueProperty, &commandQueue, &properties, &enable, &oldProperties);
    cl_int retVal = CL_INVALID_OPERATION;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "properties", properties,
                   "enable", enable,
                   "oldProperties", oldProperties);
    TRACING_EXIT(ClSetCommandQueueProperty, &retVal);
    return retVal;
}

cl_mem CL_API_CALL clCreateBuffer(cl_context context,
                                  cl_mem_flags flags,
                                  size_t size,
                                  void *hostPtr,
                                  cl_int *errcodeRet) {
    if (debugManager.flags.ForceExtendedBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedBufferSize.get());
    }

    TRACING_ENTER(ClCreateBuffer, &context, &flags, &size, &hostPtr, &errcodeRet);
    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_flags", flags,
                   "size", size,
                   "hostPtr", NEO::fileLoggerInstance().infoPointerToString(hostPtr, size));

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_properties *properties = nullptr;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem buffer = BufferFunctions::validateInputAndCreateBuffer(context, properties, flags, flagsIntel, size, hostPtr, retVal);

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("buffer", buffer);
    TRACING_EXIT(ClCreateBuffer, &buffer);
    return buffer;
}

cl_mem CL_API_CALL clCreateBufferWithProperties(cl_context context,
                                                const cl_mem_properties *properties,
                                                cl_mem_flags flags,
                                                size_t size,
                                                void *hostPtr,
                                                cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateBufferWithProperties, &context, &properties, &flags, &size, &hostPtr, &errcodeRet);
    if (debugManager.flags.ForceExtendedBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedBufferSize.get());
    }

    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_properties", properties,
                   "cl_mem_flags", flags,
                   "size", size,
                   "hostPtr", NEO::fileLoggerInstance().infoPointerToString(hostPtr, size));

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_flags_intel flagsIntel = 0;
    cl_mem buffer = BufferFunctions::validateInputAndCreateBuffer(context, properties, flags, flagsIntel, size, hostPtr, retVal);

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("buffer", buffer);
    TRACING_EXIT(ClCreateBufferWithProperties, &buffer);
    return buffer;
}

cl_mem CL_API_CALL clCreateBufferWithPropertiesINTEL(cl_context context,
                                                     const cl_mem_properties_intel *properties,
                                                     cl_mem_flags flags,
                                                     size_t size,
                                                     void *hostPtr,
                                                     cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateBufferWithPropertiesINTEL, &context, &properties, &flags, &size, &hostPtr, &errcodeRet);
    if (debugManager.flags.ForceExtendedBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedBufferSize.get());
    }

    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_properties_intel", properties,
                   "cl_mem_flags", flags,
                   "size", size,
                   "hostPtr", NEO::fileLoggerInstance().infoPointerToString(hostPtr, size));

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_flags_intel flagsIntel = 0;
    cl_mem buffer = BufferFunctions::validateInputAndCreateBuffer(context, properties, flags, flagsIntel, size, hostPtr, retVal);

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("buffer", buffer);
    TRACING_EXIT(ClCreateBufferWithPropertiesINTEL, &buffer);
    return buffer;
}

cl_mem CL_API_CALL clCreateSubBuffer(cl_mem buffer,
                                     cl_mem_flags flags,
                                     cl_buffer_create_type bufferCreateType,
                                     const void *bufferCreateInfo,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateSubBuffer, &buffer, &flags, &bufferCreateType, &bufferCreateInfo, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("buffer", buffer,
                   "flags", flags,
                   "bufferCreateType", bufferCreateType,
                   "bufferCreateInfo", bufferCreateInfo);
    cl_mem subBuffer = nullptr;
    Buffer *parentBuffer = castToObject<Buffer>(buffer);

    do {
        if (parentBuffer == nullptr) {
            retVal = CL_INVALID_MEM_OBJECT;
            break;
        }

        /* Are there some invalid flag bits? */
        if (!MemObjHelper::checkMemFlagsForSubBuffer(flags)) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (parentBuffer->isSubBuffer() == true) {
            if (!parentBuffer->getContext()->getBufferPoolAllocator().isPoolBuffer(parentBuffer->getAssociatedMemObject())) {
                retVal = CL_INVALID_MEM_OBJECT;
                break;
            }
        }

        cl_mem_flags parentFlags = parentBuffer->getFlags();
        cl_mem_flags_intel parentFlagsIntel = parentBuffer->getFlagsIntel();

        /* Check whether flag is valid. */
        if (((flags & CL_MEM_HOST_READ_ONLY) && (flags & CL_MEM_HOST_NO_ACCESS)) ||
            ((flags & CL_MEM_HOST_READ_ONLY) && (flags & CL_MEM_HOST_WRITE_ONLY)) ||
            ((flags & CL_MEM_HOST_WRITE_ONLY) && (flags & CL_MEM_HOST_NO_ACCESS))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        /* Check whether flag is valid and compatible with parent. */
        if (flags &&
            (((parentFlags & CL_MEM_WRITE_ONLY) && (flags & (CL_MEM_READ_WRITE | CL_MEM_READ_ONLY))) ||
             ((parentFlags & CL_MEM_READ_ONLY) && (flags & (CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY))) ||
             ((parentFlags & CL_MEM_HOST_WRITE_ONLY) && (flags & CL_MEM_HOST_READ_ONLY)) ||
             ((parentFlags & CL_MEM_HOST_READ_ONLY) && (flags & CL_MEM_HOST_WRITE_ONLY)) ||
             ((parentFlags & CL_MEM_HOST_NO_ACCESS) &&
              (flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY))))) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        /* Inherit some flags if we do not set. */
        if ((flags & (CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_READ_WRITE)) == 0) {
            flags |= parentFlags & (CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_READ_WRITE);
        }
        if ((flags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) == 0) {
            flags |= parentFlags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY |
                                    CL_MEM_HOST_NO_ACCESS);
        }
        flags |= parentFlags & (CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR);

        if (bufferCreateType != CL_BUFFER_CREATE_TYPE_REGION) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (bufferCreateInfo == nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        /* Must have non-zero size. */
        const cl_buffer_region *region = reinterpret_cast<const cl_buffer_region *>(bufferCreateInfo);
        if (region->size == 0) {
            retVal = CL_INVALID_BUFFER_SIZE;
            break;
        }

        /* Out of range. */
        if (region->origin > parentBuffer->getSize() ||
            region->origin + region->size > parentBuffer->getSize()) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (!parentBuffer->isValidSubBufferOffset(region->origin)) {
            retVal = CL_MISALIGNED_SUB_BUFFER_OFFSET;
            break;
        }

        subBuffer = parentBuffer->createSubBuffer(flags, parentFlagsIntel, region, retVal);
    } while (false);

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateSubBuffer, &subBuffer);
    return subBuffer;
}

cl_mem CL_API_CALL clCreateImage(cl_context context,
                                 cl_mem_flags flags,
                                 const cl_image_format *imageFormat,
                                 const cl_image_desc *imageDesc,
                                 void *hostPtr,
                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage, &context, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);

    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_flags", flags,
                   "cl_image_format.channel_data_type", imageFormat->image_channel_data_type,
                   "cl_image_format.channel_order", imageFormat->image_channel_order,
                   "cl_image_desc.width", imageDesc->image_width,
                   "cl_image_desc.height", imageDesc->image_height,
                   "cl_image_desc.depth", imageDesc->image_depth,
                   "cl_image_desc.type", imageDesc->image_type,
                   "cl_image_desc.array_size", imageDesc->image_array_size,
                   "hostPtr", hostPtr);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_properties *properties = nullptr;
    cl_mem_flags_intel flagsIntel = 0;

    retVal = Image::checkIfDeviceSupportsImages(context);

    cl_mem image = nullptr;
    if (retVal == CL_SUCCESS) {
        image = ImageFunctions::validateAndCreateImage(context, properties, flags, flagsIntel, imageFormat, imageDesc, hostPtr, retVal);
    }

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateImage, &image);
    return image;
}

cl_mem CL_API_CALL clCreateImageWithProperties(cl_context context,
                                               const cl_mem_properties *properties,
                                               cl_mem_flags flags,
                                               const cl_image_format *imageFormat,
                                               const cl_image_desc *imageDesc,
                                               void *hostPtr,
                                               cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateImageWithProperties, &context, &properties, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);
    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_properties", properties,
                   "cl_mem_flags", flags,
                   "cl_image_format.channel_data_type", imageFormat->image_channel_data_type,
                   "cl_image_format.channel_order", imageFormat->image_channel_order,
                   "cl_image_desc.width", imageDesc->image_width,
                   "cl_image_desc.height", imageDesc->image_height,
                   "cl_image_desc.depth", imageDesc->image_depth,
                   "cl_image_desc.type", imageDesc->image_type,
                   "cl_image_desc.array_size", imageDesc->image_array_size,
                   "hostPtr", hostPtr);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_flags_intel flagsIntel = 0;

    retVal = Image::checkIfDeviceSupportsImages(context);

    cl_mem image = nullptr;
    if (retVal == CL_SUCCESS) {
        image = ImageFunctions::validateAndCreateImage(context, properties, flags, flagsIntel, imageFormat, imageDesc, hostPtr, retVal);
    }

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateImageWithProperties, &image);
    return image;
}

cl_mem CL_API_CALL clCreateImageWithPropertiesINTEL(cl_context context,
                                                    const cl_mem_properties_intel *properties,
                                                    cl_mem_flags flags,
                                                    const cl_image_format *imageFormat,
                                                    const cl_image_desc *imageDesc,
                                                    void *hostPtr,
                                                    cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateImageWithPropertiesINTEL, &context, &properties, &flags, &imageFormat, &imageDesc, &hostPtr, &errcodeRet);
    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_properties_intel", properties,
                   "cl_mem_flags", flags,
                   "cl_image_format.channel_data_type", imageFormat->image_channel_data_type,
                   "cl_image_format.channel_order", imageFormat->image_channel_order,
                   "cl_image_desc.width", imageDesc->image_width,
                   "cl_image_desc.height", imageDesc->image_height,
                   "cl_image_desc.depth", imageDesc->image_depth,
                   "cl_image_desc.type", imageDesc->image_type,
                   "cl_image_desc.array_size", imageDesc->image_array_size,
                   "hostPtr", hostPtr);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_mem_flags_intel flagsIntel = 0;
    cl_mem image = ImageFunctions::validateAndCreateImage(context, properties, flags, flagsIntel, imageFormat, imageDesc, hostPtr, retVal);

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("image", image);
    TRACING_EXIT(ClCreateImageWithPropertiesINTEL, &image);
    return image;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage2D(cl_context context,
                                   cl_mem_flags flags,
                                   const cl_image_format *imageFormat,
                                   size_t imageWidth,
                                   size_t imageHeight,
                                   size_t imageRowPitch,
                                   void *hostPtr,
                                   cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage2D, &context, &flags, &imageFormat, &imageWidth, &imageHeight, &imageRowPitch, &hostPtr, &errcodeRet);

    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "imageFormat", imageFormat,
                   "imageWidth", imageWidth,
                   "imageHeight", imageHeight,
                   "imageRowPitch", imageRowPitch,
                   "hostPtr", hostPtr);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(cl_image_desc));

    imageDesc.image_height = imageHeight;
    imageDesc.image_width = imageWidth;
    imageDesc.image_row_pitch = imageRowPitch;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    cl_mem_properties *properties = nullptr;
    cl_mem_flags_intel flagsIntel = 0;

    retVal = Image::checkIfDeviceSupportsImages(context);

    cl_mem image2D = nullptr;
    if (retVal == CL_SUCCESS) {
        image2D = ImageFunctions::validateAndCreateImage(context, properties, flags, flagsIntel, imageFormat, &imageDesc, hostPtr, retVal);
    }

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("image 2D", image2D);
    TRACING_EXIT(ClCreateImage2D, &image2D);
    return image2D;
}

// deprecated OpenCL 1.1
cl_mem CL_API_CALL clCreateImage3D(cl_context context,
                                   cl_mem_flags flags,
                                   const cl_image_format *imageFormat,
                                   size_t imageWidth,
                                   size_t imageHeight,
                                   size_t imageDepth,
                                   size_t imageRowPitch,
                                   size_t imageSlicePitch,
                                   void *hostPtr,
                                   cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateImage3D, &context, &flags, &imageFormat, &imageWidth, &imageHeight, &imageDepth, &imageRowPitch, &imageSlicePitch, &hostPtr, &errcodeRet);

    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "imageFormat", imageFormat,
                   "imageWidth", imageWidth,
                   "imageHeight", imageHeight,
                   "imageDepth", imageDepth,
                   "imageRowPitch", imageRowPitch,
                   "imageSlicePitch", imageSlicePitch,
                   "hostPtr", hostPtr);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(cl_image_desc));

    imageDesc.image_depth = imageDepth;
    imageDesc.image_height = imageHeight;
    imageDesc.image_width = imageWidth;
    imageDesc.image_row_pitch = imageRowPitch;
    imageDesc.image_slice_pitch = imageSlicePitch;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;

    cl_mem_properties *properties = nullptr;
    cl_mem_flags_intel intelFlags = 0;

    retVal = Image::checkIfDeviceSupportsImages(context);

    cl_mem image3D = nullptr;
    if (retVal == CL_SUCCESS) {
        image3D = ImageFunctions::validateAndCreateImage(context, properties, flags, intelFlags, imageFormat, &imageDesc, hostPtr, retVal);
    }

    ErrorCodeHelper{errcodeRet, retVal};
    DBG_LOG_INPUTS("image 3D", image3D);
    TRACING_EXIT(ClCreateImage3D, &image3D);
    return image3D;
}

cl_int CL_API_CALL clRetainMemObject(cl_mem memobj) {
    TRACING_ENTER(ClRetainMemObject, &memobj);
    cl_int retVal = CL_INVALID_MEM_OBJECT;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("memobj", memobj);

    auto pMemObj = castToObject<MemObj>(memobj);

    if (pMemObj) {
        pMemObj->retain();
        retVal = CL_SUCCESS;
        TRACING_EXIT(ClRetainMemObject, &retVal);
        return retVal;
    }

    TRACING_EXIT(ClRetainMemObject, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseMemObject(cl_mem memobj) {
    TRACING_ENTER(ClReleaseMemObject, &memobj);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseMemObject, &retVal);
        return CL_SUCCESS;
    }
    retVal = CL_INVALID_MEM_OBJECT;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("memobj", memobj);

    auto pMemObj = castToObject<MemObj>(memobj);
    if (pMemObj) {
        pMemObj->release();
        retVal = CL_SUCCESS;
        TRACING_EXIT(ClReleaseMemObject, &retVal);
        return retVal;
    }

    TRACING_EXIT(ClReleaseMemObject, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetSupportedImageFormats(cl_context context,
                                              cl_mem_flags flags,
                                              cl_mem_object_type imageType,
                                              cl_uint numEntries,
                                              cl_image_format *imageFormats,
                                              cl_uint *numImageFormats) {
    TRACING_ENTER(ClGetSupportedImageFormats, &context, &flags, &imageType, &numEntries, &imageFormats, &numImageFormats);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "imageType", imageType,
                   "numEntries", numEntries,
                   "imageFormats", imageFormats,
                   "numImageFormats", numImageFormats);
    auto pContext = castToObject<Context>(context);
    if (pContext) {
        auto pClDevice = pContext->getDevice(0);
        if (pClDevice->getHardwareInfo().capabilityTable.supportsImages) {
            retVal = pContext->getSupportedImageFormats(&pClDevice->getDevice(), flags, imageType, numEntries,
                                                        imageFormats, numImageFormats);
        } else {
            if (numImageFormats) {
                *numImageFormats = 0u;
            }
            retVal = CL_SUCCESS;
        }
    } else {
        retVal = CL_INVALID_CONTEXT;
    }

    TRACING_EXIT(ClGetSupportedImageFormats, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetMemObjectInfo(cl_mem memobj,
                                      cl_mem_info paramName,
                                      size_t paramValueSize,
                                      void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetMemObjectInfo, &memobj, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("memobj", memobj,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    MemObj *pMemObj = nullptr;
    retVal = validateObjects(withCastToInternal(memobj, &pMemObj));
    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetMemObjectInfo, &retVal);
        return retVal;
    }

    retVal = pMemObj->getMemObjectInfo(paramName, paramValueSize,
                                       paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetMemObjectInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetImageInfo(cl_mem image,
                                  cl_image_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetImageInfo, &image, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("image", image,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    retVal = validateObjects(image);
    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetImageInfo, &retVal);
        return retVal;
    }

    auto pImgObj = castToObject<Image>(image);
    if (pImgObj == nullptr) {
        retVal = CL_INVALID_MEM_OBJECT;
        TRACING_EXIT(ClGetImageInfo, &retVal);
        return retVal;
    }

    retVal = pImgObj->getImageInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
    TRACING_EXIT(ClGetImageInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetImageParamsINTEL(cl_context context,
                                         const cl_image_format *imageFormat,
                                         const cl_image_desc *imageDesc,
                                         size_t *imageRowPitch,
                                         size_t *imageSlicePitch) {

    TRACING_ENTER(ClGetImageParamsINTEL, &context, &imageFormat, &imageDesc, &imageRowPitch, &imageSlicePitch);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "imageFormat", imageFormat,
                   "imageDesc", imageDesc,
                   "imageRowPitch", imageRowPitch,
                   "imageSlicePitch", imageSlicePitch);
    const ClSurfaceFormatInfo *surfaceFormat = nullptr;
    cl_mem_flags memFlags = CL_MEM_READ_ONLY;
    retVal = validateObjects(context);
    auto pContext = castToObject<Context>(context);

    if (CL_SUCCESS == retVal) {
        if ((imageFormat == nullptr) || (imageDesc == nullptr) || (imageRowPitch == nullptr) || (imageSlicePitch == nullptr)) {
            retVal = CL_INVALID_VALUE;
        }
    }
    if (CL_SUCCESS == retVal) {
        retVal = Image::validateImageFormat(imageFormat);
    }
    if (CL_SUCCESS == retVal) {
        auto pClDevice = pContext->getDevice(0);
        surfaceFormat = Image::getSurfaceFormatFromTable(memFlags, imageFormat,
                                                         pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);
        retVal = Image::validate(pContext, ClMemoryPropertiesHelper::createMemoryProperties(memFlags, 0, 0, &pClDevice->getDevice()),
                                 surfaceFormat, imageDesc, nullptr);
    }
    if (CL_SUCCESS == retVal) {
        retVal = Image::getImageParams(pContext, memFlags, surfaceFormat, imageDesc, imageRowPitch, imageSlicePitch);
    }
    TRACING_EXIT(ClGetImageParamsINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetMemObjectDestructorCallback(cl_mem memobj,
                                                    void(CL_CALLBACK *funcNotify)(cl_mem, void *),
                                                    void *userData) {
    TRACING_ENTER(ClSetMemObjectDestructorCallback, &memobj, &funcNotify, &userData);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("memobj", memobj, "funcNotify", reinterpret_cast<void *>(funcNotify), "userData", userData);
    retVal = validateObjects(memobj, reinterpret_cast<void *>(funcNotify));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClSetMemObjectDestructorCallback, &retVal);
        return retVal;
    }

    auto pMemObj = castToObject<MemObj>(memobj);
    retVal = pMemObj->setDestructorCallback(funcNotify, userData);
    TRACING_EXIT(ClSetMemObjectDestructorCallback, &retVal);
    return retVal;
}

cl_sampler CL_API_CALL clCreateSampler(cl_context context,
                                       cl_bool normalizedCoords,
                                       cl_addressing_mode addressingMode,
                                       cl_filter_mode filterMode,
                                       cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateSampler, &context, &normalizedCoords, &addressingMode, &filterMode, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "normalizedCoords", normalizedCoords,
                   "addressingMode", addressingMode,
                   "filterMode", filterMode);
    retVal = validateObjects(context);
    cl_sampler sampler = nullptr;

    if (retVal == CL_SUCCESS) {
        auto pContext = castToObject<Context>(context);
        sampler = Sampler::create(
            pContext,
            normalizedCoords,
            addressingMode,
            filterMode,
            CL_FILTER_NEAREST,
            0.0f,
            std::numeric_limits<float>::max(),
            retVal);
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateSampler, &sampler);
    return sampler;
}

cl_int CL_API_CALL clRetainSampler(cl_sampler sampler) {
    TRACING_ENTER(ClRetainSampler, &sampler);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("sampler", sampler);
    auto pSampler = castToObject<Sampler>(sampler);
    if (pSampler) {
        pSampler->retain();
        TRACING_EXIT(ClRetainSampler, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_SAMPLER;
    TRACING_EXIT(ClRetainSampler, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseSampler(cl_sampler sampler) {
    TRACING_ENTER(ClReleaseSampler, &sampler);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseSampler, &retVal);
        return CL_SUCCESS;
    }
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("sampler", sampler);
    auto pSampler = castToObject<Sampler>(sampler);
    if (pSampler) {
        pSampler->release();
        TRACING_EXIT(ClReleaseSampler, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_SAMPLER;
    TRACING_EXIT(ClReleaseSampler, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetSamplerInfo(cl_sampler sampler,
                                    cl_sampler_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetSamplerInfo, &sampler, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_INVALID_SAMPLER;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("sampler", sampler,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    auto pSampler = castToObject<Sampler>(sampler);

    if (pSampler) {
        retVal = pSampler->getInfo(paramName, paramValueSize,
                                   paramValue, paramValueSizeRet);
    }

    TRACING_EXIT(ClGetSamplerInfo, &retVal);
    return retVal;
}

cl_program CL_API_CALL clCreateProgramWithSource(cl_context context,
                                                 cl_uint count,
                                                 const char **strings,
                                                 const size_t *lengths,
                                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithSource, &context, &count, &strings, &lengths, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "count", count,
                   "strings", strings,
                   "lengths", lengths);
    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext), count, strings);
    cl_program program = nullptr;

    if (CL_SUCCESS == retVal) {
        program = Program::create(
            pContext,
            count,
            strings,
            lengths,
            retVal);
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateProgramWithSource, &program);
    return program;
}

cl_program CL_API_CALL clCreateProgramWithBinary(cl_context context,
                                                 cl_uint numDevices,
                                                 const cl_device_id *deviceList,
                                                 const size_t *lengths,
                                                 const unsigned char **binaries,
                                                 cl_int *binaryStatus,
                                                 cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithBinary, &context, &numDevices, &deviceList, &lengths, &binaries, &binaryStatus, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "numDevices", numDevices,
                   "deviceList", deviceList,
                   "lengths", lengths,
                   "binaries", binaries,
                   "binaryStatus", binaryStatus);
    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext), deviceList, numDevices, binaries, lengths);
    cl_program program = nullptr;
    ClDeviceVector deviceVector;

    if (retVal == CL_SUCCESS) {
        for (auto i = 0u; i < numDevices; i++) {
            auto device = castToObject<ClDevice>(deviceList[i]);
            if (!device || !pContext->isDeviceAssociated(*device)) {
                retVal = CL_INVALID_DEVICE;
                break;
            }
            if (lengths[i] == 0 || binaries[i] == nullptr) {
                retVal = CL_INVALID_VALUE;
                break;
            }
            deviceVector.push_back(device);
        }
    }

    NEO::fileLoggerInstance().dumpBinaryProgram(numDevices, lengths, binaries);

    if (CL_SUCCESS == retVal) {
        program = Program::create(
            pContext,
            deviceVector,
            lengths,
            binaries,
            binaryStatus,
            retVal);
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateProgramWithBinary, &program);
    return program;
}

cl_program CL_API_CALL clCreateProgramWithIL(cl_context context,
                                             const void *il,
                                             size_t length,
                                             cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithIl, &context, &il, &length, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "il", il,
                   "length", length);

    cl_program program = nullptr;
    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext), il);
    if (retVal == CL_SUCCESS) {
        program = ProgramFunctions::createFromIL(
            pContext,
            il,
            length,
            retVal);
    }

    if (errcodeRet != nullptr) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateProgramWithIl, &program);
    return program;
}

cl_program CL_API_CALL clCreateProgramWithBuiltInKernels(cl_context context,
                                                         cl_uint numDevices,
                                                         const cl_device_id *deviceList,
                                                         const char *kernelNames,
                                                         cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateProgramWithBuiltInKernels, &context, &numDevices, &deviceList, &kernelNames, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    cl_program program = nullptr;
    DBG_LOG_INPUTS("context", context,
                   "numDevices", numDevices,
                   "deviceList", deviceList,
                   "kernelNames", kernelNames);
    retVal = CL_INVALID_VALUE;
    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateProgramWithBuiltInKernels, &program);
    return program;
}

cl_int CL_API_CALL clRetainProgram(cl_program program) {
    TRACING_ENTER(ClRetainProgram, &program);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("program", program);
    auto pProgram = castToObject<Program>(program);
    if (pProgram) {
        pProgram->retain();
        TRACING_EXIT(ClRetainProgram, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_PROGRAM;
    TRACING_EXIT(ClRetainProgram, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseProgram(cl_program program) {
    TRACING_ENTER(ClReleaseProgram, &program);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseProgram, &retVal);
        return CL_SUCCESS;
    }
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("program", program);
    auto pProgram = castToObject<Program>(program);
    if (pProgram) {
        pProgram->release();
        TRACING_EXIT(ClReleaseProgram, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_PROGRAM;
    TRACING_EXIT(ClReleaseProgram, &retVal);
    return retVal;
}

cl_int CL_API_CALL clBuildProgram(cl_program program,
                                  cl_uint numDevices,
                                  const cl_device_id *deviceList,
                                  const char *options,
                                  void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                  void *userData) {
    TRACING_ENTER(ClBuildProgram, &program, &numDevices, &deviceList, &options, &funcNotify, &userData);
    cl_int retVal = CL_INVALID_PROGRAM;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clProgram", program, "numDevices", numDevices, "cl_device_id", deviceList, "options", (options != nullptr) ? options : "", "funcNotify", reinterpret_cast<void *>(funcNotify), "userData", userData);
    Program *pProgram = nullptr;

    retVal = validateObjects(withCastToInternal(program, &pProgram), Program::isValidCallback(funcNotify, userData));

    if (CL_SUCCESS == retVal) {
        if (pProgram->isLocked()) {
            retVal = CL_INVALID_OPERATION;
        }
    }

    ClDeviceVector deviceVector;
    ClDeviceVector *deviceVectorPtr = &deviceVector;

    if (CL_SUCCESS == retVal) {
        retVal = Program::processInputDevices(deviceVectorPtr, numDevices, deviceList, pProgram->getDevices());
    }
    if (CL_SUCCESS == retVal) {
        retVal = pProgram->build(*deviceVectorPtr, options);
        pProgram->invokeCallback(funcNotify, userData);
    }

    TRACING_EXIT(ClBuildProgram, &retVal);
    return retVal;
}

cl_int CL_API_CALL clCompileProgram(cl_program program,
                                    cl_uint numDevices,
                                    const cl_device_id *deviceList,
                                    const char *options,
                                    cl_uint numInputHeaders,
                                    const cl_program *inputHeaders,
                                    const char **headerIncludeNames,
                                    void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                    void *userData) {
    TRACING_ENTER(ClCompileProgram, &program, &numDevices, &deviceList, &options, &numInputHeaders, &inputHeaders, &headerIncludeNames, &funcNotify, &userData);
    cl_int retVal = CL_INVALID_PROGRAM;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clProgram", program, "numDevices", numDevices, "cl_device_id", deviceList, "options", (options != nullptr) ? options : "", "numInputHeaders", numInputHeaders);

    Program *pProgram = nullptr;

    retVal = validateObjects(withCastToInternal(program, &pProgram), Program::isValidCallback(funcNotify, userData));

    if (CL_SUCCESS == retVal) {
        if (pProgram->isLocked()) {
            retVal = CL_INVALID_OPERATION;
        }
    }

    ClDeviceVector deviceVector;
    ClDeviceVector *deviceVectorPtr = &deviceVector;

    if (CL_SUCCESS == retVal) {
        retVal = Program::processInputDevices(deviceVectorPtr, numDevices, deviceList, pProgram->getDevices());
    }
    if (CL_SUCCESS == retVal) {
        retVal = pProgram->compile(*deviceVectorPtr, options,
                                   numInputHeaders, inputHeaders, headerIncludeNames);
        pProgram->invokeCallback(funcNotify, userData);
    }

    TRACING_EXIT(ClCompileProgram, &retVal);
    return retVal;
}

cl_program CL_API_CALL clLinkProgram(cl_context context,
                                     cl_uint numDevices,
                                     const cl_device_id *deviceList,
                                     const char *options,
                                     cl_uint numInputPrograms,
                                     const cl_program *inputPrograms,
                                     void(CL_CALLBACK *funcNotify)(cl_program program, void *userData),
                                     void *userData,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClLinkProgram, &context, &numDevices, &deviceList, &options, &numInputPrograms, &inputPrograms, &funcNotify, &userData, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("cl_context", context, "numDevices", numDevices, "cl_device_id", deviceList, "options", (options != nullptr) ? options : "", "numInputPrograms", numInputPrograms);

    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    Context *pContext = nullptr;
    cl_program clProgram = nullptr;

    retVal = validateObjects(withCastToInternal(context, &pContext), Program::isValidCallback(funcNotify, userData));

    ClDeviceVector deviceVector;
    ClDeviceVector *deviceVectorPtr = &deviceVector;
    if (CL_SUCCESS == retVal) {
        retVal = Program::processInputDevices(deviceVectorPtr, numDevices, deviceList, pContext->getDevices());
    }

    if (CL_SUCCESS == retVal) {
        clProgram = new Program(pContext, false, *deviceVectorPtr);
        auto pProgram = castToObject<Program>(clProgram);
        retVal = pProgram->link(*deviceVectorPtr, options,
                                numInputPrograms, inputPrograms);
        pProgram->invokeCallback(funcNotify, userData);
    }

    err.set(retVal);
    TRACING_EXIT(ClLinkProgram, &clProgram);
    return clProgram;
}

cl_int CL_API_CALL clUnloadPlatformCompiler(cl_platform_id platform) {
    TRACING_ENTER(ClUnloadPlatformCompiler, &platform);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("platform", platform);

    retVal = validateObject(platform);

    TRACING_EXIT(ClUnloadPlatformCompiler, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetProgramInfo(cl_program program,
                                    cl_program_info paramName,
                                    size_t paramValueSize,
                                    void *paramValue,
                                    size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetProgramInfo, &program, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clProgram", program, "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    Program *pProgram = nullptr;
    retVal = validateObjects(withCastToInternal(program, &pProgram));

    if (CL_SUCCESS == retVal) {
        retVal = pProgram->getInfo(
            paramName,
            paramValueSize,
            paramValue,
            paramValueSizeRet);
    }
    TRACING_EXIT(ClGetProgramInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetProgramBuildInfo(cl_program program,
                                         cl_device_id device,
                                         cl_program_build_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetProgramBuildInfo, &program, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clProgram", program, "cl_device_id", device,
                   "paramName", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSize", paramValueSize, "paramValue", paramValue,
                   "paramValueSizeRet", paramValueSizeRet);
    Program *pProgram = nullptr;
    ClDevice *pClDevice = nullptr;

    retVal = validateObjects(withCastToInternal(program, &pProgram), withCastToInternal(device, &pClDevice));

    if (CL_SUCCESS == retVal) {
        if (!pProgram->isDeviceAssociated(*pClDevice)) {
            retVal = CL_INVALID_DEVICE;
        }
    }
    if (CL_SUCCESS == retVal) {
        retVal = pProgram->getBuildInfo(
            pClDevice,
            paramName,
            paramValueSize,
            paramValue,
            paramValueSizeRet);
    }
    TRACING_EXIT(ClGetProgramBuildInfo, &retVal);
    return retVal;
}

cl_kernel CL_API_CALL clCreateKernel(cl_program clProgram,
                                     const char *kernelName,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateKernel, &clProgram, &kernelName, &errcodeRet);
    API_ENTER(errcodeRet);
    Program *pProgram = nullptr;
    cl_kernel kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
    DBG_LOG_INPUTS("clProgram", clProgram, "kernelName", kernelName);

    do {
        if (!isValidObject(clProgram) ||
            !(pProgram = castToObject<Program>(clProgram))) {
            retVal = CL_INVALID_PROGRAM;
            break;
        }

        if (kernelName == nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }

        if (!pProgram->isBuilt()) {
            retVal = CL_INVALID_PROGRAM_EXECUTABLE;
            break;
        }

        bool kernelFound = false;
        KernelInfoContainer kernelInfos;
        kernelInfos.resize(pProgram->getMaxRootDeviceIndex() + 1);

        for (const auto &pClDevice : pProgram->getDevicesInProgram()) {
            auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
            auto pKernelInfo = pProgram->getKernelInfo(kernelName, rootDeviceIndex);
            if (pKernelInfo) {
                kernelFound = true;
                kernelInfos[rootDeviceIndex] = pKernelInfo;
            }
        }

        if (!kernelFound) {
            retVal = CL_INVALID_KERNEL_NAME;
            break;
        }

        kernel = MultiDeviceKernel::create(
            pProgram,
            kernelInfos,
            retVal);

        DBG_LOG_INPUTS("kernel", kernel);
    } while (false);

    if (errcodeRet) {
        *errcodeRet = retVal;
    }
    gtpinNotifyKernelCreate(kernel);
    TRACING_EXIT(ClCreateKernel, &kernel);
    return kernel;
}

cl_int CL_API_CALL clCreateKernelsInProgram(cl_program clProgram,
                                            cl_uint numKernels,
                                            cl_kernel *kernels,
                                            cl_uint *numKernelsRet) {
    TRACING_ENTER(ClCreateKernelsInProgram, &clProgram, &numKernels, &kernels, &numKernelsRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("clProgram", clProgram,
                   "numKernels", numKernels,
                   "kernels", kernels,
                   "numKernelsRet", numKernelsRet);
    auto pProgram = castToObject<Program>(clProgram);
    if (pProgram) {
        auto numKernelsInProgram = pProgram->getNumKernels();

        if (kernels) {
            if (numKernels < numKernelsInProgram) {
                retVal = CL_INVALID_VALUE;
                TRACING_EXIT(ClCreateKernelsInProgram, &retVal);
                return retVal;
            }

            for (unsigned int i = 0; i < numKernelsInProgram; ++i) {
                KernelInfoContainer kernelInfos;
                kernelInfos.resize(pProgram->getMaxRootDeviceIndex() + 1);
                for (const auto &pClDevice : pProgram->getDevicesInProgram()) {
                    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
                    auto kernelInfo = pProgram->getKernelInfo(i, rootDeviceIndex);
                    DEBUG_BREAK_IF(kernelInfo == nullptr);
                    kernelInfos[rootDeviceIndex] = kernelInfo;
                }
                kernels[i] = MultiDeviceKernel::create(
                    pProgram,
                    kernelInfos,
                    retVal);
                if (nullptr == kernels[i]) {
                    UNRECOVERABLE_IF(CL_SUCCESS == retVal);
                    for (unsigned int createdIdx = 0; createdIdx < i; ++createdIdx) {
                        auto mDKernel = castToObject<MultiDeviceKernel>(kernels[createdIdx]);
                        mDKernel->release();
                        kernels[createdIdx] = nullptr;
                    }
                    TRACING_EXIT(ClCreateKernelsInProgram, &retVal);
                    return retVal;
                }
                gtpinNotifyKernelCreate(kernels[i]);
            }
        }

        if (numKernelsRet) {
            *numKernelsRet = static_cast<cl_uint>(numKernelsInProgram);
        }
        TRACING_EXIT(ClCreateKernelsInProgram, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_PROGRAM;
    TRACING_EXIT(ClCreateKernelsInProgram, &retVal);
    return retVal;
}

cl_int CL_API_CALL clRetainKernel(cl_kernel kernel) {
    TRACING_ENTER(ClRetainKernel, &kernel);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("kernel", kernel);
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    if (pMultiDeviceKernel) {
        pMultiDeviceKernel->retain();
        TRACING_EXIT(ClRetainKernel, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_KERNEL;
    TRACING_EXIT(ClRetainKernel, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseKernel(cl_kernel kernel) {
    TRACING_ENTER(ClReleaseKernel, &kernel);
    cl_int retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseKernel, &retVal);
        return CL_SUCCESS;
    }
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("kernel", kernel);
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    if (pMultiDeviceKernel) {
        pMultiDeviceKernel->release();
        TRACING_EXIT(ClReleaseKernel, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_KERNEL;
    TRACING_EXIT(ClReleaseKernel, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetKernelArg(cl_kernel kernel,
                                  cl_uint argIndex,
                                  size_t argSize,
                                  const void *argValue) {
    TRACING_ENTER(ClSetKernelArg, &kernel, &argIndex, &argSize, &argValue);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    retVal = validateObject(withCastToInternal(kernel, &pMultiDeviceKernel));
    DBG_LOG_INPUTS("kernel", kernel, "argIndex", argIndex,
                   "argSize", argSize, "argValue", NEO::fileLoggerInstance().infoPointerToString(argValue, argSize));
    do {
        if (retVal != CL_SUCCESS) {
            break;
        }
        if (pMultiDeviceKernel->getKernelArguments().size() <= argIndex) {
            retVal = CL_INVALID_ARG_INDEX;
            break;
        }
        retVal = pMultiDeviceKernel->checkCorrectImageAccessQualifier(argIndex, argSize, argValue);
        if (retVal != CL_SUCCESS) {
            pMultiDeviceKernel->unsetArg(argIndex);
            break;
        }
        retVal = pMultiDeviceKernel->setArg(
            argIndex,
            argSize,
            argValue);
        break;

    } while (false);
    TRACING_EXIT(ClSetKernelArg, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelInfo(cl_kernel kernel,
                                   cl_kernel_info paramName,
                                   size_t paramValueSize,
                                   void *paramValue,
                                   size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelInfo, &kernel, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("kernel", kernel, "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    retVal = validateObject(withCastToInternal(kernel, &pMultiDeviceKernel));
    if (retVal == CL_SUCCESS) {
        retVal = pMultiDeviceKernel->getInfo(
            paramName,
            paramValueSize,
            paramValue,
            paramValueSizeRet);
    }
    TRACING_EXIT(ClGetKernelInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelArgInfo(cl_kernel kernel,
                                      cl_uint argIndx,
                                      cl_kernel_arg_info paramName,
                                      size_t paramValueSize,
                                      void *paramValue,
                                      size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelArgInfo, &kernel, &argIndx, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("kernel", kernel,
                   "argIndx", argIndx,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    retVal = pMultiDeviceKernel
                 ? pMultiDeviceKernel->getArgInfo(
                       argIndx,
                       paramName,
                       paramValueSize,
                       paramValue,
                       paramValueSizeRet)
                 : CL_INVALID_KERNEL;
    TRACING_EXIT(ClGetKernelArgInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelWorkGroupInfo(cl_kernel kernel,
                                            cl_device_id device,
                                            cl_kernel_work_group_info paramName,
                                            size_t paramValueSize,
                                            void *paramValue,
                                            size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelWorkGroupInfo, &kernel, &device, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("kernel", kernel,
                   "device", device,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    retVal = validateObjects(withCastToInternal(kernel, &pMultiDeviceKernel));

    ClDevice *pClDevice = nullptr;
    if (CL_SUCCESS == retVal) {
        if (pMultiDeviceKernel->getDevices().size() == 1u && !device) {
            pClDevice = pMultiDeviceKernel->getDevices()[0];
        } else {
            retVal = validateObjects(withCastToInternal(device, &pClDevice));
        }
    }
    if (CL_SUCCESS == retVal) {
        auto pKernel = pMultiDeviceKernel->getKernel(pClDevice->getRootDeviceIndex());
        retVal = pKernel->getWorkGroupInfo(
            paramName,
            paramValueSize,
            paramValue,
            paramValueSizeRet);
    }
    TRACING_EXIT(ClGetKernelWorkGroupInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clWaitForEvents(cl_uint numEvents,
                                   const cl_event *eventList) {
    TRACING_ENTER(ClWaitForEvents, &numEvents, &eventList);

    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("eventList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventList), numEvents));

    for (unsigned int i = 0; i < numEvents && retVal == CL_SUCCESS; i++)
        retVal = validateObjects(eventList[i]);

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClWaitForEvents, &retVal);
        return retVal;
    }

    retVal = Event::waitForEvents(numEvents, eventList);
    TRACING_EXIT(ClWaitForEvents, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetEventInfo(cl_event event,
                                  cl_event_info paramName,
                                  size_t paramValueSize,
                                  void *paramValue,
                                  size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("event", event,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    Event *neoEvent = castToObject<Event>(event);
    if (neoEvent == nullptr) {
        retVal = CL_INVALID_EVENT;
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    }

    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);
    auto flushEvents = true;

    switch (paramName) {
    default: {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    }
    // From OCL spec :
    // "Return the command-queue associated with event. For user event objects,"
    //  a nullptr value is returned."
    case CL_EVENT_COMMAND_QUEUE: {
        if (neoEvent->isUserEvent()) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_command_queue>(nullptr));
            TRACING_EXIT(ClGetEventInfo, &retVal);
            return retVal;
        }
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_command_queue>(neoEvent->getCommandQueue()));
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    }
    case CL_EVENT_CONTEXT:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_context>(neoEvent->getContext()));
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    case CL_EVENT_COMMAND_TYPE:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_command_type>(neoEvent->getCommandType()));
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    case CL_EVENT_COMMAND_EXECUTION_STATUS:
        if (debugManager.flags.SkipFlushingEventsOnGetStatusCalls.get()) {
            flushEvents = false;
        }
        if (flushEvents) {
            neoEvent->tryFlushEvent();
        }

        if (neoEvent->isUserEvent()) {
            auto executionStatus = neoEvent->peekExecutionStatus();
            // Spec requires initial state to be queued
            // our current design relies heavily on SUBMITTED status which directly corresponds
            // to command being able to be submitted, to overcome this we set initial status to queued
            // and we override the value stored with the value required by the spec.
            if (executionStatus == CL_QUEUED) {
                executionStatus = CL_SUBMITTED;
            }
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(executionStatus));
            TRACING_EXIT(ClGetEventInfo, &retVal);
            return retVal;
        }

        retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(neoEvent->updateEventAndReturnCurrentStatus()));
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    case CL_EVENT_REFERENCE_COUNT:
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_uint>(neoEvent->getReference()));
        TRACING_EXIT(ClGetEventInfo, &retVal);
        return retVal;
    }
}

cl_event CL_API_CALL clCreateUserEvent(cl_context context,
                                       cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateUserEvent, &context, &errcodeRet);
    API_ENTER(errcodeRet);

    DBG_LOG_INPUTS("context", context);

    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    Context *ctx = castToObject<Context>(context);
    if (ctx == nullptr) {
        err.set(CL_INVALID_CONTEXT);
        cl_event retVal = nullptr;
        TRACING_EXIT(ClCreateUserEvent, &retVal);
        return retVal;
    }

    Event *userEvent = new UserEvent(ctx);
    cl_event userClEvent = userEvent;
    DBG_LOG_INPUTS("cl_event", userClEvent, "UserEvent", userEvent);

    TRACING_EXIT(ClCreateUserEvent, &userClEvent);
    return userClEvent;
}

cl_int CL_API_CALL clRetainEvent(cl_event event) {
    TRACING_ENTER(ClRetainEvent, &event);
    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    auto pEvent = castToObject<Event>(event);
    DBG_LOG_INPUTS("cl_event", event, "Event", pEvent);

    if (pEvent) {
        pEvent->retain();
        TRACING_EXIT(ClRetainEvent, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_EVENT;
    TRACING_EXIT(ClRetainEvent, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseEvent(cl_event event) {
    TRACING_ENTER(ClReleaseEvent, &event);
    auto retVal = CL_SUCCESS;
    if (wasPlatformTeardownCalled) {
        TRACING_EXIT(ClReleaseEvent, &retVal);
        return CL_SUCCESS;
    }
    API_ENTER(&retVal);
    auto pEvent = castToObject<Event>(event);
    DBG_LOG_INPUTS("cl_event", event, "Event", pEvent);

    if (pEvent) {
        if (debugManager.flags.BlockingEventRelease.get() && pEvent->getRefApiCount() == 1 && pEvent->getCommandQueue()) {
            pEvent->wait(false, false);
        }
        pEvent->release();
        TRACING_EXIT(ClReleaseEvent, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_EVENT;
    TRACING_EXIT(ClReleaseEvent, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetUserEventStatus(cl_event event,
                                        cl_int executionStatus) {
    TRACING_ENTER(ClSetUserEventStatus, &event, &executionStatus);
    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    auto userEvent = castToObject<UserEvent>(event);
    DBG_LOG_INPUTS("cl_event", event, "executionStatus", executionStatus, "UserEvent", userEvent);

    if (userEvent == nullptr) {
        retVal = CL_INVALID_EVENT;
        TRACING_EXIT(ClSetUserEventStatus, &retVal);
        return retVal;
    }

    if (executionStatus > CL_COMPLETE) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClSetUserEventStatus, &retVal);
        return retVal;
    }

    if (!userEvent->isInitialEventStatus()) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClSetUserEventStatus, &retVal);
        return retVal;
    }

    userEvent->setStatus(executionStatus);
    TRACING_EXIT(ClSetUserEventStatus, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetEventCallback(cl_event event,
                                      cl_int commandExecCallbackType,
                                      void(CL_CALLBACK *funcNotify)(cl_event, cl_int, void *),
                                      void *userData) {
    TRACING_ENTER(ClSetEventCallback, &event, &commandExecCallbackType, &funcNotify, &userData);
    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    auto eventObject = castToObject<Event>(event);
    DBG_LOG_INPUTS("cl_event", event, "commandExecCallbackType", commandExecCallbackType, "Event", eventObject);

    if (eventObject == nullptr) {
        retVal = CL_INVALID_EVENT;
        TRACING_EXIT(ClSetEventCallback, &retVal);
        return retVal;
    }
    switch (commandExecCallbackType) {
    case CL_COMPLETE:
    case CL_SUBMITTED:
    case CL_RUNNING:
        break;
    default: {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClSetEventCallback, &retVal);
        return retVal;
    }
    }
    if (funcNotify == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClSetEventCallback, &retVal);
        return retVal;
    }

    if (eventObject->tryFlushEvent() == false) {
        retVal = CL_OUT_OF_RESOURCES;
        TRACING_EXIT(ClSetEventCallback, &retVal);
        return retVal;
    }

    eventObject->addCallback(funcNotify, commandExecCallbackType, userData);
    TRACING_EXIT(ClSetEventCallback, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetEventProfilingInfo(cl_event event,
                                           cl_profiling_info paramName,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetEventProfilingInfo, &event, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    auto retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("event", event,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);
    auto eventObject = castToObject<Event>(event);

    if (eventObject == nullptr) {
        retVal = CL_INVALID_EVENT;
        TRACING_EXIT(ClGetEventProfilingInfo, &retVal);
        return retVal;
    }

    retVal = eventObject->getEventProfilingInfo(paramName,
                                                paramValueSize,
                                                paramValue,
                                                paramValueSizeRet);
    TRACING_EXIT(ClGetEventProfilingInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clFlush(cl_command_queue commandQueue) {
    TRACING_ENTER(ClFlush, &commandQueue);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);
    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);

    retVal = pCommandQueue
                 ? pCommandQueue->flush()
                 : CL_INVALID_COMMAND_QUEUE;
    TRACING_EXIT(ClFlush, &retVal);
    return retVal;
}

cl_int CL_API_CALL clFinish(cl_command_queue commandQueue) {
    TRACING_ENTER(ClFinish, &commandQueue);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);
    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);

    retVal = pCommandQueue
                 ? pCommandQueue->finish(false)
                 : CL_INVALID_COMMAND_QUEUE;
    TRACING_EXIT(ClFinish, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReadBuffer(cl_command_queue commandQueue,
                                       cl_mem buffer,
                                       cl_bool blockingRead,
                                       size_t offset,
                                       size_t cb,
                                       void *ptr,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    TRACING_ENTER(ClEnqueueReadBuffer, &commandQueue, &buffer, &blockingRead, &offset, &cb, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    CommandQueue *pCommandQueue = nullptr;
    Buffer *pBuffer = nullptr;

    auto retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(buffer, &pBuffer),
        ptr);

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "buffer", buffer, "blockingRead", blockingRead,
                   "offset", offset, "cb", cb, "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS == retVal) {

        if (pBuffer->readMemObjFlagsInvalid()) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueReadBuffer, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueReadBuffer, &retVal);
            return retVal;
        }

        if (pCommandQueue->isValidForStagingTransfer(pBuffer, ptr, cb, CL_COMMAND_READ_BUFFER, blockingRead, numEventsInWaitList > 0)) {
            retVal = pCommandQueue->enqueueStagingBufferTransfer(
                CL_COMMAND_READ_BUFFER,
                pBuffer,
                blockingRead,
                offset,
                cb,
                ptr,
                event);
        } else {
            retVal = pCommandQueue->enqueueReadBuffer(
                pBuffer,
                blockingRead,
                offset,
                cb,
                ptr,
                nullptr,
                numEventsInWaitList,
                eventWaitList,
                event);
        }
    }

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueReadBuffer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReadBufferRect(cl_command_queue commandQueue,
                                           cl_mem buffer,
                                           cl_bool blockingRead,
                                           const size_t *bufferOrigin,
                                           const size_t *hostOrigin,
                                           const size_t *region,
                                           size_t bufferRowPitch,
                                           size_t bufferSlicePitch,
                                           size_t hostRowPitch,
                                           size_t hostSlicePitch,
                                           void *ptr,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    TRACING_ENTER(ClEnqueueReadBufferRect, &commandQueue, &buffer, &blockingRead, &bufferOrigin, &hostOrigin, &region, &bufferRowPitch, &bufferSlicePitch, &hostRowPitch, &hostSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "buffer", buffer,
                   "blockingRead", blockingRead,
                   "bufferOrigin[0]", NEO::fileLoggerInstance().getInput(bufferOrigin, 0),
                   "bufferOrigin[1]", NEO::fileLoggerInstance().getInput(bufferOrigin, 1),
                   "bufferOrigin[2]", NEO::fileLoggerInstance().getInput(bufferOrigin, 2),
                   "hostOrigin[0]", NEO::fileLoggerInstance().getInput(hostOrigin, 0),
                   "hostOrigin[1]", NEO::fileLoggerInstance().getInput(hostOrigin, 1),
                   "hostOrigin[2]", NEO::fileLoggerInstance().getInput(hostOrigin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0),
                   "region[1]", NEO::fileLoggerInstance().getInput(region, 1),
                   "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "bufferRowPitch", bufferRowPitch,
                   "bufferSlicePitch", bufferSlicePitch,
                   "hostRowPitch", hostRowPitch,
                   "hostSlicePitch", hostSlicePitch,
                   "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(buffer, &pBuffer),
        ptr);

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->readMemObjFlagsInvalid()) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->bufferRectPitchSet(bufferOrigin,
                                    region,
                                    bufferRowPitch,
                                    bufferSlicePitch,
                                    hostRowPitch,
                                    hostSlicePitch,
                                    true) == false) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueReadBufferRect(
        pBuffer,
        blockingRead,
        bufferOrigin,
        hostOrigin,
        region,
        bufferRowPitch,
        bufferSlicePitch,
        hostRowPitch,
        hostSlicePitch,
        ptr,
        numEventsInWaitList,
        eventWaitList,
        event);
    TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueWriteBuffer(cl_command_queue commandQueue,
                                        cl_mem buffer,
                                        cl_bool blockingWrite,
                                        size_t offset,
                                        size_t cb,
                                        const void *ptr,
                                        cl_uint numEventsInWaitList,
                                        const cl_event *eventWaitList,
                                        cl_event *event) {
    TRACING_ENTER(ClEnqueueWriteBuffer, &commandQueue, &buffer, &blockingWrite, &offset, &cb, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "buffer", buffer, "blockingWrite", blockingWrite,
                   "offset", offset, "cb", cb, "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(buffer, &pBuffer),
        ptr);

    if (CL_SUCCESS == retVal) {

        if (pBuffer->writeMemObjFlagsInvalid()) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueWriteBuffer, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueWriteBuffer, &retVal);
            return retVal;
        }

        if (pCommandQueue->isValidForStagingTransfer(pBuffer, ptr, cb, CL_COMMAND_WRITE_BUFFER, blockingWrite, numEventsInWaitList > 0)) {
            retVal = pCommandQueue->enqueueStagingBufferTransfer(
                CL_COMMAND_WRITE_BUFFER,
                pBuffer,
                blockingWrite,
                offset,
                cb,
                ptr,
                event);
        } else {
            retVal = pCommandQueue->enqueueWriteBuffer(
                pBuffer,
                blockingWrite,
                offset,
                cb,
                ptr,
                nullptr,
                numEventsInWaitList,
                eventWaitList,
                event);
        }
    }

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueWriteBuffer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueWriteBufferRect(cl_command_queue commandQueue,
                                            cl_mem buffer,
                                            cl_bool blockingWrite,
                                            const size_t *bufferOrigin,
                                            const size_t *hostOrigin,
                                            const size_t *region,
                                            size_t bufferRowPitch,
                                            size_t bufferSlicePitch,
                                            size_t hostRowPitch,
                                            size_t hostSlicePitch,
                                            const void *ptr,
                                            cl_uint numEventsInWaitList,
                                            const cl_event *eventWaitList,
                                            cl_event *event) {
    TRACING_ENTER(ClEnqueueWriteBufferRect, &commandQueue, &buffer, &blockingWrite, &bufferOrigin, &hostOrigin, &region, &bufferRowPitch, &bufferSlicePitch, &hostRowPitch, &hostSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "buffer", buffer, "blockingWrite", blockingWrite,
                   "bufferOrigin[0]", NEO::fileLoggerInstance().getInput(bufferOrigin, 0), "bufferOrigin[1]", NEO::fileLoggerInstance().getInput(bufferOrigin, 1), "bufferOrigin[2]", NEO::fileLoggerInstance().getInput(bufferOrigin, 2),
                   "hostOrigin[0]", NEO::fileLoggerInstance().getInput(hostOrigin, 0), "hostOrigin[1]", NEO::fileLoggerInstance().getInput(hostOrigin, 1), "hostOrigin[2]", NEO::fileLoggerInstance().getInput(hostOrigin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "bufferRowPitch", bufferRowPitch, "bufferSlicePitch", bufferSlicePitch,
                   "hostRowPitch", hostRowPitch, "hostSlicePitch", hostSlicePitch, "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(buffer, &pBuffer),
        ptr);

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->writeMemObjFlagsInvalid()) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->bufferRectPitchSet(bufferOrigin,
                                    region,
                                    bufferRowPitch,
                                    bufferSlicePitch,
                                    hostRowPitch,
                                    hostSlicePitch,
                                    true) == false) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueWriteBufferRect(
        pBuffer,
        blockingWrite,
        bufferOrigin,
        hostOrigin,
        region,
        bufferRowPitch,
        bufferSlicePitch,
        hostRowPitch,
        hostSlicePitch,
        ptr,
        numEventsInWaitList,
        eventWaitList,
        event);

    TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueFillBuffer(cl_command_queue commandQueue,
                                       cl_mem buffer,
                                       const void *pattern,
                                       size_t patternSize,
                                       size_t offset,
                                       size_t size,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    TRACING_ENTER(ClEnqueueFillBuffer, &commandQueue, &buffer, &pattern, &patternSize, &offset, &size, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "buffer", buffer,
                   "pattern", NEO::fileLoggerInstance().infoPointerToString(pattern, patternSize), "patternSize", patternSize,
                   "offset", offset, "size", size,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(buffer, &pBuffer),
        pattern,
        static_cast<PatternSize>(patternSize),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS == retVal) {
        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueFillBuffer, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueFillBuffer(
            pBuffer,
            pattern,
            patternSize,
            offset,
            size,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
    TRACING_EXIT(ClEnqueueFillBuffer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueCopyBuffer(cl_command_queue commandQueue,
                                       cl_mem srcBuffer,
                                       cl_mem dstBuffer,
                                       size_t srcOffset,
                                       size_t dstOffset,
                                       size_t cb,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    TRACING_ENTER(ClEnqueueCopyBuffer, &commandQueue, &srcBuffer, &dstBuffer, &srcOffset, &dstOffset, &cb, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "srcBuffer", srcBuffer, "dstBuffer", dstBuffer,
                   "srcOffset", srcOffset, "dstOffset", dstOffset, "cb", cb,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pSrcBuffer = nullptr;
    Buffer *pDstBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(srcBuffer, &pSrcBuffer),
        withCastToInternal(dstBuffer, &pDstBuffer));

    if (CL_SUCCESS == retVal) {
        size_t srcSize = pSrcBuffer->getSize();
        size_t dstSize = pDstBuffer->getSize();
        if (srcOffset + cb > srcSize || dstOffset + cb > dstSize) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClEnqueueCopyBuffer, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueCopyBuffer, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueCopyBuffer(
            pSrcBuffer,
            pDstBuffer,
            srcOffset,
            dstOffset,
            cb,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueCopyBuffer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueCopyBufferRect(cl_command_queue commandQueue,
                                           cl_mem srcBuffer,
                                           cl_mem dstBuffer,
                                           const size_t *srcOrigin,
                                           const size_t *dstOrigin,
                                           const size_t *region,
                                           size_t srcRowPitch,
                                           size_t srcSlicePitch,
                                           size_t dstRowPitch,
                                           size_t dstSlicePitch,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    TRACING_ENTER(ClEnqueueCopyBufferRect, &commandQueue, &srcBuffer, &dstBuffer, &srcOrigin, &dstOrigin, &region, &srcRowPitch, &srcSlicePitch, &dstRowPitch, &dstSlicePitch, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "srcBuffer", srcBuffer, "dstBuffer", dstBuffer,
                   "srcOrigin[0]", NEO::fileLoggerInstance().getInput(srcOrigin, 0), "srcOrigin[1]", NEO::fileLoggerInstance().getInput(srcOrigin, 1), "srcOrigin[2]", NEO::fileLoggerInstance().getInput(srcOrigin, 2),
                   "dstOrigin[0]", NEO::fileLoggerInstance().getInput(dstOrigin, 0), "dstOrigin[1]", NEO::fileLoggerInstance().getInput(dstOrigin, 1), "dstOrigin[2]", NEO::fileLoggerInstance().getInput(dstOrigin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "srcRowPitch", srcRowPitch, "srcSlicePitch", srcSlicePitch,
                   "dstRowPitch", dstRowPitch, "dstSlicePitch", dstSlicePitch,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pSrcBuffer = nullptr;
    Buffer *pDstBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(srcBuffer, &pSrcBuffer),
        withCastToInternal(dstBuffer, &pDstBuffer));

    if (CL_SUCCESS == retVal) {

        if (!pSrcBuffer->bufferRectPitchSet(srcOrigin,
                                            region,
                                            srcRowPitch,
                                            srcSlicePitch,
                                            dstRowPitch,
                                            dstSlicePitch,
                                            true) ||
            !pDstBuffer->bufferRectPitchSet(dstOrigin,
                                            region,
                                            srcRowPitch,
                                            srcSlicePitch,
                                            dstRowPitch,
                                            dstSlicePitch,
                                            false)) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClEnqueueCopyBufferRect, &retVal);
            return retVal;
        }
        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_RECT_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueCopyBufferRect, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueCopyBufferRect(
            pSrcBuffer,
            pDstBuffer,
            srcOrigin,
            dstOrigin,
            region,
            srcRowPitch,
            srcSlicePitch,
            dstRowPitch,
            dstSlicePitch,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueCopyBufferRect, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReadImage(cl_command_queue commandQueue,
                                      cl_mem image,
                                      cl_bool blockingRead,
                                      const size_t *origin,
                                      const size_t *region,
                                      size_t rowPitch,
                                      size_t slicePitch,
                                      void *ptr,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueReadImage, &commandQueue, &image, &blockingRead, &origin, &region, &rowPitch, &slicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;
    Image *pImage = nullptr;

    auto retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(image, &pImage));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "image", image, "blockingRead", blockingRead,
                   "origin[0]", NEO::fileLoggerInstance().getInput(origin, 0), "origin[1]", NEO::fileLoggerInstance().getInput(origin, 1), "origin[2]", NEO::fileLoggerInstance().getInput(origin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "rowPitch", rowPitch, "slicePitch", slicePitch, "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS == retVal) {

        if (pImage->readMemObjFlagsInvalid()) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueReadImage, &retVal);
            return retVal;
        }
        if (isPackedYuvImage(&pImage->getImageFormat())) {
            retVal = validateYuvOperation(origin, region);
            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueReadImage, &retVal);
                return retVal;
            }
        }
        retVal = Image::validateRegionAndOrigin(origin, region, pImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueReadImage, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueReadImage, &retVal);
            return retVal;
        }

        auto hostPtrSize = pCommandQueue->calculateHostPtrSizeForImage(region, rowPitch, slicePitch, pImage);
        if (pCommandQueue->isValidForStagingTransfer(pImage, ptr, hostPtrSize, CL_COMMAND_READ_IMAGE, blockingRead, numEventsInWaitList > 0)) {
            retVal = pCommandQueue->enqueueStagingImageTransfer(CL_COMMAND_READ_IMAGE, pImage, blockingRead, origin, region, rowPitch, slicePitch, ptr, event);
        } else {
            retVal = pCommandQueue->enqueueReadImage(
                pImage,
                blockingRead,
                origin,
                region,
                rowPitch,
                slicePitch,
                ptr,
                nullptr,
                numEventsInWaitList,
                eventWaitList,
                event);
        }
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueReadImage, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueWriteImage(cl_command_queue commandQueue,
                                       cl_mem image,
                                       cl_bool blockingWrite,
                                       const size_t *origin,
                                       const size_t *region,
                                       size_t inputRowPitch,
                                       size_t inputSlicePitch,
                                       const void *ptr,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    TRACING_ENTER(ClEnqueueWriteImage, &commandQueue, &image, &blockingWrite, &origin, &region, &inputRowPitch, &inputSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;
    Image *pImage = nullptr;

    auto retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(image, &pImage));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "image", image, "blockingWrite", blockingWrite,
                   "origin[0]", NEO::fileLoggerInstance().getInput(origin, 0), "origin[1]", NEO::fileLoggerInstance().getInput(origin, 1), "origin[2]", NEO::fileLoggerInstance().getInput(origin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "inputRowPitch", inputRowPitch, "inputSlicePitch", inputSlicePitch, "ptr", ptr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS == retVal) {
        if (pImage->writeMemObjFlagsInvalid()) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueWriteImage, &retVal);
            return retVal;
        }
        if (isPackedYuvImage(&pImage->getImageFormat())) {
            retVal = validateYuvOperation(origin, region);
            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueWriteImage, &retVal);
                return retVal;
            }
        }
        retVal = Image::validateRegionAndOrigin(origin, region, pImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueWriteImage, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueWriteImage, &retVal);
            return retVal;
        }

        auto hostPtrSize = pCommandQueue->calculateHostPtrSizeForImage(region, inputRowPitch, inputSlicePitch, pImage);
        if (pCommandQueue->isValidForStagingTransfer(pImage, ptr, hostPtrSize, CL_COMMAND_WRITE_IMAGE, blockingWrite, numEventsInWaitList > 0)) {
            retVal = pCommandQueue->enqueueStagingImageTransfer(CL_COMMAND_WRITE_IMAGE, pImage, blockingWrite, origin, region, inputRowPitch, inputSlicePitch, ptr, event);
        } else {
            retVal = pCommandQueue->enqueueWriteImage(
                pImage,
                blockingWrite,
                origin,
                region,
                inputRowPitch,
                inputSlicePitch,
                ptr,
                nullptr,
                numEventsInWaitList,
                eventWaitList,
                event);
        }
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueWriteImage, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue commandQueue,
                                      cl_mem image,
                                      const void *fillColor,
                                      const size_t *origin,
                                      const size_t *region,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueFillImage, &commandQueue, &image, &fillColor, &origin, &region, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;
    Image *dstImage = nullptr;

    auto retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(image, &dstImage),
        fillColor,
        EventWaitList(numEventsInWaitList, eventWaitList));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "image", image, "fillColor", fillColor,
                   "origin[0]", NEO::fileLoggerInstance().getInput(origin, 0), "origin[1]", NEO::fileLoggerInstance().getInput(origin, 1), "origin[2]", NEO::fileLoggerInstance().getInput(origin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS == retVal) {
        retVal = Image::validateRegionAndOrigin(origin, region, dstImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueFillImage, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_FILL_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueFillImage, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueFillImage(
            dstImage,
            fillColor,
            origin,
            region,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueFillImage, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueCopyImage(cl_command_queue commandQueue,
                                      cl_mem srcImage,
                                      cl_mem dstImage,
                                      const size_t *srcOrigin,
                                      const size_t *dstOrigin,
                                      const size_t *region,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueCopyImage, &commandQueue, &srcImage, &dstImage, &srcOrigin, &dstOrigin, &region, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;
    Image *pSrcImage = nullptr;
    Image *pDstImage = nullptr;

    auto retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue),
                                  withCastToInternal(srcImage, &pSrcImage),
                                  withCastToInternal(dstImage, &pDstImage));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "srcImage", srcImage, "dstImage", dstImage,
                   "srcOrigin[0]", NEO::fileLoggerInstance().getInput(srcOrigin, 0), "srcOrigin[1]", NEO::fileLoggerInstance().getInput(srcOrigin, 1), "srcOrigin[2]", NEO::fileLoggerInstance().getInput(srcOrigin, 2),
                   "dstOrigin[0]", NEO::fileLoggerInstance().getInput(dstOrigin, 0), "dstOrigin[1]", NEO::fileLoggerInstance().getInput(dstOrigin, 1), "dstOrigin[2]", NEO::fileLoggerInstance().getInput(dstOrigin, 2),
                   "region[0]", region ? region[0] : 0, "region[1]", region ? region[1] : 0, "region[2]", region ? region[2] : 0,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS == retVal) {
        if (memcmp(&pSrcImage->getImageFormat(), &pDstImage->getImageFormat(), sizeof(cl_image_format))) {
            retVal = CL_IMAGE_FORMAT_MISMATCH;
            TRACING_EXIT(ClEnqueueCopyImage, &retVal);
            return retVal;
        }
        if (isPackedYuvImage(&pSrcImage->getImageFormat())) {
            retVal = validateYuvOperation(srcOrigin, region);
            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueCopyImage, &retVal);
                return retVal;
            }
        }
        if (isPackedYuvImage(&pDstImage->getImageFormat())) {
            retVal = validateYuvOperation(dstOrigin, region);

            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueCopyImage, &retVal);
                return retVal;
            }
            if (pDstImage->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE2D && dstOrigin[2] != 0) {
                retVal = CL_INVALID_VALUE;
                TRACING_EXIT(ClEnqueueCopyImage, &retVal);
                return retVal;
            }
        }
        retVal = Image::validateRegionAndOrigin(srcOrigin, region, pSrcImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueCopyImage, &retVal);
            return retVal;
        }
        retVal = Image::validateRegionAndOrigin(dstOrigin, region, pDstImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueCopyImage, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueCopyImage, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueCopyImage(
            pSrcImage,
            pDstImage,
            srcOrigin,
            dstOrigin,
            region,
            numEventsInWaitList,
            eventWaitList,
            event);
    }
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueCopyImage, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueCopyImageToBuffer(cl_command_queue commandQueue,
                                              cl_mem srcImage,
                                              cl_mem dstBuffer,
                                              const size_t *srcOrigin,
                                              const size_t *region,
                                              const size_t dstOffset,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    TRACING_ENTER(ClEnqueueCopyImageToBuffer, &commandQueue, &srcImage, &dstBuffer, &srcOrigin, &region, &dstOffset, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "srcImage", srcImage, "dstBuffer", dstBuffer,
                   "srcOrigin[0]", NEO::fileLoggerInstance().getInput(srcOrigin, 0), "srcOrigin[1]", NEO::fileLoggerInstance().getInput(srcOrigin, 1), "srcOrigin[2]", NEO::fileLoggerInstance().getInput(srcOrigin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "dstOffset", dstOffset,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Image *pSrcImage = nullptr;
    Buffer *pDstBuffer = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(srcImage, &pSrcImage),
        withCastToInternal(dstBuffer, &pDstBuffer));

    if (CL_SUCCESS == retVal) {
        if (isPackedYuvImage(&pSrcImage->getImageFormat())) {
            retVal = validateYuvOperation(srcOrigin, region);
            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueCopyImageToBuffer, &retVal);
                return retVal;
            }
        }
        retVal = Image::validateRegionAndOrigin(srcOrigin, region, pSrcImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueCopyImageToBuffer, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_IMAGE_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueCopyImageToBuffer, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueCopyImageToBuffer(
            pSrcImage,
            pDstBuffer,
            srcOrigin,
            region,
            dstOffset,
            numEventsInWaitList,
            eventWaitList,
            event);
    }

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueCopyImageToBuffer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueCopyBufferToImage(cl_command_queue commandQueue,
                                              cl_mem srcBuffer,
                                              cl_mem dstImage,
                                              size_t srcOffset,
                                              const size_t *dstOrigin,
                                              const size_t *region,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    TRACING_ENTER(ClEnqueueCopyBufferToImage, &commandQueue, &srcBuffer, &dstImage, &srcOffset, &dstOrigin, &region, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "srcBuffer", srcBuffer, "dstImage", dstImage, "srcOffset", srcOffset,
                   "dstOrigin[0]", NEO::fileLoggerInstance().getInput(dstOrigin, 0), "dstOrigin[1]", NEO::fileLoggerInstance().getInput(dstOrigin, 1), "dstOrigin[2]", NEO::fileLoggerInstance().getInput(dstOrigin, 2),
                   "region[0]", NEO::fileLoggerInstance().getInput(region, 0), "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Buffer *pSrcBuffer = nullptr;
    Image *pDstImage = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(srcBuffer, &pSrcBuffer),
        withCastToInternal(dstImage, &pDstImage));

    if (CL_SUCCESS == retVal) {
        if (isPackedYuvImage(&pDstImage->getImageFormat())) {
            retVal = validateYuvOperation(dstOrigin, region);
            if (retVal != CL_SUCCESS) {
                TRACING_EXIT(ClEnqueueCopyBufferToImage, &retVal);
                return retVal;
            }
        }
        retVal = Image::validateRegionAndOrigin(dstOrigin, region, pDstImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            TRACING_EXIT(ClEnqueueCopyBufferToImage, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueCopyBufferToImage, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueCopyBufferToImage(
            pSrcBuffer,
            pDstImage,
            srcOffset,
            dstOrigin,
            region,
            numEventsInWaitList,
            eventWaitList,
            event);
    }

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueCopyBufferToImage, &retVal);
    return retVal;
}

void *CL_API_CALL clEnqueueMapBuffer(cl_command_queue commandQueue,
                                     cl_mem buffer,
                                     cl_bool blockingMap,
                                     cl_map_flags mapFlags,
                                     size_t offset,
                                     size_t cb,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event,
                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClEnqueueMapBuffer, &commandQueue, &buffer, &blockingMap, &mapFlags, &offset, &cb, &numEventsInWaitList, &eventWaitList, &event, &errcodeRet);
    void *retPtr = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    cl_int retVal;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "buffer", buffer, "blockingMap", blockingMap,
                   "mapFlags", mapFlags, "offset", offset, "cb", cb,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    do {
        auto pCommandQueue = castToObject<CommandQueue>(commandQueue);
        if (!pCommandQueue) {
            retVal = CL_INVALID_COMMAND_QUEUE;
            break;
        }

        auto pBuffer = castToObject<Buffer>(buffer);
        if (!pBuffer) {
            retVal = CL_INVALID_MEM_OBJECT;
            break;
        }

        if (pBuffer->mapMemObjFlagsInvalid(mapFlags)) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        retPtr = pCommandQueue->enqueueMapBuffer(
            pBuffer,
            blockingMap,
            mapFlags,
            offset,
            cb,
            numEventsInWaitList,
            eventWaitList,
            event,
            retVal);

    } while (false);

    err.set(retVal);
    DBG_LOG_INPUTS("retPtr", retPtr, "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));

    TRACING_EXIT(ClEnqueueMapBuffer, &retPtr);
    return retPtr;
}

void *CL_API_CALL clEnqueueMapImage(cl_command_queue commandQueue,
                                    cl_mem image,
                                    cl_bool blockingMap,
                                    cl_map_flags mapFlags,
                                    const size_t *origin,
                                    const size_t *region,
                                    size_t *imageRowPitch,
                                    size_t *imageSlicePitch,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event,
                                    cl_int *errcodeRet) {
    TRACING_ENTER(ClEnqueueMapImage, &commandQueue, &image, &blockingMap, &mapFlags, &origin, &region, &imageRowPitch, &imageSlicePitch, &numEventsInWaitList, &eventWaitList, &event, &errcodeRet);

    void *retPtr = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);
    cl_int retVal;

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue, "image", image,
                   "blockingMap", blockingMap, "mapFlags", mapFlags,
                   "origin[0]", NEO::fileLoggerInstance().getInput(origin, 0), "origin[1]", NEO::fileLoggerInstance().getInput(origin, 1),
                   "origin[2]", NEO::fileLoggerInstance().getInput(origin, 2), "region[0]", NEO::fileLoggerInstance().getInput(region, 0),
                   "region[1]", NEO::fileLoggerInstance().getInput(region, 1), "region[2]", NEO::fileLoggerInstance().getInput(region, 2),
                   "imageRowPitch", NEO::fileLoggerInstance().getInput(imageRowPitch, 0),
                   "imageSlicePitch", NEO::fileLoggerInstance().getInput(imageSlicePitch, 0),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    do {
        Image *pImage = nullptr;
        CommandQueue *pCommandQueue = nullptr;
        retVal = validateObjects(
            withCastToInternal(commandQueue, &pCommandQueue),
            withCastToInternal(image, &pImage));

        if (retVal != CL_SUCCESS) {
            break;
        }

        if (pImage->mapMemObjFlagsInvalid(mapFlags)) {
            retVal = CL_INVALID_OPERATION;
            break;
        }
        if (isPackedYuvImage(&pImage->getImageFormat())) {
            retVal = validateYuvOperation(origin, region);
            if (retVal != CL_SUCCESS) {
                break;
            }
        }

        retVal = Image::validateRegionAndOrigin(origin, region, pImage->getImageDesc());
        if (retVal != CL_SUCCESS) {
            break;
        }

        if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            break;
        }

        retPtr = pCommandQueue->enqueueMapImage(
            pImage,
            blockingMap,
            mapFlags,
            origin,
            region,
            imageRowPitch,
            imageSlicePitch,
            numEventsInWaitList,
            eventWaitList,
            event,
            retVal);

    } while (false);

    err.set(retVal);
    DBG_LOG_INPUTS("retPtr", retPtr, "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));

    TRACING_EXIT(ClEnqueueMapImage, &retPtr);
    return retPtr;
}

cl_int CL_API_CALL clEnqueueUnmapMemObject(cl_command_queue commandQueue,
                                           cl_mem memObj,
                                           void *mappedPtr,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    TRACING_ENTER(ClEnqueueUnmapMemObject, &commandQueue, &memObj, &mappedPtr, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;
    MemObj *pMemObj = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(memObj, &pMemObj));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "memObj", memObj,
                   "mappedPtr", mappedPtr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (retVal == CL_SUCCESS) {
        cl_command_queue_capabilities_intel requiredCapability = 0u;
        switch (pMemObj->peekClMemObjType()) {
        case CL_MEM_OBJECT_BUFFER:
            requiredCapability = CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL;
            break;
        case CL_MEM_OBJECT_IMAGE2D:
        case CL_MEM_OBJECT_IMAGE3D:
        case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        case CL_MEM_OBJECT_IMAGE1D:
        case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        case CL_MEM_OBJECT_IMAGE1D_BUFFER:
            requiredCapability = CL_QUEUE_CAPABILITY_MAP_IMAGE_INTEL;
            break;
        default:
            retVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClEnqueueUnmapMemObject, &retVal);
            return retVal;
        }

        if (!pCommandQueue->validateCapabilityForOperation(requiredCapability, numEventsInWaitList, eventWaitList, event)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueUnmapMemObject, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueUnmapMemObject(pMemObj, mappedPtr, numEventsInWaitList, eventWaitList, event);
    }

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueUnmapMemObject, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueMigrateMemObjects(cl_command_queue commandQueue,
                                              cl_uint numMemObjects,
                                              const cl_mem *memObjects,
                                              cl_mem_migration_flags flags,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    TRACING_ENTER(ClEnqueueMigrateMemObjects, &commandQueue, &numMemObjects, &memObjects, &flags, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numMemObjects", numMemObjects,
                   "memObjects", memObjects,
                   "flags", flags,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
        return retVal;
    }

    if (numMemObjects == 0 || memObjects == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
        return retVal;
    }

    for (unsigned int object = 0; object < numMemObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (!memObject) {
            retVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
            return retVal;
        }
    }

    const cl_mem_migration_flags allValidFlags = CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | CL_MIGRATE_MEM_OBJECT_HOST;

    if ((flags & (~allValidFlags)) != 0) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueMigrateMemObjects(numMemObjects,
                                                     memObjects,
                                                     flags,
                                                     numEventsInWaitList,
                                                     eventWaitList,
                                                     event);
    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueNDRangeKernel(cl_command_queue commandQueue,
                                          cl_kernel kernel,
                                          cl_uint workDim,
                                          const size_t *globalWorkOffset,
                                          const size_t *globalWorkSize,
                                          const size_t *localWorkSize,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
    TRACING_ENTER(ClEnqueueNdRangeKernel, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &localWorkSize, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "cl_kernel", kernel,
                   "globalWorkOffset[0]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 0),
                   "globalWorkOffset[1]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 1),
                   "globalWorkOffset[2]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 2),
                   "globalWorkSize", NEO::fileLoggerInstance().getSizes(globalWorkSize, workDim, false),
                   "localWorkSize", NEO::fileLoggerInstance().getSizes(localWorkSize, workDim, true),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(kernel, &pMultiDeviceKernel),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
        return retVal;
    }

    Kernel *pKernel = pMultiDeviceKernel->getKernel(pCommandQueue->getDevice().getRootDeviceIndex());

    auto localMemSize = static_cast<uint32_t>(pCommandQueue->getDevice().getDeviceInfo().localMemSize);
    auto slmTotalSize = pKernel->getSlmTotalSize();

    if (slmTotalSize > 0 && localMemSize < slmTotalSize) {
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Size of SLM (%u) larger than available (%u)\n", slmTotalSize, localMemSize);
        retVal = CL_OUT_OF_RESOURCES;
        TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
        return retVal;
    }

    if ((pKernel->getExecutionType() != KernelExecutionType::defaultType) ||
        pKernel->usesSyncBuffer()) {
        retVal = CL_INVALID_KERNEL;
        TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_KERNEL_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
        return retVal;
    }

    TakeOwnershipWrapper<MultiDeviceKernel> kernelOwnership(*pMultiDeviceKernel, gtpinIsGTPinInitialized());
    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyKernelSubmit(kernel, pCommandQueue);
    }

    retVal = pCommandQueue->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueTask(cl_command_queue commandQueue,
                                 cl_kernel kernel,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) {
    TRACING_ENTER(ClEnqueueTask, &commandQueue, &kernel, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "kernel", kernel,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));
    cl_uint workDim = 3;
    size_t *globalWorkOffset = nullptr;
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = (clEnqueueNDRangeKernel(
        commandQueue,
        kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event));
    TRACING_EXIT(ClEnqueueTask, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueNativeKernel(cl_command_queue commandQueue,
                                         void(CL_CALLBACK *userFunc)(void *),
                                         void *args,
                                         size_t cbArgs,
                                         cl_uint numMemObjects,
                                         const cl_mem *memList,
                                         const void **argsMemLoc,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {
    TRACING_ENTER(ClEnqueueNativeKernel, &commandQueue, &userFunc, &args, &cbArgs, &numMemObjects, &memList, &argsMemLoc, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "userFunc", reinterpret_cast<void *>(userFunc), "args", args,
                   "cbArgs", cbArgs, "numMemObjects", numMemObjects, "memList", memList, "argsMemLoc", argsMemLoc,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    TRACING_EXIT(ClEnqueueNativeKernel, &retVal);
    return retVal;
}

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueMarker(cl_command_queue commandQueue,
                                   cl_event *event) {
    TRACING_ENTER(ClEnqueueMarker, &commandQueue, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "cl_event", event);

    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);
    if (pCommandQueue) {
        if (!pCommandQueue->validateCapability(CL_QUEUE_CAPABILITY_MARKER_INTEL)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueMarker, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueMarkerWithWaitList(
            0,
            nullptr,
            event);
        TRACING_EXIT(ClEnqueueMarker, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_COMMAND_QUEUE;
    TRACING_EXIT(ClEnqueueMarker, &retVal);
    return retVal;
}

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue commandQueue,
                                          cl_uint numEvents,
                                          const cl_event *eventList) {
    TRACING_ENTER(ClEnqueueWaitForEvents, &commandQueue, &numEvents, &eventList);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "eventList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventList), numEvents));

    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);
    if (!pCommandQueue) {
        retVal = CL_INVALID_COMMAND_QUEUE;
        TRACING_EXIT(ClEnqueueWaitForEvents, &retVal);
        return retVal;
    }
    for (unsigned int i = 0; i < numEvents && retVal == CL_SUCCESS; i++) {
        retVal = validateObjects(eventList[i]);
    }

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueWaitForEvents, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilitiesForEventWaitList(numEvents, eventList)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWaitForEvents, &retVal);
        return retVal;
    }

    retVal = Event::waitForEvents(numEvents, eventList);

    TRACING_EXIT(ClEnqueueWaitForEvents, &retVal);
    return retVal;
}

// deprecated OpenCL 1.1
cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue commandQueue) {
    TRACING_ENTER(ClEnqueueBarrier, &commandQueue);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue);
    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);
    if (pCommandQueue) {
        if (!pCommandQueue->validateCapability(CL_QUEUE_CAPABILITY_BARRIER_INTEL)) {
            retVal = CL_INVALID_OPERATION;
            TRACING_EXIT(ClEnqueueBarrier, &retVal);
            return retVal;
        }

        retVal = pCommandQueue->enqueueBarrierWithWaitList(
            0,
            nullptr,
            nullptr);
        TRACING_EXIT(ClEnqueueBarrier, &retVal);
        return retVal;
    }
    retVal = CL_INVALID_COMMAND_QUEUE;
    TRACING_EXIT(ClEnqueueBarrier, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue commandQueue,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    TRACING_ENTER(ClEnqueueMarkerWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("cl_command_queue", commandQueue,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueMarkerWithWaitList, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_MARKER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueMarkerWithWaitList, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    TRACING_EXIT(ClEnqueueMarkerWithWaitList, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue commandQueue,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {
    TRACING_ENTER(ClEnqueueBarrierWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("cl_command_queue", commandQueue,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueBarrierWithWaitList, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_BARRIER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueBarrierWithWaitList, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueBarrierWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        event);
    TRACING_EXIT(ClEnqueueBarrierWithWaitList, &retVal);
    return retVal;
}

CL_API_ENTRY cl_command_queue CL_API_CALL
clCreatePerfCountersCommandQueueINTEL(
    cl_context context,
    cl_device_id device,
    cl_command_queue_properties properties,
    cl_uint configuration,
    cl_int *errcodeRet) {

    TRACING_ENTER(ClCreatePerfCountersCommandQueueINTEL, &context, &device, &properties, &configuration, &errcodeRet);
    API_ENTER(nullptr);

    DBG_LOG_INPUTS("context", context,
                   "device", device,
                   "properties", properties,
                   "configuration", configuration);

    cl_command_queue commandQueue = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    ClDevice *pDevice = nullptr;
    withCastToInternal(device, &pDevice);
    if (pDevice == nullptr) {
        err.set(CL_INVALID_DEVICE);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }

    if (!pDevice->getHardwareInfo().capabilityTable.instrumentationEnabled) {
        err.set(CL_INVALID_DEVICE);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }

    if ((properties & CL_QUEUE_PROFILING_ENABLE) == 0) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }
    if ((properties & CL_QUEUE_ON_DEVICE) != 0) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }
    if ((properties & CL_QUEUE_ON_DEVICE_DEFAULT) != 0) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }

    if (configuration != 0) {
        err.set(CL_INVALID_OPERATION);
        TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
        return commandQueue;
    }

    commandQueue = clCreateCommandQueue(context, device, properties, errcodeRet);
    if (commandQueue != nullptr) {
        auto commandQueueObject = castToObjectOrAbort<CommandQueue>(commandQueue);

        if (!commandQueueObject->setPerfCountersEnabled()) {
            clReleaseCommandQueue(commandQueue);
            commandQueue = nullptr;
            err.set(CL_OUT_OF_RESOURCES);
        }
    }
    TRACING_EXIT(ClCreatePerfCountersCommandQueueINTEL, &commandQueue);
    return commandQueue;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetPerformanceConfigurationINTEL(
    cl_device_id device,
    cl_uint count,
    cl_uint *offsets,
    cl_uint *values) {
    // Not supported, covered by Metric Library DLL.
    return CL_INVALID_OPERATION;
}

CL_API_ENTRY void *CL_API_CALL clHostMemAllocINTEL(
    cl_context context,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {

    TRACING_ENTER(ClHostMemAllocINTEL, &context, &properties, &size, &alignment, &errcodeRet);
    if (debugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    Context *neoContext = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto retVal = validateObjects(withCastToInternal(context, &neoContext));

    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        TRACING_EXIT(ClHostMemAllocINTEL, nullptr);
        return nullptr;
    }

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, alignment,
                                                                      neoContext->getRootDeviceIndices(), neoContext->getDeviceBitfields());
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if (!ClMemoryPropertiesHelper::parseMemoryProperties(properties, unifiedMemoryProperties.allocationFlags, flags, flagsIntel,
                                                         allocflags, ClMemoryPropertiesHelper::ObjType::unknown,
                                                         *neoContext)) {
        err.set(CL_INVALID_VALUE);
        TRACING_EXIT(ClHostMemAllocINTEL, nullptr);
        return nullptr;
    }

    if (size == 0 || (size > neoContext->getDevice(0u)->getSharedDeviceInfo().maxMemAllocSize &&
                      !unifiedMemoryProperties.allocationFlags.flags.allowUnrestrictedSize)) {
        err.set(CL_INVALID_BUFFER_SIZE);
        TRACING_EXIT(ClHostMemAllocINTEL, nullptr);
        return nullptr;
    }

    auto platform = neoContext->getDevice(0u)->getPlatform();
    platform->initializeHostUsmAllocationPool();

    auto allocationFromPool = platform->getHostMemAllocPool().createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    if (allocationFromPool) {
        TRACING_EXIT(ClHostMemAllocINTEL, &allocationFromPool);
        return allocationFromPool;
    }

    auto ptr = neoContext->getSVMAllocsManager()->createHostUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    TRACING_EXIT(ClHostMemAllocINTEL, &ptr);
    return ptr;
}

CL_API_ENTRY void *CL_API_CALL clDeviceMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {

    TRACING_ENTER(ClDeviceMemAllocINTEL, &context, &device, &properties, &size, &alignment, &errcodeRet);
    if (debugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    Context *neoContext = nullptr;
    ClDevice *neoDevice = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto retVal = validateObjects(withCastToInternal(context, &neoContext), withCastToInternal(device, &neoDevice));

    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        TRACING_EXIT(ClDeviceMemAllocINTEL, nullptr);
        return nullptr;
    }

    auto subDeviceBitfields = neoContext->getDeviceBitfields();
    subDeviceBitfields[neoDevice->getRootDeviceIndex()] = neoDevice->getDeviceBitfield();

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, alignment,
                                                                      neoContext->getRootDeviceIndices(), subDeviceBitfields);
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    if (!ClMemoryPropertiesHelper::parseMemoryProperties(properties, unifiedMemoryProperties.allocationFlags, flags, flagsIntel,
                                                         allocflags, ClMemoryPropertiesHelper::ObjType::unknown,
                                                         *neoContext)) {
        err.set(CL_INVALID_VALUE);
        TRACING_EXIT(ClDeviceMemAllocINTEL, nullptr);
        return nullptr;
    }

    if (size == 0 ||
        (size > neoDevice->getDevice().getDeviceInfo().maxMemAllocSize &&
         !unifiedMemoryProperties.allocationFlags.flags.allowUnrestrictedSize)) {
        err.set(CL_INVALID_BUFFER_SIZE);
        TRACING_EXIT(ClDeviceMemAllocINTEL, nullptr);
        return nullptr;
    }

    unifiedMemoryProperties.device = &neoDevice->getDevice();

    neoContext->initializeDeviceUsmAllocationPool();

    auto allocationFromPool = neoContext->getDeviceMemAllocPool().createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    if (allocationFromPool) {
        TRACING_EXIT(ClDeviceMemAllocINTEL, &allocationFromPool);
        return allocationFromPool;
    }

    auto ptr = neoContext->getSVMAllocsManager()->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    TRACING_EXIT(ClDeviceMemAllocINTEL, &ptr);
    return ptr;
}

CL_API_ENTRY void *CL_API_CALL clSharedMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel *properties,
    size_t size,
    cl_uint alignment,
    cl_int *errcodeRet) {

    TRACING_ENTER(ClSharedMemAllocINTEL, &context, &device, &properties, &size, &alignment, &errcodeRet);
    if (debugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * debugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    Context *neoContext = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    auto retVal = validateObjects(withCastToInternal(context, &neoContext));

    if (retVal != CL_SUCCESS) {
        err.set(retVal);
        TRACING_EXIT(ClSharedMemAllocINTEL, nullptr);
        return nullptr;
    }

    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    ClDevice *neoDevice = castToObject<ClDevice>(device);
    Device *unifiedMemoryPropertiesDevice = nullptr;
    auto subDeviceBitfields = neoContext->getDeviceBitfields();
    if (neoDevice) {
        if (!neoContext->isDeviceAssociated(*neoDevice)) {
            err.set(CL_INVALID_DEVICE);
            TRACING_EXIT(ClSharedMemAllocINTEL, nullptr);
            return nullptr;
        }
        unifiedMemoryPropertiesDevice = &neoDevice->getDevice();
        subDeviceBitfields[neoDevice->getRootDeviceIndex()] = neoDevice->getDeviceBitfield();
    } else {
        neoDevice = neoContext->getDevice(0);
    }
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, alignment, neoContext->getRootDeviceIndices(), subDeviceBitfields);
    unifiedMemoryProperties.device = unifiedMemoryPropertiesDevice;
    if (!ClMemoryPropertiesHelper::parseMemoryProperties(properties, unifiedMemoryProperties.allocationFlags, flags, flagsIntel,
                                                         allocflags, ClMemoryPropertiesHelper::ObjType::unknown,
                                                         *neoContext)) {
        err.set(CL_INVALID_VALUE);
        TRACING_EXIT(ClSharedMemAllocINTEL, nullptr);
        return nullptr;
    }

    if (size == 0 ||
        (size > neoDevice->getSharedDeviceInfo().maxMemAllocSize &&
         !unifiedMemoryProperties.allocationFlags.flags.allowUnrestrictedSize)) {
        err.set(CL_INVALID_BUFFER_SIZE);
        TRACING_EXIT(ClSharedMemAllocINTEL, nullptr);
        return nullptr;
    }
    auto ptr = neoContext->getSVMAllocsManager()->createSharedUnifiedMemoryAllocation(size, unifiedMemoryProperties, neoContext->getSpecialQueue(neoDevice->getRootDeviceIndex()));
    if (!ptr) {
        err.set(CL_OUT_OF_RESOURCES);
    }
    TRACING_EXIT(ClSharedMemAllocINTEL, &ptr);
    return ptr;
}

CL_API_ENTRY cl_int CL_API_CALL clMemFreeCommon(cl_context context,
                                                const void *ptr,
                                                bool blocking) {
    Context *neoContext = nullptr;
    auto retVal = validateObjects(withCastToInternal(context, &neoContext));

    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    bool successfulFree = false;

    if (ptr && neoContext->getDeviceMemAllocPool().freeSVMAlloc(const_cast<void *>(ptr), blocking)) {
        successfulFree = true;
    }

    if (!successfulFree && ptr && neoContext->getDevice(0u)->getPlatform()->getHostMemAllocPool().freeSVMAlloc(const_cast<void *>(ptr), blocking)) {
        successfulFree = true;
    }

    if (!successfulFree) {
        if (ptr && !neoContext->getSVMAllocsManager()->freeSVMAlloc(const_cast<void *>(ptr), blocking)) {
            return CL_INVALID_VALUE;
        }

        if (neoContext->getSVMAllocsManager()->getSvmMapOperation(ptr)) {
            neoContext->getSVMAllocsManager()->removeSvmMapOperation(ptr);
        }
    }

    if (blocking) {
        neoContext->getMemoryManager()->cleanTemporaryAllocationListOnAllEngines(false);
    }

    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clMemFreeINTEL(
    cl_context context,
    void *ptr) {
    TRACING_ENTER(ClMemFreeINTEL, &context, &ptr);
    auto retVal = clMemFreeCommon(context,
                                  ptr,
                                  false);
    TRACING_EXIT(ClMemFreeINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clMemBlockingFreeINTEL(
    cl_context context,
    void *ptr) {
    TRACING_ENTER(ClMemBlockingFreeINTEL, &context, &ptr);
    auto retVal = clMemFreeCommon(context,
                                  ptr,
                                  true);
    TRACING_EXIT(ClMemBlockingFreeINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clGetMemAllocInfoINTEL(
    cl_context context,
    const void *ptr,
    cl_mem_info_intel paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {

    TRACING_ENTER(ClGetMemAllocInfoINTEL, &context, &ptr, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    Context *pContext = nullptr;
    cl_int retVal = CL_SUCCESS;
    retVal = validateObject(withCastToInternal(context, &pContext));
    if (!pContext) {
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }

    auto allocationsManager = pContext->getSVMAllocsManager();
    if (!allocationsManager) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }

    GetInfoHelper info(paramValue, paramValueSize, paramValueSizeRet);
    auto unifiedMemoryAllocation = allocationsManager->getSVMAlloc(ptr);

    switch (paramName) {
    case CL_MEM_ALLOC_TYPE_INTEL: {
        if (!unifiedMemoryAllocation) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(CL_MEM_TYPE_UNKNOWN_INTEL));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        } else if (unifiedMemoryAllocation->memoryType == InternalMemoryType::hostUnifiedMemory) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(CL_MEM_TYPE_HOST_INTEL));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        } else if (unifiedMemoryAllocation->memoryType == InternalMemoryType::deviceUnifiedMemory) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(CL_MEM_TYPE_DEVICE_INTEL));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        } else {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_int>(CL_MEM_TYPE_SHARED_INTEL));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        break;
    }
    case CL_MEM_ALLOC_BASE_PTR_INTEL: {
        if (!unifiedMemoryAllocation) {
            retVal = changeGetInfoStatusToCLResultType(info.set<void *>(nullptr));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        if (auto basePtrFromDevicePool = pContext->getDeviceMemAllocPool().getPooledAllocationBasePtr(ptr)) {
            retVal = changeGetInfoStatusToCLResultType(info.set<uint64_t>(castToUint64(basePtrFromDevicePool)));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        if (auto basePtrFromHostPool = pContext->getDevice(0u)->getPlatform()->getHostMemAllocPool().getPooledAllocationBasePtr(ptr)) {
            retVal = changeGetInfoStatusToCLResultType(info.set<uint64_t>(castToUint64(basePtrFromHostPool)));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        retVal = changeGetInfoStatusToCLResultType(info.set<uint64_t>(unifiedMemoryAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()));
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }
    case CL_MEM_ALLOC_SIZE_INTEL: {
        if (!unifiedMemoryAllocation) {
            retVal = changeGetInfoStatusToCLResultType(info.set<size_t>(0u));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        if (auto sizeFromDevicePool = pContext->getDeviceMemAllocPool().getPooledAllocationSize(ptr)) {
            retVal = changeGetInfoStatusToCLResultType(info.set<size_t>(sizeFromDevicePool));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        if (auto sizeFromHostPool = pContext->getDevice(0u)->getPlatform()->getHostMemAllocPool().getPooledAllocationSize(ptr)) {
            retVal = changeGetInfoStatusToCLResultType(info.set<size_t>(sizeFromHostPool));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        retVal = changeGetInfoStatusToCLResultType(info.set<size_t>(unifiedMemoryAllocation->size));
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }
    case CL_MEM_ALLOC_FLAGS_INTEL: {
        if (!unifiedMemoryAllocation) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_mem_alloc_flags_intel>(0u));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_mem_alloc_flags_intel>(unifiedMemoryAllocation->allocationFlagsProperty.allAllocFlags));
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }
    case CL_MEM_ALLOC_DEVICE_INTEL: {
        if (!unifiedMemoryAllocation) {
            retVal = changeGetInfoStatusToCLResultType(info.set<cl_device_id>(static_cast<cl_device_id>(nullptr)));
            TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
            return retVal;
        }
        auto device = unifiedMemoryAllocation->device ? unifiedMemoryAllocation->device->getSpecializedDevice<ClDevice>() : nullptr;
        retVal = changeGetInfoStatusToCLResultType(info.set<cl_device_id>(device));
        TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
        return retVal;
    }

    default: {
    }
    }

    retVal = CL_INVALID_VALUE;
    TRACING_EXIT(ClGetMemAllocInfoINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clSetKernelArgMemPointerINTEL(
    cl_kernel kernel,
    cl_uint argIndex,
    const void *argValue) {
    TRACING_ENTER(ClSetKernelArgMemPointerINTEL, &kernel, &argIndex, &argValue);
    auto retVal = clSetKernelArgSVMPointer(kernel,
                                           argIndex,
                                           argValue);
    TRACING_EXIT(ClSetKernelArgMemPointerINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemsetINTEL(
    cl_command_queue commandQueue,
    void *dstPtr,
    cl_int value,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueMemsetINTEL, &commandQueue, &dstPtr, &value, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto retVal = clEnqueueSVMMemFill(commandQueue,
                                      dstPtr,
                                      &value,
                                      1u,
                                      size,
                                      numEventsInWaitList,
                                      eventWaitList,
                                      event);
    if (retVal == CL_SUCCESS && event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(CL_COMMAND_MEMSET_INTEL);
    }
    TRACING_EXIT(ClEnqueueMemsetINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemFillINTEL(
    cl_command_queue commandQueue,
    void *dstPtr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueMemFillINTEL, &commandQueue, &dstPtr, &pattern, &patternSize, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto retVal = clEnqueueSVMMemFill(commandQueue,
                                      dstPtr,
                                      pattern,
                                      patternSize,
                                      size,
                                      numEventsInWaitList,
                                      eventWaitList,
                                      event);
    if (retVal == CL_SUCCESS && event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(CL_COMMAND_MEMFILL_INTEL);
    }
    TRACING_EXIT(ClEnqueueMemFillINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemcpyINTEL(
    cl_command_queue commandQueue,
    cl_bool blocking,
    void *dstPtr,
    const void *srcPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueMemcpyINTEL, &commandQueue, &blocking, &dstPtr, &srcPtr, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto retVal = clEnqueueSVMMemcpy(commandQueue,
                                     blocking,
                                     dstPtr,
                                     srcPtr,
                                     size,
                                     numEventsInWaitList,
                                     eventWaitList,
                                     event);
    if (retVal == CL_SUCCESS && event) {
        auto pEvent = castToObjectOrAbort<Event>(*event);
        pEvent->setCmdType(CL_COMMAND_MEMCPY_INTEL);
    }
    TRACING_EXIT(ClEnqueueMemcpyINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMigrateMemINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueMigrateMemINTEL, &commandQueue, &ptr, &size, &flags, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), ptr, EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        pCommandQueue->enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);

        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_MIGRATEMEM_INTEL);
        }

        if (NEO::debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.get() == true) {
            auto pSvmAllocMgr = pCommandQueue->getContext().getSVMAllocsManager();
            UNRECOVERABLE_IF(pSvmAllocMgr == nullptr);

            auto allocData = pSvmAllocMgr->getSVMAlloc(ptr);
            if (allocData) {
                pSvmAllocMgr->prefetchMemory(pCommandQueue->getDevice(), pCommandQueue->getGpgpuCommandStreamReceiver(), ptr, size);
            }
        }
    }
    TRACING_EXIT(ClEnqueueMigrateMemINTEL, &retVal);
    return retVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemAdviseINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_advice_intel advice,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueMemAdviseINTEL, &commandQueue, &ptr, &size, &advice, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), ptr, EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal == CL_SUCCESS) {
        pCommandQueue->enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);

        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_MEMADVISE_INTEL);
        }
    }
    TRACING_EXIT(ClEnqueueMemAdviseINTEL, &retVal);
    return retVal;
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithPropertiesKHR(cl_context context,
                                                                   cl_device_id device,
                                                                   const cl_queue_properties_khr *properties,
                                                                   cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateCommandQueueWithPropertiesKHR, &context, &device, &properties, &errcodeRet);
    API_ENTER(errcodeRet);
    DBG_LOG_INPUTS("context", context,
                   "device", device,
                   "properties", properties);
    auto retVal = clCreateCommandQueueWithProperties(context,
                                                     device,
                                                     properties,
                                                     errcodeRet);
    TRACING_EXIT(ClCreateCommandQueueWithPropertiesKHR, &retVal);
    return retVal;
}

cl_accelerator_intel CL_API_CALL clCreateAcceleratorINTEL(
    cl_context context,
    cl_accelerator_type_intel acceleratorType,
    size_t descriptorSize,
    const void *descriptor,
    cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateAcceleratorINTEL, &context, &acceleratorType, &descriptorSize, &descriptor, &errcodeRet);
    cl_int retVal = CL_INVALID_ACCELERATOR_TYPE_INTEL;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "acceleratorType", acceleratorType,
                   "descriptorSize", descriptorSize,
                   "descriptor", NEO::fileLoggerInstance().infoPointerToString(descriptor, descriptorSize));
    cl_accelerator_intel accelerator = nullptr;

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateAcceleratorINTEL, &accelerator);
    return accelerator;
}

cl_int CL_API_CALL clRetainAcceleratorINTEL(
    cl_accelerator_intel accelerator) {

    TRACING_ENTER(ClRetainAcceleratorINTEL, &accelerator);
    cl_int retVal = CL_INVALID_ACCELERATOR_INTEL;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("accelerator", accelerator);

    TRACING_EXIT(ClRetainAcceleratorINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetAcceleratorInfoINTEL(
    cl_accelerator_intel accelerator,
    cl_accelerator_info_intel paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {

    TRACING_ENTER(ClGetAcceleratorInfoINTEL, &accelerator, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_INVALID_ACCELERATOR_INTEL;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("accelerator", accelerator,
                   "paramName", paramName,
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    TRACING_EXIT(ClGetAcceleratorInfoINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clReleaseAcceleratorINTEL(
    cl_accelerator_intel accelerator) {

    TRACING_ENTER(ClReleaseAcceleratorINTEL, &accelerator);
    cl_int retVal = CL_INVALID_ACCELERATOR_INTEL;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("accelerator", accelerator);

    TRACING_EXIT(ClReleaseAcceleratorINTEL, &retVal);
    return retVal;
}

cl_program CL_API_CALL clCreateProgramWithILKHR(cl_context context,
                                                const void *il,
                                                size_t length,
                                                cl_int *errcodeRet) {

    TRACING_ENTER(ClCreateProgramWithILKHR, &context, &il, &length, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "il", NEO::fileLoggerInstance().infoPointerToString(il, length),
                   "length", length);

    cl_program program = nullptr;
    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext), il);
    if (retVal == CL_SUCCESS) {
        program = ProgramFunctions::createFromIL(
            pContext,
            il,
            length,
            retVal);
    }

    if (errcodeRet != nullptr) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateProgramWithILKHR, &program);
    return program;
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeKHR(cl_command_queue commandQueue,
                                                        cl_kernel kernel,
                                                        cl_uint workDim,
                                                        const size_t *globalWorkOffset,
                                                        const size_t *globalWorkSize,
                                                        size_t *suggestedLocalWorkSize) {

    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSizeKHR, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    auto retVal = clGetKernelSuggestedLocalWorkSizeINTEL(commandQueue,
                                                         kernel,
                                                         workDim,
                                                         globalWorkOffset,
                                                         globalWorkSize,
                                                         suggestedLocalWorkSize);
    TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeKHR, &retVal);
    return retVal;
}

#define RETURN_FUNC_PTR_IF_EXIST(name)                                                    \
    {                                                                                     \
        if (!strcmp(funcName, #name)) {                                                   \
            void *ret = reinterpret_cast<void *>(name);                                   \
            TRACING_EXIT(ClGetExtensionFunctionAddress, reinterpret_cast<void **>(&ret)); \
            return ret;                                                                   \
        }                                                                                 \
    }
void *CL_API_CALL clGetExtensionFunctionAddress(const char *funcName) {
    TRACING_ENTER(ClGetExtensionFunctionAddress, &funcName);

    DBG_LOG_INPUTS("funcName", funcName);
    // Support an internal call by the ICD
    RETURN_FUNC_PTR_IF_EXIST(clIcdGetPlatformIDsKHR);

    // perf counters
    RETURN_FUNC_PTR_IF_EXIST(clCreatePerfCountersCommandQueueINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetPerformanceConfigurationINTEL);
    // Support device extensions
    RETURN_FUNC_PTR_IF_EXIST(clCreateAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetAcceleratorInfoINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clRetainAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clReleaseAcceleratorINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clCreateBufferWithPropertiesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clCreateImageWithPropertiesINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clAddCommentINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueVerifyMemoryINTEL);

    RETURN_FUNC_PTR_IF_EXIST(clCreateTracingHandleINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetTracingPointINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDestroyTracingHandleINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnableTracingINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDisableTracingINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetTracingStateINTEL);

    RETURN_FUNC_PTR_IF_EXIST(clHostMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clDeviceMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSharedMemAllocINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clMemFreeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clMemBlockingFreeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetMemAllocInfoINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clSetKernelArgMemPointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemsetINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemFillINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemcpyINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMigrateMemINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueMemAdviseINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceFunctionPointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetDeviceGlobalVariablePointerINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelMaxConcurrentWorkGroupCountINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSizeINTEL);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueNDCountKernelINTEL);

    RETURN_FUNC_PTR_IF_EXIST(clEnqueueAcquireExternalMemObjectsKHR);
    RETURN_FUNC_PTR_IF_EXIST(clEnqueueReleaseExternalMemObjectsKHR);

    void *ret = sharingFactory.getExtensionFunctionAddress(funcName);
    if (ret != nullptr) {
        TRACING_EXIT(ClGetExtensionFunctionAddress, &ret);
        return ret;
    }

    // SPIR-V support through the cl_khr_il_program extension
    RETURN_FUNC_PTR_IF_EXIST(clCreateProgramWithILKHR);
    RETURN_FUNC_PTR_IF_EXIST(clCreateCommandQueueWithPropertiesKHR);

    RETURN_FUNC_PTR_IF_EXIST(clSetProgramSpecializationConstant);

    RETURN_FUNC_PTR_IF_EXIST(clGetKernelSuggestedLocalWorkSizeKHR);

    ret = getAdditionalExtensionFunctionAddress(funcName);
    TRACING_EXIT(ClGetExtensionFunctionAddress, &ret);
    return ret;
}

// OpenCL 1.2
void *CL_API_CALL clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                                           const char *funcName) {
    TRACING_ENTER(ClGetExtensionFunctionAddressForPlatform, &platform, &funcName);
    DBG_LOG_INPUTS("platform", platform, "funcName", funcName);
    auto pPlatform = castToObject<Platform>(platform);

    if (pPlatform == nullptr) {
        void *ret = nullptr;
        TRACING_EXIT(ClGetExtensionFunctionAddressForPlatform, &ret);
        return ret;
    }

    void *ret = clGetExtensionFunctionAddress(funcName);
    TRACING_EXIT(ClGetExtensionFunctionAddressForPlatform, &ret);
    return ret;
}

void *CL_API_CALL clSVMAlloc(cl_context context,
                             cl_svm_mem_flags flags,
                             size_t size,
                             cl_uint alignment) {
    TRACING_ENTER(ClSvmAlloc, &context, &flags, &size, &alignment);
    DBG_LOG_INPUTS("context", context,
                   "flags", flags,
                   "size", size,
                   "alignment", alignment);
    void *pAlloc = nullptr;
    Context *pContext = nullptr;

    if (validateObjects(withCastToInternal(context, &pContext)) != CL_SUCCESS) {
        TRACING_EXIT(ClSvmAlloc, &pAlloc);
        return pAlloc;
    }

    {
        // allow CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL with every combination
        cl_svm_mem_flags tempFlags = flags & (~CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL);

        if (tempFlags == 0) {
            tempFlags = CL_MEM_READ_WRITE;
        }

        if (!((tempFlags == CL_MEM_READ_WRITE) ||
              (tempFlags == CL_MEM_WRITE_ONLY) ||
              (tempFlags == CL_MEM_READ_ONLY) ||
              (tempFlags == CL_MEM_SVM_FINE_GRAIN_BUFFER) ||
              (tempFlags == (CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS)) ||
              (tempFlags == (CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER)) ||
              (tempFlags == (CL_MEM_READ_WRITE | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS)) ||
              (tempFlags == (CL_MEM_WRITE_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER)) ||
              (tempFlags == (CL_MEM_WRITE_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS)) ||
              (tempFlags == (CL_MEM_READ_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER)) ||
              (tempFlags == (CL_MEM_READ_ONLY | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS)))) {

            TRACING_EXIT(ClSvmAlloc, &pAlloc);
            return pAlloc;
        }
    }

    auto pDevice = pContext->getDevice(0);
    bool allowUnrestrictedSize = (flags & CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL) || debugManager.flags.AllowUnrestrictedSize.get();

    if ((size == 0) ||
        (!allowUnrestrictedSize && (size > pDevice->getSharedDeviceInfo().maxMemAllocSize))) {
        TRACING_EXIT(ClSvmAlloc, &pAlloc);
        return pAlloc;
    }

    if ((alignment && (alignment & (alignment - 1))) || (alignment > sizeof(cl_ulong16))) {
        TRACING_EXIT(ClSvmAlloc, &pAlloc);
        return pAlloc;
    }

    const HardwareInfo &hwInfo = pDevice->getHardwareInfo();

    if (flags & CL_MEM_SVM_FINE_GRAIN_BUFFER) {
        bool supportsFineGrained = hwInfo.capabilityTable.ftrSupportsCoherency;
        if (debugManager.flags.ForceFineGrainedSVMSupport.get() != -1) {
            supportsFineGrained = !!debugManager.flags.ForceFineGrainedSVMSupport.get();
        }
        if (!supportsFineGrained) {
            TRACING_EXIT(ClSvmAlloc, &pAlloc);
            return pAlloc;
        }
    }

    pAlloc = pContext->getSVMAllocsManager()->createSVMAlloc(size, MemObjHelper::getSvmAllocationProperties(flags), pContext->getRootDeviceIndices(), pContext->getDeviceBitfields());

    if (pContext->isProvidingPerformanceHints()) {
        pContext->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL, CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS, pAlloc, size);
    }
    TRACING_EXIT(ClSvmAlloc, &pAlloc);
    return pAlloc;
}

void CL_API_CALL clSVMFree(cl_context context,
                           void *svmPointer) {
    TRACING_ENTER(ClSvmFree, &context, &svmPointer);
    DBG_LOG_INPUTS("context", context,
                   "svmPointer", svmPointer);

    Context *pContext = nullptr;
    cl_int retVal = validateObjects(
        withCastToInternal(context, &pContext));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClSvmFree, nullptr);
        return;
    }

    pContext->getSVMAllocsManager()->freeSVMAlloc(svmPointer);
    TRACING_EXIT(ClSvmFree, nullptr);
}

cl_int CL_API_CALL clEnqueueSVMFree(cl_command_queue commandQueue,
                                    cl_uint numSvmPointers,
                                    void *svmPointers[],
                                    void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                                   cl_uint numSvmPointers,
                                                                   void *svmPointers[],
                                                                   void *userData),
                                    void *userData,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmFree, &commandQueue, &numSvmPointers, &svmPointers, &pfnFreeFunc, &userData, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numSvmPointers", numSvmPointers,
                   "svmPointers", svmPointers,
                   "pfnFreeFunc", reinterpret_cast<void *>(pfnFreeFunc),
                   "userData", userData,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueSvmFree, &retVal);
        return retVal;
    }

    if (((svmPointers != nullptr) && (numSvmPointers == 0)) ||
        ((svmPointers == nullptr) && (numSvmPointers != 0))) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmFree, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueSVMFree(
        numSvmPointers,
        svmPointers,
        pfnFreeFunc,
        userData,
        numEventsInWaitList,
        eventWaitList,
        event);

    TRACING_EXIT(ClEnqueueSvmFree, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueSVMMemcpy(cl_command_queue commandQueue,
                                      cl_bool blockingCopy,
                                      void *dstPtr,
                                      const void *srcPtr,
                                      size_t size,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMemcpy, &commandQueue, &blockingCopy, &dstPtr, &srcPtr, &size, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "blockingCopy", blockingCopy,
                   "dstPtr", dstPtr,
                   "srcPtr", srcPtr,
                   "size", size,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueSvmMemcpy, &retVal);
        return retVal;
    }

    auto &device = pCommandQueue->getDevice();

    if ((dstPtr == nullptr) || (srcPtr == nullptr)) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmMemcpy, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_TRANSFER_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueSvmMemcpy, &retVal);
        return retVal;
    }

    if (size != 0) {
        if (pCommandQueue->isValidForStagingBufferCopy(device, dstPtr, srcPtr, size, numEventsInWaitList > 0)) {
            retVal = pCommandQueue->enqueueStagingBufferMemcpy(blockingCopy, dstPtr, srcPtr, size, event);
        } else {
            retVal = pCommandQueue->enqueueSVMMemcpy(
                blockingCopy,
                dstPtr,
                srcPtr,
                size,
                numEventsInWaitList,
                eventWaitList,
                event,
                nullptr);
        }
    } else {
        retVal = pCommandQueue->enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);

        if (event) {
            auto pEvent = castToObjectOrAbort<Event>(*event);
            pEvent->setCmdType(CL_COMMAND_SVM_MEMCPY);
        }
    }
    TRACING_EXIT(ClEnqueueSvmMemcpy, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueSVMMemFill(cl_command_queue commandQueue,
                                       void *svmPtr,
                                       const void *pattern,
                                       size_t patternSize,
                                       size_t size,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMemFill, &commandQueue, &svmPtr, &pattern, &patternSize, &size, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "svmPtr", svmPtr,
                   "pattern", NEO::fileLoggerInstance().infoPointerToString(pattern, patternSize),
                   "patternSize", patternSize,
                   "size", size,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueSvmMemFill, &retVal);
        return retVal;
    }

    if ((svmPtr == nullptr) && (size != 0)) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmMemFill, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_FILL_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueSvmMemFill, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueSVMMemFill(
        svmPtr,
        pattern,
        patternSize,
        size,
        numEventsInWaitList,
        eventWaitList,
        event);

    TRACING_EXIT(ClEnqueueSvmMemFill, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueSVMMap(cl_command_queue commandQueue,
                                   cl_bool blockingMap,
                                   cl_map_flags mapFlags,
                                   void *svmPtr,
                                   size_t size,
                                   cl_uint numEventsInWaitList,
                                   const cl_event *eventWaitList,
                                   cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMap, &commandQueue, &blockingMap, &mapFlags, &svmPtr, &size, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "blockingMap", blockingMap,
                   "mapFlags", mapFlags,
                   "svmPtr", NEO::fileLoggerInstance().infoPointerToString(svmPtr, size),
                   "size", size,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueSvmMap, &retVal);
        return retVal;
    }

    if ((svmPtr == nullptr) || (size == 0)) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmMap, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueSvmMap, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueSVMMap(
        blockingMap,
        mapFlags,
        svmPtr,
        size,
        numEventsInWaitList,
        eventWaitList,
        event,
        true);

    TRACING_EXIT(ClEnqueueSvmMap, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue commandQueue,
                                     void *svmPtr,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmUnmap, &commandQueue, &svmPtr, &numEventsInWaitList, &eventWaitList, &event);

    CommandQueue *pCommandQueue = nullptr;

    cl_int retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList),
        svmPtr);

    API_ENTER(&retVal);

    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "svmPtr", svmPtr,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueSvmUnmap, &retVal);
        return retVal;
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_MAP_BUFFER_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueSvmUnmap, &retVal);
        return retVal;
    }

    retVal = pCommandQueue->enqueueSVMUnmap(
        svmPtr,
        numEventsInWaitList,
        eventWaitList,
        event,
        true);

    TRACING_EXIT(ClEnqueueSvmUnmap, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetKernelArgSVMPointer(cl_kernel kernel,
                                            cl_uint argIndex,
                                            const void *argValue) {
    TRACING_ENTER(ClSetKernelArgSvmPointer, &kernel, &argIndex, &argValue);

    MultiDeviceKernel *multiDeviceKernel = nullptr;

    auto retVal = validateObjects(withCastToInternal(kernel, &multiDeviceKernel));
    API_ENTER(&retVal);

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
        return retVal;
    }

    if (argIndex >= multiDeviceKernel->getKernelArgsNumber()) {
        retVal = CL_INVALID_ARG_INDEX;
        TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
        return retVal;
    }

    const auto svmManager = multiDeviceKernel->getContext().getSVMAllocsManager();

    if (argValue != nullptr) {
        if (multiDeviceKernel->getKernelArguments()[argIndex].allocId > 0 &&
            multiDeviceKernel->getKernelArguments()[argIndex].value == argValue) {
            bool reuseFromCache = false;
            const auto allocationsCounter = svmManager->allocationsCounter.load();
            if (allocationsCounter > 0) {
                if (allocationsCounter == multiDeviceKernel->getKernelArguments()[argIndex].allocIdMemoryManagerCounter) {
                    reuseFromCache = true;
                } else {
                    const auto svmData = svmManager->getSVMAlloc(argValue);
                    if (svmData && multiDeviceKernel->getKernelArguments()[argIndex].allocId == svmData->getAllocId()) {
                        reuseFromCache = true;
                        multiDeviceKernel->storeKernelArgAllocIdMemoryManagerCounter(argIndex, allocationsCounter);
                    }
                }
                if (reuseFromCache) {
                    TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
                    return CL_SUCCESS;
                }
            }
        }
    } else {
        if (multiDeviceKernel->getKernelArguments()[argIndex].isSetToNullptr) {
            TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
            return CL_SUCCESS;
        }
    }

    DBG_LOG_INPUTS("kernel", kernel, "argIndex", argIndex, "argValue", argValue);

    for (const auto &pDevice : multiDeviceKernel->getDevices()) {
        auto pKernel = multiDeviceKernel->getKernel(pDevice->getRootDeviceIndex());
        cl_int kernelArgAddressQualifier = asClKernelArgAddressQualifier(pKernel->getKernelInfo()
                                                                             .kernelDescriptor.payloadMappings.explicitArgs[argIndex]
                                                                             .getTraits()
                                                                             .getAddressQualifier());
        if ((kernelArgAddressQualifier != CL_KERNEL_ARG_ADDRESS_GLOBAL) &&
            (kernelArgAddressQualifier != CL_KERNEL_ARG_ADDRESS_CONSTANT)) {
            retVal = CL_INVALID_ARG_VALUE;
            TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
            return retVal;
        }
    }

    MultiGraphicsAllocation *svmAllocs = nullptr;
    uint32_t allocId = 0u;
    if (argValue != nullptr) {
        auto svmData = svmManager->getSVMAlloc(argValue);
        if (svmData == nullptr) {
            if (debugManager.flags.DetectIncorrectPointersOnSetArgCalls.get() == 1) {
                for (const auto &pDevice : multiDeviceKernel->getDevices()) {
                    if ((pDevice->getHardwareInfo().capabilityTable.sharedSystemMemCapabilities == 0) || (debugManager.flags.EnableSharedSystemUsmSupport.get() == 0)) {
                        retVal = CL_INVALID_ARG_VALUE;
                        TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
                        return retVal;
                    }
                }
            }
        } else {
            svmAllocs = &svmData->gpuAllocations;
            allocId = svmData->getAllocId();
        }
    }
    retVal = multiDeviceKernel->setArgSvmAlloc(argIndex, const_cast<void *>(argValue), svmAllocs, allocId);
    TRACING_EXIT(ClSetKernelArgSvmPointer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetKernelExecInfo(cl_kernel kernel,
                                       cl_kernel_exec_info paramName,
                                       size_t paramValueSize,
                                       const void *paramValue) {
    TRACING_ENTER(ClSetKernelExecInfo, &kernel, &paramName, &paramValueSize, &paramValue);

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    auto retVal = validateObjects(withCastToInternal(kernel, &pMultiDeviceKernel));
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("kernel", kernel, "paramName", paramName,
                   "paramValueSize", paramValueSize, "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClSetKernelExecInfo, &retVal);
        return retVal;
    }

    switch (paramName) {
    case CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL:
    case CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL: {
        if (NEO::debugManager.flags.DisableIndirectAccess.get() != 1 && pMultiDeviceKernel->getHasIndirectAccess() == true) {
            auto propertyValue = *reinterpret_cast<const cl_bool *>(paramValue);
            pMultiDeviceKernel->setUnifiedMemoryProperty(paramName, propertyValue);
        }
    } break;

    case CL_KERNEL_EXEC_INFO_SVM_PTRS:
    case CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL: {
        if ((paramValueSize == 0 && paramValue) ||
            (paramValueSize % sizeof(void *)) ||
            (paramValueSize && paramValue == nullptr)) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &retVal);
            return retVal;
        }
        size_t numPointers = paramValueSize / sizeof(void *);
        auto pSvmPtrList = reinterpret_cast<void **>(const_cast<void *>(paramValue));

        if (paramName == CL_KERNEL_EXEC_INFO_SVM_PTRS) {
            pMultiDeviceKernel->clearSvmKernelExecInfo();
        } else {
            pMultiDeviceKernel->clearUnifiedMemoryExecInfo();
        }

        for (uint32_t i = 0; i < numPointers; i++) {
            auto svmData = pMultiDeviceKernel->getContext().getSVMAllocsManager()->getSVMAlloc(pSvmPtrList[i]);
            if (svmData == nullptr) {
                retVal = CL_INVALID_VALUE;
                TRACING_EXIT(ClSetKernelExecInfo, &retVal);
                return retVal;
            }
            auto &svmAllocs = svmData->gpuAllocations;

            if (paramName == CL_KERNEL_EXEC_INFO_SVM_PTRS) {
                pMultiDeviceKernel->setSvmKernelExecInfo(svmAllocs);
            } else {
                pMultiDeviceKernel->setUnifiedMemoryExecInfo(svmAllocs);
            }
        }
        break;
    }
    case CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL: {
        auto propertyValue = *static_cast<const uint32_t *>(paramValue);
        retVal = pMultiDeviceKernel->setKernelThreadArbitrationPolicy(propertyValue);
        TRACING_EXIT(ClSetKernelExecInfo, &retVal);
        return retVal;
    }
    case CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM: {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClSetKernelExecInfo, &retVal);
        return retVal;
    }
    case CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL: {
        if (paramValueSize != sizeof(cl_execution_info_kernel_type_intel) ||
            paramValue == nullptr) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClSetKernelExecInfo, &retVal);
            return retVal;
        }
        auto kernelType = *static_cast<const cl_execution_info_kernel_type_intel *>(paramValue);
        retVal = pMultiDeviceKernel->setKernelExecutionType(kernelType);
        TRACING_EXIT(ClSetKernelExecInfo, &retVal);
        return retVal;
    }
    default: {
        retVal = CL_INVALID_VALUE;
        break;
    }
    }

    TRACING_EXIT(ClSetKernelExecInfo, &retVal);
    return retVal;
};

cl_mem CL_API_CALL clCreatePipe(cl_context context,
                                cl_mem_flags flags,
                                cl_uint pipePacketSize,
                                cl_uint pipeMaxPackets,
                                const cl_pipe_properties *properties,
                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreatePipe, &context, &flags, &pipePacketSize, &pipeMaxPackets, &properties, &errcodeRet);
    cl_int retVal = CL_INVALID_OPERATION;
    cl_mem pipe = nullptr;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("cl_context", context,
                   "cl_mem_flags", flags,
                   "cl_uint", pipePacketSize,
                   "cl_uint", pipeMaxPackets,
                   "const cl_pipe_properties", properties,
                   "cl_int", errcodeRet);

    if (errcodeRet) {
        *errcodeRet = retVal;
    }
    DBG_LOG_INPUTS("pipe", pipe);
    TRACING_EXIT(ClCreatePipe, &pipe);
    return pipe;
}

cl_int CL_API_CALL clGetPipeInfo(cl_mem pipe,
                                 cl_pipe_info paramName,
                                 size_t paramValueSize,
                                 void *paramValue,
                                 size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetPipeInfo, &pipe, &paramName, &paramValueSize, &paramValue, &paramValueSizeRet);

    cl_int retVal = CL_INVALID_MEM_OBJECT;
    API_ENTER(&retVal);

    DBG_LOG_INPUTS("cl_mem", pipe,
                   "cl_pipe_info", paramName,
                   "size_t", paramValueSize,
                   "void *", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "size_t*", paramValueSizeRet);

    TRACING_EXIT(ClGetPipeInfo, &retVal);
    return retVal;
}

cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context context,
                                                                cl_device_id device,
                                                                const cl_queue_properties *properties,
                                                                cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateCommandQueueWithProperties, &context, &device, &properties, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "device", device,
                   "properties", properties);

    cl_command_queue commandQueue = nullptr;
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    Context *pContext = nullptr;
    ClDevice *pDevice = nullptr;

    retVal = validateObjects(
        withCastToInternal(context, &pContext),
        withCastToInternal(device, &pDevice));

    if (CL_SUCCESS != retVal) {
        err.set(retVal);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    if (!pContext->isDeviceAssociated(*pDevice)) {
        err.set(CL_INVALID_DEVICE);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    auto tokenValue = properties ? *properties : 0;
    auto propertiesAddress = properties;

    while (tokenValue != 0) {
        switch (tokenValue) {
        case CL_QUEUE_PROPERTIES:
        case CL_QUEUE_SIZE:
        case CL_QUEUE_PRIORITY_KHR:
        case CL_QUEUE_THROTTLE_KHR:
        case CL_QUEUE_SLICE_COUNT_INTEL:
        case CL_QUEUE_FAMILY_INTEL:
        case CL_QUEUE_INDEX_INTEL:
        case CL_QUEUE_MDAPI_PROPERTIES_INTEL:
        case CL_QUEUE_MDAPI_CONFIGURATION_INTEL:
            break;
        default:
            err.set(CL_INVALID_VALUE);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
            return commandQueue;
        }

        propertiesAddress += 2;
        tokenValue = *propertiesAddress;
    }

    auto commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    uint32_t maxOnDeviceQueueSize = pDevice->getDeviceInfo().queueOnDeviceMaxSize;

    if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE)) {
        if (!(commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE))) {
            err.set(CL_INVALID_VALUE);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
            return commandQueue;
        }
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE_DEFAULT)) {
        if (!(commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE))) {
            err.set(CL_INVALID_VALUE);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
            return commandQueue;
        }
    }

    if (getCmdQueueProperties<cl_command_queue_properties>(properties, CL_QUEUE_SIZE) > maxOnDeviceQueueSize) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE)) {
        if (getCmdQueueProperties<cl_queue_priority_khr>(properties, CL_QUEUE_PRIORITY_KHR)) {
            err.set(CL_INVALID_QUEUE_PROPERTIES);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
            return commandQueue;
        }
    }

    if (commandQueueProperties & static_cast<cl_command_queue_properties>(CL_QUEUE_ON_DEVICE)) {
        if (getCmdQueueProperties<cl_queue_throttle_khr>(properties, CL_QUEUE_THROTTLE_KHR)) {
            err.set(CL_INVALID_QUEUE_PROPERTIES);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
            return commandQueue;
        }
    }

    if (getCmdQueueProperties<cl_command_queue_properties>(properties, CL_QUEUE_SLICE_COUNT_INTEL) > pDevice->getDeviceInfo().maxSliceCount) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    bool queueFamilySelected = false;
    bool queueSelected = false;
    const auto queueFamilyIndex = getCmdQueueProperties<cl_uint>(properties, CL_QUEUE_FAMILY_INTEL, &queueFamilySelected);
    const auto queueIndex = getCmdQueueProperties<cl_uint>(properties, CL_QUEUE_INDEX_INTEL, &queueSelected);
    if (queueFamilySelected != queueSelected) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }
    if (queueFamilySelected &&
        (queueFamilyIndex >= pDevice->getDeviceInfo().queueFamilyProperties.size() ||
         queueIndex >= pDevice->getDeviceInfo().queueFamilyProperties[queueFamilyIndex].count)) {
        err.set(CL_INVALID_QUEUE_PROPERTIES);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    bool mdapiPropertySet = false;
    bool mdapiConfigurationSet = false;
    cl_command_queue_mdapi_properties_intel mdapiProperties = getCmdQueueProperties<cl_command_queue_mdapi_properties_intel>(properties, CL_QUEUE_MDAPI_PROPERTIES_INTEL, &mdapiPropertySet);
    cl_uint mdapiConfiguration = getCmdQueueProperties<cl_uint>(properties, CL_QUEUE_MDAPI_CONFIGURATION_INTEL, &mdapiConfigurationSet);

    if (mdapiConfigurationSet && mdapiConfiguration != 0) {
        err.set(CL_INVALID_OPERATION);
        TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
        return commandQueue;
    }

    commandQueue = CommandQueue::create(
        pContext,
        pDevice,
        properties,
        false,
        retVal);

    if (mdapiPropertySet && (mdapiProperties & CL_QUEUE_MDAPI_ENABLE_INTEL)) {
        auto commandQueueObj = castToObjectOrAbort<CommandQueue>(commandQueue);

        if (!commandQueueObj->setPerfCountersEnabled()) {
            clReleaseCommandQueue(commandQueue);
            TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);

            err.set(CL_OUT_OF_RESOURCES);
            return nullptr;
        }
    }

    if (pContext->isProvidingPerformanceHints()) {
        pContext->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, DRIVER_CALLS_INTERNAL_CL_FLUSH);
        if (castToObjectOrAbort<CommandQueue>(commandQueue)->isProfilingEnabled()) {
            pContext->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, PROFILING_ENABLED);
        }
    }

    if (!commandQueue) {
        retVal = CL_OUT_OF_HOST_MEMORY;
    }

    DBG_LOG_INPUTS("commandQueue", commandQueue, "properties", static_cast<int>(getCmdQueueProperties<cl_command_queue_properties>(properties)));

    err.set(retVal);

    TRACING_EXIT(ClCreateCommandQueueWithProperties, &commandQueue);
    return commandQueue;
}

cl_sampler CL_API_CALL clCreateSamplerWithProperties(cl_context context,
                                                     const cl_sampler_properties *samplerProperties,
                                                     cl_int *errcodeRet) {
    TRACING_ENTER(ClCreateSamplerWithProperties, &context, &samplerProperties, &errcodeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "samplerProperties", samplerProperties);
    cl_sampler sampler = nullptr;
    retVal = validateObjects(context);

    if (CL_SUCCESS == retVal) {
        sampler = Sampler::create(
            castToObject<Context>(context),
            samplerProperties,
            retVal);
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }

    TRACING_EXIT(ClCreateSamplerWithProperties, &sampler);
    return sampler;
}

cl_int CL_API_CALL clUnloadCompiler() {
    TRACING_ENTER(ClUnloadCompiler);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    TRACING_EXIT(ClUnloadCompiler, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelSubGroupInfoKHR(cl_kernel kernel,
                                              cl_device_id device,
                                              cl_kernel_sub_group_info paramName,
                                              size_t inputValueSize,
                                              const void *inputValue,
                                              size_t paramValueSize,
                                              void *paramValue,
                                              size_t *paramValueSizeRet) {

    TRACING_ENTER(ClGetKernelSubGroupInfoKHR, &kernel, &device, &paramName, &inputValueSize, &inputValue, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("kernel", kernel,
                   "device", device,
                   "paramName", paramName,
                   "inputValueSize", inputValueSize,
                   "inputValue", NEO::fileLoggerInstance().infoPointerToString(inputValue, inputValueSize),
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    retVal = validateObjects(withCastToInternal(kernel, &pMultiDeviceKernel));

    ClDevice *pClDevice = nullptr;
    if (CL_SUCCESS == retVal) {
        if (pMultiDeviceKernel->getDevices().size() == 1u && !device) {
            pClDevice = pMultiDeviceKernel->getDevices()[0];
        } else {
            retVal = validateObjects(withCastToInternal(device, &pClDevice));
        }
    }

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetKernelSubGroupInfoKHR, &retVal);
        return retVal;
    }
    auto pKernel = pMultiDeviceKernel->getKernel(pClDevice->getRootDeviceIndex());

    switch (paramName) {
    case CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE:
    case CL_KERNEL_SUB_GROUP_COUNT_FOR_NDRANGE:
    case CL_KERNEL_COMPILE_SUB_GROUP_SIZE_INTEL:
        retVal = pKernel->getSubGroupInfo(paramName,
                                          inputValueSize, inputValue,
                                          paramValueSize, paramValue,
                                          paramValueSizeRet);
        TRACING_EXIT(ClGetKernelSubGroupInfoKHR, &retVal);
        return retVal;
    default: {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetKernelSubGroupInfoKHR, &retVal);
        return retVal;
    }
    }
}

cl_int CL_API_CALL clGetDeviceAndHostTimer(cl_device_id device,
                                           cl_ulong *deviceTimestamp,
                                           cl_ulong *hostTimestamp) {
    TRACING_ENTER(ClGetDeviceAndHostTimer, &device, &deviceTimestamp, &hostTimestamp);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device,
                   "deviceTimestamp", deviceTimestamp,
                   "hostTimestamp", hostTimestamp);
    do {
        ClDevice *pDevice = castToObject<ClDevice>(device);
        if (pDevice == nullptr) {
            retVal = CL_INVALID_DEVICE;
            break;
        }
        if (deviceTimestamp == nullptr || hostTimestamp == nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        if (!pDevice->getDeviceAndHostTimer(static_cast<uint64_t *>(deviceTimestamp), static_cast<uint64_t *>(hostTimestamp))) {
            retVal = CL_OUT_OF_RESOURCES;
            break;
        }
    } while (false);

    TRACING_EXIT(ClGetDeviceAndHostTimer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetHostTimer(cl_device_id device,
                                  cl_ulong *hostTimestamp) {
    TRACING_ENTER(ClGetHostTimer, &device, &hostTimestamp);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device,
                   "hostTimestamp", hostTimestamp);

    do {
        ClDevice *pDevice = castToObject<ClDevice>(device);
        if (pDevice == nullptr) {
            retVal = CL_INVALID_DEVICE;
            break;
        }
        if (hostTimestamp == nullptr) {
            retVal = CL_INVALID_VALUE;
            break;
        }
        if (!pDevice->getHostTimer(static_cast<uint64_t *>(hostTimestamp))) {
            retVal = CL_OUT_OF_RESOURCES;
            break;
        }
    } while (false);

    TRACING_EXIT(ClGetHostTimer, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelSubGroupInfo(cl_kernel kernel,
                                           cl_device_id device,
                                           cl_kernel_sub_group_info paramName,
                                           size_t inputValueSize,
                                           const void *inputValue,
                                           size_t paramValueSize,
                                           void *paramValue,
                                           size_t *paramValueSizeRet) {
    TRACING_ENTER(ClGetKernelSubGroupInfo, &kernel, &device, &paramName, &inputValueSize, &inputValue, &paramValueSize, &paramValue, &paramValueSizeRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("kernel", kernel,
                   "device", device,
                   "paramName", paramName,
                   "inputValueSize", inputValueSize,
                   "inputValue", NEO::fileLoggerInstance().infoPointerToString(inputValue, inputValueSize),
                   "paramValueSize", paramValueSize,
                   "paramValue", NEO::fileLoggerInstance().infoPointerToString(paramValue, paramValueSize),
                   "paramValueSizeRet", paramValueSizeRet);

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    retVal = validateObjects(withCastToInternal(kernel, &pMultiDeviceKernel));

    ClDevice *pClDevice = nullptr;
    if (CL_SUCCESS == retVal) {
        if (pMultiDeviceKernel->getDevices().size() == 1u && !device) {
            pClDevice = pMultiDeviceKernel->getDevices()[0];
        } else {
            retVal = validateObjects(withCastToInternal(device, &pClDevice));
        }
    }

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetKernelSubGroupInfo, &retVal);
        return retVal;
    }

    auto pKernel = pMultiDeviceKernel->getKernel(pClDevice->getRootDeviceIndex());
    retVal = pKernel->getSubGroupInfo(paramName,
                                      inputValueSize, inputValue,
                                      paramValueSize, paramValue,
                                      paramValueSizeRet);
    TRACING_EXIT(ClGetKernelSubGroupInfo, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetDefaultDeviceCommandQueue(cl_context context,
                                                  cl_device_id device,
                                                  cl_command_queue commandQueue) {
    TRACING_ENTER(ClSetDefaultDeviceCommandQueue, &context, &device, &commandQueue);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("context", context,
                   "device", device,
                   "commandQueue", commandQueue);

    Context *pContext = nullptr;
    ClDevice *pClDevice = nullptr;

    retVal = validateObjects(withCastToInternal(context, &pContext),
                             withCastToInternal(device, &pClDevice));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClSetDefaultDeviceCommandQueue, &retVal);
        return retVal;
    }

    retVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClSetDefaultDeviceCommandQueue, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueSVMMigrateMem(cl_command_queue commandQueue,
                                          cl_uint numSvmPointers,
                                          const void **svmPointers,
                                          const size_t *sizes,
                                          const cl_mem_migration_flags flags,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMigrateMem, &commandQueue, &numSvmPointers, &svmPointers, &sizes, &flags, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "numSvmPointers", numSvmPointers,
                   "svmPointers", NEO::fileLoggerInstance().infoPointerToString(svmPointers ? svmPointers[0] : 0, NEO::fileLoggerInstance().getInput(sizes, 0)),
                   "sizes", NEO::fileLoggerInstance().getInput(sizes, 0),
                   "flags", flags,
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
        return retVal;
    }

    if (numSvmPointers == 0 || svmPointers == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
        return retVal;
    }

    const cl_mem_migration_flags allValidFlags =
        CL_MIGRATE_MEM_OBJECT_HOST | CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED;

    if ((flags & (~allValidFlags)) != 0) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
        return retVal;
    }

    auto pSvmAllocMgr = pCommandQueue->getContext().getSVMAllocsManager();
    UNRECOVERABLE_IF(pSvmAllocMgr == nullptr);

    for (uint32_t i = 0; i < numSvmPointers; i++) {
        auto svmData = pSvmAllocMgr->getSVMAlloc(svmPointers[i]);
        if (svmData == nullptr) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
            return retVal;
        }
        if (sizes != nullptr && sizes[i] != 0) {
            svmData = pSvmAllocMgr->getSVMAlloc(ptrOffset(svmPointers[i], sizes[i] - 1));
            if (svmData == nullptr) {
                retVal = CL_INVALID_VALUE;
                TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
                return retVal;
            }
        }
    }

    for (uint32_t i = 0; i < numEventsInWaitList; i++) {
        auto pEvent = castToObject<Event>(eventWaitList[i]);
        if (pEvent->getContext() != &pCommandQueue->getContext()) {
            retVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
            return retVal;
        }
    }
    retVal = pCommandQueue->enqueueSVMMigrateMem(numSvmPointers,
                                                 svmPointers,
                                                 sizes,
                                                 flags,
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
    TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
    return retVal;
}

cl_kernel CL_API_CALL clCloneKernel(cl_kernel sourceKernel,
                                    cl_int *errcodeRet) {
    TRACING_ENTER(ClCloneKernel, &sourceKernel, &errcodeRet);
    MultiDeviceKernel *pSourceMultiDeviceKernel = nullptr;
    cl_kernel clonedMultiDeviceKernel = nullptr;

    auto retVal = validateObjects(withCastToInternal(sourceKernel, &pSourceMultiDeviceKernel));
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("sourceKernel", sourceKernel);

    if (CL_SUCCESS == retVal) {
        clonedMultiDeviceKernel = MultiDeviceKernel::create(pSourceMultiDeviceKernel->getProgram(),
                                                            pSourceMultiDeviceKernel->getKernelInfos(),
                                                            retVal);
        UNRECOVERABLE_IF((clonedMultiDeviceKernel == nullptr) || (retVal != CL_SUCCESS));

        auto pClonedMultiDeviceKernel = castToObject<MultiDeviceKernel>(clonedMultiDeviceKernel);
        retVal = pClonedMultiDeviceKernel->cloneKernel(pSourceMultiDeviceKernel);
    }

    if (errcodeRet) {
        *errcodeRet = retVal;
    }
    if (clonedMultiDeviceKernel != nullptr) {
        gtpinNotifyKernelCreate(clonedMultiDeviceKernel);
    }

    TRACING_EXIT(ClCloneKernel, &clonedMultiDeviceKernel);
    return clonedMultiDeviceKernel;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueVerifyMemoryINTEL(cl_command_queue commandQueue,
                                                           const void *allocationPtr,
                                                           const void *expectedData,
                                                           size_t sizeOfComparison,
                                                           cl_uint comparisonMode) {

    TRACING_ENTER(ClEnqueueVerifyMemoryINTEL, &commandQueue, &allocationPtr, &expectedData, &sizeOfComparison, &comparisonMode);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "allocationPtr", allocationPtr,
                   "expectedData", expectedData,
                   "sizeOfComparison", sizeOfComparison,
                   "comparisonMode", comparisonMode);

    if (sizeOfComparison == 0 || expectedData == nullptr || allocationPtr == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &retVal);
        return retVal;
    }

    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue));
    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &retVal);
        return retVal;
    }

    auto activeCopyEngines = pCommandQueue->peekActiveBcsStates();
    for (auto &copyEngine : activeCopyEngines) {
        if (copyEngine.isValid()) {
            pCommandQueue->getBcsCommandStreamReceiver(copyEngine.engineType)->pollForCompletion();
        }
    }

    auto &csr = pCommandQueue->getGpgpuCommandStreamReceiver();
    auto status = csr.expectMemory(allocationPtr, expectedData, sizeOfComparison, comparisonMode);
    retVal = status ? CL_SUCCESS : CL_INVALID_VALUE;
    TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clAddCommentINTEL(cl_device_id device, const char *comment) {

    TRACING_ENTER(ClAddCommentINTEL, &device, &comment);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device, "comment", comment);

    ClDevice *pDevice = nullptr;
    retVal = validateObjects(withCastToInternal(device, &pDevice));
    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClAddCommentINTEL, &retVal);
        return retVal;
    }
    auto aubCenter = pDevice->getRootDeviceEnvironment().aubCenter.get();

    if (!comment || (aubCenter && !aubCenter->getAubManager())) {
        retVal = CL_INVALID_VALUE;
    }

    if (retVal == CL_SUCCESS && aubCenter) {
        aubCenter->getAubManager()->addComment(comment);
    }

    TRACING_EXIT(ClAddCommentINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetDeviceGlobalVariablePointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *globalVariableName,
    size_t *globalVariableSizeRet,
    void **globalVariablePointerRet) {
    TRACING_ENTER(ClGetDeviceGlobalVariablePointerINTEL, &device, &program, &globalVariableName, &globalVariableSizeRet, &globalVariablePointerRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device, "program", program,
                   "globalVariableName", globalVariableName,
                   "globalVariablePointerRet", globalVariablePointerRet);
    Program *pProgram = nullptr;
    ClDevice *pDevice = nullptr;
    retVal = validateObjects(withCastToInternal(program, &pProgram), withCastToInternal(device, &pDevice));
    if (globalVariablePointerRet == nullptr) {
        retVal = CL_INVALID_ARG_VALUE;
    }

    if (CL_SUCCESS == retVal) {
        const auto &symbols = pProgram->getSymbols(pDevice->getRootDeviceIndex());
        auto symbolIt = symbols.find(globalVariableName);
        if ((symbolIt == symbols.end()) || (symbolIt->second.symbol.segment == NEO::SegmentType::instructions)) {
            retVal = CL_INVALID_ARG_VALUE;
        } else {
            if (globalVariableSizeRet != nullptr) {
                *globalVariableSizeRet = static_cast<size_t>(symbolIt->second.symbol.size);
            }
            *globalVariablePointerRet = reinterpret_cast<void *>(symbolIt->second.gpuAddress);
        }
    }

    TRACING_EXIT(ClGetDeviceGlobalVariablePointerINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetDeviceFunctionPointerINTEL(
    cl_device_id device,
    cl_program program,
    const char *functionName,
    cl_ulong *functionPointerRet) {

    TRACING_ENTER(ClGetDeviceFunctionPointerINTEL, &device, &program, &functionName, &functionPointerRet);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("device", device, "program", program,
                   "functionName", functionName,
                   "functionPointerRet", functionPointerRet);

    Program *pProgram = nullptr;
    ClDevice *pDevice = nullptr;
    retVal = validateObjects(withCastToInternal(program, &pProgram), withCastToInternal(device, &pDevice));
    if ((CL_SUCCESS == retVal) && (functionPointerRet == nullptr)) {
        retVal = CL_INVALID_ARG_VALUE;
    }

    if (CL_SUCCESS == retVal) {
        const auto &symbols = pProgram->getSymbols(pDevice->getRootDeviceIndex());
        auto symbolIt = symbols.find(functionName);
        if ((symbolIt == symbols.end()) || (symbolIt->second.symbol.segment != NEO::SegmentType::instructions)) {
            retVal = CL_INVALID_ARG_VALUE;
        } else {
            *functionPointerRet = static_cast<cl_ulong>(symbolIt->second.gpuAddress);
        }
    }

    TRACING_EXIT(ClGetDeviceFunctionPointerINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetProgramReleaseCallback(cl_program program,
                                               void(CL_CALLBACK *pfnNotify)(cl_program /* program */, void * /* user_data */),
                                               void *userData) {

    TRACING_ENTER(ClSetProgramReleaseCallback, &program, &pfnNotify, &userData);
    DBG_LOG_INPUTS("program", program,
                   "pfnNotify", reinterpret_cast<void *>(pfnNotify),
                   "userData", userData);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    Program *pProgram = nullptr;
    retVal = validateObjects(withCastToInternal(program, &pProgram),
                             reinterpret_cast<void *>(pfnNotify));

    if (retVal == CL_SUCCESS) {
        retVal = CL_INVALID_OPERATION;
    }

    TRACING_EXIT(ClSetProgramReleaseCallback, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetProgramSpecializationConstant(cl_program program, cl_uint specId, size_t specSize, const void *specValue) {

    TRACING_ENTER(ClSetProgramSpecializationConstant, &program, &specId, &specSize, &specValue);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("program", program,
                   "specId", specId,
                   "specSize", specSize,
                   "specValue", specValue);

    Program *pProgram = nullptr;
    retVal = validateObjects(withCastToInternal(program, &pProgram), specValue);

    if (retVal == CL_SUCCESS) {
        retVal = pProgram->setProgramSpecializationConstant(specId, specSize, specValue);
    }

    TRACING_EXIT(ClSetProgramSpecializationConstant, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelSuggestedLocalWorkSizeINTEL(cl_command_queue commandQueue,
                                                          cl_kernel kernel,
                                                          cl_uint workDim,
                                                          const size_t *globalWorkOffset,
                                                          const size_t *globalWorkSize,
                                                          size_t *suggestedLocalWorkSize) {

    TRACING_ENTER(ClGetKernelSuggestedLocalWorkSizeINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &suggestedLocalWorkSize);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "cl_kernel", kernel,
                   "globalWorkOffset[0]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 0),
                   "globalWorkOffset[1]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 1),
                   "globalWorkOffset[2]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 2),
                   "globalWorkSize", NEO::fileLoggerInstance().getSizes(globalWorkSize, workDim, true),
                   "suggestedLocalWorkSize", suggestedLocalWorkSize);

    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    CommandQueue *pCommandQueue = nullptr;
    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), withCastToInternal(kernel, &pMultiDeviceKernel));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    if ((workDim == 0) || (workDim > 3)) {
        retVal = CL_INVALID_WORK_DIMENSION;
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    if (globalWorkSize == nullptr ||
        globalWorkSize[0] == 0 ||
        (workDim > 1 && globalWorkSize[1] == 0) ||
        (workDim > 2 && globalWorkSize[2] == 0)) {
        retVal = CL_INVALID_GLOBAL_WORK_SIZE;
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    auto pKernel = pMultiDeviceKernel->getKernel(pCommandQueue->getDevice().getRootDeviceIndex());
    if (!pKernel->isPatched()) {
        retVal = CL_INVALID_KERNEL;
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    if (suggestedLocalWorkSize == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
        return retVal;
    }

    pKernel->getSuggestedLocalWorkSize(workDim, globalWorkSize, globalWorkOffset, suggestedLocalWorkSize);

    TRACING_EXIT(ClGetKernelSuggestedLocalWorkSizeINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clGetKernelMaxConcurrentWorkGroupCountINTEL(cl_command_queue commandQueue,
                                                               cl_kernel kernel,
                                                               cl_uint workDim,
                                                               const size_t *globalWorkOffset,
                                                               const size_t *localWorkSize,
                                                               size_t *suggestedWorkGroupCount) {
    TRACING_ENTER(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &localWorkSize, &suggestedWorkGroupCount);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "cl_kernel", kernel,
                   "globalWorkOffset[0]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 0),
                   "globalWorkOffset[1]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 1),
                   "globalWorkOffset[2]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 2),
                   "localWorkSize", NEO::fileLoggerInstance().getSizes(localWorkSize, workDim, true),
                   "suggestedWorkGroupCount", suggestedWorkGroupCount);

    CommandQueue *pCommandQueue = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;

    retVal = validateObjects(withCastToInternal(commandQueue, &pCommandQueue), withCastToInternal(kernel, &pMultiDeviceKernel));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    if ((workDim == 0) || (workDim > 3)) {
        retVal = CL_INVALID_WORK_DIMENSION;
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    if (localWorkSize == nullptr) {
        retVal = CL_INVALID_WORK_GROUP_SIZE;
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    auto pKernel = pMultiDeviceKernel->getKernel(pCommandQueue->getDevice().getRootDeviceIndex());
    if (!pKernel->isPatched()) {
        retVal = CL_INVALID_KERNEL;
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    if (suggestedWorkGroupCount == nullptr) {
        retVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
        return retVal;
    }

    for (size_t i = 0; i < workDim; i++) {
        if (localWorkSize[i] == 0) {
            retVal = CL_INVALID_WORK_GROUP_SIZE;
            TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
            return retVal;
        }
    }

    withCastToInternal(commandQueue, &pCommandQueue);
    *suggestedWorkGroupCount = pKernel->getMaxWorkGroupCount(workDim, localWorkSize, pCommandQueue, false);

    TRACING_EXIT(ClGetKernelMaxConcurrentWorkGroupCountINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueNDCountKernelINTEL(cl_command_queue commandQueue,
                                               cl_kernel kernel,
                                               cl_uint workDim,
                                               const size_t *globalWorkOffset,
                                               const size_t *workgroupCount,
                                               const size_t *localWorkSize,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {

    TRACING_ENTER(ClEnqueueNDCountKernelINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &workgroupCount, &localWorkSize, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue, "cl_kernel", kernel,
                   "globalWorkOffset[0]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 0),
                   "globalWorkOffset[1]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 1),
                   "globalWorkOffset[2]", NEO::fileLoggerInstance().getInput(globalWorkOffset, 2),
                   "workgroupCount", NEO::fileLoggerInstance().getSizes(workgroupCount, workDim, false),
                   "localWorkSize", NEO::fileLoggerInstance().getSizes(localWorkSize, workDim, true),
                   "numEventsInWaitList", numEventsInWaitList,
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    CommandQueue *pCommandQueue = nullptr;
    Kernel *pKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;

    retVal = validateObjects(
        withCastToInternal(commandQueue, &pCommandQueue),
        withCastToInternal(kernel, &pMultiDeviceKernel),
        EventWaitList(numEventsInWaitList, eventWaitList));

    if (CL_SUCCESS != retVal) {
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
        return retVal;
    }

    auto &device = pCommandQueue->getClDevice();
    auto rootDeviceIndex = device.getRootDeviceIndex();

    pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    size_t globalWorkSize[3];
    for (size_t i = 0; i < workDim; i++) {
        if (localWorkSize[i] == 0) {
            retVal = CL_INVALID_WORK_GROUP_SIZE;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
            return retVal;
        }
        globalWorkSize[i] = workgroupCount[i] * localWorkSize[i];
    }

    if (pKernel->usesSyncBuffer()) {
        if (pKernel->getExecutionType() != KernelExecutionType::concurrent) {
            retVal = CL_INVALID_KERNEL;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
            return retVal;
        }

        auto &hardwareInfo = device.getHardwareInfo();
        auto &gfxCoreHelper = device.getGfxCoreHelper();
        auto engineGroupType = gfxCoreHelper.getEngineGroupType(pCommandQueue->getGpgpuEngine().getEngineType(),
                                                                pCommandQueue->getGpgpuEngine().getEngineUsage(), hardwareInfo);
        if (!gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, device.getRootDeviceEnvironment())) {
            retVal = CL_INVALID_COMMAND_QUEUE;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
            return retVal;
        }
    }

    if (pKernel->getExecutionType() == KernelExecutionType::concurrent) {
        size_t requestedNumberOfWorkgroups = 1;
        for (size_t i = 0; i < workDim; i++) {
            requestedNumberOfWorkgroups *= workgroupCount[i];
        }
        size_t maximalNumberOfWorkgroupsAllowed = pKernel->getMaxWorkGroupCount(workDim, localWorkSize, pCommandQueue, false);
        if (requestedNumberOfWorkgroups > maximalNumberOfWorkgroupsAllowed) {
            retVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
            return retVal;
        }
    }

    if (!pCommandQueue->validateCapabilityForOperation(CL_QUEUE_CAPABILITY_KERNEL_INTEL, numEventsInWaitList, eventWaitList, event)) {
        retVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
        return retVal;
    }

    if (pKernel->usesSyncBuffer()) {
        device.getDevice().allocateSyncBufferHandler();
    }

    TakeOwnershipWrapper<MultiDeviceKernel> kernelOwnership(*pMultiDeviceKernel, gtpinIsGTPinInitialized());
    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyKernelSubmit(kernel, pCommandQueue);
    }

    retVal = pCommandQueue->enqueueKernel(
        pKernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);

    DBG_LOG_INPUTS("event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1u));
    TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
    return retVal;
}

cl_int CL_API_CALL clSetContextDestructorCallback(cl_context context,
                                                  void(CL_CALLBACK *pfnNotify)(cl_context /* context */, void * /* user_data */),
                                                  void *userData) {

    TRACING_ENTER(ClSetContextDestructorCallback, &context, &pfnNotify, &userData);
    DBG_LOG_INPUTS("program", context,
                   "pfnNotify", reinterpret_cast<void *>(pfnNotify),
                   "userData", userData);

    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);

    Context *pContext = nullptr;
    retVal = validateObjects(withCastToInternal(context, &pContext),
                             reinterpret_cast<void *>(pfnNotify));

    if (retVal == CL_SUCCESS) {
        retVal = pContext->setDestructorCallback(pfnNotify, userData);
    }

    TRACING_EXIT(ClSetContextDestructorCallback, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    cl_int retVal = CL_SUCCESS;
    API_ENTER(&retVal);
    DBG_LOG_INPUTS("commandQueue", commandQueue,
                   "memObjects", getClFileLogger().getMemObjects(reinterpret_cast<const uintptr_t *>(memObjects), numMemObjects),
                   "eventWaitList", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(eventWaitList), numEventsInWaitList),
                   "event", getClFileLogger().getEvents(reinterpret_cast<const uintptr_t *>(event), 1));

    auto pCommandQueue = castToObject<CommandQueue>(commandQueue);

    retVal = validateObjects(MemObjList(numMemObjects, memObjects),
                             withCastToInternal(commandQueue, &pCommandQueue),
                             EventWaitList(numEventsInWaitList, eventWaitList));

    if (retVal != CL_SUCCESS) {
        TRACING_EXIT(ClEnqueueExternalMemObjectsKHR, &retVal);
        return retVal;
    }

    for (unsigned int num = 0; num < numEventsInWaitList; num++) {
        auto pEvent = castToObject<Event>(eventWaitList[num]);
        if (pEvent->peekExecutionStatus() < CL_COMPLETE) {
            retVal = CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
            TRACING_EXIT(ClEnqueueExternalMemObjectsKHR, &retVal);
            return retVal;
        }
    }

    retVal = pCommandQueue->enqueueMarkerWithWaitList(numEventsInWaitList, eventWaitList, event);

    TRACING_EXIT(ClEnqueueExternalMemObjectsKHR, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueAcquireExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueAcquireExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    auto retVal = clEnqueueExternalMemObjectsKHR(commandQueue,
                                                 numMemObjects,
                                                 memObjects,
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
    TRACING_EXIT(ClEnqueueAcquireExternalMemObjectsKHR, &retVal);
    return retVal;
}

cl_int CL_API_CALL clEnqueueReleaseExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    TRACING_ENTER(ClEnqueueReleaseExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    auto retVal = clEnqueueExternalMemObjectsKHR(commandQueue,
                                                 numMemObjects,
                                                 memObjects,
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
    TRACING_EXIT(ClEnqueueReleaseExternalMemObjectsKHR, &retVal);
    return retVal;
}
