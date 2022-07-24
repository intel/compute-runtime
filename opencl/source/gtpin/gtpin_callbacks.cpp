/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_defs.h"
#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/mem_obj/buffer.h"

#include "CL/cl.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

#include <deque>
#include <vector>

using namespace gtpin;

namespace NEO {

using GTPinLockType = std::recursive_mutex;

extern gtpin::ocl::gtpin_events_t GTPinCallbacks;

igc_init_t *pIgcInit = nullptr;
std::atomic<int> sequenceCount(1);
CommandQueue *pCmdQueueForFlushTask = nullptr;
std::deque<gtpinkexec_t> kernelExecQueue;
GTPinLockType kernelExecQueueLock;

void gtpinNotifyContextCreate(cl_context context) {
    if (isGTPinInitialized) {
        platform_info_t gtpinPlatformInfo;
        auto pContext = castToObjectOrAbort<Context>(context);
        auto pDevice = pContext->getDevice(0);
        UNRECOVERABLE_IF(pDevice == nullptr);
        GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        gtpinPlatformInfo.gen_version = (gtpin::GTPIN_GEN_VERSION)gtpinHelper.getGenVersion();
        gtpinPlatformInfo.device_id = static_cast<uint32_t>(pDevice->getHardwareInfo().platform.usDeviceID);
        (*GTPinCallbacks.onContextCreate)((context_handle_t)context, &gtpinPlatformInfo, &pIgcInit);
    }
}

void gtpinNotifyContextDestroy(cl_context context) {
    if (isGTPinInitialized) {
        (*GTPinCallbacks.onContextDestroy)((context_handle_t)context);
    }
}

void gtpinNotifyKernelCreate(cl_kernel kernel) {
    if (nullptr == kernel) {
        return;
    }
    if (isGTPinInitialized) {
        auto pMultiDeviceKernel = castToObjectOrAbort<MultiDeviceKernel>(kernel);
        auto pKernel = pMultiDeviceKernel->getDefaultKernel();
        auto &device = pMultiDeviceKernel->getDevices()[0]->getDevice();
        size_t gtpinBTI = pKernel->getNumberOfBindingTableStates();
        // Enlarge local copy of SSH by 1 SS
        GFXCORE_FAMILY genFamily = device.getHardwareInfo().platform.eRenderCoreFamily;
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
        if (!gtpinHelper.addSurfaceState(pKernel)) {
            // Kernel with no SSH or Kernel EM, not supported
            return;
        }
        if (pKernel->isKernelHeapSubstituted()) {
            // ISA for this kernel was already substituted
            return;
        }
        // Notify GT-Pin that new kernel was created
        Context *pContext = &(pKernel->getContext());
        cl_context context = pContext;
        auto &kernelInfo = pKernel->getKernelInfo();
        instrument_params_in_t paramsIn = {};

        paramsIn.kernel_type = GTPIN_KERNEL_TYPE_CS;
        paramsIn.simd = (GTPIN_SIMD_WIDTH)kernelInfo.getMaxSimdSize();
        paramsIn.orig_kernel_binary = (uint8_t *)pKernel->getKernelHeap();
        paramsIn.orig_kernel_size = static_cast<uint32_t>(pKernel->getKernelHeapSize());
        paramsIn.buffer_type = GTPIN_BUFFER_BINDFULL;
        paramsIn.buffer_desc.BTI = static_cast<uint32_t>(gtpinBTI);
        paramsIn.igc_hash_id = kernelInfo.shaderHashCode;
        paramsIn.kernel_name = (char *)kernelInfo.kernelDescriptor.kernelMetadata.kernelName.c_str();
        paramsIn.igc_info = kernelInfo.igcInfoForGtpin;
        if (kernelInfo.debugData.vIsa != nullptr) {
            paramsIn.debug_data = kernelInfo.debugData.vIsa;
            paramsIn.debug_data_size = static_cast<uint32_t>(kernelInfo.debugData.vIsaSize);
        } else {
            paramsIn.debug_data = nullptr;
            paramsIn.debug_data_size = 0;
        }
        instrument_params_out_t paramsOut = {0};
        (*GTPinCallbacks.onKernelCreate)((context_handle_t)(cl_context)context, &paramsIn, &paramsOut);
        // Substitute ISA of created kernel with instrumented code
        pKernel->substituteKernelHeap(paramsOut.inst_kernel_binary, paramsOut.inst_kernel_size);
        pKernel->setKernelId(paramsOut.kernel_id);
    }
}

void gtpinNotifyKernelSubmit(cl_kernel kernel, void *pCmdQueue) {
    if (isGTPinInitialized) {
        auto pCmdQ = reinterpret_cast<CommandQueue *>(pCmdQueue);
        auto &device = pCmdQ->getDevice();
        auto rootDeviceIndex = device.getRootDeviceIndex();
        auto pMultiDeviceKernel = castToObjectOrAbort<MultiDeviceKernel>(kernel);
        auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
        if (pKernel->getSurfaceStateHeapSize() == 0) {
            // Kernel with no SSH, not supported
            return;
        }
        Context *pContext = &(pKernel->getContext());
        cl_context context = (cl_context)pContext;
        uint64_t kernelId = pKernel->getKernelId();
        command_buffer_handle_t commandBuffer = (command_buffer_handle_t)((uintptr_t)(sequenceCount++));
        uint32_t kernelOffset = 0;
        resource_handle_t resource = 0;
        // Notify GT-Pin that abstract "command buffer" was created
        (*GTPinCallbacks.onCommandBufferCreate)((context_handle_t)context, commandBuffer);
        // Notify GT-Pin that kernel was submited for execution
        (*GTPinCallbacks.onKernelSubmit)(commandBuffer, kernelId, &kernelOffset, &resource);
        // Create new record in Kernel Execution Queue describing submited kernel
        pKernel->setStartOffset(kernelOffset);
        gtpinkexec_t kExec;
        kExec.pKernel = pKernel;
        kExec.gtpinResource = (cl_mem)resource;
        kExec.commandBuffer = commandBuffer;
        kExec.pCommandQueue = (CommandQueue *)pCmdQueue;
        std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
        kernelExecQueue.push_back(kExec);
        lock.unlock();
        // Patch SSH[gtpinBTI] with GT-Pin resource
        if (!resource) {
            return;
        }
        GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
        size_t gtpinBTI = pKernel->getNumberOfBindingTableStates() - 1;
        void *pSurfaceState = gtpinHelper.getSurfaceState(pKernel, gtpinBTI);
        if (gtpinHelper.canUseSharedAllocation(device.getHardwareInfo())) {
            auto allocData = reinterpret_cast<SvmAllocationData *>(resource);
            auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
            size_t size = gpuAllocation->getUnderlyingBufferSize();
            Buffer::setSurfaceState(&device, pSurfaceState, false, false, size, gpuAllocation->getUnderlyingBuffer(), 0, gpuAllocation, 0, 0,
                                    pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, pContext->getNumDevices());
            device.getMemoryManager()->getPageFaultManager()->moveAllocationToGpuDomain(reinterpret_cast<void *>(gpuAllocation->getGpuAddress()));
        } else {
            cl_mem buffer = (cl_mem)resource;
            auto pBuffer = castToObjectOrAbort<Buffer>(buffer);
            pBuffer->setArgStateful(pSurfaceState, false, false, false, false, device,
                                    pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, pContext->getNumDevices());
        }
    }
}

void gtpinNotifyPreFlushTask(void *pCmdQueue) {
    if (isGTPinInitialized) {
        pCmdQueueForFlushTask = (CommandQueue *)pCmdQueue;
    }
}

void gtpinNotifyFlushTask(uint32_t flushedTaskCount) {
    if (isGTPinInitialized) {
        std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pCommandQueue == pCmdQueueForFlushTask) && !kernelExecQueue[n].isTaskCountValid) {
                // Update record in Kernel Execution Queue with kernel's TC
                kernelExecQueue[n].isTaskCountValid = true;
                kernelExecQueue[n].taskCount = flushedTaskCount;
                break;
            }
        }
        pCmdQueueForFlushTask = nullptr;
    }
}

