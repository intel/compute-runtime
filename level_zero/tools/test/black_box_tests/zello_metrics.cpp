/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <vector>

#define ROOT_DEVICE 0xFFFFFFFF

#define VALIDATECALL(myZeCall)                  \
    do {                                        \
        if ((myZeCall) != ZE_RESULT_SUCCESS) {  \
            std::cout << "Error at "            \
                      << #myZeCall << ": "      \
                      << __FUNCTION__ << ": "   \
                      << __LINE__ << std::endl; \
            std::terminate();                   \
        }                                       \
    } while (0);

///////////////////////////
/// Device
///////////////////////////
struct Device {

    uint32_t deviceIndex;
    uint32_t subDeviceIndex;

    Device(uint32_t inDeviceIndex, uint32_t inSubDeviceIndex) {
        deviceIndex = inDeviceIndex;
        subDeviceIndex = inSubDeviceIndex;
    }
};

///////////////////////////
/// Sample
///////////////////////////
struct Sample {

  private:
    ///////////////////////////
    /// L0 core api objects
    ///////////////////////////
    ze_driver_handle_t driverHandle = {};
    ze_context_handle_t contextHandle = {};
    ze_device_handle_t deviceHandle = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_command_queue_desc_t queueDescription = {};
    ze_command_list_handle_t commandList = {};

    ///////////////////////////
    /// Metrics groups
    ///////////////////////////
    zet_metric_group_handle_t metricGroup = nullptr;
    zet_metric_group_properties_t metricGroupProperties = {};

    ///////////////////////////
    /// Notification events
    ///////////////////////////
    ze_event_pool_handle_t eventPool = {};
    ze_event_handle_t notificationEvent = {};

    ///////////////////////////
    /// Metric streamer
    ///////////////////////////
    zet_metric_streamer_handle_t metricStreamer = {};
    const uint32_t notifyReportCount = 10;
    const uint32_t samplingPeriod = 40000;
    uint32_t metricStreamerMarker = 0;

    ///////////////////////////
    /// Metric query
    ///////////////////////////
    const uint32_t queryPoolCount = 1000;
    const uint32_t querySlotIndex = 0;

    zet_metric_query_pool_handle_t queryPoolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t queryPoolDesc = {};

    ///////////////////////////
    /// Metrics raw output data
    ///////////////////////////
    const uint32_t maxRawReportCount = 5;
    std::vector<uint8_t> rawData = {};
    size_t rawDataSize = 0;

    ///////////////////////////
    /// Workload
    ///////////////////////////
    uint32_t allocationSize = 4096;
    void *sourceBuffer = nullptr;
    void *destinationBuffer = nullptr;

    ///////////////////////////
    /// Options
    ///////////////////////////
    bool verbose = false;
    Device device = {0, 0};

  public:
    ///////////////////////////
    /// Sample constructor
    ///////////////////////////
    Sample(Device inDevice,
           bool useMetrics,
           int32_t argc,
           char *argv[])
        : device(inDevice) {
        wait(argc, argv);
        enableMetrics(useMetrics);
        create();
    }

    ///////////////////////////
    /// Sample destructor
    ///////////////////////////
    ~Sample() {
        destroy();
    }

    ///////////////////////////
    /// isArgumentEnabled
    ///////////////////////////
    static bool isArgumentEnabled(int argc, char *argv[], const char *shortName, const char *longName) {
        char **arg = &argv[1];
        char **argE = &argv[argc];

        for (; arg != argE; ++arg) {
            if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
                return true;
            }
        }

