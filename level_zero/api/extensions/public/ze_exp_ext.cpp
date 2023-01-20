/*
 * Copyright (C) 2020-2022 Intel Corporation
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

} // extern "C"
