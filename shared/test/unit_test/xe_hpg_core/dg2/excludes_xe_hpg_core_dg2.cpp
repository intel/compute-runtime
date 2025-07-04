/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test_base.h"

HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedForDefaultEngineTypeAdjustmentThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenAskedIfStorageInfoAdjustmentIsRequiredThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenProductHelperWhenGetL1CachePolicyThenReturnWriteByPass_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, givenAtLeastXeHpgCoreWhenGetL1CachePolicyThenReturnCorrectValue_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CommandEncodeStatesTest, givenSlmTotalSizeEqualZeroWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerTests, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInDevicePoolThenLocalMemoryPoolIsUsed, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(MemoryManagerTests, givenEnabledLocalMemoryWhenAllocateKernelIsaInDevicePoolThenLocalMemoryPoolIsUsed, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenProgramCorrectL1CachePolicy_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(XeHpgSbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramWBPL1CachePolicy, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(GfxCoreHelperTest, GivenZeroSlmSizeWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned_IsHeapfulSupported, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperCommonTest, givenHwHelperWhenIsFusedEuDisabledForDpasCalledThenFalseReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperCommonTest, givenProductHelperWhenCallingIsCalculationForDisablingEuFusionWithDpasNeededThenFalseReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ProductHelperTest, whenDisableL3ForDebugCalledThenFalseIsReturned, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, GivenSingleCCSEnabledSetupThenCorrectCommandsAreAdded_IsXeHpgCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed_IsXeHpgCore, IGFX_DG2);