void gtpinNotifyTaskCompletion(uint32_t completedTaskCount) {
    std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
    size_t numElems = kernelExecQueue.size();
    for (size_t n = 0; n < numElems;) {
        if (kernelExecQueue[n].isTaskCountValid && (kernelExecQueue[n].taskCount <= completedTaskCount)) {
            // Notify GT-Pin that execution of "command buffer" was completed
            (*GTPinCallbacks.onCommandBufferComplete)(kernelExecQueue[n].commandBuffer);
            // Remove kernel's record from Kernel Execution Queue
            kernelExecQueue.erase(kernelExecQueue.begin() + n);
            numElems--;
        } else {
            n++;
        }
    }
}

void gtpinNotifyMakeResident(void *pKernel, void *pCSR) {
    if (isGTPinInitialized) {
        std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pKernel == pKernel) && !kernelExecQueue[n].isResourceResident && kernelExecQueue[n].gtpinResource) {
                // It's time for kernel to make resident its GT-Pin resource
                CommandStreamReceiver *pCommandStreamReceiver = reinterpret_cast<CommandStreamReceiver *>(pCSR);
                GraphicsAllocation *pGfxAlloc = nullptr;
                Context &context = static_cast<Kernel *>(pKernel)->getContext();
                GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);
                if (gtpinHelper.canUseSharedAllocation(context.getDevice(0)->getHardwareInfo())) {
                    auto allocData = reinterpret_cast<SvmAllocationData *>(kernelExecQueue[n].gtpinResource);
                    pGfxAlloc = allocData->gpuAllocations.getGraphicsAllocation(pCommandStreamReceiver->getRootDeviceIndex());
                } else {
                    cl_mem gtpinBuffer = kernelExecQueue[n].gtpinResource;
                    auto pBuffer = castToObjectOrAbort<Buffer>(gtpinBuffer);
                    pGfxAlloc = pBuffer->getGraphicsAllocation(pCommandStreamReceiver->getRootDeviceIndex());
                }
                pCommandStreamReceiver->makeResident(*pGfxAlloc);
                kernelExecQueue[n].isResourceResident = true;
                break;
            }
        }
    }
}