        return false;
    }

    ///////////////////////////
    /// getArgumentValue
    ///////////////////////////
    static uint32_t getArgumentValue(int argc, char *argv[], const char *shortName, const char *longName) {
        char **arg = &argv[1];
        char **argE = &argv[argc];

        for (; arg != argE; ++arg) {
            if ((0 == strcmp(*arg, shortName)) || (0 == strcmp(*arg, longName))) {
                ++arg;
                VALIDATECALL(arg != argE ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_INVALID_ARGUMENT);
                return atoi(*arg);
            }
        }

        return false;
    }

    ///////////////////////////
    /// activateMetrics
    ///////////////////////////
    void activateMetrics(const char *metricsName, zet_metric_group_sampling_type_flag_t metricsType) {

        // Find time based metric group.
        findMetricGroup(metricsName, metricsType);

        // Metric Streamer begin.
        VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &metricGroupProperties));

        // Activate metric group.
        VALIDATECALL(zetContextActivateMetricGroups(contextHandle, deviceHandle, 1, &metricGroup));
        std::cout << std::endl
                  << "MetricGroup activated" << std::endl;
    }

    ///////////////////////////
    /// deactivateMetrics
    ///////////////////////////
    void deactivateMetrics() {

        VALIDATECALL(zetContextActivateMetricGroups(contextHandle, deviceHandle, 0, nullptr));
        std::cout << "MetricGroup deactivated" << std::endl;
    }

    ///////////////////////////
    /// executeOnlyWorkload
    ///////////////////////////
    void executeOnlyWorkload() {

        // Execute a test workload that will be measured by metric stream.
        executeWorkload();

        // Execute command list.
        exectuteCommandList();

        // Validate output data.
        validateResults();
    }

    ///////////////////////////
    /// executeStreamWorkload
    ///////////////////////////
    void executeStreamWorkload() {

        // Create metric stream instance.
        createMetricStream();

        // Append metric Streamer marker #1.
        // It allows to correlate time based reports from metric Streamer with a given workload (dispatch, copy, etc.).
        VALIDATECALL(zetCommandListAppendMetricStreamerMarker(commandList, metricStreamer, ++metricStreamerMarker));

        // Execute a test workload that will be measured by metric stream.
        executeWorkload();

        // Append metric Streamer marker #2.
        VALIDATECALL(zetCommandListAppendMetricStreamerMarker(commandList, metricStreamer, ++metricStreamerMarker));

        // Copying of data must finish before running the user function.
        VALIDATECALL(zeCommandListAppendBarrier(commandList, nullptr, 0, nullptr));

        // Append metric Streamer marker #3.
        VALIDATECALL(zetCommandListAppendMetricStreamerMarker(commandList, metricStreamer, ++metricStreamerMarker));

        // Execute command list.
        exectuteCommandList();

        // Validate output data.
        validateResults();

        // Obtain raw stream metrics.
        obtainRawStreamMetrics();

        // Obtain final stream metrics.
        obtainCalculatedMetrics();
    }

    ///////////////////////////
    /// executeQueryWorkload
    ///////////////////////////
    void executeQueryWorkload() {

        // Create metric query instance.
        createMetricQuery();

        // Metric query begin.
        VALIDATECALL(zetCommandListAppendMetricQueryBegin(reinterpret_cast<zet_command_list_handle_t>(commandList), queryHandle));

        executeWorkload();

        // Metric query end.
        VALIDATECALL(zetCommandListAppendMetricQueryEnd(reinterpret_cast<zet_command_list_handle_t>(commandList), queryHandle, notificationEvent, 0, nullptr));

        // An optional memory barrier to flush gpu caches.
        VALIDATECALL(zetCommandListAppendMetricMemoryBarrier(reinterpret_cast<zet_command_list_handle_t>(commandList)));

        // Execute command list.
        exectuteCommandList();

        // Validate output data.
        validateResults();

        // Obtain raw stream metrics.
        obtainRawQueryMetrics();

        // Obtain final stream metrics.
        obtainCalculatedMetrics();
    }

  private:
    ///////////////////////////
    /// enableMetrics
    ///////////////////////////
    void enableMetrics(bool enable) {

        if (enable) {
#if defined(_WIN32)
            _putenv(const_cast<char *>("ZET_ENABLE_METRICS=1"));
#else
            putenv(const_cast<char *>("ZET_ENABLE_METRICS=1"));
#endif
        }
    }

    ///////////////////////////
    /// create
    ///////////////////////////
    void create() {

        createL0();
        createDevice();
        createSubDevice();
        createCommandQueue();
        createCommandList();
        createEvent();
        createResources();
    }

    ///////////////////////////
    /// destroy
    ///////////////////////////
    void destroy() {

        // Close metric streamer.
        if (metricStreamer) {
            VALIDATECALL(zetMetricStreamerClose(metricStreamer));
            std::cout << "MetricStreamer closed" << std::endl;
        }

        // Destroy metric query pool.
        if (queryHandle) {
            VALIDATECALL(zetMetricQueryDestroy(queryHandle));
        }

        if (queryPoolHandle) {
            VALIDATECALL(zetMetricQueryPoolDestroy(queryPoolHandle));
        }
        // Destroy notification event.
        VALIDATECALL(zeMemFree(contextHandle, sourceBuffer));
        VALIDATECALL(zeMemFree(contextHandle, destinationBuffer));

        VALIDATECALL(zeEventDestroy(notificationEvent));
        VALIDATECALL(zeEventPoolDestroy(eventPool));

        VALIDATECALL(zeCommandListDestroy(commandList));

        VALIDATECALL(zeCommandQueueDestroy(commandQueue));
        VALIDATECALL(zeContextDestroy(contextHandle));
    }

    //////////////////////////
    /// createDevice
    ///////////////////////////
    void createL0() {

        static bool enableOnce = true;

        if (enableOnce) {
            enableOnce = false;
        } else {
            return;
        }

        VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));
    }

    ///////////////////////////
    /// createDevice
    ///////////////////////////
    void createDevice() {

        uint32_t driverCount = 0;
        uint32_t deviceCount = 0;
        uint32_t driverVersion = 0;
        ze_api_version_t apiVersion = {};
        ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
        std::vector<ze_device_handle_t> devices;

        // Obtain driver.
        VALIDATECALL(zeDriverGet(&driverCount, nullptr));
        VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));

        // Driver properties.
        if (verbose) {

            ze_driver_properties_t driverProperties = {};

            VALIDATECALL(zeDriverGetProperties(driverHandle, &driverProperties));
            driverVersion = driverProperties.driverVersion;

            const uint32_t driverMajorVersion = ZE_MAJOR_VERSION(driverVersion);
            const uint32_t driverMinorVersion = ZE_MINOR_VERSION(driverVersion);

            std::cout << "Driver version: "
                      << driverMajorVersion << "."
                      << driverMinorVersion << "\n";

            VALIDATECALL(zeDriverGetApiVersion(driverHandle, &apiVersion));

            const uint32_t apiMajorVersion = ZE_MAJOR_VERSION(apiVersion);
            const uint32_t apiMinorVersion = ZE_MINOR_VERSION(apiVersion);

            std::cout << "API version: "
                      << apiMajorVersion << "."
                      << apiMinorVersion << "\n";
        }

        // Obtain context.
        ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
        VALIDATECALL(zeContextCreate(driverHandle, &contextDesc, &contextHandle));

        // Obtain all devices.
        VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));
        devices.resize(deviceCount);
        VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

        printf("\nDevices count     : %u", deviceCount);
        printf("\nDevice index      : %u", device.deviceIndex);

        // Obtain selected device.
        VALIDATECALL(device.deviceIndex < deviceCount ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_INVALID_ARGUMENT);
        deviceHandle = devices[device.deviceIndex];

        // Obtain device properties.
        VALIDATECALL(zeDeviceGetProperties(deviceHandle, &deviceProperties));
    }

    ///////////////////////////
    /// createSubDevice
    ///////////////////////////
    void createSubDevice() {

        uint32_t subDevicesCount = 0;
        std::vector<ze_device_handle_t> subDevices;

        // Sub devices count.
        VALIDATECALL(zeDeviceGetSubDevices(deviceHandle, &subDevicesCount, nullptr));

        printf("\nSub devices count : %u", subDevicesCount);
        printf("\nSub device index  : %u\n", device.subDeviceIndex);
        printf("\nIs root device    : %u\n", device.subDeviceIndex == ROOT_DEVICE);

        const bool subDevicesAvailable = subDevicesCount > 0;
        const bool useRootDevice = device.subDeviceIndex == ROOT_DEVICE;

        if (subDevicesAvailable && !useRootDevice) {

            VALIDATECALL((device.subDeviceIndex < subDevicesCount ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_DEVICE_LOST));

            // Obtain sub devices.
            if (subDevicesCount > 1) {
                subDevices.resize(subDevicesCount);
                VALIDATECALL(zeDeviceGetSubDevices(deviceHandle, &subDevicesCount, subDevices.data()));
                deviceHandle = subDevices[device.subDeviceIndex];
            } else {
                printf("\nUsing root device.");
            }
        }
    }

    ///////////////////////////
    /// createCommandQueue
    ///////////////////////////
    void
    createCommandQueue() {

        uint32_t queueGroupsCount = 0;

        VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount, nullptr));

        if (queueGroupsCount == 0) {
            std::cout << "No queue groups found!\n";
            std::terminate();
        }

        std::vector<ze_command_queue_group_properties_t> queueProperties(queueGroupsCount);
        VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(deviceHandle, &queueGroupsCount,
                                                            queueProperties.data()));

        for (uint32_t i = 0; i < queueGroupsCount; ++i) {
            if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
                queueDescription.ordinal = i;
            }
        }

        queueDescription.index = 0;
        queueDescription.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        VALIDATECALL(zeCommandQueueCreate(contextHandle, deviceHandle, &queueDescription, &commandQueue));
    }

    ///////////////////////////
    /// createCommandList
    ///////////////////////////
    void createCommandList() {

        ze_command_list_desc_t commandListDesc = {};
        commandListDesc.commandQueueGroupOrdinal = queueDescription.ordinal;

        // Create command list.
        VALIDATECALL(zeCommandListCreate(contextHandle, deviceHandle, &commandListDesc, &commandList));
    }

    ///////////////////////////
    /// createResources
    ///////////////////////////
    void createResources() {

        ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
        deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        deviceDesc.ordinal = 0;

        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        hostDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;

        VALIDATECALL(zeMemAllocShared(contextHandle, &deviceDesc, &hostDesc,
                                      allocationSize, 1, deviceHandle,
                                      &sourceBuffer));
        VALIDATECALL(zeMemAllocShared(contextHandle, &deviceDesc, &hostDesc,
                                      allocationSize, 1, deviceHandle,
                                      &destinationBuffer));

        // Initialize memory
        memset(sourceBuffer, 55, allocationSize);
        memset(destinationBuffer, 0, allocationSize);
    }

    ///////////////////////////
    /// createEvent
    ///////////////////////////
    void createEvent() {

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ze_event_pool_flag_t::ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

        ze_event_desc_t notificationEventDesc = {};
        notificationEventDesc.index = 0;
        notificationEventDesc.wait = ze_event_scope_flag_t::ZE_EVENT_SCOPE_FLAG_HOST;
        notificationEventDesc.signal = ze_event_scope_flag_t::ZE_EVENT_SCOPE_FLAG_DEVICE;

        // Optional notification event to know if Streamer reports are ready to read.
        VALIDATECALL(zeEventPoolCreate(contextHandle, &eventPoolDesc, 1, &deviceHandle, &eventPool));
        VALIDATECALL(zeEventCreate(eventPool, &notificationEventDesc, &notificationEvent));
    }

    ///////////////////////////
    /// executeWorkload
    ///////////////////////////
    void executeWorkload() {

        std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);

        if (file.is_open()) {
            file.seekg(0, file.end);
            auto length = file.tellg();
            file.seekg(0, file.beg);

            std::cout << "Using copy_buffer_to_buffer.spv" << std::endl;

            std::unique_ptr<char[]> spirvInput(new char[length]);
            file.read(spirvInput.get(), length);

            ze_module_desc_t moduleDesc = {};
            ze_module_build_log_handle_t buildlog;
            moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
            moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
            moduleDesc.inputSize = length;
            moduleDesc.pBuildFlags = "";

            ze_module_handle_t module = nullptr;
            ze_kernel_handle_t kernel = nullptr;

            if (zeModuleCreate(contextHandle, deviceHandle, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
                size_t szLog = 0;
                zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

                char *strLog = (char *)malloc(szLog);
                zeModuleBuildLogGetString(buildlog, &szLog, strLog);
                std::cout << "Build log:" << strLog << std::endl;

                free(strLog);
            }
            VALIDATECALL(zeModuleBuildLogDestroy(buildlog));

            ze_kernel_desc_t kernelDesc = {};
            kernelDesc.pKernelName = "CopyBufferToBufferBytes";
            VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));

            uint32_t groupSizeX = 32u;
            uint32_t groupSizeY = 1u;
            uint32_t groupSizeZ = 1u;
            VALIDATECALL(zeKernelSuggestGroupSize(kernel, allocationSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
            VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

            uint32_t offset = 0;
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(destinationBuffer), &destinationBuffer));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(sourceBuffer), &sourceBuffer));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(uint32_t), &offset));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 3, sizeof(uint32_t), &offset));
            VALIDATECALL(zeKernelSetArgumentValue(kernel, 4, sizeof(uint32_t), &offset));

            ze_group_count_t dispatchTraits;
            dispatchTraits.groupCountX = allocationSize / groupSizeX;
            dispatchTraits.groupCountY = 1u;
            dispatchTraits.groupCountZ = 1u;

            VALIDATECALL(zeCommandListAppendLaunchKernel(commandList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));
            file.close();
        } else {

            std::cout << "Using zeCommandListAppendMemoryCopy" << std::endl;

            VALIDATECALL(zeCommandListAppendMemoryCopy(commandList, destinationBuffer, sourceBuffer, allocationSize, nullptr, 0, nullptr));
        }
    }

    //////////////////////////
    /// exectuteCommandList
    ///////////////////////////
    void exectuteCommandList() {

        // Close command list.
        VALIDATECALL(zeCommandListClose(commandList));

        // Execute workload.
        VALIDATECALL(zeCommandQueueExecuteCommandLists(commandQueue, 1, &commandList, nullptr));

        // If using async command queue, explicit sync must be used for correctness.
        VALIDATECALL(zeCommandQueueSynchronize(commandQueue, std::numeric_limits<uint32_t>::max()));

        // Check if notification event meets zet_metric_group_properties_t::notifyEveryNReports requirments.
        const bool notificationOccured = zeEventQueryStatus(notificationEvent) == ZE_RESULT_SUCCESS;
        std::cout << "Requested report count ready to read:  " << (notificationOccured ? "true" : "false") << std::endl;
    }

    //////////////////////////
    /// validateResults
    ///////////////////////////
    void validateResults() {

        // Validate.
        const bool outputValidationSuccessful = (memcmp(destinationBuffer, sourceBuffer, allocationSize) == 0);

        if (!outputValidationSuccessful) {
            // Validate
            uint8_t *srcCharBuffer = static_cast<uint8_t *>(sourceBuffer);
            uint8_t *dstCharBuffer = static_cast<uint8_t *>(destinationBuffer);
            for (size_t i = 0; i < allocationSize; i++) {
                if (srcCharBuffer[i] != dstCharBuffer[i]) {
                    std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                              << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                    break;
                }
            }
        }

        std::cout << "\nResults validation "
                  << (outputValidationSuccessful ? "PASSED" : "FAILED")
                  << std::endl;

        VALIDATECALL(outputValidationSuccessful ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)
    }

    ///////////////////////////
    /// createMetricStream
    ///////////////////////////
    void createMetricStream() {

        zet_metric_streamer_desc_t streamerProperties = {};
        streamerProperties.notifyEveryNReports = notifyReportCount;
        streamerProperties.samplingPeriod = samplingPeriod;

        VALIDATECALL(zetMetricStreamerOpen(contextHandle, deviceHandle, metricGroup, &streamerProperties, notificationEvent, &metricStreamer));

        std::cout << "Metric Streamer opened"
                  << " StreamerProp.notifyEveryNReports: " << streamerProperties.notifyEveryNReports
                  << " StreamerProp.samplingPeriod: " << streamerProperties.samplingPeriod << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ///////////////////////////
    /// createMetricQuery
    ///////////////////////////
    void createMetricQuery() {

        queryPoolDesc.count = queryPoolCount;
        queryPoolDesc.type = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;

        // Create metric query pool.
        VALIDATECALL(zetMetricQueryPoolCreate(contextHandle, deviceHandle, metricGroup, &queryPoolDesc, &queryPoolHandle));

        // Obtain metric query from pool.
        VALIDATECALL(zetMetricQueryCreate(queryPoolHandle, querySlotIndex, &queryHandle));
    }

    ///////////////////////////
    /// obtainRawStreamMetrics
    ///////////////////////////
    void obtainRawStreamMetrics() {

        // Read raw buffer size.
        VALIDATECALL(zetMetricStreamerReadData(metricStreamer, maxRawReportCount, &rawDataSize, nullptr));
        std::cout << "Streamer read requires: " << rawDataSize << " bytes buffer" << std::endl;

        // Read raw data.
        rawData.resize(rawDataSize, 0);
        VALIDATECALL(zetMetricStreamerReadData(metricStreamer, maxRawReportCount, &rawDataSize, rawData.data()));
        std::cout << "Streamer read raw bytes: " << rawDataSize << std::endl;
    }

    ///////////////////////////
    /// obtainRawQueryMetrics
    ///////////////////////////
    void obtainRawQueryMetrics() {

        // Obtain metric query report size.
        VALIDATECALL(zetMetricQueryGetData(queryHandle, &rawDataSize, nullptr));

        // Obtain report.
        rawData.resize(rawDataSize);
        VALIDATECALL(zetMetricQueryGetData(queryHandle, &rawDataSize, rawData.data()));
    }

    ///////////////////////////
    /// obtainCalculatedMetrics
    ///////////////////////////
    void obtainCalculatedMetrics() {

        uint32_t setCount = 0;
        uint32_t totalCalculatedMetricCount = 0;
        zet_metric_group_properties_t properties = {};
        std::vector<uint32_t> metricCounts = {};
        std::vector<zet_typed_value_t> results = {};
        std::vector<zet_metric_handle_t> metrics = {};
        ze_result_t result = ZE_RESULT_SUCCESS;

        // Obtain maximum space for calculated metrics.
        result = zetMetricGroupCalculateMetricValues(
            metricGroup,
            ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
            rawDataSize, rawData.data(),
            &totalCalculatedMetricCount,
            nullptr);

        if (result == ZE_RESULT_ERROR_UNKNOWN) {

            // Try to use calculate for multiple metric values.
            VALIDATECALL(zetMetricGroupCalculateMultipleMetricValuesExp(
                metricGroup,
                ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                rawDataSize, rawData.data(),
                &setCount, &totalCalculatedMetricCount,
                nullptr, nullptr));

            // Allocate space for calculated reports.
            metricCounts.resize(setCount);
            results.resize(totalCalculatedMetricCount);

            // Obtain calculated metrics and their count.
            VALIDATECALL(zetMetricGroupCalculateMultipleMetricValuesExp(
                metricGroup,
                ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                rawDataSize, rawData.data(),
                &setCount, &totalCalculatedMetricCount,
                metricCounts.data(), results.data()));

        } else {
            // Allocate space for calculated reports.
            setCount = 1;
            metricCounts.resize(setCount);
            results.resize(totalCalculatedMetricCount);
            metricCounts[0] = totalCalculatedMetricCount;

            // Obtain calculated metrics and their count.
            VALIDATECALL(zetMetricGroupCalculateMetricValues(
                metricGroup,
                ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES,
                rawDataSize, rawData.data(),
                &totalCalculatedMetricCount, results.data()));
        }

        // Obtain metric group properties to show each metric.
        VALIDATECALL(zetMetricGroupGetProperties(metricGroup, &properties));

        // Allocate space for all metrics from a given metric group.
        metrics.resize(properties.metricCount);

        // Obtain metrics from a given metric group.
        VALIDATECALL(zetMetricGet(metricGroup, &properties.metricCount, metrics.data()));

        for (uint32_t i = 0; i < setCount; ++i) {

            std::cout << "\r\nSet " << i;

            const uint32_t metricCount = properties.metricCount;
            const uint32_t metricCountForSet = metricCounts[i];

            for (uint32_t j = 0; j < metricCountForSet; j++) {

                const uint32_t resultIndex = j + metricCount * i;
                const uint32_t metricIndex = j % metricCount;
                zet_metric_properties_t metricProperties = {};

                VALIDATECALL((resultIndex < totalCalculatedMetricCount) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

                // Obtain single metric properties to learn output value type.
                VALIDATECALL(zetMetricGetProperties(metrics[metricIndex], &metricProperties));

                VALIDATECALL((results[resultIndex].type == metricProperties.resultType) ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN)

                if (metricIndex == 0) {
                    std::cout << "\r\n";
                }

                std::cout << "\r\n";
                std::cout << std::setw(25) << metricProperties.name << ": ";

                switch (results[resultIndex].type) {
                case zet_value_type_t::ZET_VALUE_TYPE_BOOL8:
                    std::cout << std::setw(12);
                    std::cout << (results[resultIndex].value.b8 ? "true" : "false");
                    break;

                case zet_value_type_t::ZET_VALUE_TYPE_FLOAT32:
                    std::cout << std::setw(12);
                    std::cout << results[resultIndex].value.fp32;
                    break;

                case zet_value_type_t::ZET_VALUE_TYPE_FLOAT64:
                    std::cout << std::setw(12);
                    std::cout << results[resultIndex].value.fp64;
                    break;

                case zet_value_type_t::ZET_VALUE_TYPE_UINT32:
                    std::cout << std::setw(12);
                    std::cout << results[resultIndex].value.ui32;
                    break;

                case zet_value_type_t::ZET_VALUE_TYPE_UINT64:
                    std::cout << std::setw(12);
                    std::cout << results[resultIndex].value.ui64;
                    break;

                default:
                    break;
                }
            }

            std::cout << "\r\n";
        }
    }

    ///////////////////////////
    /// findMetricGroup
    ///////////////////////////
    void findMetricGroup(const char *groupName,
                         const zet_metric_group_sampling_type_flag_t samplingType) {

        uint32_t metricGroupCount = 0;
        std::vector<zet_metric_group_handle_t> metricGroups = {};

        // Obtain metric group count for a given device.
        VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, nullptr));

        // Obtain all metric groups.
        metricGroups.resize(metricGroupCount);
        VALIDATECALL(zetMetricGroupGet(deviceHandle, &metricGroupCount, metricGroups.data()));

        // Enumerate metric groups to find a particular one with a given group name
        // and sampling type requested by the user.
        for (uint32_t i = 0; i < metricGroupCount; ++i) {

            const zet_metric_group_handle_t metricGroupHandle = metricGroups[i];
            zet_metric_group_properties_t metricGroupProperties = {};

            // Obtain metric group properties to check the group name and sampling type.
            VALIDATECALL(zetMetricGroupGetProperties(metricGroupHandle, &metricGroupProperties));

            printMetricGroupProperties(metricGroupProperties);

            const bool validGroupName = strcmp(metricGroupProperties.name, groupName) == 0;
            const bool validSamplingType = (metricGroupProperties.samplingType & samplingType);

            // Validating the name and sampling type.
            if (validSamplingType) {

                // Print metrics from metric group.
                uint32_t metricCount = 0;
                std::vector<zet_metric_handle_t> metrics = {};

                // Obtain metrics count for verbose purpose.
                VALIDATECALL(zetMetricGet(metricGroupHandle, &metricCount, nullptr));

                // Obtain metrics for verbose purpose.
                metrics.resize(metricCount);
                VALIDATECALL(zetMetricGet(metricGroupHandle, &metricCount, metrics.data()));

                // Enumerate metric group metrics for verbose purpose.
                for (uint32_t j = 0; j < metricCount; ++j) {

                    const zet_metric_handle_t metric = metrics[j];
                    zet_metric_properties_t metricProperties = {};

                    VALIDATECALL(zetMetricGetProperties(metric, &metricProperties));

                    printMetricProperties(metricProperties);
                }

                // Obtain metric group handle.
                if (validGroupName) {
                    metricGroup = metricGroupHandle;
                    return;
                }
            }
        }

        // Unable to find metric group.
        VALIDATECALL(ZE_RESULT_ERROR_UNKNOWN);
    }

    ///////////////////////////
    /// printMetricGroupProperties
    ///////////////////////////
    void printMetricGroupProperties(const zet_metric_group_properties_t &properties) {

        if (verbose) {
            std::cout << "METRIC GROUP: "
                      << "name: " << properties.name << ", "
                      << "desc: " << properties.description << ", "
                      << "samplingType: " << properties.samplingType << ", "
                      << "domain: " << properties.domain << ", "
                      << "metricCount: " << properties.metricCount << std::endl;
        }
    }

    ///////////////////////////
    /// printMetricProperties
    ///////////////////////////
    void printMetricProperties(const zet_metric_properties_t &properties) {

        if (verbose) {
            std::cout << "\tMETRIC: "
                      << "name: " << properties.name << ", "
                      << "desc: " << properties.description << ", "
                      << "component: " << properties.component << ", "
                      << "tier: " << properties.tierNumber << ", "
                      << "metricType: " << properties.metricType << ", "
                      << "resultType: " << properties.resultType << ", "
                      << "units: " << properties.resultUnits << std::endl;
        }
    }

    ///////////////////////////
    /// wait
    ///////////////////////////
    void wait(int argc, char *argv[]) {

        static bool waitEnabled = isArgumentEnabled(argc, argv, "-w", "--wait");
        static uint32_t waitTime = getArgumentValue(argc, argv, "-w", "--wait");

        if (waitEnabled) {
            for (uint32_t i = 0; i < waitTime; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                printf("\nwait %u", i);
            }
        }
    }
};

