/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/aub_subcapture_status.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/api_intercept.h"
#include "shared/source/utilities/staging_buffer_manager.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/csr_selection_args.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event_builder.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/convert_color.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/mipmap.h"
#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/migration_controller.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/sharings/sharing.h"

#include "CL/cl_ext.h"

#include <limits>
#include <map>

namespace NEO {

// Global table of create functions
CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE] = {};

CommandQueue *CommandQueue::create(Context *context,
                                   ClDevice *device,
                                   const cl_queue_properties *properties,
                                   bool internalUsage,
                                   cl_int &retVal) {
    retVal = CL_SUCCESS;

    auto funcCreate = commandQueueFactory[device->getRenderCoreFamily()];
    DEBUG_BREAK_IF(nullptr == funcCreate);

    return funcCreate(context, device, properties, internalUsage);
}

cl_int CommandQueue::getErrorCodeFromTaskCount(TaskCountType taskCount) {
    switch (taskCount) {
    case CompletionStamp::failed:
    case CompletionStamp::gpuHang:
    case CompletionStamp::outOfDeviceMemory:
        return CL_OUT_OF_RESOURCES;
    case CompletionStamp::outOfHostMemory:
        return CL_OUT_OF_HOST_MEMORY;
    default:
        return CL_SUCCESS;
    }
}

CommandQueue::CommandQueue(Context *context, ClDevice *device, const cl_queue_properties *properties, bool internalUsage)
    : context(context), device(device), isInternalUsage(internalUsage) {
    if (context) {
        context->incRefInternal();
    }

    commandQueueProperties = getCmdQueueProperties<cl_command_queue_properties>(properties);
    flushStamp.reset(new FlushStampTracker(true));

    storeProperties(properties);
    processProperties(properties);

    if (device) {
        auto &hwInfo = device->getHardwareInfo();
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto &productHelper = device->getProductHelper();
        auto &compilerProductHelper = device->getCompilerProductHelper();
        auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();

        bcsAllowed = !device->getDevice().isAnyDirectSubmissionLightEnabled() &&
                     productHelper.isBlitterFullySupported(hwInfo) &&
                     gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, device->getDeviceBitfield(), aub_stream::EngineType::ENGINE_BCS);

        if (bcsAllowed || device->getDefaultEngine().commandStreamReceiver->peekTimestampPacketWriteEnabled()) {
            timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
            deferredTimestampPackets = std::make_unique<TimestampPacketContainer>();
        }
        if (context && context->getRootDeviceIndices().size() > 1) {
            deferredMultiRootSyncNodes = std::make_unique<TimestampPacketContainer>();
        }
        auto deferCmdQBcsInitialization = hwInfo.featureTable.ftrBcsInfo.count() > 1u;

        if (debugManager.flags.DeferCmdQBcsInitialization.get() != -1) {
            deferCmdQBcsInitialization = debugManager.flags.DeferCmdQBcsInitialization.get();
        }

        if (!deferCmdQBcsInitialization) {
            this->constructBcsEngine(internalUsage);
        }

        if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getDevice().getL0Debugger()) {
            device->getDevice().getL0Debugger()->notifyCommandQueueCreated(&device->getDevice());
        }

        this->heaplessModeEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);
        this->heaplessStateInitEnabled = compilerProductHelper.isHeaplessStateInitEnabled(this->heaplessModeEnabled);
        this->isForceStateless = compilerProductHelper.isForceToStatelessRequired();
        this->l3FlushAfterPostSyncEnabled = productHelper.isL3FlushAfterPostSyncSupported(this->heaplessModeEnabled);
        this->shouldRegisterEnqueuedWalkerWithProfiling = productHelper.shouldRegisterEnqueuedWalkerWithProfiling();
    }
}

CommandQueue::~CommandQueue() {
    if (virtualEvent) {
        UNRECOVERABLE_IF(this->virtualEvent->getCommandQueue() != this && this->virtualEvent->getCommandQueue() != nullptr);
        virtualEvent->decRefInternal();
    }

    if (device) {
        if (commandStream) {
            auto storageForAllocation = gpgpuEngine->commandStreamReceiver->getInternalAllocationStorage();
            storageForAllocation->storeAllocation(std::unique_ptr<GraphicsAllocation>(commandStream->getGraphicsAllocation()), REUSABLE_ALLOCATION);
        }
        delete commandStream;

        if (this->perfCountersEnabled) {
            device->getPerformanceCounters()->shutdown();
        }

        this->releaseMainCopyEngine();

        if (NEO::Debugger::isDebugEnabled(isInternalUsage) && device->getDevice().getL0Debugger()) {
            device->getDevice().getL0Debugger()->notifyCommandQueueDestroyed(&device->getDevice());
        }
        if (gpgpuEngine) {
            gpgpuEngine->commandStreamReceiver->releasePreallocationRequest();
        }
    }

    timestampPacketContainer.reset();
    // for normal queue, decrement ref count on context
    // special queue is owned by context so ref count doesn't have to be decremented
    if (context && !isSpecialCommandQueue) {
        context->decRefInternal();
    }
    gtpinRemoveCommandQueue(this);
}

void tryAssignSecondaryEngine(Device &device, EngineControl *&engineControl, EngineTypeUsage engineTypeUsage) {
    auto newEngine = device.getSecondaryEngineCsr(engineTypeUsage, 0, false);
    if (newEngine) {
        engineControl = newEngine;
    }
}

void CommandQueue::initializeGpgpu() const {
    if (gpgpuEngine == nullptr) {
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        if (gpgpuEngine == nullptr) {
            auto defaultEngineType = device->getDefaultEngine().getEngineType();

            const GfxCoreHelper &gfxCoreHelper = getDevice().getGfxCoreHelper();
            bool secondaryContextsEnabled = gfxCoreHelper.areSecondaryContextsSupported();

            if (secondaryContextsEnabled && EngineHelpers::isCcs(defaultEngineType)) {
                tryAssignSecondaryEngine(device->getDevice(), gpgpuEngine, {defaultEngineType, EngineUsage::regular});
            }

            if (gpgpuEngine == nullptr) {
                this->gpgpuEngine = &device->getDefaultEngine();
            }

            this->initializeGpgpuInternals();
        }
    }
}

void CommandQueue::initializeGpgpuInternals() const {
    auto &rootDeviceEnvironment = device->getDevice().getRootDeviceEnvironment();
    auto &productHelper = device->getProductHelper();

    if (device->getDevice().getDebugger() && !this->gpgpuEngine->commandStreamReceiver->getDebugSurfaceAllocation()) {
        auto maxDbgSurfaceSize = NEO::SipKernel::getSipKernel(device->getDevice(), nullptr).getStateSaveAreaSize(&device->getDevice());
        auto debugSurface = this->gpgpuEngine->commandStreamReceiver->allocateDebugSurface(maxDbgSurfaceSize);
        memset(debugSurface->getUnderlyingBuffer(), 0, debugSurface->getUnderlyingBufferSize());

        auto &stateSaveAreaHeader = SipKernel::getSipKernel(device->getDevice(), nullptr).getStateSaveAreaHeader();
        if (stateSaveAreaHeader.size() > 0) {
            NEO::MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *debugSurface),
                                                                  device->getDevice(), debugSurface, 0, stateSaveAreaHeader.data(),
                                                                  stateSaveAreaHeader.size());
        }
    }

    gpgpuEngine->commandStreamReceiver->initializeResources(false, device->getPreemptionMode());
    gpgpuEngine->commandStreamReceiver->requestPreallocation();
    gpgpuEngine->commandStreamReceiver->initDirectSubmission();

    if (getCmdQueueProperties<cl_queue_properties>(propertiesVector.data(), CL_QUEUE_PROPERTIES) & static_cast<cl_queue_properties>(CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) && !this->gpgpuEngine->commandStreamReceiver->isUpdateTagFromWaitEnabled()) {
        this->gpgpuEngine->commandStreamReceiver->overrideDispatchPolicy(DispatchMode::batchedDispatch);
        if (debugManager.flags.CsrDispatchMode.get() != 0) {
            this->gpgpuEngine->commandStreamReceiver->overrideDispatchPolicy(static_cast<DispatchMode>(debugManager.flags.CsrDispatchMode.get()));
        }
        this->gpgpuEngine->commandStreamReceiver->enableNTo1SubmissionModel();
    }
}

CommandStreamReceiver &CommandQueue::getGpgpuCommandStreamReceiver() const {
    this->initializeGpgpu();
    return *gpgpuEngine->commandStreamReceiver;
}

