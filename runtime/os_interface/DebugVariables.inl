/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*DEBUG FLAGS*/
DECLARE_DEBUG_VARIABLE(bool, EnableDebugBreak, true, "Enable DEBUG_BREAKs")
DECLARE_DEBUG_VARIABLE(bool, FlushAllCaches, false, "pipe controls between enqueues flush all possible caches")
DECLARE_DEBUG_VARIABLE(bool, MakeEachEnqueueBlocking, false, "equivalent of finish after each enqueue")
DECLARE_DEBUG_VARIABLE(bool, DoCpuCopyOnReadBuffer, false, "triggers CPU copy path for Read Buffer calls, only supported for some basic use cases ( no events, not blocked calls )")
DECLARE_DEBUG_VARIABLE(bool, DoCpuCopyOnWriteBuffer, false, "triggers CPU copy path for Write Buffer calls, only supported for some basic use cases ( no events, not blocked calls )")
DECLARE_DEBUG_VARIABLE(bool, DisableResourceRecycling, false, "when set to true disables resource recycling optimization")
DECLARE_DEBUG_VARIABLE(int32_t, InitializeMemoryInDebug, 0x10, "Memory initialization in debug")
DECLARE_DEBUG_VARIABLE(int32_t, SchedulerSimulationReturnInstance, 0, "prints execution model related debug information")
DECLARE_DEBUG_VARIABLE(bool, ForceDispatchScheduler, false, "dispatches scheduler kernel instead of kernel enqueued")
DECLARE_DEBUG_VARIABLE(bool, TrackParentEvents, false, "events track their parents")
/*LOGGING FLAGS*/
DECLARE_DEBUG_VARIABLE(bool, PrintDebugMessages, false, "when enabled, some debug messages will be propagated to console")
DECLARE_DEBUG_VARIABLE(bool, DumpKernels, false, "Enables dumping kernels' program source code to text files and program from binary to bin file")
DECLARE_DEBUG_VARIABLE(bool, DumpKernelArgs, false, "Enables dumping kernels args to binary files")
DECLARE_DEBUG_VARIABLE(bool, LogApiCalls, false, "Enables logging api function calls, inputs and outputs to file")
DECLARE_DEBUG_VARIABLE(bool, LogPatchTokens, false, "Enables logging patch tokens, inputs and outputs to file")
DECLARE_DEBUG_VARIABLE(bool, LogTaskCounts, false, "Enables logging taskCounts and taskLevels to file")
DECLARE_DEBUG_VARIABLE(bool, LogAlignedAllocations, false, "Logs alignedMalloc and alignedFree allocations")
DECLARE_DEBUG_VARIABLE(bool, LogMemoryObject, false, "Logs memory object ptrs, sizes and operations")
DECLARE_DEBUG_VARIABLE(bool, ResidencyDebugEnable, 0, "enables debug messages and checks for Residency Model")
DECLARE_DEBUG_VARIABLE(bool, EventsDebugEnable, 0, "enables debug messages for events, virtual events, blocked enqueues, events trees etc.")
DECLARE_DEBUG_VARIABLE(bool, PrintEMDebugInformation, false, "prints execution model related debug information")
DECLARE_DEBUG_VARIABLE(bool, PrintLWSSizes, false, "prints driver choosen local workgroup sizes")
DECLARE_DEBUG_VARIABLE(bool, PrintDispatchParameters, false, "prints dispatch paramters of kernels passed to clEnqueueNDRangeKernel")
DECLARE_DEBUG_VARIABLE(int32_t, PrintDriverDiagnostics, -1, "prints driver diagnostics messages to stdout (or file if PrintDriverDiagnostics is set), value corresponds to hint level")
DECLARE_DEBUG_VARIABLE(bool, PrintDriverDiagnosticsToFile, false, "If PrintDriverDiagnostics is set, print driver diagnostics messages to file")
/*PERFORMANCE FLAGS*/
DECLARE_DEBUG_VARIABLE(bool, EnableNullHardware, false, "works on Windows only, sets the Null Hardware flag that makes all Command buffers completed while GPU does nothing")
DECLARE_DEBUG_VARIABLE(bool, ForceLinearImages, false, "Force linear images. Default is Y-tiled.")
DECLARE_DEBUG_VARIABLE(bool, ForceSLML3Config, false, "Forces L3Config with SLM for all kernels")
DECLARE_DEBUG_VARIABLE(bool, Force32bitAddressing, false, "Forces 32 bit addresses to be used in 64 bit dll")
DECLARE_DEBUG_VARIABLE(bool, DisableStatelessToStatefulOptimization, false, "Disables stateless to stateful optimization for buffers")
DECLARE_DEBUG_VARIABLE(bool, DisableConcurrentBlockExecution, 0, "disables concurrent block kernel execution")
DECLARE_DEBUG_VARIABLE(bool, UseNewHeapAllocator, true, "Custom 4GB heap allocator is used")
DECLARE_DEBUG_VARIABLE(bool, UseNoRingFlushesKmdMode, true, "Windows only, passes flag to KMD that informs KMD to not emit any ring buffer flushes.")
/*SIMULATION FLAGS*/
DECLARE_DEBUG_VARIABLE(int32_t, SetCommandStreamReceiver, 0, "Set command stream receiver")
DECLARE_DEBUG_VARIABLE(std::string, TbxServer, std::string("127.0.0.1"), "TCP-IP address of TBX server")
DECLARE_DEBUG_VARIABLE(int32_t, TbxPort, 4321, "TCP-IP port of TBX server")
DECLARE_DEBUG_VARIABLE(std::string, ProductFamilyOverride, std::string("unk"), "Specify product for use in AUB/TBX")
DECLARE_DEBUG_VARIABLE(bool, DisableAUBBufferDump, false, "Avoid dumping buffers in AUB files")
DECLARE_DEBUG_VARIABLE(bool, DisableAUBImageDump, false, "Avoid dumping images in AUB files")
DECLARE_DEBUG_VARIABLE(bool, FlattenBatchBufferForAUBDump, false, "Dump multi-level batch buffers to AUB as single, flat batch buffer")
DECLARE_DEBUG_VARIABLE(bool, AddPatchInfoCommentsForAUBDump, false, "Dump comments containing allocations and patching information")
/*FEATURE FLAGS*/
DECLARE_DEBUG_VARIABLE(bool, EnableNV12, true, "Enables NV12 extension")
DECLARE_DEBUG_VARIABLE(bool, EnablePackedYuv, true, "Enables cl_packed_yuv extension")
DECLARE_DEBUG_VARIABLE(bool, EnableIntelVme, true, "Enables cl_intel_motion_estimation extension")
DECLARE_DEBUG_VARIABLE(bool, EnableIntelAdvancedVme, true, "Enables cl_intel_advanced_motion_estimation extension")
DECLARE_DEBUG_VARIABLE(bool, EnableStatelessToStatefulBufferOffsetOpt, false, "Temporary debug variable to help in enabling buffer-offset improvement of the stateless to stateful optimization")
DECLARE_DEBUG_VARIABLE(bool, EnableDeferredDeleter, true, "Enables async deleter")
DECLARE_DEBUG_VARIABLE(bool, EnableAsyncDestroyAllocations, true, "Enables async destroying graphics allocations in mem obj destructor")
DECLARE_DEBUG_VARIABLE(bool, EnableAsyncEventsHandler, true, "Enables async events handler")
DECLARE_DEBUG_VARIABLE(bool, EnableForcePin, true, "Enables early pinning for memory object")
DECLARE_DEBUG_VARIABLE(int32_t, Enable64kbpages, -1, "-1: default behaviour, 0 Disables, 1 Enables support for 64KB pages for driver allocated fine grain svm buffers")
DECLARE_DEBUG_VARIABLE(bool, EnableComputeWorkSizeND, true, "Enables diffrent algorithm to compute local work size")
DECLARE_DEBUG_VARIABLE(bool, EnableComputeWorkSizeSquared, false, "Enables algorithm to compute the most squared work group as possible")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideEnableKmdNotify, -1, "-1: dont override, 0: disable, 1: enable")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideKmdNotifyDelayMicroseconds, -1, "-1: dont override, 0: infinite timeout, >0: timeout in microseconds")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideEnableQuickKmdSleep, -1, "-1: dont override, 0: disable, 1: enable. It works only when Kmd Notify is enabled.")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideQuickKmdSleepDelayMicroseconds, -1, "-1: dont override, 0: infinite timeout, >0: timeout in microseconds")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideEnableQuickKmdSleepForSporadicWaits, -1, "-1: dont override, 0: disable, 1: enable. It works only when QuickKmdSleep is enabled.")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds, -1, "-1: dont override, >0: timeout in microseconds")
DECLARE_DEBUG_VARIABLE(bool, EnableVaLibCalls, true, "Enable cl-va sharing lib calls")
DECLARE_DEBUG_VARIABLE(int32_t, CsrDispatchMode, 0, "Chooses DispatchMode for Csr")
/*DRIVER TOGGLES*/
DECLARE_DEBUG_VARIABLE(int32_t, ForceOCLVersion, 0, "Force specific OpenCL API version")
DECLARE_DEBUG_VARIABLE(int32_t, ForcePreemptionMode, -1, "Keep this variable in sync with PreemptionMode enum. -1 - devices default mode, 1 - disable, 2 - midBatch, 3 - threadGroup, 4 - midThread")
DECLARE_DEBUG_VARIABLE(int32_t, NodeOrdinal, -1, "-1: default do not override, 0: ENGINE_RCS")
DECLARE_DEBUG_VARIABLE(bool, UseMaxSimdSizeToDeduceMaxWorkgroupSize, false, "With this flag on, max workgroup size is deduced using SIMD32 instead of SIMD8, this causes the max wkg size to be 4 times bigger")
DECLARE_DEBUG_VARIABLE(int32_t, OverrideThreadArbitrationPolicy, -1, "-1 (dont override) or any valid config (0: Age Based, 1: Round Robin)")