///////////////////////////
/// noMetric
///////////////////////////
bool sample(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== No metric: device " << 0 << " ====-" << std::endl;

    Sample sample({0, 0}, false, argc, argv);

    sample.executeOnlyWorkload();

    return true;
}

/////////////////////////////
///// query
/////////////////////////////
bool query(int argc, char *argv[], std::vector<Device> devices, std::vector<std::string> sets) {

    std::vector<Sample *> samples;

    // Create samples for each device.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples.push_back(new Sample(devices[i], true, argc, argv));
    }

    // Activate metric sets.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->activateMetrics(sets[i].c_str(), ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_EVENT_BASED);
    }

    // Activate metric sets.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->executeQueryWorkload();
    }

    // Deactivate metric sets.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->deactivateMetrics();
    }

    // Remove samples.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        delete samples[i];
    }

    return true;
}

///////////////////////////
/// query_device_0
///////////////////////////
bool query_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 ====-" << std::endl;

    query(argc, argv, {Device(0, 0)}, {"TestOa"});

    return true;
}

///////////////////////////
/// query_device_root
///////////////////////////
bool query_device_root(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 ====-" << std::endl;

    query(argc, argv, {Device(0, ROOT_DEVICE)}, {"TestOa"});

    return true;
}

///////////////////////////
/// query_device_0_sub_device_1
///////////////////////////
bool query_device_0_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / sub_device 1 ====-" << std::endl;

    query(argc, argv, {Device(0, 1)}, {"TestOa"});

    return true;
}

