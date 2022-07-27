/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <level_zero/zet_api.h>

#include <iostream>
#include <vector>

#define VALIDATECALL(myZeCall)                  \
    do {                                        \
        if ((myZeCall) != ZE_RESULT_SUCCESS) {  \
            std::cout << "Validate Error at "   \
                      << #myZeCall << ": "      \
                      << __FILE__ << ": "       \
                      << __LINE__ << std::endl; \
            std::terminate();                   \
        }                                       \
    } while (0);

#define EXPECT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::cout << "Error @ " << __LINE__ << " of " << __FILE__ << "\n"; \
            std::terminate();                                                  \
        }                                                                      \
    } while (0);

class SystemParameter {

  public:
    SystemParameter(ze_device_handle_t device) : device(device) {}
    virtual void capture(std::string s) = 0;
    virtual void showAll(double minDiff) = 0;

  protected:
    ze_device_handle_t device = nullptr;
};

class ExecutionContext {

  public:
    ExecutionContext() = default;
    virtual ~ExecutionContext() = default;
    virtual bool run() = 0;

    virtual ze_driver_handle_t getDriverHandle(uint32_t index) = 0;
    virtual ze_context_handle_t getContextHandle(uint32_t index) = 0;
    virtual ze_device_handle_t getDeviceHandle(uint32_t index) = 0;
    virtual ze_command_queue_handle_t getCommandQueue(uint32_t index) = 0;
    virtual ze_command_list_handle_t getCommandList(uint32_t index) = 0;

    virtual void addActiveMetricGroup(zet_metric_group_handle_t &metricGroup);
    virtual void removeActiveMetricGroup(zet_metric_group_handle_t metricGroup);
    virtual bool activateMetricGroups();
    virtual bool deactivateMetricGroups();

    void addSystemParameterCapture(SystemParameter *sysParameter) {
        systemParameterList.push_back(sysParameter);
    }

  protected:
    std::vector<zet_metric_group_handle_t> activeMetricGroups = {};
    std::vector<SystemParameter *> systemParameterList = {};
};

class Workload {
  public:
    Workload(ExecutionContext *execCtxt) { executionCtxt = execCtxt; }
    virtual ~Workload() = default;
    virtual bool appendCommands() = 0;
    virtual bool validate() = 0;

  protected:
    ExecutionContext *executionCtxt = nullptr;
};

class Collector {
  public:
    Collector(ExecutionContext *execCtxt) { executionCtxt = execCtxt; }
    virtual ~Collector() = default;
    virtual bool prefixCommands() = 0;
    virtual bool suffixCommands() = 0;
    virtual bool isDataAvailable() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void showResults() = 0;

  protected:
    ExecutionContext *executionCtxt = nullptr;
};

class SingleDeviceSingleQueueExecutionCtxt : public ExecutionContext {

  public:
    SingleDeviceSingleQueueExecutionCtxt(uint32_t deviceIndex, int32_t subDeviceIndex = -1) { initialize(deviceIndex, subDeviceIndex); }
    ~SingleDeviceSingleQueueExecutionCtxt() override { finalize(); }
    bool run() override;

    ze_driver_handle_t getDriverHandle(uint32_t index) override { return driverHandle; }
    ze_context_handle_t getContextHandle(uint32_t index) override { return contextHandle; }
    ze_device_handle_t getDeviceHandle(uint32_t index) override { return deviceHandle; }
    ze_command_queue_handle_t getCommandQueue(uint32_t index) override { return commandQueue; }
    ze_command_list_handle_t getCommandList(uint32_t index) override { return commandList; }

    void setExecutionTimeInMilliseconds(uint32_t value) { executionTimeInMilliSeconds = value; }

  private:
    ze_driver_handle_t driverHandle = {};
    ze_context_handle_t contextHandle = {};
    ze_device_handle_t deviceHandle = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_command_list_handle_t commandList = {};
    bool initialize(uint32_t deviceIndex, int32_t subDeviceIndex);
    bool finalize();

    uint32_t executionTimeInMilliSeconds = 0;
};

class AppendMemoryCopyFromHeapToDeviceAndBackToHost : public Workload {
  public:
    AppendMemoryCopyFromHeapToDeviceAndBackToHost(ExecutionContext *execCtxt) : Workload(execCtxt) { initialize(); }
    ~AppendMemoryCopyFromHeapToDeviceAndBackToHost() override { finalize(); };
    bool appendCommands() override;
    bool validate() override;

  private:
    void initialize();
    void finalize();

  private:
    static constexpr size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder
    char *heapBuffer1 = nullptr;
    char *heapBuffer2 = nullptr;
    void *zeBuffer = nullptr;
};

