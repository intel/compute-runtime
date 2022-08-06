/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zet_api.h>

namespace L0 {

ze_result_t zeKernelSetGlobalOffsetExp(
    ze_kernel_handle_t hKernel,
    uint32_t offsetX,
    uint32_t offsetY,
    uint32_t offsetZ);

ze_result_t zeImageGetMemoryPropertiesExp(
    ze_image_handle_t hImage,
    ze_image_memory_properties_exp_t *pMemoryProperties);

ze_result_t zeImageGetAllocPropertiesExt(
    ze_context_handle_t hContext,
    ze_image_handle_t hImage,
    ze_image_allocation_ext_properties_t *pAllocProperties);

ze_result_t zeImageViewCreateExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView);

ze_result_t zeEventQueryTimestampsExp(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_kernel_timestamp_result_t *pTimestamps);

ze_result_t zetMetricGroupCalculateMultipleMetricValuesExp(
    zet_metric_group_handle_t hMetricGroup,
    zet_metric_group_calculation_type_t type,
    size_t rawDataSize,
    const uint8_t *pRawData,
    uint32_t *pSetCount,
    uint32_t *pTotalMetricValueCount,
    uint32_t *pMetricCounts,
    zet_typed_value_t *pMetricValues);

ze_result_t zeFabricVertexGetExp(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phVertices);

ze_result_t zeFabricVertexGetSubVerticesExp(
    ze_fabric_vertex_handle_t hVertex,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phSubvertices);

ze_result_t zeFabricVertexGetPropertiesExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_fabric_vertex_exp_properties_t *pVertexProperties);

ze_result_t zeFabricVertexGetDeviceExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_device_handle_t *phDevice);

} // namespace L0
