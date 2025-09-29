/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

extern "C" {

typedef struct _ze_gpu_driver_dditable_t {
    ze_dditable_t coreDdiTable;
} ze_gpu_driver_dditable_t;

extern ze_gpu_driver_dditable_t driverDdiTable;

} // extern "C"

template <typename FuncType>
inline void fillDdiEntry(FuncType &entry, FuncType function, ze_api_version_t loaderVersion, ze_api_version_t requiredVersion) {
    if (loaderVersion >= requiredVersion) {
        entry = function;
    }
}

namespace L0 {

struct DriverDispatch {
    DriverDispatch();
    ze_dditable_driver_t core{};
    zet_dditable_driver_t tools{};
    zes_dditable_driver_t sysman{};

    ze_rtas_builder_dditable_t coreRTASBuilder{};
    ze_rtas_builder_exp_dditable_t coreRTASBuilderExp{};
    ze_rtas_parallel_operation_exp_dditable_t coreRTASParallelOperationExp{};
    ze_rtas_parallel_operation_dditable_t coreRTASParallelOperation{};
    ze_global_dditable_t coreGlobal{};
    ze_driver_dditable_t coreDriver{};
    ze_driver_exp_dditable_t coreDriverExp{};
    ze_device_dditable_t coreDevice{};
    ze_device_exp_dditable_t coreDeviceExp{};
    ze_context_dditable_t coreContext{};
    ze_command_queue_dditable_t coreCommandQueue{};
    ze_command_list_dditable_t coreCommandList{};
    ze_command_list_exp_dditable_t coreCommandListExp{};
    ze_image_dditable_t coreImage{};
    ze_image_exp_dditable_t coreImageExp{};
    ze_mem_dditable_t coreMem{};
    ze_mem_exp_dditable_t coreMemExp{};
    ze_fence_dditable_t coreFence{};
    ze_event_pool_dditable_t coreEventPool{};
    ze_event_dditable_t coreEvent{};
    ze_event_exp_dditable_t coreEventExp{};
    ze_module_dditable_t coreModule{};
    ze_module_build_log_dditable_t coreModuleBuildLog{};
    ze_kernel_dditable_t coreKernel{};
    ze_kernel_exp_dditable_t coreKernelExp{};
    ze_sampler_dditable_t coreSampler{};
    ze_physical_mem_dditable_t corePhysicalMem{};
    ze_virtual_mem_dditable_t coreVirtualMem{};
    ze_fabric_vertex_exp_dditable_t coreFabricVertexExp{};
    ze_fabric_edge_exp_dditable_t coreFabricEdgeExp{};

    zet_metric_programmable_exp_dditable_t toolsMetricProgrammableExp{};
    zet_metric_tracer_exp_dditable_t toolsMetricTracerExp{};
    zet_metric_decoder_exp_dditable_t toolsMetricDecoderExp{};
    zet_device_dditable_t toolsDevice{};
    zet_device_exp_dditable_t toolsDeviceExp{};
    zet_context_dditable_t toolsContext{};
    zet_command_list_dditable_t toolsCommandList{};
    zet_command_list_exp_dditable_t toolsCommandListExp{};
    zet_module_dditable_t toolsModule{};
    zet_kernel_dditable_t toolsKernel{};
    zet_metric_dditable_t toolsMetric{};
    zet_metric_exp_dditable_t toolsMetricExp{};
    zet_metric_group_dditable_t toolsMetricGroup{};
    zet_metric_group_exp_dditable_t toolsMetricGroupExp{};
    zet_metric_streamer_dditable_t toolsMetricStreamer{};
    zet_metric_query_pool_dditable_t toolsMetricQueryPool{};
    zet_metric_query_dditable_t toolsMetricQuery{};
    zet_tracer_exp_dditable_t toolsTracerExp{};
    zet_debug_dditable_t toolsDebug{};

    zes_global_dditable_t sysmanGlobal{};
    zes_device_dditable_t sysmanDevice{};
    zes_device_exp_dditable_t sysmanDeviceExp{};
    zes_driver_dditable_t sysmanDriver{};
    zes_driver_exp_dditable_t sysmanDriverExp{};
    zes_overclock_dditable_t sysmanOverclock{};
    zes_scheduler_dditable_t sysmanScheduler{};
    zes_performance_factor_dditable_t sysmanPerformanceFactor{};
    zes_power_dditable_t sysmanPower{};
    zes_frequency_dditable_t sysmanFrequency{};
    zes_engine_dditable_t sysmanEngine{};
    zes_standby_dditable_t sysmanStandby{};
    zes_firmware_dditable_t sysmanFirmware{};
    zes_firmware_exp_dditable_t sysmanFirmwareExp{};
    zes_memory_dditable_t sysmanMemory{};
    zes_fabric_port_dditable_t sysmanFabricPort{};
    zes_temperature_dditable_t sysmanTemperature{};
    zes_psu_dditable_t sysmanPsu{};
    zes_fan_dditable_t sysmanFan{};
    zes_led_dditable_t sysmanLed{};
    zes_ras_dditable_t sysmanRas{};
    zes_ras_exp_dditable_t sysmanRasExp{};
    zes_diagnostics_dditable_t sysmanDiagnostics{};
    zes_vf_management_exp_dditable_t sysmanVFManagementExp{};
};

extern DriverDispatch globalDriverDispatch;

}; // namespace L0