CommandStreamReceiver *CommandQueue::getBcsCommandStreamReceiver(aub_stream::EngineType bcsEngineType) {
    initializeBcsEngine(isSpecial());
    const EngineControl *engine = this->bcsEngines[EngineHelpers::getBcsIndex(bcsEngineType)];
    if (engine == nullptr) {
        return nullptr;
    } else {
        return engine->commandStreamReceiver;
    }
}

CommandStreamReceiver *CommandQueue::getBcsForAuxTranslation() {
    initializeBcsEngine(isSpecial());
    for (const EngineControl *engine : this->bcsEngines) {
        if (engine != nullptr) {
            return engine->commandStreamReceiver;
        }
    }
    return nullptr;
}

CommandStreamReceiver &CommandQueue::selectCsrForBuiltinOperation(const CsrSelectionArgs &args) {
    initializeBcsEngine(isSpecial());
    if (isCopyOnly) {
        return *getBcsCommandStreamReceiver(*bcsQueueEngineType);
    }

    if (!blitEnqueueAllowed(args)) {
        return getGpgpuCommandStreamReceiver();
    }
    bool preferBcs = true;
    aub_stream::EngineType preferredBcsEngineType = aub_stream::EngineType::NUM_ENGINES;
    switch (args.direction) {
    case TransferDirection::localToLocal: {
        const auto &clGfxCoreHelper = device->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
        preferBcs = clGfxCoreHelper.preferBlitterForLocalToLocalTransfers();
        if (auto flag = debugManager.flags.PreferCopyEngineForCopyBufferToBuffer.get(); flag != -1) {
            preferBcs = static_cast<bool>(flag);
        }
        if (preferBcs) {
            preferredBcsEngineType = aub_stream::EngineType::ENGINE_BCS;
        }
        break;
    }
    case TransferDirection::hostToHost:
    case TransferDirection::hostToLocal:
    case TransferDirection::localToHost: {
        auto isAccessToImageFromBuffer = (args.dstResource.image && args.dstResource.image->isImageFromBuffer()) ||
                                         (args.srcResource.image && args.srcResource.image->isImageFromBuffer());
        auto &productHelper = device->getProductHelper();
        preferBcs = device->getRootDeviceEnvironment().isWddmOnLinux() || productHelper.blitEnqueuePreferred(isAccessToImageFromBuffer);
        if (debugManager.flags.EnableBlitterForEnqueueOperations.get() == 1) {
            preferBcs = true;
        } else if (debugManager.flags.EnableBlitterForEnqueueOperations.get() == 2) {
            preferBcs = isAccessToImageFromBuffer;
        }
        auto preferredBCSType = true;

        if (debugManager.flags.AssignBCSAtEnqueue.get() != -1) {
            preferredBCSType = debugManager.flags.AssignBCSAtEnqueue.get();
        }

        if (preferredBCSType) {
            if (this->priority == QueuePriority::high) {
                const auto &gfxCoreHelper = device->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
                const auto &hwInfo = device->getHardwareInfo();
                preferredBcsEngineType = gfxCoreHelper.getDefaultHpCopyEngine(hwInfo);
            }

            if (preferredBcsEngineType == aub_stream::EngineType::NUM_ENGINES) {
                preferredBcsEngineType = EngineHelpers::getBcsEngineType(device->getRootDeviceEnvironment(), device->getDeviceBitfield(),
                                                                         device->getSelectorCopyEngine(), false);
            }
        }

        if (!preferBcs && isOOQEnabled() && getGpgpuCommandStreamReceiver().isBusy()) {
            // If CCS is preferred but it's OOQ and compute engine is busy, select BCS instead
            preferBcs = true;
        }
        break;
    }
    default:
        UNRECOVERABLE_IF(true);
    }

    CommandStreamReceiver *selectedCsr = nullptr;
    if (preferBcs) {
        auto assignBCS = true;

        if (debugManager.flags.AssignBCSAtEnqueue.get() != -1) {
            assignBCS = debugManager.flags.AssignBCSAtEnqueue.get();
        }

        if (assignBCS) {
            selectedCsr = getBcsCommandStreamReceiver(preferredBcsEngineType);
        }

        if (selectedCsr == nullptr && bcsQueueEngineType.has_value()) {
            selectedCsr = getBcsCommandStreamReceiver(*bcsQueueEngineType);
        }
    }
    if (selectedCsr == nullptr) {
        selectedCsr = &getGpgpuCommandStreamReceiver();
    }

    UNRECOVERABLE_IF(selectedCsr == nullptr);
    return *selectedCsr;
}

void CommandQueue::constructBcsEngine(bool internalUsage) {
    if (!bcsAllowed) {
        return;
    }
    if (!bcsInitialized) {
        const std::lock_guard lock{bcsInitMutex};

        if (!bcsInitialized) {
            auto &gfxCoreHelper = device->getGfxCoreHelper();
            auto &neoDevice = device->getNearestGenericSubDevice(0)->getDevice();
            auto &selectorCopyEngine = neoDevice.getSelectorCopyEngine();
            auto bcsEngineType = EngineHelpers::getBcsEngineType(device->getRootDeviceEnvironment(), device->getDeviceBitfield(), selectorCopyEngine, internalUsage);
            auto bcsIndex = EngineHelpers::getBcsIndex(bcsEngineType);
            auto engineUsage = (internalUsage && gfxCoreHelper.preferInternalBcsEngine()) ? EngineUsage::internal : EngineUsage::regular;

            if (priority == QueuePriority::high) {
                auto hpBcs = neoDevice.getHpCopyEngine();

                if (hpBcs) {
                    bcsEngineType = hpBcs->getEngineType();
                    engineUsage = EngineUsage::highPriority;
                    bcsIndex = EngineHelpers::getBcsIndex(bcsEngineType);
                    bcsEngines[bcsIndex] = hpBcs;
                }
            }

            if (bcsEngines[bcsIndex] == nullptr) {
                bcsEngines[bcsIndex] = neoDevice.tryGetEngine(bcsEngineType, engineUsage);
            }

            if (bcsEngines[bcsIndex]) {
                bcsQueueEngineType = bcsEngineType;

                if (gfxCoreHelper.areSecondaryContextsSupported() && !internalUsage) {
                    tryAssignSecondaryEngine(device->getDevice(), bcsEngines[bcsIndex], {bcsEngineType, engineUsage});
                }

                bcsEngines[bcsIndex]->osContext->ensureContextInitialized(false);
                bcsEngines[bcsIndex]->commandStreamReceiver->initDirectSubmission();
            }
            bcsInitialized = true;
        }
    }
}

void CommandQueue::initializeBcsEngine(bool internalUsage) {
    constructBcsEngine(internalUsage);
}

void CommandQueue::constructBcsEnginesForSplit() {
    if (this->bcsSplitInitialized) {
        return;
    }

    if (debugManager.flags.SplitBcsMask.get() > 0) {
        this->splitEngines = debugManager.flags.SplitBcsMask.get();
    }

    for (uint32_t i = 0; i < bcsInfoMaskSize; i++) {
        if (this->splitEngines.test(i) && !bcsEngines[i]) {
            auto &neoDevice = device->getNearestGenericSubDevice(0)->getDevice();
            auto engineType = EngineHelpers::mapBcsIndexToEngineType(i, true);
            bcsEngines[i] = neoDevice.tryGetEngine(engineType, EngineUsage::regular);

            if (bcsEngines[i]) {
                bcsQueueEngineType = engineType;
                bcsEngines[i]->commandStreamReceiver->initializeResources(false, device->getPreemptionMode());
                bcsEngines[i]->commandStreamReceiver->initDirectSubmission();
            }
        }
    }

    if (debugManager.flags.SplitBcsMaskD2H.get() > 0) {
        this->d2hEngines = debugManager.flags.SplitBcsMaskD2H.get();
    }
    if (debugManager.flags.SplitBcsMaskH2D.get() > 0) {
        this->h2dEngines = debugManager.flags.SplitBcsMaskH2D.get();
    }

    this->bcsSplitInitialized = true;
}

void CommandQueue::prepareHostPtrSurfaceForSplit(bool split, GraphicsAllocation &allocation) {
    if (split) {
        for (const auto bcsEngine : this->bcsEngines) {
            if (bcsEngine) {
                if (allocation.getTaskCount(bcsEngine->commandStreamReceiver->getOsContext().getContextId()) == GraphicsAllocation::objectNotUsed) {
                    allocation.updateTaskCount(0u, bcsEngine->commandStreamReceiver->getOsContext().getContextId());
                }
            }
        }
    }
}

CommandStreamReceiver &CommandQueue::selectCsrForHostPtrAllocation(bool split, CommandStreamReceiver &csr) {
    return split ? getGpgpuCommandStreamReceiver() : csr;
}

