/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/extensions/public/ze_exp_ext.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/rtas/rtas.h"

namespace L0 {
ze_result_t zeKernelSetGlobalOffsetExp(
    ze_kernel_handle_t hKernel,
    uint32_t offsetX,
    uint32_t offsetY,
    uint32_t offsetZ) {
    return L0::Kernel::fromHandle(hKernel)->setGlobalOffsetExp(offsetX, offsetY, offsetZ);
}

ze_result_t zeImageGetMemoryPropertiesExp(
    ze_image_handle_t hImage,
    ze_image_memory_properties_exp_t *pMemoryProperties) {
    return L0::Image::fromHandle(hImage)->getMemoryProperties(pMemoryProperties);
}

ze_result_t zeImageGetAllocPropertiesExt(
    ze_context_handle_t hContext,
    ze_image_handle_t hImage,
    ze_image_allocation_ext_properties_t *pAllocProperties) {
    return L0::Context::fromHandle(hContext)->getImageAllocProperties(L0::Image::fromHandle(hImage), pAllocProperties);
}

ze_result_t zeImageViewCreateExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView) {
    return L0::Image::fromHandle(hImage)->createView(L0::Device::fromHandle(hDevice), desc, phImageView);
}

ze_result_t zeImageViewCreateExt(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView) {
    return L0::Image::fromHandle(hImage)->createView(L0::Device::fromHandle(hDevice), desc, phImageView);
}

ze_result_t zeEventQueryTimestampsExp(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_kernel_timestamp_result_t *pTimestamps) {
    return L0::Event::fromHandle(hEvent)->queryTimestampsExp(L0::Device::fromHandle(hDevice), pCount, pTimestamps);
}

ze_result_t zeFabricVertexGetExp(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phVertices) {

    return L0::DriverHandle::fromHandle(hDriver)->fabricVertexGetExp(pCount, phVertices);
}

ze_result_t zeFabricVertexGetSubVerticesExp(
    ze_fabric_vertex_handle_t hVertex,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phSubvertices) {

    return L0::FabricVertex::fromHandle(hVertex)->getSubVertices(pCount, phSubvertices);
}

ze_result_t zeFabricVertexGetPropertiesExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_fabric_vertex_exp_properties_t *pVertexProperties) {

    return L0::FabricVertex::fromHandle(hVertex)->getProperties(pVertexProperties);
}

ze_result_t zeFabricVertexGetDeviceExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_device_handle_t *phDevice) {

    return L0::FabricVertex::fromHandle(hVertex)->getDevice(phDevice);
}

ze_result_t zeDeviceGetFabricVertexExp(ze_device_handle_t hDevice, ze_fabric_vertex_handle_t *phVertex) {

    return L0::Device::fromHandle(hDevice)->getFabricVertex(phVertex);
}

ze_result_t zeFabricEdgeGetExp(ze_fabric_vertex_handle_t hVertexA, ze_fabric_vertex_handle_t hVertexB,
                               uint32_t *pCount, ze_fabric_edge_handle_t *phEdges) {

    return L0::FabricVertex::fromHandle(hVertexA)->edgeGet(hVertexB, pCount, phEdges);
}

ze_result_t zeFabricEdgeGetVerticesExp(ze_fabric_edge_handle_t hEdge, ze_fabric_vertex_handle_t *phVertexA,
                                       ze_fabric_vertex_handle_t *phVertexB) {
    return L0::FabricEdge::fromHandle(hEdge)->getVertices(phVertexA, phVertexB);
}

ze_result_t zeFabricEdgeGetPropertiesExp(ze_fabric_edge_handle_t hEdge,
                                         ze_fabric_edge_exp_properties_t *pEdgeProperties) {
    return L0::FabricEdge::fromHandle(hEdge)->getProperties(pEdgeProperties);
}

ze_result_t zeRTASBuilderCreateExp(ze_driver_handle_t hDriver,
                                   const ze_rtas_builder_exp_desc_t *pDescriptor,
                                   ze_rtas_builder_exp_handle_t *phBuilder) {
    return L0::DriverHandle::fromHandle(hDriver)->createRTASBuilder(pDescriptor, phBuilder);
}

ze_result_t zeRTASBuilderGetBuildPropertiesExp(ze_rtas_builder_exp_handle_t hBuilder,
                                               const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
                                               ze_rtas_builder_exp_properties_t *pProperties) {
    return L0::RTASBuilder::fromHandle(hBuilder)->getProperties(pBuildOpDescriptor, pProperties);
}

