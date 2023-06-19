/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/dirty_state_helpers.h"
namespace NEO {
class TagNodeBase;
template <typename GfxFamily>
class DeviceCommandStreamReceiver;
struct PipeControlArgs;

template <typename GfxFamily>
class CommandStreamReceiverHw : public CommandStreamReceiver {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

    struct ImmediateFlushData {
        PipelineSelectArgs pipelineSelectArgs{};
        size_t estimatedSize = 0;

        bool pipelineSelectFullConfigurationNeeded = false;
        bool pipelineSelectDirty = false;
        bool frontEndFullConfigurationNeeded = false;
        bool frontEndDirty = false;
        bool stateComputeModeFullConfigurationNeeded = false;
        bool stateComputeModeDirty = false;
        bool stateBaseAddressFullConfigurationNeeded = false;
        bool stateBaseAddressDirty = false;
        bool contextOneTimeInit = false;
    };

  public:
    static CommandStreamReceiver *create(ExecutionEnvironment &executionEnvironment,
                                         uint32_t rootDeviceIndex,
                                         const DeviceBitfield deviceBitfield) {
        return new CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }

    CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                            uint32_t rootDeviceIndex,
                            const DeviceBitfield deviceBitfield);
    ~CommandStreamReceiverHw() override;

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override;