void CommandQueue::releaseMainCopyEngine() {
    const auto &productHelper = device->getRootDeviceEnvironment().getProductHelper();
    const auto mainBcsIndex = EngineHelpers::getBcsIndex(productHelper.getDefaultCopyEngine());
    if (auto mainBcs = bcsEngines[mainBcsIndex]; mainBcs != nullptr) {
        auto &selectorCopyEngineSubDevice = device->getNearestGenericSubDevice(0)->getSelectorCopyEngine();
        EngineHelpers::releaseBcsEngineType(mainBcs->getEngineType(), selectorCopyEngineSubDevice, device->getRootDeviceEnvironment());
        auto &selectorCopyEngine = device->getSelectorCopyEngine();
        EngineHelpers::releaseBcsEngineType(mainBcs->getEngineType(), selectorCopyEngine, device->getRootDeviceEnvironment());
    }
}

bool CommandQueue::waitOnDestructionNeeded() const {
    return (device && (getRefApiCount() == 1) && (taskCount < CompletionStamp::notReady));
}

Device &CommandQueue::getDevice() const noexcept {
    return device->getDevice();
}

TagAddressType CommandQueue::getHwTag() const {
    TagAddressType tag = *getHwTagAddress();
    return tag;
}

volatile TagAddressType *CommandQueue::getHwTagAddress() const {
    return getGpgpuCommandStreamReceiver().getTagAddress();
}

bool CommandQueue::isCompleted(TaskCountType gpgpuTaskCount, std::span<const CopyEngineState> bcsStates) {
    DEBUG_BREAK_IF(getHwTag() == CompletionStamp::notReady);

    if (getGpgpuCommandStreamReceiver().testTaskCountReady(getHwTagAddress(), gpgpuTaskCount)) {
        for (auto &bcsState : bcsStates) {
            if (bcsState.isValid()) {
                auto bcsCsr = getBcsCommandStreamReceiver(bcsState.engineType);
                if (!bcsCsr->testTaskCountReady(bcsCsr->getTagAddress(), peekBcsTaskCount(bcsState.engineType))) {
                    return false;
                }
            }
        }

        return true;
    }

    return false;
}

WaitStatus CommandQueue::waitUntilComplete(TaskCountType gpgpuTaskCountToWait, std::span<CopyEngineState> copyEnginesToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool cleanTemporaryAllocationList, bool skipWait) {
    WAIT_ENTER()

    WaitStatus waitStatus{WaitStatus::ready};

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Waiting for taskCount:", gpgpuTaskCountToWait);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", getHwTag());

    if (!skipWait) {
        if (flushStampToWait == 0 && getGpgpuCommandStreamReceiver().isKmdWaitOnTaskCountAllowed()) {
            flushStampToWait = gpgpuTaskCountToWait;
        }
        waitStatus = getGpgpuCommandStreamReceiver().waitForTaskCountWithKmdNotifyFallback(gpgpuTaskCountToWait,
                                                                                           flushStampToWait,
                                                                                           useQuickKmdSleep,
                                                                                           this->getThrottle());
        if (waitStatus == WaitStatus::gpuHang) {
            return WaitStatus::gpuHang;
        }

        DEBUG_BREAK_IF(getHwTag() < gpgpuTaskCountToWait);

        if (gtpinIsGTPinInitialized()) {
            gtpinNotifyTaskCompletion(gpgpuTaskCountToWait);
        }

        for (const CopyEngineState &copyEngine : copyEnginesToWait) {
            auto bcsCsr = getBcsCommandStreamReceiver(copyEngine.engineType);

            waitStatus = bcsCsr->waitForTaskCountWithKmdNotifyFallback(copyEngine.taskCount, 0, false, this->getThrottle());
            if (waitStatus == WaitStatus::gpuHang) {
                return WaitStatus::gpuHang;
            }
        }
    } else if (gtpinIsGTPinInitialized()) {
        gtpinNotifyTaskCompletion(gpgpuTaskCountToWait);
    }

    for (const CopyEngineState &copyEngine : copyEnginesToWait) {
        auto bcsCsr = getBcsCommandStreamReceiver(copyEngine.engineType);

        waitStatus = bcsCsr->waitForTaskCountAndCleanTemporaryAllocationList(copyEngine.taskCount);
        if (waitStatus == WaitStatus::gpuHang) {
            return WaitStatus::gpuHang;
        }
    }

    waitStatus = cleanTemporaryAllocationList
                     ? getGpgpuCommandStreamReceiver().waitForTaskCountAndCleanTemporaryAllocationList(gpgpuTaskCountToWait)
                     : getGpgpuCommandStreamReceiver().waitForTaskCount(gpgpuTaskCountToWait);

    WAIT_LEAVE()
    if (this->context->getStagingBufferManager()) {
        this->context->getStagingBufferManager()->resetDetectedPtrs();
    }
    return waitStatus;
}

bool CommandQueue::isQueueBlocked() {
    TakeOwnershipWrapper<CommandQueue> takeOwnershipWrapper(*this);
    // check if we have user event and if so, if it is in blocked state.
    if (this->virtualEvent) {
        auto executionStatus = this->virtualEvent->peekExecutionStatus();
        if (executionStatus <= CL_SUBMITTED) {
            UNRECOVERABLE_IF(this->virtualEvent == nullptr);

            if (this->virtualEvent->isStatusCompletedByTermination(executionStatus) == false) {
                taskCount = this->virtualEvent->peekTaskCount();
                flushStamp->setStamp(this->virtualEvent->flushStamp->peekStamp());
                taskLevel = this->virtualEvent->taskLevel;
                // If this isn't an OOQ, update the taskLevel for the queue
                if (!isOOQEnabled()) {
                    taskLevel++;
                }
            } else {
                // at this point we may reset queue TaskCount, since all command previous to this were aborted
                taskCount = 0;
                flushStamp->setStamp(0);
                taskLevel = getGpgpuCommandStreamReceiver().peekTaskLevel();
            }

            fileLoggerInstance().log(debugManager.flags.EventsDebugEnable.get(), "isQueueBlocked taskLevel change from", taskLevel, "to new from virtualEvent", this->virtualEvent, "new tasklevel", this->virtualEvent->taskLevel.load());

            // close the access to virtual event, driver added only 1 ref count.
            this->virtualEvent->decRefInternal();
            this->virtualEvent = nullptr;
            return false;
        }
        return true;
    }
    return false;
}

cl_int CommandQueue::getCommandQueueInfo(cl_command_queue_info paramName,
                                         size_t paramValueSize,
                                         void *paramValue,
                                         size_t *paramValueSizeRet) {
    return getQueueInfo(this, paramName, paramValueSize, paramValue, paramValueSizeRet);
}

TaskCountType CommandQueue::getTaskLevelFromWaitList(TaskCountType taskLevel,
                                                     cl_uint numEventsInWaitList,
                                                     const cl_event *eventWaitList) {
    for (auto iEvent = 0u; iEvent < numEventsInWaitList; ++iEvent) {
        auto pEvent = static_cast<const Event *>(eventWaitList[iEvent]);
        TaskCountType eventTaskLevel = pEvent->peekTaskLevel();
        taskLevel = std::max(taskLevel, eventTaskLevel);
    }
    return taskLevel;
}

LinearStream &CommandQueue::getCS(size_t minRequiredSize) {
    DEBUG_BREAK_IF(nullptr == device);

    if (!commandStream) {
        commandStream = new LinearStream(nullptr);
    }

    minRequiredSize += CSRequirements::minCommandQueueCommandStreamSize;
    constexpr static auto additionalAllocationSize = CSRequirements::minCommandQueueCommandStreamSize + CSRequirements::csOverfetchSize;
    getGpgpuCommandStreamReceiver().ensureCommandBufferAllocation(*commandStream, minRequiredSize, additionalAllocationSize);
    return *commandStream;
}

cl_int CommandQueue::enqueueAcquireSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }

        int result = memObject->peekSharingHandler()->acquire(memObject, getDevice().getRootDeviceIndex());
        if (result != CL_SUCCESS) {
            return result;
        }
        memObject->acquireCount++;
    }
    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }

    return status;
}

