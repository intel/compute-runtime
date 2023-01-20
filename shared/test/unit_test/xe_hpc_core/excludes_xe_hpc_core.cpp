/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, givenOverrideSlmTotalSizeDebugVariableWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsDefaultThenThreadGroupDispatchSizeIsNotChanged, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(DrmMemoryManagerTest, givenDrmAllocationWithWithAlignmentFromUserptrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(WalkerPartitionTests, givenMiAtomicWhenItIsProgrammedThenAllFieldsAreSetCorrectly, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(WalkerPartitionTests, givenProgramBatchBufferStartCommandWhenItIsCalledThenCommandIsProgrammedCorrectly, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, givenDefaultGfxCoreHelperHwWhenMinimalSIMDSizeIsQueriedThen8IsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, whenQueryingMaxNumSamplersThenReturnSixteen, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, givenGfxCoreHelperWhenGettingIfRevisionSpecificBinaryBuiltinIsRequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, whenGettingNumberOfCacheRegionsThenReturnZero, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(PipeControlHelperTests, givenGfxCoreHelperwhenAskingForDcFlushThenReturnTrue, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterPreemptionTests, GivenDebuggerUsedWhenProgrammingStateSipThenStateSipIsAdded, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeThenSCMIsNotAdded, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded_ForceNonCoherentSupportedMatcher, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed_ForceNonCoherentSupportedMatcher, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(BlitTests, givenXyCopyBltCommandWhenAppendBlitCommandsMemCopyIsCalledThenNothingChanged, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsAdjustProgrammableIdPreferredSlmSizeRequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsComputeDispatchAllWalkerEnableInComputeWalkerRequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsGlobalFenceInCommandStreamRequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsSystolicModeConfigurabledThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenGetThreadEuRatioForScratchThen8IsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenCallingGetDeviceMemoryNameThenDdrIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenTrueIsReturned, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(DirectSubmissionTest, givenDebugFlagSetWhenDispatchingPrefetcherThenSetCorrectValue, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenRingBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(GetAllocationDataTestHw, givenSemaphoreBufferAllocationWhenGetAllocationDataIsCalledThenItHasProperFieldsSet, IGFX_XE_HPC_CORE);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerGetAlloctionDataTests, givenCommandBufferAllocationTypeWhenGetAllocationDataIsCalledThenSystemMemoryIsRequested, IGFX_XE_HPC_CORE);