class CopyBufferToBuffer : public Workload {
  public:
    CopyBufferToBuffer(ExecutionContext *execCtxt) : Workload(execCtxt) { initialize(); }
    ~CopyBufferToBuffer() override { finalize(); };
    bool appendCommands() override;
    bool validate() override;

  private:
    void initialize();
    void finalize();

  private:
    const uint32_t allocationSize = 4096;
    void *sourceBuffer = nullptr;
    void *destinationBuffer = nullptr;
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;
    bool executeFromSpirv = false;
};

class SingleMetricCollector : public Collector {

  protected:
    SingleMetricCollector(ExecutionContext *executionCtxt,
                          const char *metricGroupName,
                          const zet_metric_group_sampling_type_flag_t samplingType);
    ~SingleMetricCollector() override = default;

    bool prefixCommands() override = 0;
    bool suffixCommands() override = 0;
    bool isDataAvailable() override = 0;
    bool start() override = 0;
    bool stop() override = 0;
    void showResults() override = 0;

    zet_metric_group_handle_t metricGroup = {};
    zet_metric_group_sampling_type_flag_t samplingType = {};
};

class SingleMetricStreamerCollector : public SingleMetricCollector {

  public:
    SingleMetricStreamerCollector(ExecutionContext *executionCtxt,
                                  const char *metricGroupName);

    ~SingleMetricStreamerCollector() override = default;

    bool prefixCommands() override;
    bool suffixCommands() override;
    bool isDataAvailable() override;

    bool start() override;
    bool stop() override;

    void setNotifyReportCount(uint32_t reportCount) { notifyReportCount = reportCount; }
    void setSamplingPeriod(uint32_t period) { samplingPeriod = period; }
    void setMaxRequestRawReportCount(uint32_t reportCount) { maxRequestRawReportCount = reportCount; }

    void showResults() override;

  protected:
    zet_metric_streamer_handle_t metricStreamer = {};
    ze_event_pool_handle_t eventPool = {};
    ze_event_handle_t notificationEvent = {};
    uint32_t notifyReportCount = 1;
    uint32_t samplingPeriod = 10000;
    uint32_t maxRequestRawReportCount = 5;
};

class SingleMetricQueryCollector : public SingleMetricCollector {

  public:
    SingleMetricQueryCollector(ExecutionContext *executionCtxt,
                               const char *metricGroupName);

    ~SingleMetricQueryCollector() override = default;

    bool prefixCommands() override;
    bool suffixCommands() override;
    bool isDataAvailable() override;

    bool start() override;
    bool stop() override;

    void setAttributes(uint32_t notifyReportCount, zet_metric_query_pool_type_t queryPoolType);
    void showResults() override;

  private:
    zet_metric_query_pool_type_t queryPoolType = ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE;
    uint32_t queryPoolCount = 1000;
    uint32_t querySlotIndex = 0;
    ze_event_pool_handle_t eventPool = {};
    ze_event_handle_t notificationEvent = {};
    uint32_t notifyReportCount = 1;

    zet_metric_query_pool_handle_t queryPoolHandle = {};
    zet_metric_query_handle_t queryHandle = {};
    zet_metric_query_pool_desc_t queryPoolDesc = {};
};

class SingleDeviceTestRunner {

  public:
    SingleDeviceTestRunner(ExecutionContext *context) : executionCtxt(context) {}
    virtual ~SingleDeviceTestRunner() = default;

    void addCollector(Collector *collector);
    void addWorkload(Workload *workload);
    // In case of streamer collector only cases
    void disableCommandsExecution() { disableCommandListExecution = true; }
    virtual bool run();

  private:
    std::vector<Collector *> collectorList = {};
    std::vector<Workload *> workloadList = {};
    ExecutionContext *executionCtxt = nullptr;
    bool disableCommandListExecution = false;
};

class Power : public SystemParameter {
  public:
    Power(ze_device_handle_t device);
    virtual ~Power() = default;
    void capture(std::string s) override;
    void showAll(double minDiff) override;
    void getMinMax(double &min, double &max);

  private:
    std::vector<std::pair<zes_power_energy_counter_t, std::string>> energyCounters = {};
    zes_pwr_handle_t handle = nullptr;
};

class Frequency : public SystemParameter {
  public:
    Frequency(ze_device_handle_t device);
    virtual ~Frequency() = default;
    void capture(std::string s) override;
    void showAll(double minDiff) override;
    void getMinMax(double &min, double &max);

  private:
    zes_freq_handle_t handle = nullptr;
    std::vector<std::pair<double, std::string>> frequencies = {};
};