cl_int CommandQueue::enqueueReleaseSharedObjects(cl_uint numObjects, const cl_mem *memObjects, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *oclEvent, cl_uint cmdType) {
    if ((memObjects == nullptr && numObjects != 0) || (memObjects != nullptr && numObjects == 0)) {
        return CL_INVALID_VALUE;
    }

    std::for_each(eventWaitList, eventWaitList + numEventsInWaitList, [](const auto event) {
        auto eventObject = castToObjectOrAbort<Event>(event);
        if (!eventObject->isUserEvent()) {
            eventObject->wait(false, false);
        };
    });
    if (!this->isOOQEnabled()) {
        this->finish(false);
    }

    bool isImageReleased = false;
    bool isDisplayableReleased = false;
    for (unsigned int object = 0; object < numObjects; object++) {
        auto memObject = castToObject<MemObj>(memObjects[object]);
        if (memObject == nullptr || memObject->peekSharingHandler() == nullptr) {
            return CL_INVALID_MEM_OBJECT;
        }
        isImageReleased |= memObject->getMultiGraphicsAllocation().getAllocationType() == AllocationType::sharedImage;
        isDisplayableReleased |= memObject->isMemObjDisplayable();

        memObject->peekSharingHandler()->release(memObject, getDevice().getRootDeviceIndex());
        DEBUG_BREAK_IF(memObject->acquireCount <= 0);
        memObject->acquireCount--;
    }

    if (this->getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled()) {
        if (isDisplayableReleased) {
            this->getGpgpuCommandStreamReceiver().sendRenderStateCacheFlush();
            {
                TakeOwnershipWrapper<CommandQueue> queueOwnership(*this);
                this->taskCount = this->getGpgpuCommandStreamReceiver().peekTaskCount();
            }
            this->finish(false);
        } else if (isImageReleased) {
            this->getGpgpuCommandStreamReceiver().sendRenderStateCacheFlush();
        }
    }

    auto status = enqueueMarkerWithWaitList(
        numEventsInWaitList,
        eventWaitList,
        oclEvent);

    if (oclEvent) {
        castToObjectOrAbort<Event>(*oclEvent)->setCmdType(cmdType);
    }
    return status;
}

void CommandQueue::updateFromCompletionStamp(const CompletionStamp &completionStamp, Event *outEvent) {
    DEBUG_BREAK_IF(this->taskLevel > completionStamp.taskLevel);
    DEBUG_BREAK_IF(this->taskCount > completionStamp.taskCount);
    if (completionStamp.taskCount != CompletionStamp::notReady) {
        taskCount = completionStamp.taskCount;
    }
    flushStamp->setStamp(completionStamp.flushStamp);
    this->taskLevel = completionStamp.taskLevel;

    if (outEvent) {
        outEvent->updateCompletionStamp(completionStamp.taskCount, outEvent->peekBcsTaskCountFromCommandQueue(), completionStamp.taskLevel, completionStamp.flushStamp);
        fileLoggerInstance().log(debugManager.flags.EventsDebugEnable.get(), "updateCompletionStamp Event", outEvent, "taskLevel", outEvent->taskLevel.load());
    }
}

bool CommandQueue::setPerfCountersEnabled() {
    DEBUG_BREAK_IF(device == nullptr);

    auto perfCounters = device->getPerformanceCounters();
    bool isCcsEngine = EngineHelpers::isCcs(getGpgpuEngine().osContext->getEngineType());

    perfCountersEnabled = perfCounters->enable(isCcsEngine);

    if (!perfCountersEnabled) {
        perfCounters->shutdown();
    }

    return perfCountersEnabled;
}

PerformanceCounters *CommandQueue::getPerfCounters() {
    return device->getPerformanceCounters();
}

cl_int CommandQueue::enqueueWriteMemObjForUnmap(MemObj *memObj, void *mappedPtr, EventsRequest &eventsRequest) {
    cl_int retVal = CL_SUCCESS;

    MapInfo unmapInfo;
    if (!memObj->findMappedPtr(mappedPtr, unmapInfo)) {
        return CL_INVALID_VALUE;
    }

    if (!unmapInfo.readOnly) {
        memObj->getMapAllocation(getDevice().getRootDeviceIndex())->setAubWritable(true, GraphicsAllocation::defaultBank);
        memObj->getMapAllocation(getDevice().getRootDeviceIndex())->setTbxWritable(true, GraphicsAllocation::defaultBank);

        if (memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            auto buffer = castToObject<Buffer>(memObj);

            retVal = enqueueWriteBuffer(buffer, CL_FALSE, unmapInfo.offset[0], unmapInfo.size[0], mappedPtr, memObj->getMapAllocation(getDevice().getRootDeviceIndex()),
                                        eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
        } else {
            auto image = castToObjectOrAbort<Image>(memObj);
            size_t writeOrigin[4] = {unmapInfo.offset[0], unmapInfo.offset[1], unmapInfo.offset[2], 0};
            auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
            UNRECOVERABLE_IF(mipIdx >= 4);
            writeOrigin[mipIdx] = unmapInfo.mipLevel;
            retVal = enqueueWriteImage(image, CL_FALSE, writeOrigin, &unmapInfo.size[0],
                                       image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(), mappedPtr, memObj->getMapAllocation(getDevice().getRootDeviceIndex()),
                                       eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
        }
    } else {
        retVal = enqueueMarkerWithWaitList(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
    }

    if (retVal == CL_SUCCESS) {
        memObj->removeMappedPtr(mappedPtr);
        if (eventsRequest.outEvent) {
            auto event = castToObject<Event>(*eventsRequest.outEvent);
            event->setCmdType(CL_COMMAND_UNMAP_MEM_OBJECT);
        }
    }
    return retVal;
}

void *CommandQueue::enqueueReadMemObjForMap(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet) {
    void *basePtr = transferProperties.memObj->getBasePtrForMap(getDevice().getRootDeviceIndex());
    size_t mapPtrOffset = transferProperties.memObj->calculateOffsetForMapping(transferProperties.offset) + transferProperties.mipPtrOffset;
    if (transferProperties.memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
        mapPtrOffset += transferProperties.memObj->getOffset();
    }
    void *returnPtr = ptrOffset(basePtr, mapPtrOffset);

    if (!transferProperties.memObj->addMappedPtr(returnPtr, transferProperties.memObj->calculateMappedPtrLength(transferProperties.size),
                                                 transferProperties.mapFlags, transferProperties.size, transferProperties.offset, transferProperties.mipLevel,
                                                 transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()))) {
        errcodeRet = CL_INVALID_OPERATION;
        return nullptr;
    }

    if (transferProperties.mapFlags == CL_MAP_WRITE_INVALIDATE_REGION) {
        errcodeRet = enqueueMarkerWithWaitList(eventsRequest.numEventsInWaitList, eventsRequest.eventWaitList, eventsRequest.outEvent);
    } else {
        if (transferProperties.memObj->peekClMemObjType() == CL_MEM_OBJECT_BUFFER) {
            auto buffer = castToObject<Buffer>(transferProperties.memObj);
            errcodeRet = enqueueReadBuffer(buffer, transferProperties.blocking, transferProperties.offset[0], transferProperties.size[0],
                                           returnPtr, transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()), eventsRequest.numEventsInWaitList,
                                           eventsRequest.eventWaitList, eventsRequest.outEvent);
        } else {
            auto image = castToObjectOrAbort<Image>(transferProperties.memObj);
            size_t readOrigin[4] = {transferProperties.offset[0], transferProperties.offset[1], transferProperties.offset[2], 0};
            auto mipIdx = getMipLevelOriginIdx(image->peekClMemObjType());
            UNRECOVERABLE_IF(mipIdx >= 4);
            readOrigin[mipIdx] = transferProperties.mipLevel;
            errcodeRet = enqueueReadImage(image, transferProperties.blocking, readOrigin, &transferProperties.size[0],
                                          image->getHostPtrRowPitch(), image->getHostPtrSlicePitch(),
                                          returnPtr, transferProperties.memObj->getMapAllocation(getDevice().getRootDeviceIndex()), eventsRequest.numEventsInWaitList,
                                          eventsRequest.eventWaitList, eventsRequest.outEvent);
        }
    }

    if (errcodeRet != CL_SUCCESS) {
        transferProperties.memObj->removeMappedPtr(returnPtr);
        return nullptr;
    }
    if (eventsRequest.outEvent) {
        auto event = castToObject<Event>(*eventsRequest.outEvent);
        event->setCmdType(transferProperties.cmdType);
    }
    return returnPtr;
}

void *CommandQueue::enqueueMapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest, cl_int &errcodeRet) {
    if (transferProperties.memObj->mappingOnCpuAllowed()) {
        return cpuDataTransferHandler(transferProperties, eventsRequest, errcodeRet);
    } else {
        return enqueueReadMemObjForMap(transferProperties, eventsRequest, errcodeRet);
    }
}

cl_int CommandQueue::enqueueUnmapMemObject(TransferProperties &transferProperties, EventsRequest &eventsRequest) {
    cl_int retVal = CL_SUCCESS;
    if (transferProperties.memObj->mappingOnCpuAllowed()) {
        cpuDataTransferHandler(transferProperties, eventsRequest, retVal);
    } else {
        retVal = enqueueWriteMemObjForUnmap(transferProperties.memObj, transferProperties.ptr, eventsRequest);
    }
    return retVal;
}