///////////////////////////
/// query_device_0_sub_device_0_1
///////////////////////////
bool query_device_0_sub_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / sub_device 0 : 1 ====-" << std::endl;

    query(argc, argv, {Device(0, 0), Device(0, 1)}, {"TestOa", "ComputeBasic"});

    return true;
}

///////////////////////////
/// query_device_1_sub_device_1
///////////////////////////
bool query_device_1_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 1 / sub_device 1 ====-" << std::endl;

    query(argc, argv, {Device(1, 1)}, {"TestOa"});

    return true;
}

///////////////////////////
/// query_device_1
///////////////////////////
bool query_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 1 ====-" << std::endl;

    query(argc, argv, {Device(1, 0)}, {"TestOa"});

    return true;
}

///////////////////////////
/// query_device_0_1
///////////////////////////
bool query_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric query: device 0 / 1 ====-" << std::endl;

    query(argc, argv, {Device(0, 0), Device(1, 0)}, {"TestOa", "ComputeBasic"});

    return true;
}

///////////////////////////
/// stream
///////////////////////////
bool stream(int argc, char *argv[], std::vector<Device> devices, std::vector<std::string> sets) {

    std::vector<Sample *> samples;

    // Create samples for each device.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples.push_back(new Sample(devices[i], true, argc, argv));
    }

    // Activate metric sets.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->activateMetrics(sets[i].c_str(), ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED);
    }

    // Execute workload.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->executeStreamWorkload();
    }

    // Deactivate metric sets.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        samples[i]->deactivateMetrics();
    }

    // Destroy samples.
    for (uint32_t i = 0; i < devices.size(); ++i) {
        delete samples[i];
    }

    return true;
}