ze_result_t zeDriverRTASFormatCompatibilityCheckExp(ze_driver_handle_t hDriver,
                                                    ze_rtas_format_exp_t rtasFormatA,
                                                    ze_rtas_format_exp_t rtasFormatB) {
    return L0::DriverHandle::fromHandle(hDriver)->formatRTASCompatibilityCheck(rtasFormatA, rtasFormatB);
}

ze_result_t zeRTASBuilderBuildExp(ze_rtas_builder_exp_handle_t hBuilder,
                                  const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
                                  void *pScratchBuffer,
                                  size_t scratchBufferSizeBytes,
                                  void *pRtasBuffer,
                                  size_t rtasBufferSizeBytes,
                                  ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                  void *pBuildUserPtr,
                                  ze_rtas_aabb_exp_t *pBounds,
                                  size_t *pRtasBufferSizeBytes) {
    return L0::RTASBuilder::fromHandle(hBuilder)->build(pBuildOpDescriptor,
                                                        pScratchBuffer,
                                                        scratchBufferSizeBytes,
                                                        pRtasBuffer,
                                                        rtasBufferSizeBytes,
                                                        hParallelOperation,
                                                        pBuildUserPtr,
                                                        pBounds,
                                                        pRtasBufferSizeBytes);
}

ze_result_t zeRTASBuilderDestroyExp(ze_rtas_builder_exp_handle_t hBuilder) {
    return L0::RTASBuilder::fromHandle(hBuilder)->destroy();
}

ze_result_t zeRTASParallelOperationCreateExp(ze_driver_handle_t hDriver,
                                             ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
    return L0::DriverHandle::fromHandle(hDriver)->createRTASParallelOperation(phParallelOperation);
}

ze_result_t zeRTASParallelOperationGetPropertiesExp(ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
                                                    ze_rtas_parallel_operation_exp_properties_t *pProperties) {
    return L0::RTASParallelOperation::fromHandle(hParallelOperation)->getProperties(pProperties);
}

ze_result_t zeRTASParallelOperationJoinExp(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
    return L0::RTASParallelOperation::fromHandle(hParallelOperation)->join();
}

ze_result_t zeRTASParallelOperationDestroyExp(ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
    return L0::RTASParallelOperation::fromHandle(hParallelOperation)->destroy();
}

// RTAs Ext

ze_result_t zeRTASBuilderCreateExt(ze_driver_handle_t hDriver,
                                   const ze_rtas_builder_ext_desc_t *pDescriptor,
                                   ze_rtas_builder_ext_handle_t *phBuilder) {
    return L0::DriverHandle::fromHandle(hDriver)->createRTASBuilderExt(pDescriptor, phBuilder);
}

ze_result_t zeRTASBuilderGetBuildPropertiesExt(ze_rtas_builder_ext_handle_t hBuilder,
                                               const ze_rtas_builder_build_op_ext_desc_t *pBuildOpDescriptor,
                                               ze_rtas_builder_ext_properties_t *pProperties) {
    return L0::RTASBuilderExt::fromHandle(hBuilder)->getProperties(pBuildOpDescriptor, pProperties);
}

ze_result_t zeDriverRTASFormatCompatibilityCheckExt(ze_driver_handle_t hDriver,
                                                    ze_rtas_format_ext_t rtasFormatA,
                                                    ze_rtas_format_ext_t rtasFormatB) {
    return L0::DriverHandle::fromHandle(hDriver)->formatRTASCompatibilityCheckExt(rtasFormatA, rtasFormatB);
}

ze_result_t zeRTASBuilderBuildExt(ze_rtas_builder_ext_handle_t hBuilder,
                                  const ze_rtas_builder_build_op_ext_desc_t *pBuildOpDescriptor,
                                  void *pScratchBuffer,
                                  size_t scratchBufferSizeBytes,
                                  void *pRtasBuffer,
                                  size_t rtasBufferSizeBytes,
                                  ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                  void *pBuildUserPtr,
                                  ze_rtas_aabb_ext_t *pBounds,
                                  size_t *pRtasBufferSizeBytes) {
    return L0::RTASBuilderExt::fromHandle(hBuilder)->build(pBuildOpDescriptor,
                                                           pScratchBuffer,
                                                           scratchBufferSizeBytes,
                                                           pRtasBuffer,
                                                           rtasBufferSizeBytes,
                                                           hParallelOperation,
                                                           pBuildUserPtr,
                                                           pBounds,
                                                           pRtasBufferSizeBytes);
}

