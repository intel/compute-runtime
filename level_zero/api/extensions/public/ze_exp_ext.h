/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/driver_experimental/ze_bindless_image_exp.h"
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

ze_result_t zeImageViewCreateExt(
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

ze_result_t zetMetricGroupGetGlobalTimestampsExp(
    zet_metric_group_handle_t hMetricGroup,
    ze_bool_t synchronizedWithHost,
    uint64_t *globalTimestamp,
    uint64_t *metricTimestamp);

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

ze_result_t zeDeviceGetFabricVertexExp(
    ze_device_handle_t hDevice,
    ze_fabric_vertex_handle_t *phVertex);

ze_result_t zeFabricEdgeGetExp(
    ze_fabric_vertex_handle_t hVertexA,
    ze_fabric_vertex_handle_t hVertexB,
    uint32_t *pCount,
    ze_fabric_edge_handle_t *phEdges);

ze_result_t zeFabricEdgeGetVerticesExp(
    ze_fabric_edge_handle_t hEdge,
    ze_fabric_vertex_handle_t *phVertexA,
    ze_fabric_vertex_handle_t *phVertexB);

ze_result_t zeFabricEdgeGetPropertiesExp(
    ze_fabric_edge_handle_t hEdge,
    ze_fabric_edge_exp_properties_t *pEdgeProperties);

ze_result_t zeRTASBuilderCreateExp(
    ze_driver_handle_t hDriver,
    const ze_rtas_builder_exp_desc_t *pDescriptor,
    ze_rtas_builder_exp_handle_t *phBuilder);

ze_result_t zeRTASBuilderGetBuildPropertiesExp(
    ze_rtas_builder_exp_handle_t hBuilder,
    const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
    ze_rtas_builder_exp_properties_t *pProperties);

ze_result_t zeDriverRTASFormatCompatibilityCheckExp(
    ze_driver_handle_t hDriver,
    ze_rtas_format_exp_t rtasFormatA,
    ze_rtas_format_exp_t rtasFormatB);

ze_result_t zeRTASBuilderBuildExp(
    ze_rtas_builder_exp_handle_t hBuilder,
    const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
    void *pScratchBuffer,
    size_t scratchBufferSizeBytes,
    void *pRtasBuffer,
    size_t rtasBufferSizeBytes,
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
    void *pBuildUserPtr,
    ze_rtas_aabb_exp_t *pBounds,
    size_t *pRtasBufferSizeBytes);

ze_result_t zeRTASBuilderDestroyExp(
    ze_rtas_builder_exp_handle_t hBuilder);

ze_result_t zeRTASParallelOperationCreateExp(
    ze_driver_handle_t hDriver,
    ze_rtas_parallel_operation_exp_handle_t *phParallelOperation);

ze_result_t zeRTASParallelOperationGetPropertiesExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
    ze_rtas_parallel_operation_exp_properties_t *pProperties);

ze_result_t zeRTASParallelOperationJoinExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation);

ze_result_t zeRTASParallelOperationDestroyExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation);

ze_result_t zeMemSetAtomicAccessAttributeExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_atomic_attr_exp_flags_t attr);

ze_result_t zeMemGetAtomicAccessAttributeExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_atomic_attr_exp_flags_t *pAttr);

ze_result_t zeMemGetPitchFor2dImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t imageWidth,
    size_t imageHeight,
    unsigned int elementSizeInBytes,
    size_t *rowPitch);

ze_result_t zeImageGetDeviceOffsetExp(
    ze_image_handle_t hImage,
    uint64_t *pDeviceOffset);

} // namespace L0