void gtpinNotifyUpdateResidencyList(void *pKernel, void *pResVec) {
    if (isGTPinInitialized) {
        std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
        size_t numElems = kernelExecQueue.size();
        for (size_t n = 0; n < numElems; n++) {
            if ((kernelExecQueue[n].pKernel == pKernel) && !kernelExecQueue[n].isResourceResident && kernelExecQueue[n].gtpinResource) {
                // It's time for kernel to update its residency list with its GT-Pin resource
                std::vector<Surface *> *pResidencyVector = (std::vector<Surface *> *)pResVec;
                cl_mem gtpinBuffer = kernelExecQueue[n].gtpinResource;
                auto pBuffer = castToObjectOrAbort<Buffer>(gtpinBuffer);
                auto rootDeviceIndex = kernelExecQueue[n].pCommandQueue->getDevice().getRootDeviceIndex();
                GraphicsAllocation *pGfxAlloc = pBuffer->getGraphicsAllocation(rootDeviceIndex);
                GeneralSurface *pSurface = new GeneralSurface(pGfxAlloc);
                pResidencyVector->push_back(pSurface);
                kernelExecQueue[n].isResourceResident = true;
                break;
            }
        }
    }
}

void gtpinNotifyPlatformShutdown() {
    if (isGTPinInitialized) {
        // Clear Kernel Execution Queue
        kernelExecQueue.clear();
    }
}
void *gtpinGetIgcInit() {
    return pIgcInit;
}
void gtpinSetIgcInit(void *pIgcInitPtr) {
    pIgcInit = static_cast<igc_init_t *>(pIgcInitPtr);
}

void gtpinRemoveCommandQueue(void *pCmdQueue) {
    if (isGTPinInitialized) {
        std::unique_lock<GTPinLockType> lock{kernelExecQueueLock};
        size_t n = 0;
        while (n < kernelExecQueue.size()) {
            if (kernelExecQueue[n].pCommandQueue == pCmdQueue) {
                kernelExecQueue.erase(kernelExecQueue.begin() + n);
            } else {
                n++;
            }
        }
    }
}

} // namespace NEO