ze_result_t zeRTASBuilderDestroyExt(ze_rtas_builder_ext_handle_t hBuilder) {
    return L0::RTASBuilderExt::fromHandle(hBuilder)->destroy();
}

ze_result_t zeRTASBuilderCommandListAppendCopyExt(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return zeCommandListAppendMemoryCopy(hCommandList, dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
}

ze_result_t zeRTASParallelOperationCreateExt(ze_driver_handle_t hDriver,
                                             ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) {
    return L0::DriverHandle::fromHandle(hDriver)->createRTASParallelOperationExt(phParallelOperation);
}

ze_result_t zeRTASParallelOperationGetPropertiesExt(ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
                                                    ze_rtas_parallel_operation_ext_properties_t *pProperties) {
    return L0::RTASParallelOperationExt::fromHandle(hParallelOperation)->getProperties(pProperties);
}

ze_result_t zeRTASParallelOperationJoinExt(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
    return L0::RTASParallelOperationExt::fromHandle(hParallelOperation)->join();
}

ze_result_t zeRTASParallelOperationDestroyExt(ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
    return L0::RTASParallelOperationExt::fromHandle(hParallelOperation)->destroy();
}
// end RTAs Ext
ze_result_t zeMemSetAtomicAccessAttributeExp(ze_context_handle_t hContext, ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t attr) {
    return L0::Context::fromHandle(hContext)->setAtomicAccessAttribute(L0::Device::fromHandle(hDevice), ptr, size, attr);
}

ze_result_t zeMemGetAtomicAccessAttributeExp(ze_context_handle_t hContext, ze_device_handle_t hDevice, const void *ptr, size_t size, ze_memory_atomic_attr_exp_flags_t *pAttr) {
    return L0::Context::fromHandle(hContext)->getAtomicAccessAttribute(L0::Device::fromHandle(hDevice), ptr, size, pAttr);
}

ze_result_t ZE_APICALL zeMemGetPitchFor2dImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t imageWidth,
    size_t imageHeight,
    unsigned int elementSizeInBytes,
    size_t *rowPitch) {
    return L0::Context::fromHandle(toInternalType(hContext))->getPitchFor2dImage(toInternalType(hDevice), imageWidth, imageHeight, elementSizeInBytes, rowPitch);
}

ze_result_t ZE_APICALL zeImageGetDeviceOffsetExp(
    ze_image_handle_t hImage,
    uint64_t *pDeviceOffset) {
    return L0::Image::fromHandle(toInternalType(hImage))->getDeviceOffset(pDeviceOffset);
}

} // namespace L0

