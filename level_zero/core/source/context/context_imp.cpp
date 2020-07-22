/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {

ze_result_t ContextImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::getStatus() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

DriverHandle *ContextImp::getDriverHandle() {
    return this->driverHandle;
}

ContextImp::ContextImp(DriverHandle *driverHandle) {
    this->driverHandle = driverHandle;
}

ze_result_t ContextImp::allocHostMem(ze_host_mem_alloc_flag_t flags,
                                     size_t size,
                                     size_t alignment,
                                     void **ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->allocHostMem(flags,
                                            size,
                                            alignment,
                                            ptr);
}

ze_result_t ContextImp::allocDeviceMem(ze_device_handle_t hDevice,
                                       ze_device_mem_alloc_flag_t flags,
                                       size_t size,
                                       size_t alignment, void **ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->allocDeviceMem(hDevice,
                                              flags,
                                              size,
                                              alignment,
                                              ptr);
}

ze_result_t ContextImp::allocSharedMem(ze_device_handle_t hDevice,
                                       ze_device_mem_alloc_flag_t deviceFlags,
                                       ze_host_mem_alloc_flag_t hostFlags,
                                       size_t size,
                                       size_t alignment,
                                       void **ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->allocSharedMem(hDevice,
                                              deviceFlags,
                                              hostFlags,
                                              size,
                                              alignment,
                                              ptr);
}

ze_result_t ContextImp::freeMem(const void *ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->freeMem(ptr);
}

ze_result_t ContextImp::getMemAddressRange(const void *ptr,
                                           void **pBase,
                                           size_t *pSize) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->getMemAddressRange(ptr,
                                                  pBase,
                                                  pSize);
}

ze_result_t ContextImp::closeIpcMemHandle(const void *ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->closeIpcMemHandle(ptr);
}

ze_result_t ContextImp::getIpcMemHandle(const void *ptr,
                                        ze_ipc_mem_handle_t *pIpcHandle) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->getIpcMemHandle(ptr,
                                               pIpcHandle);
}

ze_result_t ContextImp::openIpcMemHandle(ze_device_handle_t hDevice,
                                         ze_ipc_mem_handle_t handle,
                                         ze_ipc_memory_flags_t flags,
                                         void **ptr) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->openIpcMemHandle(hDevice,
                                                handle,
                                                ZE_IPC_MEMORY_FLAG_NONE,
                                                ptr);
}

ze_result_t ContextImp::getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) {
    DEBUG_BREAK_IF(nullptr == this->driverHandle);
    return this->driverHandle->getMemAllocProperties(ptr,
                                                     pMemAllocProperties,
                                                     phDevice);
}

ze_result_t ContextImp::createModule(ze_device_handle_t hDevice,
                                     const ze_module_desc_t *desc,
                                     ze_module_handle_t *phModule,
                                     ze_module_build_log_handle_t *phBuildLog) {
    return L0::Device::fromHandle(hDevice)->createModule(desc, phModule, phBuildLog);
}

ze_result_t ContextImp::createSampler(ze_device_handle_t hDevice,
                                      const ze_sampler_desc_t *pDesc,
                                      ze_sampler_handle_t *phSampler) {
    return L0::Device::fromHandle(hDevice)->createSampler(pDesc, phSampler);
}

ze_result_t ContextImp::createCommandQueue(ze_device_handle_t hDevice,
                                           const ze_command_queue_desc_t *desc,
                                           ze_command_queue_handle_t *commandQueue) {
    return L0::Device::fromHandle(hDevice)->createCommandQueue(desc, commandQueue);
}

ze_result_t ContextImp::createCommandList(ze_device_handle_t hDevice,
                                          const ze_command_list_desc_t *desc,
                                          ze_command_list_handle_t *commandList) {
    return L0::Device::fromHandle(hDevice)->createCommandList(desc, commandList);
}

ze_result_t ContextImp::createCommandListImmediate(ze_device_handle_t hDevice,
                                                   const ze_command_queue_desc_t *desc,
                                                   ze_command_list_handle_t *commandList) {
    return L0::Device::fromHandle(hDevice)->createCommandListImmediate(desc, commandList);
}

} // namespace L0