///////////////////////////
/// stream_device_root
///////////////////////////
bool stream_device_root(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 ====-" << std::endl;

    stream(argc, argv, {Device(0, ROOT_DEVICE)}, {"TestOa"});

    return true;
}

///////////////////////////
/// stream_device_0
///////////////////////////
bool stream_device_0(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 ====-" << std::endl;

    stream(argc, argv, {Device(0, 0)}, {"TestOa"});

    return true;
}

///////////////////////////
/// stream_device_0_sub_device_1
///////////////////////////
bool stream_device_0_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / sub_device 1 ====-" << std::endl;

    stream(argc, argv, {Device(0, 1)}, {"TestOa"});

    return true;
}

///////////////////////////
/// stream_device_0_sub_device_0_1
///////////////////////////
bool stream_device_0_sub_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / sub_device 0 : 1 ====-" << std::endl;

    stream(argc, argv, {Device(0, 0), Device(0, 1)}, {"TestOa", "ComputeBasic"});

    return true;
}

///////////////////////////
/// stream_device_1
///////////////////////////
bool stream_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 1 ====-" << std::endl;

    stream(argc, argv, {Device(1, 0)}, {"TestOa"});

    return true;
}

///////////////////////////
/// stream_device_1_sub_device_1
///////////////////////////
bool stream_device_1_sub_device_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 1 / sub_device 1 ====-" << std::endl;

    stream(argc, argv, {Device(1, 1)}, {"TestOa"});

    return true;
}