void *CommandQueue::enqueueMapBuffer(Buffer *buffer, cl_bool blockingMap,
                                     cl_map_flags mapFlags, size_t offset,
                                     size_t size, cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList, cl_event *event,
                                     cl_int &errcodeRet) {
    TransferProperties transferProperties(buffer, CL_COMMAND_MAP_BUFFER, mapFlags, blockingMap != CL_FALSE, &offset, &size, nullptr, false, getDevice().getRootDeviceIndex());
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    return enqueueMapMemObject(transferProperties, eventsRequest, errcodeRet);
}

void *CommandQueue::enqueueMapImage(Image *image, cl_bool blockingMap,
                                    cl_map_flags mapFlags, const size_t *origin,
                                    const size_t *region, size_t *imageRowPitch,
                                    size_t *imageSlicePitch,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList, cl_event *event,
                                    cl_int &errcodeRet) {
    TransferProperties transferProperties(image, CL_COMMAND_MAP_IMAGE, mapFlags, blockingMap != CL_FALSE,
                                          const_cast<size_t *>(origin), const_cast<size_t *>(region), nullptr, false, getDevice().getRootDeviceIndex());
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    if (image->isMemObjZeroCopy() && image->mappingOnCpuAllowed()) {
        GetInfoHelper::set(imageSlicePitch, image->getImageDesc().image_slice_pitch);
        if (image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            // There are differences in qPitch programming between Gen8 vs Gen9+ devices.
            // For Gen8 qPitch is distance in rows while Gen9+ it is in pixels.
            // Minimum value of qPitch is 4 and this causes slicePitch = 4*rowPitch on Gen8.
            // To allow zero-copy we have to tell what is correct value rowPitch which should equal to slicePitch.
            GetInfoHelper::set(imageRowPitch, image->getImageDesc().image_slice_pitch);
        } else {
            GetInfoHelper::set(imageRowPitch, image->getImageDesc().image_row_pitch);
        }
    } else {
        GetInfoHelper::set(imageSlicePitch, image->getHostPtrSlicePitch());
        GetInfoHelper::set(imageRowPitch, image->getHostPtrRowPitch());
    }
    if (Image::hasSlices(image->peekClMemObjType()) == false) {
        GetInfoHelper::set(imageSlicePitch, static_cast<size_t>(0));
    }
    return enqueueMapMemObject(transferProperties, eventsRequest, errcodeRet);
}

cl_int CommandQueue::enqueueUnmapMemObject(MemObj *memObj, void *mappedPtr, cl_uint numEventsInWaitList, const cl_event *eventWaitList, cl_event *event) {
    TransferProperties transferProperties(memObj, CL_COMMAND_UNMAP_MEM_OBJECT, 0, false, nullptr, nullptr, mappedPtr, false, getDevice().getRootDeviceIndex());
    EventsRequest eventsRequest(numEventsInWaitList, eventWaitList, event);

    return enqueueUnmapMemObject(transferProperties, eventsRequest);
}

void CommandQueue::enqueueBlockedMapUnmapOperation(const cl_event *eventWaitList,
                                                   size_t numEventsInWaitlist,
                                                   MapOperationType opType,
                                                   MemObj *memObj,
                                                   MemObjSizeArray &copySize,
                                                   MemObjOffsetArray &copyOffset,
                                                   bool readOnly,
                                                   EventBuilder &externalEventBuilder) {
    EventBuilder internalEventBuilder;
    EventBuilder *eventBuilder;
    // check if event will be exposed externally
    if (externalEventBuilder.getEvent()) {
        externalEventBuilder.getEvent()->incRefInternal();
        eventBuilder = &externalEventBuilder;
    } else {
        // it will be an internal event
        internalEventBuilder.create<VirtualEvent>(this, context);
        eventBuilder = &internalEventBuilder;
    }

    // store task data in event
    auto cmd = std::unique_ptr<Command>(new CommandMapUnmap(opType, *memObj, copySize, copyOffset, readOnly, *this));
    eventBuilder->getEvent()->setCommand(std::move(cmd));

    // bind output event with input events
    eventBuilder->addParentEvents(ArrayRef<const cl_event>(eventWaitList, numEventsInWaitlist));
    eventBuilder->addParentEvent(this->virtualEvent);
    eventBuilder->finalize();

    if (this->virtualEvent) {
        this->virtualEvent->decRefInternal();
    }
    this->virtualEvent = eventBuilder->getEvent();
}

bool CommandQueue::setupDebugSurface(Kernel *kernel) {
    auto debugSurface = getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation();
    auto surfaceState = ptrOffset(reinterpret_cast<uintptr_t *>(kernel->getSurfaceStateHeap()),
                                  kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful);
    void *addressToPatch = reinterpret_cast<void *>(debugSurface->getGpuAddress());
    size_t sizeToPatch = debugSurface->getUnderlyingBufferSize();
    Buffer::setSurfaceState(&device->getDevice(), surfaceState, false, false, sizeToPatch,
                            addressToPatch, 0, debugSurface, 0, 0,
                            kernel->areMultipleSubDevicesInContext());
    return true;
}

bool CommandQueue::validateCapability(cl_command_queue_capabilities_intel capability) const {
    return this->queueCapabilities == CL_QUEUE_DEFAULT_CAPABILITIES_INTEL || isValueSet(this->queueCapabilities, capability);
}

bool CommandQueue::validateCapabilitiesForEventWaitList(cl_uint numEventsInWaitList, const cl_event *waitList) const {
    for (cl_uint eventIndex = 0u; eventIndex < numEventsInWaitList; eventIndex++) {
        const Event *event = castToObject<Event>(waitList[eventIndex]);
        if (event->isUserEvent()) {
            continue;
        }

        const CommandQueue *eventCommandQueue = event->getCommandQueue();
        const bool crossQueue = this != eventCommandQueue;
        const cl_command_queue_capabilities_intel createCap = crossQueue ? CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL
                                                                         : CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL;
        const cl_command_queue_capabilities_intel waitCap = crossQueue ? CL_QUEUE_CAPABILITY_CROSS_QUEUE_EVENT_WAIT_LIST_INTEL
                                                                       : CL_QUEUE_CAPABILITY_SINGLE_QUEUE_EVENT_WAIT_LIST_INTEL;
        if (!validateCapability(waitCap) || !eventCommandQueue->validateCapability(createCap)) {
            return false;
        }
    }

    return true;
}

bool CommandQueue::validateCapabilityForOperation(cl_command_queue_capabilities_intel capability,
                                                  cl_uint numEventsInWaitList,
                                                  const cl_event *waitList,
                                                  const cl_event *outEvent) const {
    const bool operationValid = validateCapability(capability);
    const bool waitListValid = validateCapabilitiesForEventWaitList(numEventsInWaitList, waitList);
    const bool outEventValid = outEvent == nullptr ||
                               validateCapability(CL_QUEUE_CAPABILITY_CREATE_SINGLE_QUEUE_EVENTS_INTEL) ||
                               validateCapability(CL_QUEUE_CAPABILITY_CREATE_CROSS_QUEUE_EVENTS_INTEL);
    return operationValid && waitListValid && outEventValid;
}

cl_uint CommandQueue::getQueueFamilyIndex() const {
    if (isQueueFamilySelected()) {
        return queueFamilyIndex;
    } else {
        const auto &hwInfo = device->getHardwareInfo();
        const auto &gfxCoreHelper = device->getGfxCoreHelper();
        const auto engineGroupType = gfxCoreHelper.getEngineGroupType(getGpgpuEngine().getEngineType(), getGpgpuEngine().getEngineUsage(), hwInfo);
        const auto familyIndex = device->getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType);
        return static_cast<cl_uint>(familyIndex);
    }
}

void CommandQueue::updateBcsTaskCount(aub_stream::EngineType bcsEngineType, TaskCountType newBcsTaskCount) {
    CopyEngineState &state = bcsStates[EngineHelpers::getBcsIndex(bcsEngineType)];
    state.engineType = bcsEngineType;
    state.taskCount = newBcsTaskCount;
}

TaskCountType CommandQueue::peekBcsTaskCount(aub_stream::EngineType bcsEngineType) const {
    const CopyEngineState &state = bcsStates[EngineHelpers::getBcsIndex(bcsEngineType)];
    return state.taskCount;
}

bool CommandQueue::isTextureCacheFlushNeeded(uint32_t commandType) const {
    auto isDirectSubmissionEnabled = getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled();
    if (this->isImageWriteOperation(commandType)) {
        return isDirectSubmissionEnabled;
    }
    switch (commandType) {
    case CL_COMMAND_READ_IMAGE:
    case CL_COMMAND_COPY_IMAGE_TO_BUFFER:
        return isDirectSubmissionEnabled && getDevice().getGfxCoreHelper().isCacheFlushPriorImageReadRequired();
    default:
        return false;
    }
}