extern "C" {

ZE_APIEXPORT ze_result_t ZE_APICALL
zeKernelSetGlobalOffsetExp(
    ze_kernel_handle_t hKernel,
    uint32_t offsetX,
    uint32_t offsetY,
    uint32_t offsetZ) {
    return L0::zeKernelSetGlobalOffsetExp(hKernel, offsetX, offsetY, offsetZ);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetMemoryPropertiesExp(
    ze_image_handle_t hImage,
    ze_image_memory_properties_exp_t *pMemoryProperties) {
    return L0::zeImageGetMemoryPropertiesExp(hImage, pMemoryProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageGetAllocPropertiesExt(
    ze_context_handle_t hContext,
    ze_image_handle_t hImage,
    ze_image_allocation_ext_properties_t *pAllocProperties) {
    return L0::zeImageGetAllocPropertiesExt(hContext, hImage, pAllocProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageViewCreateExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView) {
    return L0::zeImageViewCreateExp(hContext, hDevice, desc, hImage, phImageView);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeImageViewCreateExt(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const ze_image_desc_t *desc,
    ze_image_handle_t hImage,
    ze_image_handle_t *phImageView) {
    return L0::zeImageViewCreateExt(hContext, hDevice, desc, hImage, phImageView);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeEventQueryTimestampsExp(
    ze_event_handle_t hEvent,
    ze_device_handle_t hDevice,
    uint32_t *pCount,
    ze_kernel_timestamp_result_t *pTimestamps) {
    return L0::zeEventQueryTimestampsExp(hEvent, hDevice, pCount, pTimestamps);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricVertexGetExp(
    ze_driver_handle_t hDriver,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phVertices) {
    return L0::zeFabricVertexGetExp(hDriver, pCount, phVertices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricVertexGetSubVerticesExp(
    ze_fabric_vertex_handle_t hVertex,
    uint32_t *pCount,
    ze_fabric_vertex_handle_t *phSubvertices) {
    return L0::zeFabricVertexGetSubVerticesExp(hVertex, pCount, phSubvertices);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricVertexGetPropertiesExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_fabric_vertex_exp_properties_t *pVertexProperties) {
    return L0::zeFabricVertexGetPropertiesExp(hVertex, pVertexProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricVertexGetDeviceExp(
    ze_fabric_vertex_handle_t hVertex,
    ze_device_handle_t *phDevice) {
    return L0::zeFabricVertexGetDeviceExp(hVertex, phDevice);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDeviceGetFabricVertexExp(
    ze_device_handle_t hDevice,
    ze_fabric_vertex_handle_t *phVertex) {
    return L0::zeDeviceGetFabricVertexExp(hDevice, phVertex);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricEdgeGetExp(
    ze_fabric_vertex_handle_t hVertexA,
    ze_fabric_vertex_handle_t hVertexB,
    uint32_t *pCount,
    ze_fabric_edge_handle_t *phEdges) {
    return L0::zeFabricEdgeGetExp(hVertexA, hVertexB, pCount, phEdges);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricEdgeGetVerticesExp(
    ze_fabric_edge_handle_t hEdge,
    ze_fabric_vertex_handle_t *phVertexA,
    ze_fabric_vertex_handle_t *phVertexB) {
    return L0::zeFabricEdgeGetVerticesExp(hEdge, phVertexA, phVertexB);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeFabricEdgeGetPropertiesExp(
    ze_fabric_edge_handle_t hEdge,
    ze_fabric_edge_exp_properties_t *pEdgeProperties) {
    return L0::zeFabricEdgeGetPropertiesExp(hEdge, pEdgeProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderCreateExp(
    ze_driver_handle_t hDriver,
    const ze_rtas_builder_exp_desc_t *pDescriptor,
    ze_rtas_builder_exp_handle_t *phBuilder) {
    return L0::zeRTASBuilderCreateExp(hDriver, pDescriptor, phBuilder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderGetBuildPropertiesExp(
    ze_rtas_builder_exp_handle_t hBuilder,
    const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
    ze_rtas_builder_exp_properties_t *pProperties) {
    return L0::zeRTASBuilderGetBuildPropertiesExp(hBuilder, pBuildOpDescriptor, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverRTASFormatCompatibilityCheckExp(
    ze_driver_handle_t hDriver,
    ze_rtas_format_exp_t rtasFormatA,
    ze_rtas_format_exp_t rtasFormatB) {
    return L0::zeDriverRTASFormatCompatibilityCheckExp(hDriver, rtasFormatA, rtasFormatB);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderBuildExp(
    ze_rtas_builder_exp_handle_t hBuilder,
    const ze_rtas_builder_build_op_exp_desc_t *pBuildOpDescriptor,
    void *pScratchBuffer,
    size_t scratchBufferSizeBytes,
    void *pRtasBuffer,
    size_t rtasBufferSizeBytes,
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
    void *pBuildUserPtr,
    ze_rtas_aabb_exp_t *pBounds,
    size_t *pRtasBufferSizeBytes) {
    return L0::zeRTASBuilderBuildExp(hBuilder, pBuildOpDescriptor, pScratchBuffer,
                                     scratchBufferSizeBytes, pRtasBuffer, rtasBufferSizeBytes,
                                     hParallelOperation, pBuildUserPtr, pBounds, pRtasBufferSizeBytes);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderDestroyExp(
    ze_rtas_builder_exp_handle_t hBuilder) {
    return L0::zeRTASBuilderDestroyExp(hBuilder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationCreateExp(
    ze_driver_handle_t hDriver,
    ze_rtas_parallel_operation_exp_handle_t *phParallelOperation) {
    return L0::zeRTASParallelOperationCreateExp(hDriver, phParallelOperation);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationGetPropertiesExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation,
    ze_rtas_parallel_operation_exp_properties_t *pProperties) {
    return L0::zeRTASParallelOperationGetPropertiesExp(hParallelOperation, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationJoinExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
    return L0::zeRTASParallelOperationJoinExp(hParallelOperation);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationDestroyExp(
    ze_rtas_parallel_operation_exp_handle_t hParallelOperation) {
    return L0::zeRTASParallelOperationDestroyExp(hParallelOperation);
}

// RTAS Ext

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderCreateExt(
    ze_driver_handle_t hDriver,
    const ze_rtas_builder_ext_desc_t *pDescriptor,
    ze_rtas_builder_ext_handle_t *phBuilder) {
    return L0::zeRTASBuilderCreateExt(hDriver, pDescriptor, phBuilder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderGetBuildPropertiesExt(
    ze_rtas_builder_ext_handle_t hBuilder,
    const ze_rtas_builder_build_op_ext_desc_t *pBuildOpDescriptor,
    ze_rtas_builder_ext_properties_t *pProperties) {
    return L0::zeRTASBuilderGetBuildPropertiesExt(hBuilder, pBuildOpDescriptor, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeDriverRTASFormatCompatibilityCheckExt(
    ze_driver_handle_t hDriver,
    ze_rtas_format_ext_t rtasFormatA,
    ze_rtas_format_ext_t rtasFormatB) {
    return L0::zeDriverRTASFormatCompatibilityCheckExt(hDriver, rtasFormatA, rtasFormatB);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderCommandListAppendCopyExt(
    ze_command_list_handle_t hCommandList,
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    return L0::zeRTASBuilderCommandListAppendCopyExt(hCommandList, dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderBuildExt(
    ze_rtas_builder_ext_handle_t hBuilder,
    const ze_rtas_builder_build_op_ext_desc_t *pBuildOpDescriptor,
    void *pScratchBuffer,
    size_t scratchBufferSizeBytes,
    void *pRtasBuffer,
    size_t rtasBufferSizeBytes,
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
    void *pBuildUserPtr,
    ze_rtas_aabb_ext_t *pBounds,
    size_t *pRtasBufferSizeBytes) {
    return L0::zeRTASBuilderBuildExt(hBuilder, pBuildOpDescriptor, pScratchBuffer,
                                     scratchBufferSizeBytes, pRtasBuffer, rtasBufferSizeBytes,
                                     hParallelOperation, pBuildUserPtr, pBounds, pRtasBufferSizeBytes);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASBuilderDestroyExt(
    ze_rtas_builder_ext_handle_t hBuilder) {
    return L0::zeRTASBuilderDestroyExt(hBuilder);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationCreateExt(
    ze_driver_handle_t hDriver,
    ze_rtas_parallel_operation_ext_handle_t *phParallelOperation) {
    return L0::zeRTASParallelOperationCreateExt(hDriver, phParallelOperation);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationGetPropertiesExt(
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation,
    ze_rtas_parallel_operation_ext_properties_t *pProperties) {
    return L0::zeRTASParallelOperationGetPropertiesExt(hParallelOperation, pProperties);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationJoinExt(
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
    return L0::zeRTASParallelOperationJoinExt(hParallelOperation);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeRTASParallelOperationDestroyExt(
    ze_rtas_parallel_operation_ext_handle_t hParallelOperation) {
    return L0::zeRTASParallelOperationDestroyExt(hParallelOperation);
}
// End RTAS Ext

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemSetAtomicAccessAttributeExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_atomic_attr_exp_flags_t attr) {
    return L0::zeMemSetAtomicAccessAttributeExp(hContext, hDevice, ptr, size, attr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zeMemGetAtomicAccessAttributeExp(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    const void *ptr,
    size_t size,
    ze_memory_atomic_attr_exp_flags_t *pAttr) {
    return L0::zeMemGetAtomicAccessAttributeExp(hContext, hDevice, ptr, size, pAttr);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeMemGetPitchFor2dImage(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t imageWidth,
    size_t imageHeight,
    unsigned int elementSizeInBytes,
    size_t *rowPitch) {
    return L0::zeMemGetPitchFor2dImage(hContext, hDevice, imageWidth, imageHeight, elementSizeInBytes, rowPitch);
}

ZE_APIEXPORT ze_result_t ZE_APICALL zeImageGetDeviceOffsetExp(
    ze_image_handle_t hImage,
    uint64_t *pDeviceOffset) {
    return L0::zeImageGetDeviceOffsetExp(hImage, pDeviceOffset);
}

} // extern "C"