///////////////////////////
/// stream_device_0_1
///////////////////////////
bool stream_device_0_1(int argc, char *argv[]) {

    std::cout << std::endl
              << "-==== Metric stream: device 0 / 1 ====-" << std::endl;

    stream(argc, argv, {Device(0, 0), Device(1, 0)}, {"TestOa", "ComputeBasic"});

    return true;
}

/////////////////////////////
///// main
/////////////////////////////
int main(int argc, char *argv[]) {

    printf("Zello metrics\n");
    fflush(stdout);

    std::map<std::string, std::function<bool(int argc, char *argv[])>> tests;

    tests["sample"] = sample;

    tests["query_device_root"] = query_device_root;
    tests["query_device_0"] = query_device_0;
    tests["query_device_0_sub_device_1"] = query_device_0_sub_device_1;
    tests["query_device_0_sub_device_0_1"] = query_device_0_sub_device_0_1;
    tests["query_device_1"] = query_device_1;
    tests["query_device_1_sub_device_1"] = query_device_1_sub_device_1;
    tests["query_device_0_1"] = query_device_0_1;

    tests["stream_device_root"] = stream_device_root;
    tests["stream_device_0"] = stream_device_0;
    tests["stream_device_0_sub_device_1"] = stream_device_0_sub_device_1;
    tests["stream_device_0_sub_device_0_1"] = stream_device_0_sub_device_0_1;
    tests["stream_device_1"] = stream_device_1;
    tests["stream_device_1_sub_device_1"] = stream_device_1_sub_device_1;
    tests["stream_device_0_1"] = stream_device_0_1;

    // Run test.
    for (auto &test : tests) {

        if (Sample::isArgumentEnabled(argc, argv, "", test.first.c_str())) {

            return test.second(argc, argv);
        }
    }

    // Print available tests.
    for (auto &test : tests) {
        std::cout << test.first.c_str() << std::endl;
    }

    return 0;
}