IndirectHeap &CommandQueue::getIndirectHeap(IndirectHeapType heapType, size_t minRequiredSize) {
    return getGpgpuCommandStreamReceiver().getIndirectHeap(heapType, minRequiredSize);
}

void CommandQueue::allocateHeapMemory(IndirectHeapType heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap) {
    getGpgpuCommandStreamReceiver().allocateHeapMemory(heapType, minRequiredSize, indirectHeap);
}

void CommandQueue::releaseIndirectHeap(IndirectHeapType heapType) {
    getGpgpuCommandStreamReceiver().releaseIndirectHeap(heapType);
}

void CommandQueue::releaseVirtualEvent() {
    if (this->virtualEvent != nullptr) {
        this->virtualEvent->decRefInternal();
        this->virtualEvent = nullptr;
    }
}

void CommandQueue::obtainNewTimestampPacketNodes(size_t numberOfNodes, TimestampPacketContainer &previousNodes, bool clearAllDependencies, CommandStreamReceiver &csr) {
    TagAllocatorBase *allocator = csr.getTimestampPacketAllocator();

    previousNodes.swapNodes(*timestampPacketContainer);

    if (clearAllDependencies) {
        previousNodes.moveNodesToNewContainer(*deferredTimestampPackets);
    }

    DEBUG_BREAK_IF(timestampPacketContainer->peekNodes().size() > 0);

    for (size_t i = 0; i < numberOfNodes; i++) {
        auto newTag = allocator->getTag();

        if (!csr.isHardwareMode()) {
            auto tagAlloc = newTag->getBaseGraphicsAllocation()->getGraphicsAllocation(csr.getRootDeviceIndex());

            // initialize full page tables for the first time
            csr.writeMemory(*tagAlloc, false, 0, 0);
        }

        timestampPacketContainer->add(newTag);
    }
}

size_t CommandQueue::estimateTimestampPacketNodesCount(const MultiDispatchInfo &dispatchInfo) const {
    return dispatchInfo.size();
}

bool CommandQueue::bufferCpuCopyAllowed(Buffer *buffer, cl_command_type commandType, cl_bool blocking, size_t size, void *ptr,
                                        cl_uint numEventsInWaitList, const cl_event *eventWaitList) {

    auto &productHelper = device->getProductHelper();
    if (CL_COMMAND_READ_BUFFER == commandType && productHelper.isCpuCopyNecessary(ptr, buffer->getMemoryManager())) {
        return true;
    }

    auto debugVariableSet = false;
    // Requested by debug variable or allowed by Buffer
    if (CL_COMMAND_READ_BUFFER == commandType && debugManager.flags.DoCpuCopyOnReadBuffer.get() != -1) {
        if (debugManager.flags.DoCpuCopyOnReadBuffer.get() == 0) {
            return false;
        }
        debugVariableSet = true;
    }
    if (CL_COMMAND_WRITE_BUFFER == commandType && debugManager.flags.DoCpuCopyOnWriteBuffer.get() != -1) {
        if (debugManager.flags.DoCpuCopyOnWriteBuffer.get() == 0) {
            return false;
        }
        debugVariableSet = true;
    }

    // if we are blocked by user events, we can't service the call on CPU
    if (Event::checkUserEventDependencies(numEventsInWaitList, eventWaitList)) {
        return false;
    }

    // check if buffer is compatible
    if (!buffer->isReadWriteOnCpuAllowed(device->getDevice())) {
        return false;
    }

    if (buffer->getMemoryManager() && buffer->getMemoryManager()->isCpuCopyRequired(ptr)) {
        return true;
    }

    if (debugVariableSet) {
        return true;
    }

    // non blocking transfers are not expected to be serviced by CPU
    // we do not want to artificially stall the pipeline to allow CPU access
    if (blocking == CL_FALSE) {
        return false;
    }

    // check if it is beneficial to do transfer on CPU
    if (!buffer->isReadWriteOnCpuPreferred(ptr, size, getDevice())) {
        return false;
    }

    // make sure that event wait list is empty
    if (numEventsInWaitList == 0) {
        return true;
    }

    return false;
}

bool CommandQueue::queueDependenciesClearRequired() const {
    return isOOQEnabled() || debugManager.flags.OmitTimestampPacketDependencies.get();
}

bool CommandQueue::blitEnqueueAllowed(const CsrSelectionArgs &args) const {
    bool blitEnqueueAllowed = getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() || this->isCopyOnly;
    blitEnqueueAllowed &= this->priority != QueuePriority::low;
    if (debugManager.flags.EnableBlitterForEnqueueOperations.get() != -1) {
        blitEnqueueAllowed = debugManager.flags.EnableBlitterForEnqueueOperations.get();
    }
    if (!blitEnqueueAllowed) {
        return false;
    }

    switch (args.cmdType) {
    case CL_COMMAND_READ_BUFFER:
    case CL_COMMAND_WRITE_BUFFER:
    case CL_COMMAND_COPY_BUFFER:
    case CL_COMMAND_READ_BUFFER_RECT:
    case CL_COMMAND_WRITE_BUFFER_RECT:
    case CL_COMMAND_COPY_BUFFER_RECT:
    case CL_COMMAND_SVM_MEMCPY:
    case CL_COMMAND_SVM_MAP:
    case CL_COMMAND_SVM_UNMAP:
        return true;
    case CL_COMMAND_READ_IMAGE:
        UNRECOVERABLE_IF(args.srcResource.image == nullptr);
        return blitEnqueueImageAllowed(args.srcResource.imageOrigin, args.size, *args.srcResource.image);
    case CL_COMMAND_WRITE_IMAGE:
        UNRECOVERABLE_IF(args.dstResource.image == nullptr);
        return blitEnqueueImageAllowed(args.dstResource.imageOrigin, args.size, *args.dstResource.image);

    case CL_COMMAND_COPY_IMAGE:
        UNRECOVERABLE_IF(args.srcResource.image == nullptr);
        UNRECOVERABLE_IF(args.dstResource.image == nullptr);
        return blitEnqueueImageAllowed(args.srcResource.imageOrigin, args.size, *args.srcResource.image) &&
               blitEnqueueImageAllowed(args.dstResource.imageOrigin, args.size, *args.dstResource.image);

    default:
        return false;
    }
}

bool CommandQueue::blitEnqueueImageAllowed(const size_t *origin, const size_t *region, const Image &image) const {
    const auto &hwInfo = device->getHardwareInfo();
    auto &productHelper = device->getProductHelper();
    auto releaseHelper = device->getDevice().getReleaseHelper();
    auto blitEnqueueImageAllowed = productHelper.isBlitterForImagesSupported();

    if (debugManager.flags.EnableBlitterForEnqueueImageOperations.get() != -1) {
        blitEnqueueImageAllowed = debugManager.flags.EnableBlitterForEnqueueImageOperations.get();
    }
    if (releaseHelper) {
        blitEnqueueImageAllowed &= !(Image::isDepthFormat(image.getImageFormat()) && !releaseHelper->isBlitImageAllowedForDepthFormat());
    }

    blitEnqueueImageAllowed &= !isMipMapped(image.getImageDesc());

    const auto &defaultGmm = image.getGraphicsAllocation(device->getRootDeviceIndex())->getDefaultGmm();
    if (defaultGmm != nullptr) {
        auto isTile64 = defaultGmm->gmmResourceInfo->getResourceFlags()->Info.Tile64;
        auto imageType = image.getImageDesc().image_type;
        if (isTile64 && (imageType == CL_MEM_OBJECT_IMAGE3D)) {
            blitEnqueueImageAllowed &= productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo);
        }
    }

    return blitEnqueueImageAllowed;
}