    CompletionStamp flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart, const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) override;

    CompletionStamp flushImmediateTask(LinearStream &immediateCommandStream, size_t immediateCommandStreamStart,
                                       ImmediateDispatchFlags &dispatchFlags, Device &device) override;

    void forcePipeControl(NEO::LinearStream &commandStreamCSR);

    bool flushBatchedSubmissions() override;
    void programHardwareContext(LinearStream &cmdStream) override;
    size_t getCmdsSizeForHardwareContext() const override;

    static void addBatchBufferEnd(LinearStream &commandStream, void **patchLocation);
    void programEndingCmd(LinearStream &commandStream, void **patchLocation, bool directSubmissionEnabled, bool hasRelaxedOrderingDependencies, bool sipWaAllowed);
    void addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary);

    size_t getRequiredStateBaseAddressSize(const Device &device) const;
    size_t getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags, Device &device);
    size_t getRequiredCmdStreamSize(const DispatchBcsFlags &dispatchBcsFlags);
    size_t getRequiredCmdStreamSizeAligned(const DispatchBcsFlags &dispatchBcsFlags);
    size_t getRequiredCmdSizeForPreamble(Device &device) const;
    size_t getCmdSizeForPreemption(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForEpilogue(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForEpilogueCommands(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForL3Config() const;
    size_t getCmdSizeForPipelineSelect() const;
    size_t getCmdSizeForMediaSampler(bool mediaSamplerRequired) const;
    size_t getCmdSizeForEngineMode(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo);
    size_t getCmdSizeForActivePartitionConfig() const;
    size_t getCmdSizeForStallingCommands(const DispatchFlags &dispatchFlags) const;
    size_t getCmdSizeForStallingNoPostSyncCommands() const;
    size_t getCmdSizeForStallingPostSyncCommands() const;
    size_t getCmdSizeForComputeMode();
    MOCKABLE_VIRTUAL bool hasSharedHandles();

    bool isPipelineSelectAlreadyProgrammed() const;
    void programComputeMode(LinearStream &csr, DispatchFlags &dispatchFlags, const HardwareInfo &hwInfo);

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) override;

    void collectStateBaseAddresPatchInfo(
        uint64_t commandBufferAddress,
        uint64_t commandOffset,
        const LinearStream *dsh,
        const LinearStream *ioh,
        const LinearStream *ssh,
        uint64_t generalStateBase,
        bool imagesSupported);

    void collectStateBaseAddresIohPatchInfo(uint64_t commandBufferAddress, uint64_t commandOffset, const LinearStream &ioh);

    void resetKmdNotifyHelper(KmdNotifyHelper *newHelper);

    CommandStreamReceiverType getType() const override {
        return CommandStreamReceiverType::CSR_HW;
    }

    TaskCountType flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override;

    SubmissionStatus flushTagUpdate() override;
    SubmissionStatus flushMiFlushDW();
    SubmissionStatus flushPipeControl(bool stateCacheFlush);
    SubmissionStatus flushSmallTask(LinearStream &commandStreamTask,
                                    size_t commandStreamStartTask);
    SubmissionStatus flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);
    SubmissionStatus sendRenderStateCacheFlush() override;

    bool isUpdateTagFromWaitEnabled() override;
    void updateTagFromWait() override;

    bool isMultiOsContextCapable() const override;

    MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired) const override;

    bool isDirectSubmissionEnabled() const override {
        return directSubmission.get() != nullptr;
    }

    bool isBlitterDirectSubmissionEnabled() const override {
        return blitterDirectSubmission.get() != nullptr;
    }

    bool directSubmissionRelaxedOrderingEnabled() const override;

    void stopDirectSubmission() override;

    virtual bool isKmdWaitModeActive() { return true; }

    bool initDirectSubmission() override;
    GraphicsAllocation *getClearColorAllocation() override;

    TagAllocatorBase *getTimestampPacketAllocator() override;
    std::unique_ptr<TagAllocatorBase> createMultiRootDeviceTimestampPacketAllocator(const RootDeviceIndicesContainer rootDeviceIndices) override;

    void postInitFlagsSetup() override;
    void programActivePartitionConfig(LinearStream &csr);

    void programComputeBarrierCommand(LinearStream &cmdStream) override {
        programStallingNoPostSyncCommandsForBarrier(cmdStream);
    }
    size_t getCmdsSizeForComputeBarrierCommand() const override {
        return getCmdSizeForStallingNoPostSyncCommands();
    }
    SubmissionStatus initializeDeviceWithFirstSubmission() override;

    HeapDirtyState &getDshState() {
        return dshState;
    }
    HeapDirtyState &getSshState() {
        return sshState;
    }
    HeapDirtyState &getIohState() {
        return iohState;
    }

    void dispatchRayTracingStateCommand(LinearStream &cmdStream, Device &device);

  protected:
    void programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programL3(LinearStream &csr, uint32_t &newL3Config);
    void programPreamble(LinearStream &csr, Device &device, uint32_t &newL3Config);
    void programPipelineSelect(LinearStream &csr, PipelineSelectArgs &pipelineSelectArgs);
    void programEpilogue(LinearStream &csr, Device &device, void **batchBufferEndLocation, DispatchFlags &dispatchFlags);
    void programEpliogueCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programMediaSampler(LinearStream &csr, DispatchFlags &dispatchFlags);
    void programPerDssBackedBuffer(LinearStream &scr, Device &device, DispatchFlags &dispatchFlags);
    void programStateSip(LinearStream &cmdStream, Device &device);
    void programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads);
    void programStallingCommandsForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags);
    void programStallingNoPostSyncCommandsForBarrier(LinearStream &cmdStream);
    void programStallingPostSyncCommandsForBarrier(LinearStream &cmdStream, TagNodeBase &tagNode);
    void programEngineModeCommands(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programEngineModeEpliogue(LinearStream &csr, const DispatchFlags &dispatchFlags);
    void programActivePartitionConfigFlushTask(LinearStream &csr);

    void programEnginePrologue(LinearStream &csr);
    size_t getCmdSizeForPrologue() const;

    void setClearSlmWorkAroundParameter(PipeControlArgs &args);
    void addPipeControlBeforeStateSip(LinearStream &commandStream, Device &device);
    void addPipeControlBefore3dState(LinearStream &commandStream, DispatchFlags &dispatchFlags);
    bool are4GbHeapsAvailable() const;

    uint64_t getScratchPatchAddress();
    void createScratchSpaceController();

    bool detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const;
    bool checkPlatformSupportsNewResourceImplicitFlush() const;
    bool checkPlatformSupportsGpuIdleImplicitFlush() const;
    void configurePostSyncWriteOffset();
    void unregisterDirectSubmissionFromController();
    void createKernelArgsBufferAllocation() override;
    void handleFrontEndStateTransition(const DispatchFlags &dispatchFlags);
    void handlePipelineSelectStateTransition(const DispatchFlags &dispatchFlags);
    void handleStateBaseAddressStateTransition(const DispatchFlags &dispatchFlags, bool &isStateBaseAddressDirty);
    void updateStreamTaskCount(LinearStream &stream, TaskCountType newTaskCount);
    inline void programStateBaseAddress(const IndirectHeap *dsh,
                                        const IndirectHeap *ioh,
                                        const IndirectHeap *ssh,
                                        DispatchFlags &dispatchFlags,
                                        Device &device, LinearStream &commandStreamCSR,
                                        bool stateBaseAddressDirty);

    inline void reprogramStateBaseAddress(const IndirectHeap *dsh,
                                          const IndirectHeap *ioh,
                                          const IndirectHeap *ssh,
                                          DispatchFlags &dispatchFlags,
                                          Device &device, LinearStream &commandStreamCSR,
                                          bool force32BitAllocations, bool sshDirty, bool bindingTablePoolCommandNeeded);
    inline void programStateBaseAddressCommon(const IndirectHeap *dsh,
                                              const IndirectHeap *ioh,
                                              const IndirectHeap *ssh,
                                              StateBaseAddressProperties *sbaProperties,
                                              uint64_t generalStateBaseAddress,
                                              uint64_t indirectObjectStateBaseAddress,
                                              PipelineSelectArgs &pipelineSelectArgs,
                                              Device &device,
                                              LinearStream &csrCommandStream,
                                              bool dispatchBindingTableCommand,
                                              bool areMultipleSubDevicesInContext);

    inline void programSamplerCacheFlushBetweenRedescribedSurfaceReads(LinearStream &commandStreamCSR);
    bool bcsRelaxedOrderingAllowed(const BlitPropertiesContainer &blitPropertiesContainer, bool hasStallingCmds) const;
    inline void handleImmediateFlushPipelineSelectState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData);
    inline void dispatchImmediateFlushPipelineSelectCommand(ImmediateFlushData &flushData, LinearStream &csrStream);
    inline void handleImmediateFlushFrontEndState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData);
    inline void dispatchImmediateFlushFrontEndCommand(uint64_t scratchAddress, ImmediateFlushData &flushData, Device &device, LinearStream &csrStream);
    inline void handleImmediateFlushStateComputeModeState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData);
    inline void dispatchImmediateFlushStateComputeModeCommand(ImmediateFlushData &flushData, LinearStream &csrStream);
    inline void handleImmediateFlushStateBaseAddressState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData, Device &device);
    inline void dispatchImmediateFlushStateBaseAddressCommand(ImmediateFlushData &flushData, LinearStream &csrStream, Device &device);

    inline void handleImmediateFlushOneTimeContextInitState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData);
    inline void dispatchImmediateFlushOneTimeContextInitCommand(ImmediateFlushData &flushData, LinearStream &csrStream);

    inline void handleImmediateFlushAllocationsResidency();

    HeapDirtyState dshState;
    HeapDirtyState iohState;
    HeapDirtyState sshState;

    CsrSizeRequestFlags csrSizeRequestFlags = {};

    bool wasSubmittedToSingleSubdevice = false;

    std::unique_ptr<DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>> directSubmission;
    std::unique_ptr<DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>> blitterDirectSubmission;

    size_t cmdStreamStart = 0;
    uint32_t latestSentBcsWaValue = std::numeric_limits<uint32_t>::max();
};

} // namespace NEO