bool CommandQueue::isBlockedCommandStreamRequired(uint32_t commandType, const EventsRequest &eventsRequest, bool blockedQueue, bool isMarkerWithProfiling) const {
    if (!blockedQueue) {
        return false;
    }

    if (isCacheFlushCommand(commandType) || !isCommandWithoutKernel(commandType) || isMarkerWithProfiling) {
        return true;
    }

    if (CL_COMMAND_BARRIER == commandType || CL_COMMAND_MARKER == commandType) {
        auto timestampPacketWriteEnabled = getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled();
        if (timestampPacketWriteEnabled || context->getRootDeviceIndices().size() > 1) {

            for (size_t i = 0; i < eventsRequest.numEventsInWaitList; i++) {
                auto waitlistEvent = castToObjectOrAbort<Event>(eventsRequest.eventWaitList[i]);
                if (timestampPacketWriteEnabled && waitlistEvent->getTimestampPacketNodes()) {
                    return true;
                }
                if (waitlistEvent->getCommandQueue() && waitlistEvent->getCommandQueue()->getDevice().getRootDeviceIndex() != this->getDevice().getRootDeviceIndex()) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool CommandQueue::isDependenciesFlushForMarkerRequired(const EventsRequest &eventsRequest) const {
    return this->getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled() && !this->isOOQEnabled() &&
           (eventsRequest.outEvent || eventsRequest.numEventsInWaitList > 0);
}

void CommandQueue::storeProperties(const cl_queue_properties *properties) {
    if (properties) {
        for (size_t i = 0; properties[i] != 0; i += 2) {
            propertiesVector.push_back(properties[i]);
            propertiesVector.push_back(properties[i + 1]);
        }
        propertiesVector.push_back(0);
    }
}

void CommandQueue::processProperties(const cl_queue_properties *properties) {
    if (properties != nullptr) {
        bool specificEngineSelected = false;
        cl_uint selectedQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
        cl_uint selectedQueueIndex = std::numeric_limits<uint32_t>::max();

        for (auto currentProperties = properties; *currentProperties != 0; currentProperties += 2) {
            switch (*currentProperties) {
            case CL_QUEUE_FAMILY_INTEL:
                selectedQueueFamilyIndex = static_cast<cl_uint>(*(currentProperties + 1));
                specificEngineSelected = true;
                break;
            case CL_QUEUE_INDEX_INTEL:
                selectedQueueIndex = static_cast<cl_uint>(*(currentProperties + 1));
                auto nodeOrdinal = debugManager.flags.NodeOrdinal.get();
                if (nodeOrdinal != -1) {
                    int currentEngineIndex = 0;
                    const HardwareInfo &hwInfo = getDevice().getHardwareInfo();
                    const GfxCoreHelper &gfxCoreHelper = getDevice().getGfxCoreHelper();

                    auto engineGroupType = gfxCoreHelper.getEngineGroupType(static_cast<aub_stream::EngineType>(nodeOrdinal), EngineUsage::regular, hwInfo);
                    selectedQueueFamilyIndex = static_cast<cl_uint>(getDevice().getEngineGroupIndexFromEngineGroupType(engineGroupType));
                    const auto &engines = getDevice().getRegularEngineGroups()[selectedQueueFamilyIndex].engines;
                    for (const auto &engine : engines) {
                        if (engine.getEngineType() == static_cast<aub_stream::EngineType>(nodeOrdinal)) {
                            selectedQueueIndex = currentEngineIndex;
                            break;
                        }
                        currentEngineIndex++;
                    }
                }
                specificEngineSelected = true;
                break;
            }
        }

        if (specificEngineSelected) {
            this->queueFamilySelected = true;
            if (!getDevice().hasRootCsr()) {
                const auto &engine = getDevice().getRegularEngineGroups()[selectedQueueFamilyIndex].engines[selectedQueueIndex];
                auto engineType = engine.getEngineType();
                auto engineUsage = engine.getEngineUsage();
                if ((debugManager.flags.EngineUsageHint.get() != -1) &&
                    (getDevice().tryGetEngine(engineType, static_cast<EngineUsage>(debugManager.flags.EngineUsageHint.get())) != nullptr)) {
                    engineUsage = static_cast<EngineUsage>(debugManager.flags.EngineUsageHint.get());
                }
                this->overrideEngine(engineType, engineUsage);
                this->queueCapabilities = getClDevice().getDeviceInfo().queueFamilyProperties[selectedQueueFamilyIndex].capabilities;
                this->queueFamilyIndex = selectedQueueFamilyIndex;
                this->queueIndexWithinFamily = selectedQueueIndex;
            }
        }
    }
}

void CommandQueue::overrideEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    const HardwareInfo &hwInfo = getDevice().getHardwareInfo();
    const GfxCoreHelper &gfxCoreHelper = getDevice().getGfxCoreHelper();
    const EngineGroupType engineGroupType = gfxCoreHelper.getEngineGroupType(engineType, engineUsage, hwInfo);
    const bool isEngineCopyOnly = EngineHelper::isCopyOnlyEngineType(engineGroupType);

    bool secondaryContextsEnabled = gfxCoreHelper.areSecondaryContextsSupported();

    if (isEngineCopyOnly) {
        std::fill(bcsEngines.begin(), bcsEngines.end(), nullptr);
        auto engineIndex = EngineHelpers::getBcsIndex(engineType);

        bcsEngines[engineIndex] = &device->getEngine(engineType, EngineUsage::regular);

        if (bcsEngines[engineIndex]) {
            bcsQueueEngineType = engineType;

            if (secondaryContextsEnabled) {
                tryAssignSecondaryEngine(device->getDevice(), bcsEngines[engineIndex], {engineType, engineUsage});
            }
        }
        timestampPacketContainer = std::make_unique<TimestampPacketContainer>();
        deferredTimestampPackets = std::make_unique<TimestampPacketContainer>();
        isCopyOnly = true;
        bcsInitialized = true;
    } else {
        if (secondaryContextsEnabled && EngineHelpers::isCcs(engineType)) {
            tryAssignSecondaryEngine(device->getDevice(), gpgpuEngine, {engineType, engineUsage});
        }

        if (!gpgpuEngine) {
            gpgpuEngine = &device->getEngine(engineType, engineUsage);
        }
    }
}

void CommandQueue::aubCaptureHook(bool &blocking, bool &clearAllDependencies, const MultiDispatchInfo &multiDispatchInfo) {
    if (debugManager.flags.AUBDumpSubCaptureMode.get()) {
        auto status = getGpgpuCommandStreamReceiver().checkAndActivateAubSubCapture(multiDispatchInfo.empty() ? "" : multiDispatchInfo.peekMainKernel()->getDescriptor().kernelMetadata.kernelName);
        if (!status.isActive) {
            // make each enqueue blocking when subcapture is not active to split batch buffer
            blocking = true;
        } else if (!status.wasActiveInPreviousEnqueue) {
            // omit timestamp packet dependencies dependencies upon subcapture activation
            clearAllDependencies = true;
        }
    }

    if (getGpgpuCommandStreamReceiver().getType() > CommandStreamReceiverType::hardware) {
        for (auto &dispatchInfo : multiDispatchInfo) {
            auto &kernelName = dispatchInfo.getKernel()->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName;
            getGpgpuCommandStreamReceiver().addAubComment(kernelName.c_str());
        }
    }
}

void CommandQueue::assignDataToOverwrittenBcsNode(TagNodeBase *node) {
    std::array<uint32_t, 8u> timestampData;
    timestampData.fill(std::numeric_limits<uint32_t>::max());
    if (node->refCountFetchSub(0) <= 2) { // One ref from deferred container and one from bcs barrier container it is going to be released from
        for (uint32_t i = 0; i < node->getPacketsUsed(); i++) {
            node->assignDataToAllTimestamps(i, timestampData.data());
        }
    }
}

bool CommandQueue::isWaitForTimestampsEnabled() const {
    auto &productHelper = getDevice().getProductHelper();

    auto enabled = CommandQueue::isTimestampWaitEnabled();
    enabled &= productHelper.isTimestampWaitSupportedForQueues(this->heaplessModeEnabled);

    if (productHelper.isL3FlushAfterPostSyncSupported(this->heaplessModeEnabled)) {
        enabled &= true;
    } else {
        enabled &= !productHelper.isDcFlushAllowed();
    }

    enabled &= !getDevice().getRootDeviceEnvironment().isWddmOnLinux();
    enabled &= !this->isOOQEnabled(); // TSP for OOQ dispatch is optional. We need to wait for task count.

    switch (debugManager.flags.EnableTimestampWaitForQueues.get()) {
    case 0:
        enabled = false;
        break;
    case 1:
        enabled = getGpgpuCommandStreamReceiver().isUpdateTagFromWaitEnabled();
        break;
    case 2:
        enabled = getGpgpuCommandStreamReceiver().isDirectSubmissionEnabled();
        break;
    case 3:
        enabled = getGpgpuCommandStreamReceiver().isAnyDirectSubmissionEnabled();
        break;
    case 4:
        enabled = true;
        break;
    }

    return enabled;
}

WaitStatus CommandQueue::waitForAllEngines(bool blockedQueue, PrintfHandler *printfHandler, bool cleanTemporaryAllocationsList, bool waitForTaskCountRequired) {
    if (blockedQueue) {
        while (isQueueBlocked()) {
        }
    }

    StackVec<CopyEngineState, bcsInfoMaskSize> activeBcsStates{};
    for (auto currentBcsIndex = 0u; currentBcsIndex < bcsEngineCount; currentBcsIndex++) {
        CopyEngineState &state = this->bcsStates[currentBcsIndex];
        if (state.isValid()) {
            activeBcsStates.push_back(state);
        }
    }

    auto waitStatus = WaitStatus::notReady;
    bool waitedOnTimestamps = false;

    waitedOnTimestamps = waitForTimestamps(activeBcsStates, waitStatus, this->timestampPacketContainer.get(), this->deferredTimestampPackets.get());
    if (waitStatus == WaitStatus::gpuHang) {
        return WaitStatus::gpuHang;
    }

    TakeOwnershipWrapper<CommandQueue> queueOwnership(*this);
    auto taskCountToWait = taskCount;
    queueOwnership.unlock();

    bool skipWaitOnTaskCount = waitedOnTimestamps;
    if (waitForTaskCountRequired) {
        skipWaitOnTaskCount = false; // PC with L3 flush is required after CPU read if L3 Flush After Post Sync is enabled, so we need to wait for task count
    }

    waitStatus = waitUntilComplete(taskCountToWait, activeBcsStates, flushStamp->peekStamp(), false, cleanTemporaryAllocationsList, skipWaitOnTaskCount);

    {
        queueOwnership.lock();
        /*
           Check if queue resources cleanup after wait is possible.
           If new submission happened during wait, we need to query completion (without waiting).
         */

        bool checkCompletion = (this->taskCount != taskCountToWait);

        for (auto &state : activeBcsStates) {
            if (this->bcsStates[EngineHelpers::getBcsIndex(state.engineType)].taskCount != state.taskCount) {
                checkCompletion = true;
                break;
            }
        }

        handlePostCompletionOperations(checkCompletion);
    }

    if (printfHandler) {
        if (!printfHandler->printEnqueueOutput()) {
            return WaitStatus::gpuHang;
        }
    }

    return waitStatus;
}

void CommandQueue::setupBarrierTimestampForBcsEngines(aub_stream::EngineType engineType, TimestampPacketDependencies &timestampPacketDependencies) {
    if (!isStallingCommandsOnNextFlushRequired()) {
        return;
    }

    // Ensure we have exactly 1 barrier node.
    if (timestampPacketDependencies.barrierNodes.peekNodes().empty()) {
        timestampPacketDependencies.barrierNodes.add(getGpgpuCommandStreamReceiver().getTimestampPacketAllocator()->getTag());
    }

    if (isOOQEnabled()) {
        // Barrier node will be signalled on gpgpuCsr. Save it for later use on blitters.
        for (auto currentBcsIndex = 0u; currentBcsIndex < bcsEngineCount; currentBcsIndex++) {
            const auto currentBcsEngineType = EngineHelpers::mapBcsIndexToEngineType(currentBcsIndex, true);
            if (currentBcsEngineType == engineType) {
                // Node is already added to barrierNodes for this engine, no need to save it.
                continue;
            }

            // Save latest timestamp (override previous, if any).
            if (!bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.peekNodes().empty()) {
                for (auto &node : bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.peekNodes()) {
                    this->assignDataToOverwrittenBcsNode(node);
                }
            }
            TimestampPacketContainer newContainer{};
            newContainer.assignAndIncrementNodesRefCounts(timestampPacketDependencies.barrierNodes);
            bcsTimestampPacketContainers[currentBcsIndex].lastBarrierToWaitFor.swapNodes(newContainer);
        }
    }
}

void CommandQueue::processBarrierTimestampForBcsEngine(aub_stream::EngineType bcsEngineType, TimestampPacketDependencies &blitDependencies) {
    BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[EngineHelpers::getBcsIndex(bcsEngineType)];
    bcsContainers.lastBarrierToWaitFor.moveNodesToNewContainer(blitDependencies.barrierNodes);
}

void CommandQueue::setLastBcsPacket(aub_stream::EngineType bcsEngineType) {
    if (isOOQEnabled()) {
        TimestampPacketContainer dummyContainer{};
        dummyContainer.assignAndIncrementNodesRefCounts(*this->timestampPacketContainer);

        BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[EngineHelpers::getBcsIndex(bcsEngineType)];
        bcsContainers.lastSignalledPacket.swapNodes(dummyContainer);
    }
}

void CommandQueue::fillCsrDependenciesWithLastBcsPackets(CsrDependencies &csrDeps) {
    for (auto currentBcsIndex = 0u; currentBcsIndex < bcsEngineCount; currentBcsIndex++) {
        BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[currentBcsIndex];
        if (bcsContainers.lastSignalledPacket.peekNodes().empty()) {
            continue;
        }
        csrDeps.timestampPacketContainer.push_back(&bcsContainers.lastSignalledPacket);
    }
}

void CommandQueue::clearLastBcsPackets() {
    for (auto currentBcsIndex = 0u; currentBcsIndex < bcsEngineCount; currentBcsIndex++) {
        BcsTimestampPacketContainers &bcsContainers = bcsTimestampPacketContainers[currentBcsIndex];
        bcsContainers.lastSignalledPacket.moveNodesToNewContainer(*deferredTimestampPackets);
    }
}

bool CommandQueue::migrateMultiGraphicsAllocationsIfRequired(const BuiltinOpParams &operationParams, CommandStreamReceiver &csr) {
    bool migrationHandled = false;
    for (auto argMemObj : {operationParams.srcMemObj, operationParams.dstMemObj}) {
        if (argMemObj) {
            auto memObj = argMemObj->getHighestRootMemObj();
            auto migrateRequiredForArg = memObj->getMultiGraphicsAllocation().requiresMigrations();
            if (migrateRequiredForArg) {
                MigrationController::handleMigration(*this->context, csr, memObj);
                migrationHandled = true;
            }
        }
    }
    return migrationHandled;
}

void CommandQueue::handlePostCompletionOperations(bool checkQueueCompletion) {
    if (checkQueueCompletion && !isCompleted(this->taskCount, this->bcsStates)) {
        return;
    }

    unregisterGpgpuAndBcsCsrClients();

    TimestampPacketContainer nodesToRelease;
    if (deferredTimestampPackets) {
        deferredTimestampPackets->swapNodes(nodesToRelease);
    }
    TimestampPacketContainer multiRootSyncNodesToRelease;
    if (deferredMultiRootSyncNodes.get()) {
        deferredMultiRootSyncNodes->swapNodes(multiRootSyncNodesToRelease);
    }
}

void CommandQueue::registerGpgpuCsrClient() {
    if (!gpgpuCsrClientRegistered) {
        gpgpuCsrClientRegistered = true;

        getGpgpuCommandStreamReceiver().registerClient(this);
    }
}

void CommandQueue::registerBcsCsrClient(CommandStreamReceiver &bcsCsr) {
    auto engineType = bcsCsr.getOsContext().getEngineType();

    auto &bcsState = bcsStates[EngineHelpers::getBcsIndex(engineType)];

    if (!bcsState.csrClientRegistered) {
        bcsState.csrClientRegistered = true;
        bcsCsr.registerClient(this);
    }
}

void CommandQueue::unregisterGpgpuCsrClient() {
    if (gpgpuCsrClientRegistered) {
        gpgpuEngine->commandStreamReceiver->unregisterClient(this);
        gpgpuCsrClientRegistered = false;
    }
}

void CommandQueue::unregisterBcsCsrClient(CommandStreamReceiver &bcsCsr) {
    auto engineType = bcsCsr.getOsContext().getEngineType();

    auto &bcsState = bcsStates[EngineHelpers::getBcsIndex(engineType)];

    if (bcsState.isValid() && bcsState.csrClientRegistered) {
        bcsCsr.unregisterClient(this);
        bcsState.csrClientRegistered = false;
    }
}

void CommandQueue::unregisterGpgpuAndBcsCsrClients() {
    unregisterGpgpuCsrClient();

    for (auto &engine : this->bcsEngines) {
        if (engine) {
            unregisterBcsCsrClient(*engine->commandStreamReceiver);
        }
    }
}

size_t CommandQueue::calculateHostPtrSizeForImage(const size_t *region, size_t rowPitch, size_t slicePitch, Image *image) const {
    auto bytesPerPixel = image->getSurfaceFormatInfo().surfaceFormat.imageElementSizeInBytes;
    auto dstRowPitch = rowPitch ? rowPitch : region[0] * bytesPerPixel;
    auto dstSlicePitch = slicePitch ? slicePitch : ((image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 1 : region[1]) * dstRowPitch);

    return Image::calculateHostPtrSize(region, dstRowPitch, dstSlicePitch, bytesPerPixel, image->getImageDesc().image_type);
}

void CommandQueue::registerWalkerWithProfilingEnqueued(Event *event) {
    if (this->shouldRegisterEnqueuedWalkerWithProfiling && isProfilingEnabled() && event) {
        this->isWalkerWithProfilingEnqueued = true;
    }
}

} // namespace NEO
