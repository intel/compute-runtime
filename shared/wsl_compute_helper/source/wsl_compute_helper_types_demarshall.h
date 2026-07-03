/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// This is a generated file - please don't modify directly
#pragma once
#include "wsl_compute_helper_types_tokens.h"

template <TOK StrT>
struct Demarshaller;

template <>
struct Demarshaller<TOK_S_GMM_GFX_PARTITIONING> {
    template <typename __GMM_GFX_PARTITIONINGT>
    static bool demarshall(__GMM_GFX_PARTITIONINGT &dst, const TokenHeader *srcTokensBeg, const TokenHeader *srcTokensEnd) {
        const TokenHeader *tok = srcTokensBeg;
        while (tok < srcTokensEnd) {
            if (false == tok->flags.flag4IsVariableLength) {
                if (tok->flags.flag3IsMandatory) {
                    return false;
                }
                tok = tok + 1 + tok->valueDwordCount;
            } else {
                auto varLen = reinterpret_cast<const TokenVariableLength *>(tok);
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_S_GMM_GFX_PARTITIONING:
                    if (false == demarshall(dst, varLen->getValue<TokenHeader>(), varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader))) {
                        return false;
                    }
                    break;
                case TOK_FS_GMM_GFX_PARTITIONING__STANDARD: {
                    const TokenHeader *tokStandard = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokStandardEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokStandard < tokStandardEnd) {
                        if (false == tokStandard->flags.flag4IsVariableLength) {
                            switch (tokStandard->id) {
                            default:
                                if (tokStandard->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                dst.Standard.Base = readTokValue<decltype(dst.Standard.Base)>(*tokStandard);
                            } break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                dst.Standard.Limit = readTokValue<decltype(dst.Standard.Limit)>(*tokStandard);
                            } break;
                            };
                            tokStandard = tokStandard + 1 + tokStandard->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokStandard);
                            if (tokStandard->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokStandard = tokStandard + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokStandard == tokStandardEnd);
                } break;
                case TOK_FS_GMM_GFX_PARTITIONING__STANDARD64KB: {
                    const TokenHeader *tokStandard64KB = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokStandard64KBEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokStandard64KB < tokStandard64KBEnd) {
                        if (false == tokStandard64KB->flags.flag4IsVariableLength) {
                            switch (tokStandard64KB->id) {
                            default:
                                if (tokStandard64KB->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                dst.Standard64KB.Base = readTokValue<decltype(dst.Standard64KB.Base)>(*tokStandard64KB);
                            } break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                dst.Standard64KB.Limit = readTokValue<decltype(dst.Standard64KB.Limit)>(*tokStandard64KB);
                            } break;
                            };
                            tokStandard64KB = tokStandard64KB + 1 + tokStandard64KB->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokStandard64KB);
                            if (tokStandard64KB->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokStandard64KB = tokStandard64KB + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokStandard64KB == tokStandard64KBEnd);
                } break;
                case TOK_FS_GMM_GFX_PARTITIONING__SVM: {
                    const TokenHeader *tokSVM = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokSVMEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokSVM < tokSVMEnd) {
                        if (false == tokSVM->flags.flag4IsVariableLength) {
                            switch (tokSVM->id) {
                            default:
                                if (tokSVM->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                dst.SVM.Base = readTokValue<decltype(dst.SVM.Base)>(*tokSVM);
                            } break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                dst.SVM.Limit = readTokValue<decltype(dst.SVM.Limit)>(*tokSVM);
                            } break;
                            };
                            tokSVM = tokSVM + 1 + tokSVM->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSVM);
                            if (tokSVM->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokSVM = tokSVM + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokSVM == tokSVMEnd);
                } break;
                case TOK_FS_GMM_GFX_PARTITIONING__TR: {
                    const TokenHeader *tokTR = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokTREnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokTR < tokTREnd) {
                        if (false == tokTR->flags.flag4IsVariableLength) {
                            switch (tokTR->id) {
                            default:
                                if (tokTR->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                dst.TR.Base = readTokValue<decltype(dst.TR.Base)>(*tokTR);
                            } break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                dst.TR.Limit = readTokValue<decltype(dst.TR.Limit)>(*tokTR);
                            } break;
                            };
                            tokTR = tokTR + 1 + tokTR->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTR);
                            if (tokTR->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokTR = tokTR + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokTR == tokTREnd);
                } break;
                case TOK_FS_GMM_GFX_PARTITIONING__HEAP32: {
                    uint32_t arrayElementIdHeap32 = varLen->arrayElementId;
                    const TokenHeader *tokHeap32 = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokHeap32End = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokHeap32 < tokHeap32End) {
                        if (false == tokHeap32->flags.flag4IsVariableLength) {
                            switch (tokHeap32->id) {
                            default:
                                if (tokHeap32->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                dst.Heap32[arrayElementIdHeap32].Base = readTokValue<decltype(dst.Heap32[arrayElementIdHeap32].Base)>(*tokHeap32);
                            } break;
                            case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                dst.Heap32[arrayElementIdHeap32].Limit = readTokValue<decltype(dst.Heap32[arrayElementIdHeap32].Limit)>(*tokHeap32);
                            } break;
                            };
                            tokHeap32 = tokHeap32 + 1 + tokHeap32->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokHeap32);
                            if (tokHeap32->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokHeap32 = tokHeap32 + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokHeap32 == tokHeap32End);
                } break;
                };
                tok = tok + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
            }
        }
        WCH_ASSERT(tok == srcTokensEnd);
        return true;
    }
};

template <>
struct Demarshaller<TOK_S_ADAPTER_INFO> {
    template <typename _ADAPTER_INFOT>
    static bool demarshall(_ADAPTER_INFOT &dst, const TokenHeader *srcTokensBeg, const TokenHeader *srcTokensEnd) {
        const TokenHeader *tok = srcTokensBeg;
        while (tok < srcTokensEnd) {
            if (false == tok->flags.flag4IsVariableLength) {
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_FBD_ADAPTER_INFO__KMD_VERSION_INFO: {
                    dst.KmdVersionInfo = readTokValue<decltype(dst.KmdVersionInfo)>(*tok);
                } break;
                case TOK_FBC_ADAPTER_INFO__DEVICE_REGISTRY_PATH: {
                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tok).getValue<char>();
                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tok).getValueSizeInBytes();
                    if (srcSize < sizeof(dst.DeviceRegistryPath)) {
                        return false;
                    }
                    WCH_SAFE_COPY(dst.DeviceRegistryPath, sizeof(dst.DeviceRegistryPath), srcData, sizeof(dst.DeviceRegistryPath));
                } break;
                case TOK_FBD_ADAPTER_INFO__GFX_TIME_STAMP_FREQ: {
                    dst.GfxTimeStampFreq = readTokValue<decltype(dst.GfxTimeStampFreq)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__GFX_CORE_FREQUENCY: {
                    dst.GfxCoreFrequency = readTokValue<decltype(dst.GfxCoreFrequency)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__FSBFREQUENCY: {
                    dst.FSBFrequency = readTokValue<decltype(dst.FSBFrequency)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__MIN_RENDER_FREQ: {
                    dst.MinRenderFreq = readTokValue<decltype(dst.MinRenderFreq)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__MAX_RENDER_FREQ: {
                    dst.MaxRenderFreq = readTokValue<decltype(dst.MaxRenderFreq)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__PACKAGE_TDP: {
                    dst.PackageTdp = readTokValue<decltype(dst.PackageTdp)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__MAX_FILL_RATE: {
                    dst.MaxFillRate = readTokValue<decltype(dst.MaxFillRate)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__NUMBER_OF_EUS: {
                    dst.NumberOfEUs = readTokValue<decltype(dst.NumberOfEUs)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__DW_RELEASE_TARGET: {
                    dst.dwReleaseTarget = readTokValue<decltype(dst.dwReleaseTarget)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__SIZE_OF_DMA_BUFFER: {
                    dst.SizeOfDmaBuffer = readTokValue<decltype(dst.SizeOfDmaBuffer)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__PATCH_LOCATION_LIST_SIZE: {
                    dst.PatchLocationListSize = readTokValue<decltype(dst.PatchLocationListSize)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__ALLOCATION_LIST_SIZE: {
                    dst.AllocationListSize = readTokValue<decltype(dst.AllocationListSize)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__SMALL_PATCH_LOCATION_LIST_SIZE: {
                    dst.SmallPatchLocationListSize = readTokValue<decltype(dst.SmallPatchLocationListSize)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__DEFAULT_CMD_BUFFER_SIZE: {
                    dst.DefaultCmdBufferSize = readTokValue<decltype(dst.DefaultCmdBufferSize)>(*tok);
                } break;
                case TOK_FBQ_ADAPTER_INFO__GFX_MEMORY_SIZE: {
                    dst.GfxMemorySize = readTokValue<decltype(dst.GfxMemorySize)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__SYSTEM_MEMORY_SIZE: {
                    dst.SystemMemorySize = readTokValue<decltype(dst.SystemMemorySize)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__CACHE_LINE_SIZE: {
                    dst.CacheLineSize = readTokValue<decltype(dst.CacheLineSize)>(*tok);
                } break;
                case TOK_FE_ADAPTER_INFO__PROCESSOR_FAMILY: {
                    dst.ProcessorFamily = readTokValue<decltype(dst.ProcessorFamily)>(*tok);
                } break;
                case TOK_FBC_ADAPTER_INFO__IS_HTSUPPORTED: {
                    dst.IsHTSupported = readTokValue<decltype(dst.IsHTSupported)>(*tok);
                } break;
                case TOK_FBC_ADAPTER_INFO__IS_MUTI_CORE_CPU: {
                    dst.IsMutiCoreCpu = readTokValue<decltype(dst.IsMutiCoreCpu)>(*tok);
                } break;
                case TOK_FBC_ADAPTER_INFO__IS_VTDSUPPORTED: {
                    dst.IsVTDSupported = readTokValue<decltype(dst.IsVTDSupported)>(*tok);
                } break;
                case TOK_FBD_ADAPTER_INFO__REGISTRY_PATH_LENGTH: {
                    dst.RegistryPathLength = readTokValue<decltype(dst.RegistryPathLength)>(*tok);
                } break;
                case TOK_FBQ_ADAPTER_INFO__DEDICATED_VIDEO_MEMORY: {
                    dst.DedicatedVideoMemory = readTokValue<decltype(dst.DedicatedVideoMemory)>(*tok);
                } break;
                case TOK_FBQ_ADAPTER_INFO__SYSTEM_SHARED_MEMORY: {
                    dst.SystemSharedMemory = readTokValue<decltype(dst.SystemSharedMemory)>(*tok);
                } break;
                case TOK_FBQ_ADAPTER_INFO__SYSTEM_VIDEO_MEMORY: {
                    dst.SystemVideoMemory = readTokValue<decltype(dst.SystemVideoMemory)>(*tok);
                } break;
                };
                tok = tok + 1 + tok->valueDwordCount;
            } else {
                auto varLen = reinterpret_cast<const TokenVariableLength *>(tok);
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_S_ADAPTER_INFO:
                    if (false == demarshall(dst, varLen->getValue<TokenHeader>(), varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader))) {
                        return false;
                    }
                    break;
                case TOK_FS_ADAPTER_INFO__GFX_PLATFORM: {
                    const TokenHeader *tokGfxPlatform = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokGfxPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokGfxPlatform < tokGfxPlatformEnd) {
                        if (false == tokGfxPlatform->flags.flag4IsVariableLength) {
                            switch (tokGfxPlatform->id) {
                            default:
                                if (tokGfxPlatform->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                dst.GfxPlatform.eProductFamily = readTokValue<decltype(dst.GfxPlatform.eProductFamily)>(*tokGfxPlatform);
                            } break;
                            case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                dst.GfxPlatform.ePCHProductFamily = readTokValue<decltype(dst.GfxPlatform.ePCHProductFamily)>(*tokGfxPlatform);
                            } break;
                            case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                dst.GfxPlatform.eDisplayCoreFamily = readTokValue<decltype(dst.GfxPlatform.eDisplayCoreFamily)>(*tokGfxPlatform);
                            } break;
                            case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                dst.GfxPlatform.eRenderCoreFamily = readTokValue<decltype(dst.GfxPlatform.eRenderCoreFamily)>(*tokGfxPlatform);
                            } break;
                            case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                dst.GfxPlatform.ePlatformType = readTokValue<decltype(dst.GfxPlatform.ePlatformType)>(*tokGfxPlatform);
                            } break;
                            case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                dst.GfxPlatform.usDeviceID = readTokValue<decltype(dst.GfxPlatform.usDeviceID)>(*tokGfxPlatform);
                            } break;
                            case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                dst.GfxPlatform.usRevId = readTokValue<decltype(dst.GfxPlatform.usRevId)>(*tokGfxPlatform);
                            } break;
                            case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                dst.GfxPlatform.usDeviceID_PCH = readTokValue<decltype(dst.GfxPlatform.usDeviceID_PCH)>(*tokGfxPlatform);
                            } break;
                            case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                dst.GfxPlatform.usRevId_PCH = readTokValue<decltype(dst.GfxPlatform.usRevId_PCH)>(*tokGfxPlatform);
                            } break;
                            case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                dst.GfxPlatform.eGTType = readTokValue<decltype(dst.GfxPlatform.eGTType)>(*tokGfxPlatform);
                            } break;
                            };
                            tokGfxPlatform = tokGfxPlatform + 1 + tokGfxPlatform->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGfxPlatform);
                            switch (tokGfxPlatform->id) {
                            default:
                                if (tokGfxPlatform->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FS_PLATFORM_STR__S_RENDER_BLOCK_ID: {
                                const TokenHeader *tokSRenderBlockID = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokSRenderBlockIDEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokSRenderBlockID < tokSRenderBlockIDEnd) {
                                    if (false == tokSRenderBlockID->flags.flag4IsVariableLength) {
                                        switch (tokSRenderBlockID->id) {
                                        default:
                                            if (tokSRenderBlockID->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GFX_GMD_ID_DEF__ANONYMOUS7345__VALUE:
                                            dst.GfxPlatform.sRenderBlockID.Value = readTokValue<decltype(dst.GfxPlatform.sRenderBlockID.Value)>(*tokSRenderBlockID);
                                            break;
                                        };
                                        tokSRenderBlockID = tokSRenderBlockID + 1 + tokSRenderBlockID->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSRenderBlockID);
                                        tokSRenderBlockID = tokSRenderBlockID + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokSRenderBlockID == tokSRenderBlockIDEnd);
                            } break;
                            };
                            tokGfxPlatform = tokGfxPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokGfxPlatform == tokGfxPlatformEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__SKU_TABLE: {
                    const TokenHeader *tokSkuTable = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokSkuTableEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokSkuTable < tokSkuTableEnd) {
                        if (false == tokSkuTable->flags.flag4IsVariableLength) {
                            switch (tokSkuTable->id) {
                            default:
                                if (tokSkuTable->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_BLITTER_RING: {
                                dst.SkuTable.FtrBlitterRing = readTokValue<decltype(dst.SkuTable.FtrBlitterRing)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_ULT: {
                                dst.SkuTable.FtrULT = readTokValue<decltype(dst.SkuTable.FtrULT)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_LCIA: {
                                dst.SkuTable.FtrLCIA = readTokValue<decltype(dst.SkuTable.FtrLCIA)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSRING: {
                                dst.SkuTable.FtrCCSRing = readTokValue<decltype(dst.SkuTable.FtrCCSRing)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSNODE: {
                                dst.SkuTable.FtrCCSNode = readTokValue<decltype(dst.SkuTable.FtrCCSNode)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS3245__FTR_CCSMULTI_INSTANCE: {
                                dst.SkuTable.FtrCCSMultiInstance = readTokValue<decltype(dst.SkuTable.FtrCCSMultiInstance)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS11155__FTR_DISPLAY_DISABLED: {
                                dst.SkuTable.FtrDisplayDisabled = readTokValue<decltype(dst.SkuTable.FtrDisplayDisabled)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21584__FTR_POOLED_EU_ENABLED: {
                                dst.SkuTable.FtrPooledEuEnabled = readTokValue<decltype(dst.SkuTable.FtrPooledEuEnabled)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_BATCH_PREEMPT: {
                                dst.SkuTable.FtrGpGpuMidBatchPreempt = readTokValue<decltype(dst.SkuTable.FtrGpGpuMidBatchPreempt)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_THREAD_GROUP_LEVEL_PREEMPT: {
                                dst.SkuTable.FtrGpGpuThreadGroupLevelPreempt = readTokValue<decltype(dst.SkuTable.FtrGpGpuThreadGroupLevelPreempt)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_GP_GPU_MID_THREAD_LEVEL_PREEMPT: {
                                dst.SkuTable.FtrGpGpuMidThreadLevelPreempt = readTokValue<decltype(dst.SkuTable.FtrGpGpuMidThreadLevelPreempt)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT: {
                                dst.SkuTable.FtrPPGTT = readTokValue<decltype(dst.SkuTable.FtrPPGTT)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_IA32E_GFX_PTES: {
                                dst.SkuTable.FtrIA32eGfxPTEs = readTokValue<decltype(dst.SkuTable.FtrIA32eGfxPTEs)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MEM_TYPE_MOCS_DEFER_PAT: {
                                dst.SkuTable.FtrMemTypeMocsDeferPAT = readTokValue<decltype(dst.SkuTable.FtrMemTypeMocsDeferPAT)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PML4SUPPORT: {
                                dst.SkuTable.FtrPml4Support = readTokValue<decltype(dst.SkuTable.FtrPml4Support)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_SVM: {
                                dst.SkuTable.FtrSVM = readTokValue<decltype(dst.SkuTable.FtrSVM)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_MAPPED_RESOURCE: {
                                dst.SkuTable.FtrTileMappedResource = readTokValue<decltype(dst.SkuTable.FtrTileMappedResource)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TRANSLATION_TABLE: {
                                dst.SkuTable.FtrTranslationTable = readTokValue<decltype(dst.SkuTable.FtrTranslationTable)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_USER_MODE_TRANSLATION_TABLE: {
                                dst.SkuTable.FtrUserModeTranslationTable = readTokValue<decltype(dst.SkuTable.FtrUserModeTranslationTable)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_NULL_PAGES: {
                                dst.SkuTable.FtrNullPages = readTokValue<decltype(dst.SkuTable.FtrNullPages)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_L3IACOHERENCY: {
                                dst.SkuTable.FtrL3IACoherency = readTokValue<decltype(dst.SkuTable.FtrL3IACoherency)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_EDRAM: {
                                dst.SkuTable.FtrEDram = readTokValue<decltype(dst.SkuTable.FtrEDram)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LLCBYPASS: {
                                dst.SkuTable.FtrLLCBypass = readTokValue<decltype(dst.SkuTable.FtrLLCBypass)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CENTRAL_CACHE_POLICY: {
                                dst.SkuTable.FtrCentralCachePolicy = readTokValue<decltype(dst.SkuTable.FtrCentralCachePolicy)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2GPU_MMU: {
                                dst.SkuTable.FtrWddm2GpuMmu = readTokValue<decltype(dst.SkuTable.FtrWddm2GpuMmu)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2SVM: {
                                dst.SkuTable.FtrWddm2Svm = readTokValue<decltype(dst.SkuTable.FtrWddm2Svm)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_STANDARD_MIP_TAIL_FORMAT: {
                                dst.SkuTable.FtrStandardMipTailFormat = readTokValue<decltype(dst.SkuTable.FtrStandardMipTailFormat)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WDDM2_1_64KB_PAGES: {
                                dst.SkuTable.FtrWddm2_1_64kbPages = readTokValue<decltype(dst.SkuTable.FtrWddm2_1_64kbPages)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_E2ECOMPRESSION: {
                                dst.SkuTable.FtrE2ECompression = readTokValue<decltype(dst.SkuTable.FtrE2ECompression)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LINEAR_CCS: {
                                dst.SkuTable.FtrLinearCCS = readTokValue<decltype(dst.SkuTable.FtrLinearCCS)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY: {
                                dst.SkuTable.FtrLocalMemory = readTokValue<decltype(dst.SkuTable.FtrLocalMemory)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PPGTT64KBWALK_OPTIMIZATION: {
                                dst.SkuTable.FtrPpgtt64KBWalkOptimization = readTokValue<decltype(dst.SkuTable.FtrPpgtt64KBWalkOptimization)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_TILE_Y: {
                                dst.SkuTable.FtrTileY = readTokValue<decltype(dst.SkuTable.FtrTileY)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_XE2PLUS_TILING: {
                                dst.SkuTable.FtrXe2PlusTiling = readTokValue<decltype(dst.SkuTable.FtrXe2PlusTiling)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FLAT_PHYS_CCS: {
                                dst.SkuTable.FtrFlatPhysCCS = readTokValue<decltype(dst.SkuTable.FtrFlatPhysCCS)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_XE2COMPRESSION: {
                                dst.SkuTable.FtrXe2Compression = readTokValue<decltype(dst.SkuTable.FtrXe2Compression)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_MULTI_TILE_ARCH: {
                                dst.SkuTable.FtrMultiTileArch = readTokValue<decltype(dst.SkuTable.FtrMultiTileArch)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_LOCAL_MEMORY_ALLOWS4KB: {
                                dst.SkuTable.FtrLocalMemoryAllows4KB = readTokValue<decltype(dst.SkuTable.FtrLocalMemoryAllows4KB)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DISPLAY_XTILING: {
                                dst.SkuTable.FtrDisplayXTiling = readTokValue<decltype(dst.SkuTable.FtrDisplayXTiling)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_CAMERA_CAPTURE_CACHING: {
                                dst.SkuTable.FtrCameraCaptureCaching = readTokValue<decltype(dst.SkuTable.FtrCameraCaptureCaching)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_PML5SUPPORT: {
                                dst.SkuTable.FtrPml5Support = readTokValue<decltype(dst.SkuTable.FtrPml5Support)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_KMD_DAF: {
                                dst.SkuTable.FtrKmdDaf = readTokValue<decltype(dst.SkuTable.FtrKmdDaf)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_FRAME_BUFFER_LLC: {
                                dst.SkuTable.FtrFrameBufferLLC = readTokValue<decltype(dst.SkuTable.FtrFrameBufferLLC)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_DRIVER_FLR: {
                                dst.SkuTable.FtrDriverFLR = readTokValue<decltype(dst.SkuTable.FtrDriverFLR)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_HW_SCHEDULING: {
                                dst.SkuTable.FtrHwScheduling = readTokValue<decltype(dst.SkuTable.FtrHwScheduling)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_LDR2D: {
                                dst.SkuTable.FtrAstcLdr2D = readTokValue<decltype(dst.SkuTable.FtrAstcLdr2D)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC_HDR2D: {
                                dst.SkuTable.FtrAstcHdr2D = readTokValue<decltype(dst.SkuTable.FtrAstcHdr2D)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS37751__FTR_ASTC3D: {
                                dst.SkuTable.FtrAstc3D = readTokValue<decltype(dst.SkuTable.FtrAstc3D)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS42853__FTR_FBC: {
                                dst.SkuTable.FtrFbc = readTokValue<decltype(dst.SkuTable.FtrFbc)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_REND_COMP: {
                                dst.SkuTable.FtrRendComp = readTokValue<decltype(dst.SkuTable.FtrRendComp)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS54736__FTR_DISPLAY_YTILING: {
                                dst.SkuTable.FtrDisplayYTiling = readTokValue<decltype(dst.SkuTable.FtrDisplayYTiling)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_S3D: {
                                dst.SkuTable.FtrS3D = readTokValue<decltype(dst.SkuTable.FtrS3D)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS66219__FTR_DISPLAY_ENGINE_S3D: {
                                dst.SkuTable.FtrDisplayEngineS3d = readTokValue<decltype(dst.SkuTable.FtrDisplayEngineS3d)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS89755__FTR_VGT: {
                                dst.SkuTable.FtrVgt = readTokValue<decltype(dst.SkuTable.FtrVgt)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS91822__FTR_ASSIGNED_GPU_TILE: {
                                dst.SkuTable.FtrAssignedGpuTile = readTokValue<decltype(dst.SkuTable.FtrAssignedGpuTile)>(*tokSkuTable);
                            } break;
                            case TOK_FBD_SKU_FEATURE_TABLE__ANONYMOUS21990__FTR_WALKER_MTP:
                                dst.SkuTable.FtrWalkerMTP = readTokValue<decltype(dst.SkuTable.FtrWalkerMTP)>(*tokSkuTable);
                                break;
                            case TOK_FBD_SKU_FEATURE_TABLE__FTR_EFFICIENT64_BIT_ADDRESSING:
                                dst.SkuTable.FtrEfficient64BitAddressing = readTokValue<decltype(dst.SkuTable.FtrEfficient64BitAddressing)>(*tokSkuTable);
                                break;
                            case TOK_FBD_SKU_FEATURE_TABLE__FTR_HW_SEMAPHORE64:
                                dst.SkuTable.FtrHwSemaphore64 = readTokValue<decltype(dst.SkuTable.FtrHwSemaphore64)>(*tokSkuTable);
                                break;
                            };
                            tokSkuTable = tokSkuTable + 1 + tokSkuTable->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSkuTable);
                            if (tokSkuTable->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokSkuTable = tokSkuTable + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokSkuTable == tokSkuTableEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__WA_TABLE: {
                    const TokenHeader *tokWaTable = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokWaTableEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokWaTable < tokWaTableEnd) {
                        if (false == tokWaTable->flags.flag4IsVariableLength) {
                            switch (tokWaTable->id) {
                            default:
                                if (tokWaTable->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_WA_TABLE__WA_ALIGN_INDEX_BUFFER: {
                                dst.WaTable.WaAlignIndexBuffer = readTokValue<decltype(dst.WaTable.WaAlignIndexBuffer)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_SEND_MIFLUSHBEFORE_VFE: {
                                dst.WaTable.WaSendMIFLUSHBeforeVFE = readTokValue<decltype(dst.WaTable.WaSendMIFLUSHBeforeVFE)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISABLE_PER_CTXT_PREEMPTION_GRANULARITY_CONTROL: {
                                dst.WaTable.WaDisablePerCtxtPreemptionGranularityControl = readTokValue<decltype(dst.WaTable.WaDisablePerCtxtPreemptionGranularityControl)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISABLE_LSQCROPERFFOR_OCL: {
                                dst.WaTable.WaDisableLSQCROPERFforOCL = readTokValue<decltype(dst.WaTable.WaDisableLSQCROPERFforOCL)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_VALIGN2FOR96BPP_FORMATS: {
                                dst.WaTable.WaValign2For96bppFormats = readTokValue<decltype(dst.WaTable.WaValign2For96bppFormats)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_VALIGN2FOR_R8G8B8UINTFORMAT: {
                                dst.WaTable.WaValign2ForR8G8B8UINTFormat = readTokValue<decltype(dst.WaTable.WaValign2ForR8G8B8UINTFormat)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_CSRUNCACHABLE: {
                                dst.WaTable.WaCSRUncachable = readTokValue<decltype(dst.WaTable.WaCSRUncachable)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISABLE_FUSED_THREAD_SCHEDULING: {
                                dst.WaTable.WaDisableFusedThreadScheduling = readTokValue<decltype(dst.WaTable.WaDisableFusedThreadScheduling)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_MODIFY_VFESTATE_AFTER_GPGPUPREEMPTION: {
                                dst.WaTable.WaModifyVFEStateAfterGPGPUPreemption = readTokValue<decltype(dst.WaTable.WaModifyVFEStateAfterGPGPUPreemption)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_CURSOR16K: {
                                dst.WaTable.WaCursor16K = readTokValue<decltype(dst.WaTable.WaCursor16K)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA8K_ALIGNFOR_ASYNC_FLIP: {
                                dst.WaTable.Wa8kAlignforAsyncFlip = readTokValue<decltype(dst.WaTable.Wa8kAlignforAsyncFlip)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA29BIT_DISPLAY_ADDR_LIMIT: {
                                dst.WaTable.Wa29BitDisplayAddrLimit = readTokValue<decltype(dst.WaTable.Wa29BitDisplayAddrLimit)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_ALIGN_CONTEXT_IMAGE: {
                                dst.WaTable.WaAlignContextImage = readTokValue<decltype(dst.WaTable.WaAlignContextImage)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_FORCE_GLOBAL_GTT: {
                                dst.WaTable.WaForceGlobalGTT = readTokValue<decltype(dst.WaTable.WaForceGlobalGTT)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_REPORT_PERF_COUNT_FORCE_GLOBAL_GTT: {
                                dst.WaTable.WaReportPerfCountForceGlobalGTT = readTokValue<decltype(dst.WaTable.WaReportPerfCountForceGlobalGTT)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_OA_ADDRESS_TRANSLATION: {
                                dst.WaTable.WaOaAddressTranslation = readTokValue<decltype(dst.WaTable.WaOaAddressTranslation)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA2ROW_VERTICAL_ALIGNMENT: {
                                dst.WaTable.Wa2RowVerticalAlignment = readTokValue<decltype(dst.WaTable.Wa2RowVerticalAlignment)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_PPGTT_ALIAS_GLOBAL_GTT_SPACE: {
                                dst.WaTable.WaPpgttAliasGlobalGttSpace = readTokValue<decltype(dst.WaTable.WaPpgttAliasGlobalGttSpace)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_CLEAR_FENCE_REGISTERS_AT_DRIVER_INIT: {
                                dst.WaTable.WaClearFenceRegistersAtDriverInit = readTokValue<decltype(dst.WaTable.WaClearFenceRegistersAtDriverInit)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_RESTRICT_PITCH128KB: {
                                dst.WaTable.WaRestrictPitch128KB = readTokValue<decltype(dst.WaTable.WaRestrictPitch128KB)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_AVOID_LLC: {
                                dst.WaTable.WaAvoidLLC = readTokValue<decltype(dst.WaTable.WaAvoidLLC)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_AVOID_L3: {
                                dst.WaTable.WaAvoidL3 = readTokValue<decltype(dst.WaTable.WaAvoidL3)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA16TILE_FENCES_ONLY: {
                                dst.WaTable.Wa16TileFencesOnly = readTokValue<decltype(dst.WaTable.Wa16TileFencesOnly)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA16MBOABUFFER_ALIGNMENT: {
                                dst.WaTable.Wa16MBOABufferAlignment = readTokValue<decltype(dst.WaTable.Wa16MBOABufferAlignment)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_TRANSLATION_TABLE_UNAVAILABLE: {
                                dst.WaTable.WaTranslationTableUnavailable = readTokValue<decltype(dst.WaTable.WaTranslationTableUnavailable)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_NO_MINIMIZED_TRIVIAL_SURFACE_PADDING: {
                                dst.WaTable.WaNoMinimizedTrivialSurfacePadding = readTokValue<decltype(dst.WaTable.WaNoMinimizedTrivialSurfacePadding)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_NO_BUFFER_SAMPLER_PADDING: {
                                dst.WaTable.WaNoBufferSamplerPadding = readTokValue<decltype(dst.WaTable.WaNoBufferSamplerPadding)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_SURFACE_STATE_PLANAR_YOFFSET_ALIGN_BY2: {
                                dst.WaTable.WaSurfaceStatePlanarYOffsetAlignBy2 = readTokValue<decltype(dst.WaTable.WaSurfaceStatePlanarYOffsetAlignBy2)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_GTT_CACHING_OFF_BY_DEFAULT: {
                                dst.WaTable.WaGttCachingOffByDefault = readTokValue<decltype(dst.WaTable.WaGttCachingOffByDefault)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_TOUCH_ALL_SVM_MEMORY: {
                                dst.WaTable.WaTouchAllSvmMemory = readTokValue<decltype(dst.WaTable.WaTouchAllSvmMemory)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_IOBADDRESS_MUST_BE_VALID_IN_HW_CONTEXT: {
                                dst.WaTable.WaIOBAddressMustBeValidInHwContext = readTokValue<decltype(dst.WaTable.WaIOBAddressMustBeValidInHwContext)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_FLUSH_TLB_AFTER_CPU_GGTT_WRITES: {
                                dst.WaTable.WaFlushTlbAfterCpuGgttWrites = readTokValue<decltype(dst.WaTable.WaFlushTlbAfterCpuGgttWrites)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_MSAA8X_TILE_YDEPTH_PITCH_ALIGNMENT: {
                                dst.WaTable.WaMsaa8xTileYDepthPitchAlignment = readTokValue<decltype(dst.WaTable.WaMsaa8xTileYDepthPitchAlignment)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISABLE_NULL_PAGE_AS_DUMMY: {
                                dst.WaTable.WaDisableNullPageAsDummy = readTokValue<decltype(dst.WaTable.WaDisableNullPageAsDummy)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_USE_VALIGN16ON_TILE_XYBPP816: {
                                dst.WaTable.WaUseVAlign16OnTileXYBpp816 = readTokValue<decltype(dst.WaTable.WaUseVAlign16OnTileXYBpp816)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_GTT_PAT0: {
                                dst.WaTable.WaGttPat0 = readTokValue<decltype(dst.WaTable.WaGttPat0)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_GTT_PAT0WB: {
                                dst.WaTable.WaGttPat0WB = readTokValue<decltype(dst.WaTable.WaGttPat0WB)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_MEM_TYPE_IS_MAX_OF_PAT_AND_MOCS: {
                                dst.WaTable.WaMemTypeIsMaxOfPatAndMocs = readTokValue<decltype(dst.WaTable.WaMemTypeIsMaxOfPatAndMocs)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_GTT_PAT0GTT_WB_OVER_OS_IOMMU_ELLC_ONLY: {
                                dst.WaTable.WaGttPat0GttWbOverOsIommuEllcOnly = readTokValue<decltype(dst.WaTable.WaGttPat0GttWbOverOsIommuEllcOnly)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_ADD_DUMMY_PAGE_FOR_DISPLAY_PREFETCH: {
                                dst.WaTable.WaAddDummyPageForDisplayPrefetch = readTokValue<decltype(dst.WaTable.WaAddDummyPageForDisplayPrefetch)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_LLCCACHING_UNSUPPORTED: {
                                dst.WaTable.WaLLCCachingUnsupported = readTokValue<decltype(dst.WaTable.WaLLCCachingUnsupported)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DOUBLE_FAST_CLEAR_WIDTH_ALIGNMENT: {
                                dst.WaTable.WaDoubleFastClearWidthAlignment = readTokValue<decltype(dst.WaTable.WaDoubleFastClearWidthAlignment)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_COMPRESSED_RESOURCE_REQUIRES_CONST_VA21: {
                                dst.WaTable.WaCompressedResourceRequiresConstVA21 = readTokValue<decltype(dst.WaTable.WaCompressedResourceRequiresConstVA21)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISREGARD_PLATFORM_CHECKS: {
                                dst.WaTable.WaDisregardPlatformChecks = readTokValue<decltype(dst.WaTable.WaDisregardPlatformChecks)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_LOSSLESS_COMPRESSION_SURFACE_STRIDE: {
                                dst.WaTable.WaLosslessCompressionSurfaceStride = readTokValue<decltype(dst.WaTable.WaLosslessCompressionSurfaceStride)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_FBC_LINEAR_SURFACE_STRIDE: {
                                dst.WaTable.WaFbcLinearSurfaceStride = readTokValue<decltype(dst.WaTable.WaFbcLinearSurfaceStride)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA4K_ALIGN_UVOFFSET_NV12LINEAR_SURFACE: {
                                dst.WaTable.Wa4kAlignUVOffsetNV12LinearSurface = readTokValue<decltype(dst.WaTable.Wa4kAlignUVOffsetNV12LinearSurface)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_ASTC_CORRUPTION_FOR_ODD_COMPRESSED_BLOCK_SIZE_X: {
                                dst.WaTable.WaAstcCorruptionForOddCompressedBlockSizeX = readTokValue<decltype(dst.WaTable.WaAstcCorruptionForOddCompressedBlockSizeX)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_AUX_TABLE16KGRANULAR: {
                                dst.WaTable.WaAuxTable16KGranular = readTokValue<decltype(dst.WaTable.WaAuxTable16KGranular)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_ENCRYPTED_EDRAM_ONLY_PARTIALS: {
                                dst.WaTable.WaEncryptedEdramOnlyPartials = readTokValue<decltype(dst.WaTable.WaEncryptedEdramOnlyPartials)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_DISABLE_EDRAM_FOR_DISPLAY_RT: {
                                dst.WaTable.WaDisableEdramForDisplayRT = readTokValue<decltype(dst.WaTable.WaDisableEdramForDisplayRT)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_LIMIT128BMEDIA_COMPR: {
                                dst.WaTable.WaLimit128BMediaCompr = readTokValue<decltype(dst.WaTable.WaLimit128BMediaCompr)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_UNTYPED_BUFFER_COMPRESSION: {
                                dst.WaTable.WaUntypedBufferCompression = readTokValue<decltype(dst.WaTable.WaUntypedBufferCompression)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_SAMPLER_CACHE_FLUSH_BETWEEN_REDESCRIBED_SURFACE_READS: {
                                dst.WaTable.WaSamplerCacheFlushBetweenRedescribedSurfaceReads = readTokValue<decltype(dst.WaTable.WaSamplerCacheFlushBetweenRedescribedSurfaceReads)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA_ALIGN_YUVRESOURCE_TO_LCU: {
                                dst.WaTable.WaAlignYUVResourceToLCU = readTokValue<decltype(dst.WaTable.WaAlignYUVResourceToLCU)>(*tokWaTable);
                            } break;
                            case TOK_FBD_WA_TABLE__WA32BPP_TILE_Y2DCOLOR_NO_HALIGN4: {
                                dst.WaTable.Wa32bppTileY2DColorNoHAlign4 = readTokValue<decltype(dst.WaTable.Wa32bppTileY2DColorNoHAlign4)>(*tokWaTable);
                            } break;
                            };
                            tokWaTable = tokWaTable + 1 + tokWaTable->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokWaTable);
                            if (tokWaTable->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokWaTable = tokWaTable + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokWaTable == tokWaTableEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__OUTPUT_FRAME_RATE: {
                    const TokenHeader *tokOutputFrameRate = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokOutputFrameRateEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokOutputFrameRate < tokOutputFrameRateEnd) {
                        if (false == tokOutputFrameRate->flags.flag4IsVariableLength) {
                            switch (tokOutputFrameRate->id) {
                            default:
                                if (tokOutputFrameRate->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_FRAME_RATE__UI_NUMERATOR: {
                                dst.OutputFrameRate.uiNumerator = readTokValue<decltype(dst.OutputFrameRate.uiNumerator)>(*tokOutputFrameRate);
                            } break;
                            case TOK_FBD_FRAME_RATE__UI_DENOMINATOR: {
                                dst.OutputFrameRate.uiDenominator = readTokValue<decltype(dst.OutputFrameRate.uiDenominator)>(*tokOutputFrameRate);
                            } break;
                            };
                            tokOutputFrameRate = tokOutputFrameRate + 1 + tokOutputFrameRate->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOutputFrameRate);
                            if (tokOutputFrameRate->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokOutputFrameRate = tokOutputFrameRate + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokOutputFrameRate == tokOutputFrameRateEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__INPUT_FRAME_RATE: {
                    const TokenHeader *tokInputFrameRate = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokInputFrameRateEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokInputFrameRate < tokInputFrameRateEnd) {
                        if (false == tokInputFrameRate->flags.flag4IsVariableLength) {
                            switch (tokInputFrameRate->id) {
                            default:
                                if (tokInputFrameRate->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_FRAME_RATE__UI_NUMERATOR: {
                                dst.InputFrameRate.uiNumerator = readTokValue<decltype(dst.InputFrameRate.uiNumerator)>(*tokInputFrameRate);
                            } break;
                            case TOK_FBD_FRAME_RATE__UI_DENOMINATOR: {
                                dst.InputFrameRate.uiDenominator = readTokValue<decltype(dst.InputFrameRate.uiDenominator)>(*tokInputFrameRate);
                            } break;
                            };
                            tokInputFrameRate = tokInputFrameRate + 1 + tokInputFrameRate->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInputFrameRate);
                            if (tokInputFrameRate->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokInputFrameRate = tokInputFrameRate + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokInputFrameRate == tokInputFrameRateEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__CAPS: {
                    const TokenHeader *tokCaps = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokCapsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokCaps < tokCapsEnd) {
                        if (false == tokCaps->flags.flag4IsVariableLength) {
                            switch (tokCaps->id) {
                            default:
                                if (tokCaps->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_KMD_CAPS_INFO__GAMMA_RGB256X3X16: {
                                dst.Caps.Gamma_Rgb256x3x16 = readTokValue<decltype(dst.Caps.Gamma_Rgb256x3x16)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__GDIACCELERATION: {
                                dst.Caps.GDIAcceleration = readTokValue<decltype(dst.Caps.GDIAcceleration)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__OS_MANAGED_HW_CONTEXT: {
                                dst.Caps.OsManagedHwContext = readTokValue<decltype(dst.Caps.OsManagedHwContext)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__GRAPHICS_PREEMPTION_GRANULARITY: {
                                dst.Caps.GraphicsPreemptionGranularity = readTokValue<decltype(dst.Caps.GraphicsPreemptionGranularity)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__COMPUTE_PREEMPTION_GRANULARITY: {
                                dst.Caps.ComputePreemptionGranularity = readTokValue<decltype(dst.Caps.ComputePreemptionGranularity)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__INSTRUMENTATION_IS_ENABLED: {
                                dst.Caps.InstrumentationIsEnabled = readTokValue<decltype(dst.Caps.InstrumentationIsEnabled)>(*tokCaps);
                            } break;
                            case TOK_FBD_KMD_CAPS_INFO__DRIVER_STORE_ENABLED: {
                                dst.Caps.DriverStoreEnabled = readTokValue<decltype(dst.Caps.DriverStoreEnabled)>(*tokCaps);
                            } break;
                            };
                            tokCaps = tokCaps + 1 + tokCaps->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCaps);
                            if (tokCaps->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokCaps = tokCaps + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokCaps == tokCapsEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__OVERLAY_CAPS: {
                    const TokenHeader *tokOverlayCaps = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokOverlayCapsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokOverlayCaps < tokOverlayCapsEnd) {
                        if (false == tokOverlayCaps->flags.flag4IsVariableLength) {
                            switch (tokOverlayCaps->id) {
                            default:
                                if (tokOverlayCaps->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS_VALUE: {
                                dst.OverlayCaps.CapsValue = readTokValue<decltype(dst.OverlayCaps.CapsValue)>(*tokOverlayCaps);
                            } break;
                            case TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_OVERLAY_DISPLAY_WIDTH: {
                                dst.OverlayCaps.MaxOverlayDisplayWidth = readTokValue<decltype(dst.OverlayCaps.MaxOverlayDisplayWidth)>(*tokOverlayCaps);
                            } break;
                            case TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_OVERLAY_DISPLAY_HEIGHT: {
                                dst.OverlayCaps.MaxOverlayDisplayHeight = readTokValue<decltype(dst.OverlayCaps.MaxOverlayDisplayHeight)>(*tokOverlayCaps);
                            } break;
                            case TOK_FBC_KMD_OVERLAY_CAPS_INFO__HWSCALER_EXISTS: {
                                dst.OverlayCaps.HWScalerExists = readTokValue<decltype(dst.OverlayCaps.HWScalerExists)>(*tokOverlayCaps);
                            } break;
                            case TOK_FBD_KMD_OVERLAY_CAPS_INFO__MAX_HWSCALER_STRIDE: {
                                dst.OverlayCaps.MaxHWScalerStride = readTokValue<decltype(dst.OverlayCaps.MaxHWScalerStride)>(*tokOverlayCaps);
                            } break;
                            };
                            tokOverlayCaps = tokOverlayCaps + 1 + tokOverlayCaps->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOverlayCaps);
                            switch (tokOverlayCaps->id) {
                            default:
                                if (tokOverlayCaps->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FS_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__CAPS: {
                                const TokenHeader *tokCaps = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokCapsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokCaps < tokCapsEnd) {
                                    if (false == tokCaps->flags.flag4IsVariableLength) {
                                        switch (tokCaps->id) {
                                        default:
                                            if (tokCaps->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__FULL_RANGE_RGB: {
                                            dst.OverlayCaps.Caps.FullRangeRGB = readTokValue<decltype(dst.OverlayCaps.Caps.FullRangeRGB)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__LIMITED_RANGE_RGB: {
                                            dst.OverlayCaps.Caps.LimitedRangeRGB = readTokValue<decltype(dst.OverlayCaps.Caps.LimitedRangeRGB)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT601: {
                                            dst.OverlayCaps.Caps.YCbCr_BT601 = readTokValue<decltype(dst.OverlayCaps.Caps.YCbCr_BT601)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT709: {
                                            dst.OverlayCaps.Caps.YCbCr_BT709 = readTokValue<decltype(dst.OverlayCaps.Caps.YCbCr_BT709)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT601_XV_YCC: {
                                            dst.OverlayCaps.Caps.YCbCr_BT601_xvYCC = readTokValue<decltype(dst.OverlayCaps.Caps.YCbCr_BT601_xvYCC)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__YCB_CR_BT709_XV_YCC: {
                                            dst.OverlayCaps.Caps.YCbCr_BT709_xvYCC = readTokValue<decltype(dst.OverlayCaps.Caps.YCbCr_BT709_xvYCC)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__STRETCH_X: {
                                            dst.OverlayCaps.Caps.StretchX = readTokValue<decltype(dst.OverlayCaps.Caps.StretchX)>(*tokCaps);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_CAPS_INFO__ANONYMOUS5171__ANONYMOUS5191__STRETCH_Y: {
                                            dst.OverlayCaps.Caps.StretchY = readTokValue<decltype(dst.OverlayCaps.Caps.StretchY)>(*tokCaps);
                                        } break;
                                        };
                                        tokCaps = tokCaps + 1 + tokCaps->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCaps);
                                        if (tokCaps->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokCaps = tokCaps + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokCaps == tokCapsEnd);
                            } break;
                            case TOK_FS_KMD_OVERLAY_CAPS_INFO__OVOVERRIDE: {
                                const TokenHeader *tokOVOverride = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokOVOverrideEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokOVOverride < tokOVOverrideEnd) {
                                    if (false == tokOVOverride->flags.flag4IsVariableLength) {
                                        switch (tokOVOverride->id) {
                                        default:
                                            if (tokOVOverride->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_KMD_OVERLAY_OVERRIDE__OVERRIDE_OVERLAY_CAPS: {
                                            dst.OverlayCaps.OVOverride.OverrideOverlayCaps = readTokValue<decltype(dst.OverlayCaps.OVOverride.OverrideOverlayCaps)>(*tokOVOverride);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_OVERRIDE__RGBOVERLAY: {
                                            dst.OverlayCaps.OVOverride.RGBOverlay = readTokValue<decltype(dst.OverlayCaps.OVOverride.RGBOverlay)>(*tokOVOverride);
                                        } break;
                                        case TOK_FBD_KMD_OVERLAY_OVERRIDE__YUY2OVERLAY: {
                                            dst.OverlayCaps.OVOverride.YUY2Overlay = readTokValue<decltype(dst.OverlayCaps.OVOverride.YUY2Overlay)>(*tokOVOverride);
                                        } break;
                                        };
                                        tokOVOverride = tokOVOverride + 1 + tokOVOverride->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOVOverride);
                                        if (tokOVOverride->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokOVOverride = tokOVOverride + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokOVOverride == tokOVOverrideEnd);
                            } break;
                            };
                            tokOverlayCaps = tokOverlayCaps + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokOverlayCaps == tokOverlayCapsEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__SYSTEM_INFO: {
                    const TokenHeader *tokSystemInfo = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokSystemInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokSystemInfo < tokSystemInfoEnd) {
                        if (false == tokSystemInfo->flags.flag4IsVariableLength) {
                            switch (tokSystemInfo->id) {
                            default:
                                if (tokSystemInfo->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_GT_SYSTEM_INFO__EUCOUNT: {
                                dst.SystemInfo.EUCount = readTokValue<decltype(dst.SystemInfo.EUCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__THREAD_COUNT: {
                                dst.SystemInfo.ThreadCount = readTokValue<decltype(dst.SystemInfo.ThreadCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__SLICE_COUNT: {
                                dst.SystemInfo.SliceCount = readTokValue<decltype(dst.SystemInfo.SliceCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__SUB_SLICE_COUNT: {
                                dst.SystemInfo.SubSliceCount = readTokValue<decltype(dst.SystemInfo.SubSliceCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__DUAL_SUB_SLICE_COUNT: {
                                dst.SystemInfo.DualSubSliceCount = readTokValue<decltype(dst.SystemInfo.DualSubSliceCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBQ_GT_SYSTEM_INFO__L3CACHE_SIZE_IN_KB: {
                                dst.SystemInfo.L3CacheSizeInKb = readTokValue<decltype(dst.SystemInfo.L3CacheSizeInKb)>(*tokSystemInfo);
                            } break;
                            case TOK_FBQ_GT_SYSTEM_INFO__LLCCACHE_SIZE_IN_KB: {
                                dst.SystemInfo.LLCCacheSizeInKb = readTokValue<decltype(dst.SystemInfo.LLCCacheSizeInKb)>(*tokSystemInfo);
                            } break;
                            case TOK_FBQ_GT_SYSTEM_INFO__EDRAM_SIZE_IN_KB: {
                                dst.SystemInfo.EdramSizeInKb = readTokValue<decltype(dst.SystemInfo.EdramSizeInKb)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__L3BANK_COUNT: {
                                dst.SystemInfo.L3BankCount = readTokValue<decltype(dst.SystemInfo.L3BankCount)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_FILL_RATE: {
                                dst.SystemInfo.MaxFillRate = readTokValue<decltype(dst.SystemInfo.MaxFillRate)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__EU_COUNT_PER_POOL_MAX: {
                                dst.SystemInfo.EuCountPerPoolMax = readTokValue<decltype(dst.SystemInfo.EuCountPerPoolMax)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__EU_COUNT_PER_POOL_MIN: {
                                dst.SystemInfo.EuCountPerPoolMin = readTokValue<decltype(dst.SystemInfo.EuCountPerPoolMin)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_VS_THREADS: {
                                dst.SystemInfo.TotalVsThreads = readTokValue<decltype(dst.SystemInfo.TotalVsThreads)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_HS_THREADS: {
                                dst.SystemInfo.TotalHsThreads = readTokValue<decltype(dst.SystemInfo.TotalHsThreads)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_DS_THREADS: {
                                dst.SystemInfo.TotalDsThreads = readTokValue<decltype(dst.SystemInfo.TotalDsThreads)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_GS_THREADS: {
                                dst.SystemInfo.TotalGsThreads = readTokValue<decltype(dst.SystemInfo.TotalGsThreads)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_PS_THREADS_WINDOWER_RANGE: {
                                dst.SystemInfo.TotalPsThreadsWindowerRange = readTokValue<decltype(dst.SystemInfo.TotalPsThreadsWindowerRange)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__TOTAL_VS_THREADS_POCS: {
                                dst.SystemInfo.TotalVsThreads_Pocs = readTokValue<decltype(dst.SystemInfo.TotalVsThreads_Pocs)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__CSR_SIZE_IN_MB: {
                                dst.SystemInfo.CsrSizeInMb = readTokValue<decltype(dst.SystemInfo.CsrSizeInMb)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_EU_PER_SUB_SLICE: {
                                dst.SystemInfo.MaxEuPerSubSlice = readTokValue<decltype(dst.SystemInfo.MaxEuPerSubSlice)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_SLICES_SUPPORTED: {
                                dst.SystemInfo.MaxSlicesSupported = readTokValue<decltype(dst.SystemInfo.MaxSlicesSupported)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_SUB_SLICES_SUPPORTED: {
                                dst.SystemInfo.MaxSubSlicesSupported = readTokValue<decltype(dst.SystemInfo.MaxSubSlicesSupported)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_DUAL_SUB_SLICES_SUPPORTED: {
                                dst.SystemInfo.MaxDualSubSlicesSupported = readTokValue<decltype(dst.SystemInfo.MaxDualSubSlicesSupported)>(*tokSystemInfo);
                            } break;
                            case TOK_FBB_GT_SYSTEM_INFO__IS_L3HASH_MODE_ENABLED: {
                                dst.SystemInfo.IsL3HashModeEnabled = readTokValue<decltype(dst.SystemInfo.IsL3HashModeEnabled)>(*tokSystemInfo);
                            } break;
                            case TOK_FBB_GT_SYSTEM_INFO__IS_DYNAMICALLY_POPULATED: {
                                dst.SystemInfo.IsDynamicallyPopulated = readTokValue<decltype(dst.SystemInfo.IsDynamicallyPopulated)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__RESERVED_CCSWAYS: {
                                dst.SystemInfo.ReservedCCSWays = readTokValue<decltype(dst.SystemInfo.ReservedCCSWays)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__NUM_THREADS_PER_EU: {
                                dst.SystemInfo.NumThreadsPerEu = readTokValue<decltype(dst.SystemInfo.NumThreadsPerEu)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__MAX_VECS: {
                                dst.SystemInfo.MaxVECS = readTokValue<decltype(dst.SystemInfo.MaxVECS)>(*tokSystemInfo);
                            } break;
                            case TOK_FBD_GT_SYSTEM_INFO__SLMSIZE_IN_KB: {
                                dst.SystemInfo.SLMSizeInKb = readTokValue<decltype(dst.SystemInfo.SLMSizeInKb)>(*tokSystemInfo);
                            } break;
                            };
                            tokSystemInfo = tokSystemInfo + 1 + tokSystemInfo->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSystemInfo);
                            switch (tokSystemInfo->id) {
                            default:
                                if (tokSystemInfo->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FS_GT_SYSTEM_INFO__SLICE_INFO: {
                                uint32_t arrayElementIdSliceInfo = varLen->arrayElementId;
                                const TokenHeader *tokSliceInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokSliceInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                static constexpr auto maxDstSlicesInfo = sizeof(dst.SystemInfo.SliceInfo) / sizeof(dst.SystemInfo.SliceInfo[0]);
                                if (arrayElementIdSliceInfo >= maxDstSlicesInfo) {
                                    tokSliceInfo = tokSliceInfoEnd;
                                }
                                while (tokSliceInfo < tokSliceInfoEnd) {
                                    if (false == tokSliceInfo->flags.flag4IsVariableLength) {
                                        switch (tokSliceInfo->id) {
                                        default:
                                            if (tokSliceInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBB_GT_SLICE_INFO__ENABLED: {
                                            dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].Enabled = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].Enabled)>(*tokSliceInfo);
                                        } break;
                                        case TOK_FBD_GT_SLICE_INFO__SUB_SLICE_ENABLED_COUNT: {
                                            dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceEnabledCount = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceEnabledCount)>(*tokSliceInfo);
                                        } break;
                                        case TOK_FBD_GT_SLICE_INFO__DUAL_SUB_SLICE_ENABLED_COUNT: {
                                            dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DualSubSliceEnabledCount = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DualSubSliceEnabledCount)>(*tokSliceInfo);
                                        } break;
                                        };
                                        tokSliceInfo = tokSliceInfo + 1 + tokSliceInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSliceInfo);
                                        switch (tokSliceInfo->id) {
                                        default:
                                            if (tokSliceInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GT_SLICE_INFO__SUB_SLICE_INFO: {
                                            uint32_t arrayElementIdSubSliceInfo = varLen->arrayElementId;
                                            const TokenHeader *tokSubSliceInfo = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSubSliceInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSubSliceInfo < tokSubSliceInfoEnd) {
                                                if (false == tokSubSliceInfo->flags.flag4IsVariableLength) {
                                                    switch (tokSubSliceInfo->id) {
                                                    default:
                                                        if (tokSubSliceInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBB_GT_SUBSLICE_INFO__ENABLED: {
                                                        dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].Enabled = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].Enabled)>(*tokSubSliceInfo);
                                                    } break;
                                                    case TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_COUNT: {
                                                        dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].EuEnabledCount = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].EuEnabledCount)>(*tokSubSliceInfo);
                                                    } break;
                                                    case TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_MASK: {
                                                        dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].EuEnabledMask = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].SubSliceInfo[arrayElementIdSubSliceInfo].EuEnabledMask)>(*tokSubSliceInfo);
                                                    } break;
                                                    };
                                                    tokSubSliceInfo = tokSubSliceInfo + 1 + tokSubSliceInfo->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSubSliceInfo);
                                                    if (tokSubSliceInfo->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokSubSliceInfo = tokSubSliceInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSubSliceInfo == tokSubSliceInfoEnd);
                                        } break;
                                        case TOK_FS_GT_SLICE_INFO__DSSINFO: {
                                            uint32_t arrayElementIdDSSInfo = varLen->arrayElementId;
                                            const TokenHeader *tokDSSInfo = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokDSSInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokDSSInfo < tokDSSInfoEnd) {
                                                if (false == tokDSSInfo->flags.flag4IsVariableLength) {
                                                    switch (tokDSSInfo->id) {
                                                    default:
                                                        if (tokDSSInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBB_GT_DUALSUBSLICE_INFO__ENABLED: {
                                                        dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].Enabled = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].Enabled)>(*tokDSSInfo);
                                                    } break;
                                                    };
                                                    tokDSSInfo = tokDSSInfo + 1 + tokDSSInfo->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokDSSInfo);
                                                    switch (tokDSSInfo->id) {
                                                    default:
                                                        if (tokDSSInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_DUALSUBSLICE_INFO__SUB_SLICE: {
                                                        uint32_t arrayElementIdSubSlice = varLen->arrayElementId;
                                                        const TokenHeader *tokSubSlice = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokSubSliceEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokSubSlice < tokSubSliceEnd) {
                                                            if (false == tokSubSlice->flags.flag4IsVariableLength) {
                                                                switch (tokSubSlice->id) {
                                                                default:
                                                                    if (tokSubSlice->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBB_GT_SUBSLICE_INFO__ENABLED: {
                                                                    dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].Enabled = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].Enabled)>(*tokSubSlice);
                                                                } break;
                                                                case TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_COUNT: {
                                                                    dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].EuEnabledCount = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].EuEnabledCount)>(*tokSubSlice);
                                                                } break;
                                                                case TOK_FBD_GT_SUBSLICE_INFO__EU_ENABLED_MASK: {
                                                                    dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].EuEnabledMask = readTokValue<decltype(dst.SystemInfo.SliceInfo[arrayElementIdSliceInfo].DSSInfo[arrayElementIdDSSInfo].SubSlice[arrayElementIdSubSlice].EuEnabledMask)>(*tokSubSlice);
                                                                } break;
                                                                };
                                                                tokSubSlice = tokSubSlice + 1 + tokSubSlice->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSubSlice);
                                                                if (tokSubSlice->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokSubSlice = tokSubSlice + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokSubSlice == tokSubSliceEnd);
                                                    } break;
                                                    };
                                                    tokDSSInfo = tokDSSInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokDSSInfo == tokDSSInfoEnd);
                                        } break;
                                        };
                                        tokSliceInfo = tokSliceInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokSliceInfo == tokSliceInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__SQIDI_INFO: {
                                const TokenHeader *tokSqidiInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokSqidiInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokSqidiInfo < tokSqidiInfoEnd) {
                                    if (false == tokSqidiInfo->flags.flag4IsVariableLength) {
                                        switch (tokSqidiInfo->id) {
                                        default:
                                            if (tokSqidiInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GT_SQIDI_INFO__NUMBEROF_SQIDI: {
                                            dst.SystemInfo.SqidiInfo.NumberofSQIDI = readTokValue<decltype(dst.SystemInfo.SqidiInfo.NumberofSQIDI)>(*tokSqidiInfo);
                                        } break;
                                        case TOK_FBD_GT_SQIDI_INFO__NUMBEROF_DOORBELL_PER_SQIDI: {
                                            dst.SystemInfo.SqidiInfo.NumberofDoorbellPerSQIDI = readTokValue<decltype(dst.SystemInfo.SqidiInfo.NumberofDoorbellPerSQIDI)>(*tokSqidiInfo);
                                        } break;
                                        };
                                        tokSqidiInfo = tokSqidiInfo + 1 + tokSqidiInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSqidiInfo);
                                        if (tokSqidiInfo->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokSqidiInfo = tokSqidiInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokSqidiInfo == tokSqidiInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__CCSINFO: {
                                const TokenHeader *tokCCSInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokCCSInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokCCSInfo < tokCCSInfoEnd) {
                                    if (false == tokCCSInfo->flags.flag4IsVariableLength) {
                                        switch (tokCCSInfo->id) {
                                        default:
                                            if (tokCCSInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GT_CCS_INFO__NUMBER_OF_CCSENABLED: {
                                            dst.SystemInfo.CCSInfo.NumberOfCCSEnabled = readTokValue<decltype(dst.SystemInfo.CCSInfo.NumberOfCCSEnabled)>(*tokCCSInfo);
                                        } break;
                                        case TOK_FBB_GT_CCS_INFO__IS_VALID: {
                                            dst.SystemInfo.CCSInfo.IsValid = readTokValue<decltype(dst.SystemInfo.CCSInfo.IsValid)>(*tokCCSInfo);
                                        } break;
                                        };
                                        tokCCSInfo = tokCCSInfo + 1 + tokCCSInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCCSInfo);
                                        switch (tokCCSInfo->id) {
                                        default:
                                            if (tokCCSInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GT_CCS_INFO__INSTANCES: {
                                            const TokenHeader *tokInstances = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokInstancesEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokInstances < tokInstancesEnd) {
                                                if (false == tokInstances->flags.flag4IsVariableLength) {
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSENABLE_MASK: {
                                                        dst.SystemInfo.CCSInfo.Instances.CCSEnableMask = readTokValue<decltype(dst.SystemInfo.CCSInfo.Instances.CCSEnableMask)>(*tokInstances);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + 1 + tokInstances->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInstances);
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_CCS_INFO__CCSINSTANCES__BITS: {
                                                        const TokenHeader *tokBits = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokBitsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokBits < tokBitsEnd) {
                                                            if (false == tokBits->flags.flag4IsVariableLength) {
                                                                switch (tokBits->id) {
                                                                default:
                                                                    if (tokBits->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS0ENABLED: {
                                                                    dst.SystemInfo.CCSInfo.Instances.Bits.CCS0Enabled = readTokValue<decltype(dst.SystemInfo.CCSInfo.Instances.Bits.CCS0Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS1ENABLED: {
                                                                    dst.SystemInfo.CCSInfo.Instances.Bits.CCS1Enabled = readTokValue<decltype(dst.SystemInfo.CCSInfo.Instances.Bits.CCS1Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS2ENABLED: {
                                                                    dst.SystemInfo.CCSInfo.Instances.Bits.CCS2Enabled = readTokValue<decltype(dst.SystemInfo.CCSInfo.Instances.Bits.CCS2Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_CCS_INFO__CCSINSTANCES__CCSBIT_STRUCT__CCS3ENABLED: {
                                                                    dst.SystemInfo.CCSInfo.Instances.Bits.CCS3Enabled = readTokValue<decltype(dst.SystemInfo.CCSInfo.Instances.Bits.CCS3Enabled)>(*tokBits);
                                                                } break;
                                                                };
                                                                tokBits = tokBits + 1 + tokBits->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokBits);
                                                                if (tokBits->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokBits = tokBits + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokBits == tokBitsEnd);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokInstances == tokInstancesEnd);
                                        } break;
                                        };
                                        tokCCSInfo = tokCCSInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokCCSInfo == tokCCSInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__MULTI_TILE_ARCH_INFO: {
                                const TokenHeader *tokMultiTileArchInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokMultiTileArchInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokMultiTileArchInfo < tokMultiTileArchInfoEnd) {
                                    if (false == tokMultiTileArchInfo->flags.flag4IsVariableLength) {
                                        switch (tokMultiTileArchInfo->id) {
                                        default:
                                            if (tokMultiTileArchInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__TILE_COUNT: {
                                            dst.SystemInfo.MultiTileArchInfo.TileCount = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.TileCount)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE0: {
                                            dst.SystemInfo.MultiTileArchInfo.Tile0 = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.Tile0)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE1: {
                                            dst.SystemInfo.MultiTileArchInfo.Tile1 = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.Tile1)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE2: {
                                            dst.SystemInfo.MultiTileArchInfo.Tile2 = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.Tile2)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__ANONYMOUS8876__TILE3: {
                                            dst.SystemInfo.MultiTileArchInfo.Tile3 = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.Tile3)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBC_GT_MULTI_TILE_ARCH_INFO__ANONYMOUS8856__TILE_MASK: {
                                            dst.SystemInfo.MultiTileArchInfo.TileMask = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.TileMask)>(*tokMultiTileArchInfo);
                                        } break;
                                        case TOK_FBB_GT_MULTI_TILE_ARCH_INFO__IS_VALID: {
                                            dst.SystemInfo.MultiTileArchInfo.IsValid = readTokValue<decltype(dst.SystemInfo.MultiTileArchInfo.IsValid)>(*tokMultiTileArchInfo);
                                        } break;
                                        };
                                        tokMultiTileArchInfo = tokMultiTileArchInfo + 1 + tokMultiTileArchInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokMultiTileArchInfo);
                                        if (tokMultiTileArchInfo->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokMultiTileArchInfo = tokMultiTileArchInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokMultiTileArchInfo == tokMultiTileArchInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__VDBOX_INFO: {
                                const TokenHeader *tokVDBoxInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokVDBoxInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokVDBoxInfo < tokVDBoxInfoEnd) {
                                    if (false == tokVDBoxInfo->flags.flag4IsVariableLength) {
                                        switch (tokVDBoxInfo->id) {
                                        default:
                                            if (tokVDBoxInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GT_VDBOX_INFO__NUMBER_OF_VDBOX_ENABLED: {
                                            dst.SystemInfo.VDBoxInfo.NumberOfVDBoxEnabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.NumberOfVDBoxEnabled)>(*tokVDBoxInfo);
                                        } break;
                                        case TOK_FBB_GT_VDBOX_INFO__IS_VALID: {
                                            dst.SystemInfo.VDBoxInfo.IsValid = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.IsValid)>(*tokVDBoxInfo);
                                        } break;
                                        };
                                        tokVDBoxInfo = tokVDBoxInfo + 1 + tokVDBoxInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokVDBoxInfo);
                                        switch (tokVDBoxInfo->id) {
                                        default:
                                            if (tokVDBoxInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GT_VDBOX_INFO__INSTANCES: {
                                            const TokenHeader *tokInstances = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokInstancesEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokInstances < tokInstancesEnd) {
                                                if (false == tokInstances->flags.flag4IsVariableLength) {
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBOX_ENABLE_MASK: {
                                                        dst.SystemInfo.VDBoxInfo.Instances.VDBoxEnableMask = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.VDBoxEnableMask)>(*tokInstances);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + 1 + tokInstances->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInstances);
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_VDBOX_INFO__VDBOX_INSTANCES__BITS: {
                                                        const TokenHeader *tokBits = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokBitsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokBits < tokBitsEnd) {
                                                            if (false == tokBits->flags.flag4IsVariableLength) {
                                                                switch (tokBits->id) {
                                                                default:
                                                                    if (tokBits->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX0ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox0Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox0Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX1ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox1Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox1Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX2ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox2Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox2Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX3ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox3Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox3Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX4ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox4Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox4Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX5ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox5Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox5Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX6ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox6Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox6Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__VDBOX_INSTANCES__VDBIT_STRUCT__VDBOX7ENABLED: {
                                                                    dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox7Enabled = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.Instances.Bits.VDBox7Enabled)>(*tokBits);
                                                                } break;
                                                                };
                                                                tokBits = tokBits + 1 + tokBits->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokBits);
                                                                if (tokBits->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokBits = tokBits + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokBits == tokBitsEnd);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokInstances == tokInstancesEnd);
                                        } break;
                                        case TOK_FS_GT_VDBOX_INFO__SFCSUPPORT: {
                                            const TokenHeader *tokSFCSupport = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSFCSupportEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSFCSupport < tokSFCSupportEnd) {
                                                if (false == tokSFCSupport->flags.flag4IsVariableLength) {
                                                    switch (tokSFCSupport->id) {
                                                    default:
                                                        if (tokSFCSupport->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__VALUE: {
                                                        dst.SystemInfo.VDBoxInfo.SFCSupport.Value = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.Value)>(*tokSFCSupport);
                                                    } break;
                                                    };
                                                    tokSFCSupport = tokSFCSupport + 1 + tokSFCSupport->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSFCSupport);
                                                    switch (tokSFCSupport->id) {
                                                    default:
                                                        if (tokSFCSupport->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_VDBOX_INFO__ANONYMOUS5662__SFC_SUPPORTED_BITS: {
                                                        const TokenHeader *tokSfcSupportedBits = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokSfcSupportedBitsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokSfcSupportedBits < tokSfcSupportedBitsEnd) {
                                                            if (false == tokSfcSupportedBits->flags.flag4IsVariableLength) {
                                                                switch (tokSfcSupportedBits->id) {
                                                                default:
                                                                    if (tokSfcSupportedBits->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX0: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox0 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox0)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX1: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox1 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox1)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX2: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox2 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox2)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX3: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox3 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox3)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX4: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox4 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox4)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX5: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox5 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox5)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX6: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox6 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox6)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VDBOX_INFO__ANONYMOUS5662__ANONYMOUS5682__VDBOX7: {
                                                                    dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox7 = readTokValue<decltype(dst.SystemInfo.VDBoxInfo.SFCSupport.SfcSupportedBits.VDBox7)>(*tokSfcSupportedBits);
                                                                } break;
                                                                };
                                                                tokSfcSupportedBits = tokSfcSupportedBits + 1 + tokSfcSupportedBits->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSfcSupportedBits);
                                                                if (tokSfcSupportedBits->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokSfcSupportedBits = tokSfcSupportedBits + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokSfcSupportedBits == tokSfcSupportedBitsEnd);
                                                    } break;
                                                    };
                                                    tokSFCSupport = tokSFCSupport + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSFCSupport == tokSFCSupportEnd);
                                        } break;
                                        };
                                        tokVDBoxInfo = tokVDBoxInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokVDBoxInfo == tokVDBoxInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__VEBOX_INFO: {
                                const TokenHeader *tokVEBoxInfo = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokVEBoxInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokVEBoxInfo < tokVEBoxInfoEnd) {
                                    if (false == tokVEBoxInfo->flags.flag4IsVariableLength) {
                                        switch (tokVEBoxInfo->id) {
                                        default:
                                            if (tokVEBoxInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GT_VEBOX_INFO__NUMBER_OF_VEBOX_ENABLED: {
                                            dst.SystemInfo.VEBoxInfo.NumberOfVEBoxEnabled = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.NumberOfVEBoxEnabled)>(*tokVEBoxInfo);
                                        } break;
                                        case TOK_FBB_GT_VEBOX_INFO__IS_VALID: {
                                            dst.SystemInfo.VEBoxInfo.IsValid = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.IsValid)>(*tokVEBoxInfo);
                                        } break;
                                        };
                                        tokVEBoxInfo = tokVEBoxInfo + 1 + tokVEBoxInfo->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokVEBoxInfo);
                                        switch (tokVEBoxInfo->id) {
                                        default:
                                            if (tokVEBoxInfo->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GT_VEBOX_INFO__INSTANCES: {
                                            const TokenHeader *tokInstances = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokInstancesEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokInstances < tokInstancesEnd) {
                                                if (false == tokInstances->flags.flag4IsVariableLength) {
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBOX_ENABLE_MASK: {
                                                        dst.SystemInfo.VEBoxInfo.Instances.VEBoxEnableMask = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.Instances.VEBoxEnableMask)>(*tokInstances);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + 1 + tokInstances->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInstances);
                                                    switch (tokInstances->id) {
                                                    default:
                                                        if (tokInstances->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_VEBOX_INFO__VEBOX_INSTANCES__BITS: {
                                                        const TokenHeader *tokBits = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokBitsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokBits < tokBitsEnd) {
                                                            if (false == tokBits->flags.flag4IsVariableLength) {
                                                                switch (tokBits->id) {
                                                                default:
                                                                    if (tokBits->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX0ENABLED: {
                                                                    dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox0Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX1ENABLED: {
                                                                    dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox1Enabled = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox1Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX2ENABLED: {
                                                                    dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox2Enabled = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox2Enabled)>(*tokBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__VEBOX_INSTANCES__VEBIT_STRUCT__VEBOX3ENABLED: {
                                                                    dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox3Enabled = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.Instances.Bits.VEBox3Enabled)>(*tokBits);
                                                                } break;
                                                                };
                                                                tokBits = tokBits + 1 + tokBits->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokBits);
                                                                if (tokBits->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokBits = tokBits + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokBits == tokBitsEnd);
                                                    } break;
                                                    };
                                                    tokInstances = tokInstances + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokInstances == tokInstancesEnd);
                                        } break;
                                        case TOK_FS_GT_VEBOX_INFO__SFCSUPPORT: {
                                            const TokenHeader *tokSFCSupport = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSFCSupportEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSFCSupport < tokSFCSupportEnd) {
                                                if (false == tokSFCSupport->flags.flag4IsVariableLength) {
                                                    switch (tokSFCSupport->id) {
                                                    default:
                                                        if (tokSFCSupport->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__VALUE: {
                                                        dst.SystemInfo.VEBoxInfo.SFCSupport.Value = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.SFCSupport.Value)>(*tokSFCSupport);
                                                    } break;
                                                    };
                                                    tokSFCSupport = tokSFCSupport + 1 + tokSFCSupport->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSFCSupport);
                                                    switch (tokSFCSupport->id) {
                                                    default:
                                                        if (tokSFCSupport->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GT_VEBOX_INFO__ANONYMOUS3862__SFC_SUPPORTED_BITS: {
                                                        const TokenHeader *tokSfcSupportedBits = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokSfcSupportedBitsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokSfcSupportedBits < tokSfcSupportedBitsEnd) {
                                                            if (false == tokSfcSupportedBits->flags.flag4IsVariableLength) {
                                                                switch (tokSfcSupportedBits->id) {
                                                                default:
                                                                    if (tokSfcSupportedBits->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX0: {
                                                                    dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox0 = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox0)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX1: {
                                                                    dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox1 = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox1)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX2: {
                                                                    dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox2 = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox2)>(*tokSfcSupportedBits);
                                                                } break;
                                                                case TOK_FBD_GT_VEBOX_INFO__ANONYMOUS3862__ANONYMOUS3882__VEBOX3: {
                                                                    dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox3 = readTokValue<decltype(dst.SystemInfo.VEBoxInfo.SFCSupport.SfcSupportedBits.VEBox3)>(*tokSfcSupportedBits);
                                                                } break;
                                                                };
                                                                tokSfcSupportedBits = tokSfcSupportedBits + 1 + tokSfcSupportedBits->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSfcSupportedBits);
                                                                if (tokSfcSupportedBits->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokSfcSupportedBits = tokSfcSupportedBits + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokSfcSupportedBits == tokSfcSupportedBitsEnd);
                                                    } break;
                                                    };
                                                    tokSFCSupport = tokSFCSupport + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSFCSupport == tokSFCSupportEnd);
                                        } break;
                                        };
                                        tokVEBoxInfo = tokVEBoxInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokVEBoxInfo == tokVEBoxInfoEnd);
                            } break;
                            case TOK_FS_GT_SYSTEM_INFO__CACHE_TYPES: {
                                const TokenHeader *tokCacheTypes = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokCacheTypesEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokCacheTypes < tokCacheTypesEnd) {
                                    if (false == tokCacheTypes->flags.flag4IsVariableLength) {
                                        switch (tokCacheTypes->id) {
                                        default:
                                            if (tokCacheTypes->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__L3: {
                                            dst.SystemInfo.CacheTypes.L3 = readTokValue<decltype(dst.SystemInfo.CacheTypes.L3)>(*tokCacheTypes);
                                        } break;
                                        case TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__LLC: {
                                            dst.SystemInfo.CacheTypes.LLC = readTokValue<decltype(dst.SystemInfo.CacheTypes.LLC)>(*tokCacheTypes);
                                        } break;
                                        case TOK_FBD_GT_CACHE_TYPES__ANONYMOUS9544__E_DRAM: {
                                            dst.SystemInfo.CacheTypes.eDRAM = readTokValue<decltype(dst.SystemInfo.CacheTypes.eDRAM)>(*tokCacheTypes);
                                        } break;
                                        case TOK_FBD_GT_CACHE_TYPES__CACHE_TYPE_MASK: {
                                            dst.SystemInfo.CacheTypes.CacheTypeMask = readTokValue<decltype(dst.SystemInfo.CacheTypes.CacheTypeMask)>(*tokCacheTypes);
                                        } break;
                                        };
                                        tokCacheTypes = tokCacheTypes + 1 + tokCacheTypes->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCacheTypes);
                                        if (tokCacheTypes->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokCacheTypes = tokCacheTypes + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokCacheTypes == tokCacheTypesEnd);
                            } break;
                            };
                            tokSystemInfo = tokSystemInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokSystemInfo == tokSystemInfoEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__DEFERRED_WAIT_INFO: {
                    const TokenHeader *tokDeferredWaitInfo = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokDeferredWaitInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokDeferredWaitInfo < tokDeferredWaitInfoEnd) {
                        if (false == tokDeferredWaitInfo->flags.flag4IsVariableLength) {
                            switch (tokDeferredWaitInfo->id) {
                            default:
                                if (tokDeferredWaitInfo->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_KM_DEFERRED_WAIT_INFO__FEATURE_SUPPORTED: {
                                dst.DeferredWaitInfo.FeatureSupported = readTokValue<decltype(dst.DeferredWaitInfo.FeatureSupported)>(*tokDeferredWaitInfo);
                            } break;
                            case TOK_FBD_KM_DEFERRED_WAIT_INFO__ACTIVE_DISPLAY: {
                                dst.DeferredWaitInfo.ActiveDisplay = readTokValue<decltype(dst.DeferredWaitInfo.ActiveDisplay)>(*tokDeferredWaitInfo);
                            } break;
                            };
                            tokDeferredWaitInfo = tokDeferredWaitInfo + 1 + tokDeferredWaitInfo->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokDeferredWaitInfo);
                            if (tokDeferredWaitInfo->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokDeferredWaitInfo = tokDeferredWaitInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokDeferredWaitInfo == tokDeferredWaitInfoEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__GFX_PARTITION: {
                    const TokenHeader *tokGfxPartition = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokGfxPartitionEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokGfxPartition < tokGfxPartitionEnd) {
                        if (false == tokGfxPartition->flags.flag4IsVariableLength) {
                            if (tokGfxPartition->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokGfxPartition = tokGfxPartition + 1 + tokGfxPartition->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGfxPartition);
                            switch (tokGfxPartition->id) {
                            default:
                                if (tokGfxPartition->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FS_GMM_GFX_PARTITIONING__STANDARD: {
                                const TokenHeader *tokStandard = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokStandardEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokStandard < tokStandardEnd) {
                                    if (false == tokStandard->flags.flag4IsVariableLength) {
                                        switch (tokStandard->id) {
                                        default:
                                            if (tokStandard->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                            dst.GfxPartition.Standard.Base = readTokValue<decltype(dst.GfxPartition.Standard.Base)>(*tokStandard);
                                        } break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                            dst.GfxPartition.Standard.Limit = readTokValue<decltype(dst.GfxPartition.Standard.Limit)>(*tokStandard);
                                        } break;
                                        };
                                        tokStandard = tokStandard + 1 + tokStandard->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokStandard);
                                        if (tokStandard->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokStandard = tokStandard + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokStandard == tokStandardEnd);
                            } break;
                            case TOK_FS_GMM_GFX_PARTITIONING__STANDARD64KB: {
                                const TokenHeader *tokStandard64KB = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokStandard64KBEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokStandard64KB < tokStandard64KBEnd) {
                                    if (false == tokStandard64KB->flags.flag4IsVariableLength) {
                                        switch (tokStandard64KB->id) {
                                        default:
                                            if (tokStandard64KB->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                            dst.GfxPartition.Standard64KB.Base = readTokValue<decltype(dst.GfxPartition.Standard64KB.Base)>(*tokStandard64KB);
                                        } break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                            dst.GfxPartition.Standard64KB.Limit = readTokValue<decltype(dst.GfxPartition.Standard64KB.Limit)>(*tokStandard64KB);
                                        } break;
                                        };
                                        tokStandard64KB = tokStandard64KB + 1 + tokStandard64KB->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokStandard64KB);
                                        if (tokStandard64KB->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokStandard64KB = tokStandard64KB + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokStandard64KB == tokStandard64KBEnd);
                            } break;
                            case TOK_FS_GMM_GFX_PARTITIONING__SVM: {
                                const TokenHeader *tokSVM = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokSVMEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokSVM < tokSVMEnd) {
                                    if (false == tokSVM->flags.flag4IsVariableLength) {
                                        switch (tokSVM->id) {
                                        default:
                                            if (tokSVM->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                            dst.GfxPartition.SVM.Base = readTokValue<decltype(dst.GfxPartition.SVM.Base)>(*tokSVM);
                                        } break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                            dst.GfxPartition.SVM.Limit = readTokValue<decltype(dst.GfxPartition.SVM.Limit)>(*tokSVM);
                                        } break;
                                        };
                                        tokSVM = tokSVM + 1 + tokSVM->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSVM);
                                        if (tokSVM->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokSVM = tokSVM + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokSVM == tokSVMEnd);
                            } break;
                            case TOK_FS_GMM_GFX_PARTITIONING__HEAP32: {
                                uint32_t arrayElementIdHeap32 = varLen->arrayElementId;
                                const TokenHeader *tokHeap32 = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokHeap32End = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokHeap32 < tokHeap32End) {
                                    if (false == tokHeap32->flags.flag4IsVariableLength) {
                                        switch (tokHeap32->id) {
                                        default:
                                            if (tokHeap32->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__BASE: {
                                            dst.GfxPartition.Heap32[arrayElementIdHeap32].Base = readTokValue<decltype(dst.GfxPartition.Heap32[arrayElementIdHeap32].Base)>(*tokHeap32);
                                        } break;
                                        case TOK_FBQ_GMM_GFX_PARTITIONING__ANONYMOUS7117__LIMIT: {
                                            dst.GfxPartition.Heap32[arrayElementIdHeap32].Limit = readTokValue<decltype(dst.GfxPartition.Heap32[arrayElementIdHeap32].Limit)>(*tokHeap32);
                                        } break;
                                        };
                                        tokHeap32 = tokHeap32 + 1 + tokHeap32->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokHeap32);
                                        if (tokHeap32->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokHeap32 = tokHeap32 + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokHeap32 == tokHeap32End);
                            } break;
                            };
                            tokGfxPartition = tokGfxPartition + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokGfxPartition == tokGfxPartitionEnd);
                } break;
                case TOK_FS_ADAPTER_INFO__ST_ADAPTER_BDF: {
                    const TokenHeader *tokStAdapterBDF = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokStAdapterBDFEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokStAdapterBDF < tokStAdapterBDFEnd) {
                        if (false == tokStAdapterBDF->flags.flag4IsVariableLength) {
                            switch (tokStAdapterBDF->id) {
                            default:
                                if (tokStAdapterBDF->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__BUS: {
                                dst.stAdapterBDF.Bus = readTokValue<decltype(dst.stAdapterBDF.Bus)>(*tokStAdapterBDF);
                            } break;
                            case TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__DEVICE: {
                                dst.stAdapterBDF.Device = readTokValue<decltype(dst.stAdapterBDF.Device)>(*tokStAdapterBDF);
                            } break;
                            case TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__ANONYMOUS8193__FUNCTION: {
                                dst.stAdapterBDF.Function = readTokValue<decltype(dst.stAdapterBDF.Function)>(*tokStAdapterBDF);
                            } break;
                            case TOK_FBD_ADAPTER_BDF___ANONYMOUS8173__DATA: {
                                dst.stAdapterBDF.Data = readTokValue<decltype(dst.stAdapterBDF.Data)>(*tokStAdapterBDF);
                            } break;
                            };
                            tokStAdapterBDF = tokStAdapterBDF + 1 + tokStAdapterBDF->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokStAdapterBDF);
                            if (tokStAdapterBDF->flags.flag3IsMandatory) {
                                return false;
                            }
                            tokStAdapterBDF = tokStAdapterBDF + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokStAdapterBDF == tokStAdapterBDFEnd);
                } break;
                };
                tok = tok + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
            }
        }
        WCH_ASSERT(tok == srcTokensEnd);
        return true;
    }
};

template <>
struct Demarshaller<TOK_S_CREATECONTEXT_PVTDATA> {
    template <typename _CREATECONTEXT_PVTDATAT>
    static bool demarshall(_CREATECONTEXT_PVTDATAT &dst, const TokenHeader *srcTokensBeg, const TokenHeader *srcTokensEnd) {
        const TokenHeader *tok = srcTokensBeg;
        while (tok < srcTokensEnd) {
            if (false == tok->flags.flag4IsVariableLength) {
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_PBQ_CREATECONTEXT_PVTDATA__ANONYMOUS18449__P_HW_CONTEXT_ID: {
                    dst.pHwContextId = readTokValue<decltype(dst.pHwContextId)>(*tok);
                } break;
                case TOK_FBD_CREATECONTEXT_PVTDATA__NUMBER_OF_HW_CONTEXT_IDS: {
                    dst.NumberOfHwContextIds = readTokValue<decltype(dst.NumberOfHwContextIds)>(*tok);
                } break;
                case TOK_FBD_CREATECONTEXT_PVTDATA__PROCESS_ID: {
                    dst.ProcessID = readTokValue<decltype(dst.ProcessID)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__IS_PROTECTED_PROCESS: {
                    dst.IsProtectedProcess = readTokValue<decltype(dst.IsProtectedProcess)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__IS_DWM: {
                    dst.IsDwm = readTokValue<decltype(dst.IsDwm)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__IS_MEDIA_USAGE: {
                    dst.IsMediaUsage = readTokValue<decltype(dst.IsMediaUsage)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__GPU_VACONTEXT: {
                    dst.GpuVAContext = readTokValue<decltype(dst.GpuVAContext)>(*tok);
                } break;
                case TOK_FBD_CREATECONTEXT_PVTDATA__UMD_CONTEXT_TYPE: {
                    dst.UmdContextType = readTokValue<decltype(dst.UmdContextType)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__NO_RING_FLUSHES: {
                    dst.NoRingFlushes = readTokValue<decltype(dst.NoRingFlushes)>(*tok);
                } break;
                case TOK_FBD_CREATECONTEXT_PVTDATA__USE_HW64B_TOKEN: {
                    dst.UseHw64bToken = readTokValue<decltype(dst.UseHw64bToken)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__DISABLE_WMTP: {
                    dst.DisableWmtp = readTokValue<decltype(dst.DisableWmtp)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__NOTIFY_PREEMPT_EXCEED_THRESHOLD: {
                    dst.NotifyPreemptExceedThreshold = readTokValue<decltype(dst.NotifyPreemptExceedThreshold)>(*tok);
                } break;
                case TOK_FBQ_CREATECONTEXT_PVTDATA__H_PREEMPT_CPU_EVENT_OBJECT: {
                    dst.hPreemptCpuEventObject = readTokValue<decltype(dst.hPreemptCpuEventObject)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__DEBUG_SIPINSTALLED: {
                    dst.DebugSIPInstalled = readTokValue<decltype(dst.DebugSIPInstalled)>(*tok);
                } break;
                case TOK_FBC_CREATECONTEXT_PVTDATA__NUM_HW_QUEUES: {
                    dst.NumHwQueues = readTokValue<decltype(dst.NumHwQueues)>(*tok);
                } break;
                };
                tok = tok + 1 + tok->valueDwordCount;
            } else {
                auto varLen = reinterpret_cast<const TokenVariableLength *>(tok);
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_S_CREATECONTEXT_PVTDATA:
                    if (false == demarshall(dst, varLen->getValue<TokenHeader>(), varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader))) {
                        return false;
                    }
                    break;
                };
                tok = tok + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
            }
        }
        WCH_ASSERT(tok == srcTokensEnd);
        return true;
    }
};

template <>
struct Demarshaller<TOK_S_COMMAND_BUFFER_HEADER_REC> {
    template <typename COMMAND_BUFFER_HEADER_RECT>
    static bool demarshall(COMMAND_BUFFER_HEADER_RECT &dst, const TokenHeader *srcTokensBeg, const TokenHeader *srcTokensEnd) {
        const TokenHeader *tok = srcTokensBeg;
        while (tok < srcTokensEnd) {
            if (false == tok->flags.flag4IsVariableLength) {
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_CONTEXT_TYPE: {
                    dst.UmdContextType = readTokValue<decltype(dst.UmdContextType)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_PATCH_LIST: {
                    dst.UmdPatchList = readTokValue<decltype(dst.UmdPatchList)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_SLICE_STATE: {
                    dst.UmdRequestedSliceState = readTokValue<decltype(dst.UmdRequestedSliceState)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_SUBSLICE_COUNT: {
                    dst.UmdRequestedSubsliceCount = readTokValue<decltype(dst.UmdRequestedSubsliceCount)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__UMD_REQUESTED_EUCOUNT: {
                    dst.UmdRequestedEUCount = readTokValue<decltype(dst.UmdRequestedEUCount)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__USES_RESOURCE_STREAMER: {
                    dst.UsesResourceStreamer = readTokValue<decltype(dst.UsesResourceStreamer)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__NEEDS_MID_BATCH_PRE_EMPTION_SUPPORT: {
                    dst.NeedsMidBatchPreEmptionSupport = readTokValue<decltype(dst.NeedsMidBatchPreEmptionSupport)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__USES_GPGPUPIPELINE: {
                    dst.UsesGPGPUPipeline = readTokValue<decltype(dst.UsesGPGPUPipeline)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__REQUIRES_COHERENCY: {
                    dst.RequiresCoherency = readTokValue<decltype(dst.RequiresCoherency)>(*tok);
                } break;
                case TOK_FBD_COMMAND_BUFFER_HEADER_REC__PERF_TAG: {
                    dst.PerfTag = readTokValue<decltype(dst.PerfTag)>(*tok);
                } break;
                case TOK_FBQ_COMMAND_BUFFER_HEADER_REC__MONITOR_FENCE_VA: {
                    dst.MonitorFenceVA = readTokValue<decltype(dst.MonitorFenceVA)>(*tok);
                } break;
                case TOK_FBQ_COMMAND_BUFFER_HEADER_REC__MONITOR_FENCE_VALUE: {
                    dst.MonitorFenceValue = readTokValue<decltype(dst.MonitorFenceValue)>(*tok);
                } break;
                };
                tok = tok + 1 + tok->valueDwordCount;
            } else {
                auto varLen = reinterpret_cast<const TokenVariableLength *>(tok);
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_S_COMMAND_BUFFER_HEADER_REC:
                    if (false == demarshall(dst, varLen->getValue<TokenHeader>(), varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader))) {
                        return false;
                    }
                    break;
                };
                tok = tok + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
            }
        }
        WCH_ASSERT(tok == srcTokensEnd);
        return true;
    }
};

template <>
struct Demarshaller<TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT> {
    template <typename GmmResourceInfoWinStructT>
    static bool demarshall(GmmResourceInfoWinStructT &dst, const TokenHeader *srcTokensBeg, const TokenHeader *srcTokensEnd) {
        const TokenHeader *tok = srcTokensBeg;
        while (tok < srcTokensEnd) {
            if (false == tok->flags.flag4IsVariableLength) {
                if (tok->flags.flag3IsMandatory) {
                    return false;
                }
                tok = tok + 1 + tok->valueDwordCount;
            } else {
                auto varLen = reinterpret_cast<const TokenVariableLength *>(tok);
                switch (tok->id) {
                default:
                    if (tok->flags.flag3IsMandatory) {
                        return false;
                    }
                    break;
                case TOK_S_GMM_RESOURCE_INFO_WIN_STRUCT:
                    if (false == demarshall(dst, varLen->getValue<TokenHeader>(), varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader))) {
                        return false;
                    }
                    break;
                case TOK_FS_GMM_RESOURCE_INFO_WIN_STRUCT__GMM_RESOURCE_INFO_COMMON: {
                    const TokenHeader *tokGmmResourceInfoCommon = varLen->getValue<TokenHeader>();
                    const TokenHeader *tokGmmResourceInfoCommonEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                    while (tokGmmResourceInfoCommon < tokGmmResourceInfoCommonEnd) {
                        if (false == tokGmmResourceInfoCommon->flags.flag4IsVariableLength) {
                            switch (tokGmmResourceInfoCommon->id) {
                            default:
                                if (tokGmmResourceInfoCommon->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FE_GMM_RESOURCE_INFO_COMMON_STRUCT__CLIENT_TYPE: {
                                dst.GmmResourceInfoCommon.ClientType = readTokValue<decltype(dst.GmmResourceInfoCommon.ClientType)>(*tokGmmResourceInfoCommon);
                            } break;
                            case TOK_FBD_GMM_RESOURCE_INFO_COMMON_STRUCT__ROTATE_INFO: {
                                dst.GmmResourceInfoCommon.RotateInfo = readTokValue<decltype(dst.GmmResourceInfoCommon.RotateInfo)>(*tokGmmResourceInfoCommon);
                            } break;
                            case TOK_FBQ_GMM_RESOURCE_INFO_COMMON_STRUCT__SVM_ADDRESS: {
                                dst.GmmResourceInfoCommon.SvmAddress = readTokValue<decltype(dst.GmmResourceInfoCommon.SvmAddress)>(*tokGmmResourceInfoCommon);
                            } break;
                            case TOK_FBQ_GMM_RESOURCE_INFO_COMMON_STRUCT__P_PRIVATE_DATA: {
                                dst.GmmResourceInfoCommon.pPrivateData = readTokValue<decltype(dst.GmmResourceInfoCommon.pPrivateData)>(*tokGmmResourceInfoCommon);
                            } break;
                            };
                            tokGmmResourceInfoCommon = tokGmmResourceInfoCommon + 1 + tokGmmResourceInfoCommon->valueDwordCount;
                        } else {
                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGmmResourceInfoCommon);
                            switch (tokGmmResourceInfoCommon->id) {
                            default:
                                if (tokGmmResourceInfoCommon->flags.flag3IsMandatory) {
                                    return false;
                                }
                                break;
                            case TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__SURF: {
                                const TokenHeader *tokSurf = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokSurfEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokSurf < tokSurfEnd) {
                                    if (false == tokSurf->flags.flag4IsVariableLength) {
                                        switch (tokSurf->id) {
                                        default:
                                            if (tokSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TYPE: {
                                            dst.GmmResourceInfoCommon.Surf.Type = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Type)>(*tokSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__FORMAT: {
                                            dst.GmmResourceInfoCommon.Surf.Format = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Format)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BITS_PER_PIXEL: {
                                            dst.GmmResourceInfoCommon.Surf.BitsPerPixel = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.BitsPerPixel)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__BASE_WIDTH: {
                                            dst.GmmResourceInfoCommon.Surf.BaseWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.BaseWidth)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BASE_HEIGHT: {
                                            dst.GmmResourceInfoCommon.Surf.BaseHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.BaseHeight)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__DEPTH: {
                                            dst.GmmResourceInfoCommon.Surf.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Depth)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAX_LOD: {
                                            dst.GmmResourceInfoCommon.Surf.MaxLod = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.MaxLod)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__ARRAY_SIZE: {
                                            dst.GmmResourceInfoCommon.Surf.ArraySize = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.ArraySize)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CP_TAG: {
                                            dst.GmmResourceInfoCommon.Surf.CpTag = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.CpTag)>(*tokSurf);
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_MODE: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.MmcMode)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.MmcMode, sizeof(dst.GmmResourceInfoCommon.Surf.MmcMode), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.MmcMode));
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_HINT: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.MmcHint)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.MmcHint, sizeof(dst.GmmResourceInfoCommon.Surf.MmcHint), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.MmcHint));
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__PITCH: {
                                            dst.GmmResourceInfoCommon.Surf.Pitch = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Pitch)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__OVERRIDE_PITCH: {
                                            dst.GmmResourceInfoCommon.Surf.OverridePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OverridePitch)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE: {
                                            dst.GmmResourceInfoCommon.Surf.Size = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Size)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__CCSIZE: {
                                            dst.GmmResourceInfoCommon.Surf.CCSize = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.CCSize)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__UNPADDED_SIZE: {
                                            dst.GmmResourceInfoCommon.Surf.UnpaddedSize = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.UnpaddedSize)>(*tokSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE_REPORT_TO_OS: {
                                            dst.GmmResourceInfoCommon.Surf.SizeReportToOS = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.SizeReportToOS)>(*tokSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TILE_MODE: {
                                            dst.GmmResourceInfoCommon.Surf.TileMode = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.TileMode)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CCSMODE_ALIGN: {
                                            dst.GmmResourceInfoCommon.Surf.CCSModeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.CCSModeAlign)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__LEGACY_FLAGS: {
                                            dst.GmmResourceInfoCommon.Surf.LegacyFlags = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.LegacyFlags)>(*tokSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAXIMUM_RENAMING_LIST_LENGTH: {
                                            dst.GmmResourceInfoCommon.Surf.MaximumRenamingListLength = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.MaximumRenamingListLength)>(*tokSurf);
                                        } break;
                                        };
                                        tokSurf = tokSurf + 1 + tokSurf->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSurf);
                                        switch (tokSurf->id) {
                                        default:
                                            if (tokSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__FLAGS: {
                                            const TokenHeader *tokFlags = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokFlagsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokFlags < tokFlagsEnd) {
                                                if (false == tokFlags->flags.flag4IsVariableLength) {
                                                    if (tokFlags->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokFlags = tokFlags + 1 + tokFlags->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokFlags);
                                                    switch (tokFlags->id) {
                                                    default:
                                                        if (tokFlags->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__GPU: {
                                                        const TokenHeader *tokGpu = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokGpuEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokGpu < tokGpuEnd) {
                                                            if (false == tokGpu->flags.flag4IsVariableLength) {
                                                                switch (tokGpu->id) {
                                                                default:
                                                                    if (tokGpu->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CAMERA_CAPTURE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.CameraCapture = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.CameraCapture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.CCS = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.CCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_DISCARD: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorDiscard = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorDiscard)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorSeparation = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorSeparation)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION_RGBX: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorSeparationRGBX = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ColorSeparationRGBX)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CONSTANT: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Constant = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Constant)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__DEPTH: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Depth)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.FlipChain = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.FlipChain)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN_PREFERRED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.FlipChainPreferred = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.FlipChainPreferred)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HISTORY_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.HistoryBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.HistoryBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HI_Z: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.HiZ = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.HiZ)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDEX: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Index = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Index)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDIRECT_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.IndirectClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.IndirectClearColor)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INSTRUCTION_FLAT: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.InstructionFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.InstructionFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INTERLACED_SCAN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.InterlacedScan = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.InterlacedScan)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MCS = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MMC: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MMC = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MMC)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MOTION_COMP: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MotionComp = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.MotionComp)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__NO_RESTRICTION: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.NoRestriction = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.NoRestriction)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__OVERLAY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Overlay = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Overlay)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PRESENTABLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Presentable = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Presentable)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PROCEDURAL_TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ProceduralTexture = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ProceduralTexture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__QUERY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Query = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Query)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__RENDER_TARGET: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.RenderTarget = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.RenderTarget)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.S3d = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.S3d)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D_DX: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.S3dDx = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.S3dDx)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_NON_PACKED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__S3dNonPacked = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__S3dNonPacked)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_WIDI: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__S3dWidi = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__S3dWidi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SCRATCH_FLAT: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ScratchFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.ScratchFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SEPARATE_STENCIL: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.SeparateStencil = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.SeparateStencil)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.State = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.State)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE_DX9CONSTANT_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.StateDx9ConstantBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.StateDx9ConstantBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STREAM: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Stream = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Stream)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXT_API: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TextApi = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TextApi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Texture = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Texture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILED_RESOURCE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TiledResource = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TiledResource)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILE_POOL: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TilePool = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.TilePool)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__UNIFIED_AUX_SURFACE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.UnifiedAuxSurface = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.UnifiedAuxSurface)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VERTEX: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Vertex = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Vertex)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VIDEO: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Video = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.Video)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_XCCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaTileXCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaTileXCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_YCCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaTileYCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaTileYCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____MSAA_TILE_MCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__MsaaTileMcs = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__MsaaTileMcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_LINEAR_CCS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaLinearCCS = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__NonMsaaLinearCCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____REMAINING: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__Remaining = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Gpu.__Remaining)>(*tokGpu);
                                                                } break;
                                                                };
                                                                tokGpu = tokGpu + 1 + tokGpu->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGpu);
                                                                if (tokGpu->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokGpu = tokGpu + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokGpu == tokGpuEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__INFO: {
                                                        const TokenHeader *tokInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokInfo < tokInfoEnd) {
                                                            if (false == tokInfo->flags.flag4IsVariableLength) {
                                                                switch (tokInfo->id) {
                                                                default:
                                                                    if (tokInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ALLOW_VIRTUAL_PADDING: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.AllowVirtualPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.AllowVirtualPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__BIG_PAGE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.BigPage = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.BigPage)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CACHEABLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Cacheable = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Cacheable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CONTIG_PHYS_MEMORY_FORI_DART: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.ContigPhysMemoryForiDART = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.ContigPhysMemoryForiDART)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CORNER_TEXEL_MODE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.CornerTexelMode = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.CornerTexelMode)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__EXISTING_SYS_MEM: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.ExistingSysMem = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.ExistingSysMem)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__FORCE_RESIDENCY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.ForceResidency = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.ForceResidency)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GFDT: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Gfdt = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Gfdt)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GTT_MAP_TYPE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.GttMapType = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.GttMapType)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__HARDWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.HardwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.HardwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__KERNEL_MODE_MAPPED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.KernelModeMapped = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.KernelModeMapped)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_BELOW: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.LayoutBelow = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.LayoutBelow)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_MONO: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.LayoutMono = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.LayoutMono)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.LocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.LocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LINEAR: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Linear = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Linear)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__MEDIA_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.MediaCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.MediaCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_OPTIMIZATION_PADDING: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.NoOptimizationPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.NoOptimizationPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_PHYS_MEMORY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.NoPhysMemory = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.NoPhysMemory)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NOT_LOCKABLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.NotLockable = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.NotLockable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NON_LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.NonLocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.NonLocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.StdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.StdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__PSEUDO_STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.PseudoStdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.PseudoStdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__UNDEFINED64KBSWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Undefined64KBSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Undefined64KBSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__REDECRIBED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.RedecribedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.RedecribedPlanes)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__RENDER_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.RenderCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.RenderCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ROTATED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Rotated = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Rotated)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SHARED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Shared = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Shared)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SOFTWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.SoftwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.SoftwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SVM: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.SVM)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE4: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Tile4 = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Tile4)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE64: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.Tile64 = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.Tile64)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_W: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledW = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledW)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_X: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledX = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledX)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_Y: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledY = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledY)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YF: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledYf = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledYf)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledYs = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.TiledYs)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__WDDM_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.WddmProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.WddmProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__XADAPTER: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.XAdapter = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.XAdapter)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PREALLOCATED_RES_INFO: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.__PreallocatedResInfo = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.__PreallocatedResInfo)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PRE_WDDM2SVM: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Info.__PreWddm2SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Info.__PreWddm2SVM)>(*tokInfo);
                                                                } break;
                                                                };
                                                                tokInfo = tokInfo + 1 + tokInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInfo);
                                                                if (tokInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokInfo = tokInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokInfo == tokInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__WA: {
                                                        const TokenHeader *tokWa = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokWaEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokWa < tokWaEnd) {
                                                            if (false == tokWa->flags.flag4IsVariableLength) {
                                                                switch (tokWa->id) {
                                                                default:
                                                                    if (tokWa->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__GTMFX2ND_LEVEL_BATCH_RING_SIZE_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_MPR_ROW_STORE32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_DMV_BUFFER32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_BUFFER_SAMPLER_PADDING: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.NoBufferSamplerPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.NoBufferSamplerPadding)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_LEGACY_PLANAR_LINEAR_VIDEO_RESTRICTIONS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__CHVASTC_SKIP_VIRTUAL_MIPS: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.CHVAstcSkipVirtualMips = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.CHVAstcSkipVirtualMips)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_PACKED_MIP_TAIL: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisablePackedMipTail = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisablePackedMipTail)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521____FORCE_OTHER_HVALIGN4: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.__ForceOtherHVALIGN4 = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.__ForceOtherHVALIGN4)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisableDisplayCcsClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisableDisplayCcsClearColor)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisableDisplayCcsCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.DisableDisplayCcsCompression)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__PRE_GEN12FAST_CLEAR_ONLY: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.PreGen12FastClearOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.PreGen12FastClearOnly)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__FORCE_STD_ALLOC_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.ForceStdAllocAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.ForceStdAllocAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DENIABLE_LOCAL_ONLY_FOR_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.Surf.Flags.Wa.DeniableLocalOnlyForCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Flags.Wa.DeniableLocalOnlyForCompression)>(*tokWa);
                                                                } break;
                                                                };
                                                                tokWa = tokWa + 1 + tokWa->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokWa);
                                                                if (tokWa->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokWa = tokWa + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokWa == tokWaEnd);
                                                    } break;
                                                    };
                                                    tokFlags = tokFlags + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokFlags == tokFlagsEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__CACHE_POLICY: {
                                            const TokenHeader *tokCachePolicy = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokCachePolicyEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokCachePolicy < tokCachePolicyEnd) {
                                                if (false == tokCachePolicy->flags.flag4IsVariableLength) {
                                                    switch (tokCachePolicy->id) {
                                                    default:
                                                        if (tokCachePolicy->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_TEXTURE_INFO_REC__ANONYMOUS4927__USAGE: {
                                                        dst.GmmResourceInfoCommon.Surf.CachePolicy.Usage = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.CachePolicy.Usage)>(*tokCachePolicy);
                                                    } break;
                                                    };
                                                    tokCachePolicy = tokCachePolicy + 1 + tokCachePolicy->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCachePolicy);
                                                    if (tokCachePolicy->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokCachePolicy = tokCachePolicy + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokCachePolicy == tokCachePolicyEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__MSAA: {
                                            const TokenHeader *tokMSAA = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokMSAAEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokMSAA < tokMSAAEnd) {
                                                if (false == tokMSAA->flags.flag4IsVariableLength) {
                                                    switch (tokMSAA->id) {
                                                    default:
                                                        if (tokMSAA->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_RESOURCE_MSAA_INFO_REC__SAMPLE_PATTERN: {
                                                        dst.GmmResourceInfoCommon.Surf.MSAA.SamplePattern = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.MSAA.SamplePattern)>(*tokMSAA);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_MSAA_INFO_REC__NUM_SAMPLES: {
                                                        dst.GmmResourceInfoCommon.Surf.MSAA.NumSamples = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.MSAA.NumSamples)>(*tokMSAA);
                                                    } break;
                                                    };
                                                    tokMSAA = tokMSAA + 1 + tokMSAA->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokMSAA);
                                                    if (tokMSAA->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokMSAA = tokMSAA + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokMSAA == tokMSAAEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__ALIGNMENT: {
                                            const TokenHeader *tokAlignment = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokAlignmentEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokAlignment < tokAlignmentEnd) {
                                                if (false == tokAlignment->flags.flag4IsVariableLength) {
                                                    switch (tokAlignment->id) {
                                                    default:
                                                        if (tokAlignment->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_RESOURCE_ALIGNMENT_REC__ARRAY_SPACING_SINGLE_LOD: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.ArraySpacingSingleLod = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.ArraySpacingSingleLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__BASE_ALIGNMENT: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.BaseAlignment = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.BaseAlignment)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__HALIGN: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.HAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.HAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__VALIGN: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.VAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.VAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__DALIGN: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.DAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.DAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__MIP_TAIL_START_LOD: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.MipTailStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.MipTailStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_START_LOD: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_WIDTH: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipWidth)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.PackedMipHeight)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__QPITCH: {
                                                        dst.GmmResourceInfoCommon.Surf.Alignment.QPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Alignment.QPitch)>(*tokAlignment);
                                                    } break;
                                                    };
                                                    tokAlignment = tokAlignment + 1 + tokAlignment->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokAlignment);
                                                    if (tokAlignment->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokAlignment = tokAlignment + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokAlignment == tokAlignmentEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__OFFSET_INFO: {
                                            const TokenHeader *tokOffsetInfo = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokOffsetInfo < tokOffsetInfoEnd) {
                                                if (false == tokOffsetInfo->flags.flag4IsVariableLength) {
                                                    if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokOffsetInfo = tokOffsetInfo + 1 + tokOffsetInfo->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOffsetInfo);
                                                    switch (tokOffsetInfo->id) {
                                                    default:
                                                        if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE3DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture3DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture3DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture3DOffsetInfo < tokTexture3DOffsetInfoEnd) {
                                                            if (false == tokTexture3DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture3DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__MIP0SLICE_PITCH: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch)>(*tokTexture3DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture3DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + 1 + tokTexture3DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture3DOffsetInfo);
                                                                if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture3DOffsetInfo == tokTexture3DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE2DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture2DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture2DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture2DOffsetInfo < tokTexture2DOffsetInfoEnd) {
                                                            if (false == tokTexture2DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture2DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_LOCK: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_RENDER: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Texture2DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + 1 + tokTexture2DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture2DOffsetInfo);
                                                                if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture2DOffsetInfo == tokTexture2DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__PLANE: {
                                                        const TokenHeader *tokPlane = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokPlaneEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokPlane < tokPlaneEnd) {
                                                            if (false == tokPlane->flags.flag4IsVariableLength) {
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ARRAY_QPITCH: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.ArrayQPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.ArrayQPitch)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__X: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.X)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.X, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.X), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.X));
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__Y: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.Y)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.Y, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.Y), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.Y));
                                                                } break;
                                                                case TOK_FBD_GMM_PLANAR_OFFSET_INFO_REC__NO_OF_PLANES: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.NoOfPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.NoOfPlanes)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBB_GMM_PLANAR_OFFSET_INFO_REC__IS_TILE_ALIGNED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.IsTileAlignedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.IsTileAlignedPlanes)>(*tokPlane);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + 1 + tokPlane->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlane);
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FS_GMM_PLANAR_OFFSET_INFO_REC__UN_ALIGNED: {
                                                                    const TokenHeader *tokUnAligned = varLen->getValue<TokenHeader>();
                                                                    const TokenHeader *tokUnAlignedEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                                    while (tokUnAligned < tokUnAlignedEnd) {
                                                                        if (false == tokUnAligned->flags.flag4IsVariableLength) {
                                                                            switch (tokUnAligned->id) {
                                                                            default:
                                                                                if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                    return false;
                                                                                }
                                                                                break;
                                                                            case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ANONYMOUS1851__HEIGHT: {
                                                                                auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValue<char>();
                                                                                auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValueSizeInBytes();
                                                                                if (srcSize < sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.UnAligned.Height)) {
                                                                                    return false;
                                                                                }
                                                                                WCH_SAFE_COPY(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.UnAligned.Height, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.UnAligned.Height), srcData, sizeof(dst.GmmResourceInfoCommon.Surf.OffsetInfo.Plane.UnAligned.Height));
                                                                            } break;
                                                                            };
                                                                            tokUnAligned = tokUnAligned + 1 + tokUnAligned->valueDwordCount;
                                                                        } else {
                                                                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokUnAligned);
                                                                            if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                return false;
                                                                            }
                                                                            tokUnAligned = tokUnAligned + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                                        }
                                                                    }
                                                                    WCH_ASSERT(tokUnAligned == tokUnAlignedEnd);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokPlane == tokPlaneEnd);
                                                    } break;
                                                    };
                                                    tokOffsetInfo = tokOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokOffsetInfo == tokOffsetInfoEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__S3D: {
                                            const TokenHeader *tokS3d = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokS3dEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokS3d < tokS3dEnd) {
                                                if (false == tokS3d->flags.flag4IsVariableLength) {
                                                    switch (tokS3d->id) {
                                                    default:
                                                        if (tokS3d->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__DISPLAY_MODE_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.DisplayModeHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.DisplayModeHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__NUM_BLANK_ACTIVE_LINES: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.NumBlankActiveLines = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.NumBlankActiveLines)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__RFRAME_OFFSET: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.RFrameOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.RFrameOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__BLANK_AREA_OFFSET: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.BlankAreaOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.BlankAreaOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.TallBufferHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.TallBufferHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_SIZE: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.TallBufferSize = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.TallBufferSize)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBC_GMM_S3D_INFO_REC__IS_RFRAME: {
                                                        dst.GmmResourceInfoCommon.Surf.S3d.IsRFrame = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.S3d.IsRFrame)>(*tokS3d);
                                                    } break;
                                                    };
                                                    tokS3d = tokS3d + 1 + tokS3d->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokS3d);
                                                    if (tokS3d->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokS3d = tokS3d + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokS3d == tokS3dEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__SEGMENT_OVERRIDE: {
                                            const TokenHeader *tokSegmentOverride = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSegmentOverrideEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSegmentOverride < tokSegmentOverrideEnd) {
                                                if (false == tokSegmentOverride->flags.flag4IsVariableLength) {
                                                    switch (tokSegmentOverride->id) {
                                                    default:
                                                        if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__SEG1: {
                                                        dst.GmmResourceInfoCommon.Surf.SegmentOverride.Seg1 = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.SegmentOverride.Seg1)>(*tokSegmentOverride);
                                                    } break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__EVICT: {
                                                        dst.GmmResourceInfoCommon.Surf.SegmentOverride.Evict = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.SegmentOverride.Evict)>(*tokSegmentOverride);
                                                    } break;
                                                    };
                                                    tokSegmentOverride = tokSegmentOverride + 1 + tokSegmentOverride->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSegmentOverride);
                                                    if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokSegmentOverride = tokSegmentOverride + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSegmentOverride == tokSegmentOverrideEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__PLATFORM: {
#if _DEBUG || _RELEASE_INTERNAL
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.Surf.Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__EXISTING_SYS_MEM: {
                                            const TokenHeader *tokExistingSysMem = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokExistingSysMemEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokExistingSysMem < tokExistingSysMemEnd) {
                                                if (false == tokExistingSysMem->flags.flag4IsVariableLength) {
                                                    switch (tokExistingSysMem->id) {
                                                    default:
                                                        if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_GMM_ALLOCATED: {
                                                        dst.GmmResourceInfoCommon.Surf.ExistingSysMem.IsGmmAllocated = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.ExistingSysMem.IsGmmAllocated)>(*tokExistingSysMem);
                                                    } break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_PAGE_ALIGNED: {
                                                        dst.GmmResourceInfoCommon.Surf.ExistingSysMem.IsPageAligned = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.ExistingSysMem.IsPageAligned)>(*tokExistingSysMem);
                                                    } break;
                                                    };
                                                    tokExistingSysMem = tokExistingSysMem + 1 + tokExistingSysMem->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokExistingSysMem);
                                                    if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokExistingSysMem = tokExistingSysMem + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokExistingSysMem == tokExistingSysMemEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC____PLATFORM: {
#if !(_DEBUG || _RELEASE_INTERNAL)
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.Surf.__Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.Surf.__Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        };
                                        tokSurf = tokSurf + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokSurf == tokSurfEnd);
                            } break;
                            case TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__AUX_SURF: {
                                const TokenHeader *tokAuxSurf = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokAuxSurfEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokAuxSurf < tokAuxSurfEnd) {
                                    if (false == tokAuxSurf->flags.flag4IsVariableLength) {
                                        switch (tokAuxSurf->id) {
                                        default:
                                            if (tokAuxSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TYPE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.Type = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Type)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__FORMAT: {
                                            dst.GmmResourceInfoCommon.AuxSurf.Format = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Format)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BITS_PER_PIXEL: {
                                            dst.GmmResourceInfoCommon.AuxSurf.BitsPerPixel = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.BitsPerPixel)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__BASE_WIDTH: {
                                            dst.GmmResourceInfoCommon.AuxSurf.BaseWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.BaseWidth)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BASE_HEIGHT: {
                                            dst.GmmResourceInfoCommon.AuxSurf.BaseHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.BaseHeight)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__DEPTH: {
                                            dst.GmmResourceInfoCommon.AuxSurf.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Depth)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAX_LOD: {
                                            dst.GmmResourceInfoCommon.AuxSurf.MaxLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.MaxLod)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__ARRAY_SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.ArraySize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.ArraySize)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CP_TAG: {
                                            dst.GmmResourceInfoCommon.AuxSurf.CpTag = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.CpTag)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_MODE: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokAuxSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokAuxSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcMode)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.MmcMode, sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcMode), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcMode));
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_HINT: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokAuxSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokAuxSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcHint)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.MmcHint, sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcHint), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.MmcHint));
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__PITCH: {
                                            dst.GmmResourceInfoCommon.AuxSurf.Pitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Pitch)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__OVERRIDE_PITCH: {
                                            dst.GmmResourceInfoCommon.AuxSurf.OverridePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OverridePitch)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.Size = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Size)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__CCSIZE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.CCSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.CCSize)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__UNPADDED_SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.UnpaddedSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.UnpaddedSize)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE_REPORT_TO_OS: {
                                            dst.GmmResourceInfoCommon.AuxSurf.SizeReportToOS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.SizeReportToOS)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TILE_MODE: {
                                            dst.GmmResourceInfoCommon.AuxSurf.TileMode = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.TileMode)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CCSMODE_ALIGN: {
                                            dst.GmmResourceInfoCommon.AuxSurf.CCSModeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.CCSModeAlign)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__LEGACY_FLAGS: {
                                            dst.GmmResourceInfoCommon.AuxSurf.LegacyFlags = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.LegacyFlags)>(*tokAuxSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAXIMUM_RENAMING_LIST_LENGTH: {
                                            dst.GmmResourceInfoCommon.AuxSurf.MaximumRenamingListLength = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.MaximumRenamingListLength)>(*tokAuxSurf);
                                        } break;
                                        };
                                        tokAuxSurf = tokAuxSurf + 1 + tokAuxSurf->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokAuxSurf);
                                        switch (tokAuxSurf->id) {
                                        default:
                                            if (tokAuxSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__FLAGS: {
                                            const TokenHeader *tokFlags = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokFlagsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokFlags < tokFlagsEnd) {
                                                if (false == tokFlags->flags.flag4IsVariableLength) {
                                                    if (tokFlags->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokFlags = tokFlags + 1 + tokFlags->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokFlags);
                                                    switch (tokFlags->id) {
                                                    default:
                                                        if (tokFlags->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__GPU: {
                                                        const TokenHeader *tokGpu = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokGpuEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokGpu < tokGpuEnd) {
                                                            if (false == tokGpu->flags.flag4IsVariableLength) {
                                                                switch (tokGpu->id) {
                                                                default:
                                                                    if (tokGpu->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CAMERA_CAPTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.CameraCapture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.CameraCapture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.CCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.CCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_DISCARD: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorDiscard = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorDiscard)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorSeparation = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorSeparation)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION_RGBX: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorSeparationRGBX = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ColorSeparationRGBX)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CONSTANT: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Constant = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Constant)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__DEPTH: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Depth)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.FlipChain = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.FlipChain)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN_PREFERRED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.FlipChainPreferred = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.FlipChainPreferred)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HISTORY_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.HistoryBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.HistoryBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HI_Z: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.HiZ = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.HiZ)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDEX: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Index = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Index)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDIRECT_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.IndirectClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.IndirectClearColor)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INSTRUCTION_FLAT: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.InstructionFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.InstructionFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INTERLACED_SCAN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.InterlacedScan = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.InterlacedScan)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MMC: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MMC = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MMC)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MOTION_COMP: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MotionComp = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.MotionComp)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__NO_RESTRICTION: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.NoRestriction = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.NoRestriction)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__OVERLAY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Overlay = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Overlay)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PRESENTABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Presentable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Presentable)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PROCEDURAL_TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ProceduralTexture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ProceduralTexture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__QUERY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Query = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Query)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__RENDER_TARGET: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.RenderTarget = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.RenderTarget)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.S3d = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.S3d)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D_DX: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.S3dDx = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.S3dDx)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_NON_PACKED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__S3dNonPacked = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__S3dNonPacked)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_WIDI: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__S3dWidi = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__S3dWidi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SCRATCH_FLAT: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ScratchFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.ScratchFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SEPARATE_STENCIL: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.SeparateStencil = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.SeparateStencil)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.State = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.State)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE_DX9CONSTANT_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.StateDx9ConstantBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.StateDx9ConstantBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STREAM: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Stream = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Stream)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXT_API: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TextApi = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TextApi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Texture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Texture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILED_RESOURCE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TiledResource = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TiledResource)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILE_POOL: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TilePool = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.TilePool)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__UNIFIED_AUX_SURFACE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.UnifiedAuxSurface = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.UnifiedAuxSurface)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VERTEX: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Vertex = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Vertex)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VIDEO: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Video = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.Video)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_XCCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaTileXCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaTileXCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_YCCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaTileYCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaTileYCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____MSAA_TILE_MCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__MsaaTileMcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__MsaaTileMcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_LINEAR_CCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaLinearCCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__NonMsaaLinearCCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____REMAINING: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__Remaining = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Gpu.__Remaining)>(*tokGpu);
                                                                } break;
                                                                };
                                                                tokGpu = tokGpu + 1 + tokGpu->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGpu);
                                                                if (tokGpu->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokGpu = tokGpu + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokGpu == tokGpuEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__INFO: {
                                                        const TokenHeader *tokInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokInfo < tokInfoEnd) {
                                                            if (false == tokInfo->flags.flag4IsVariableLength) {
                                                                switch (tokInfo->id) {
                                                                default:
                                                                    if (tokInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ALLOW_VIRTUAL_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.AllowVirtualPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.AllowVirtualPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__BIG_PAGE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.BigPage = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.BigPage)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CACHEABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Cacheable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Cacheable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CONTIG_PHYS_MEMORY_FORI_DART: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ContigPhysMemoryForiDART = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ContigPhysMemoryForiDART)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CORNER_TEXEL_MODE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.CornerTexelMode = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.CornerTexelMode)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__EXISTING_SYS_MEM: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ExistingSysMem = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ExistingSysMem)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__FORCE_RESIDENCY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ForceResidency = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.ForceResidency)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GFDT: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Gfdt = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Gfdt)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GTT_MAP_TYPE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.GttMapType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.GttMapType)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__HARDWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.HardwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.HardwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__KERNEL_MODE_MAPPED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.KernelModeMapped = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.KernelModeMapped)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_BELOW: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LayoutBelow = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LayoutBelow)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_MONO: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LayoutMono = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LayoutMono)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.LocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LINEAR: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Linear = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Linear)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__MEDIA_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.MediaCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.MediaCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_OPTIMIZATION_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NoOptimizationPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NoOptimizationPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_PHYS_MEMORY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NoPhysMemory = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NoPhysMemory)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NOT_LOCKABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NotLockable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NotLockable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NON_LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NonLocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.NonLocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.StdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.StdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__PSEUDO_STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.PseudoStdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.PseudoStdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__UNDEFINED64KBSWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Undefined64KBSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Undefined64KBSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__REDECRIBED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.RedecribedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.RedecribedPlanes)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__RENDER_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.RenderCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.RenderCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ROTATED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Rotated = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Rotated)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SHARED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Shared = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Shared)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SOFTWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.SoftwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.SoftwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SVM: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.SVM)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE4: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Tile4 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Tile4)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE64: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Tile64 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.Tile64)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_W: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledW = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledW)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_X: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledX = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledX)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_Y: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledY = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledY)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YF: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledYf = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledYf)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledYs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.TiledYs)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__WDDM_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.WddmProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.WddmProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__XADAPTER: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.XAdapter = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.XAdapter)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PREALLOCATED_RES_INFO: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.__PreallocatedResInfo = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.__PreallocatedResInfo)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PRE_WDDM2SVM: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.__PreWddm2SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Info.__PreWddm2SVM)>(*tokInfo);
                                                                } break;
                                                                };
                                                                tokInfo = tokInfo + 1 + tokInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInfo);
                                                                if (tokInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokInfo = tokInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokInfo == tokInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__WA: {
                                                        const TokenHeader *tokWa = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokWaEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokWa < tokWaEnd) {
                                                            if (false == tokWa->flags.flag4IsVariableLength) {
                                                                switch (tokWa->id) {
                                                                default:
                                                                    if (tokWa->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__GTMFX2ND_LEVEL_BATCH_RING_SIZE_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_MPR_ROW_STORE32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_DMV_BUFFER32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_BUFFER_SAMPLER_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.NoBufferSamplerPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.NoBufferSamplerPadding)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_LEGACY_PLANAR_LINEAR_VIDEO_RESTRICTIONS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__CHVASTC_SKIP_VIRTUAL_MIPS: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.CHVAstcSkipVirtualMips = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.CHVAstcSkipVirtualMips)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_PACKED_MIP_TAIL: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisablePackedMipTail = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisablePackedMipTail)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521____FORCE_OTHER_HVALIGN4: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.__ForceOtherHVALIGN4 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.__ForceOtherHVALIGN4)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisableDisplayCcsClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisableDisplayCcsClearColor)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisableDisplayCcsCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DisableDisplayCcsCompression)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__PRE_GEN12FAST_CLEAR_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.PreGen12FastClearOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.PreGen12FastClearOnly)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__FORCE_STD_ALLOC_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ForceStdAllocAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.ForceStdAllocAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DENIABLE_LOCAL_ONLY_FOR_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DeniableLocalOnlyForCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Flags.Wa.DeniableLocalOnlyForCompression)>(*tokWa);
                                                                } break;
                                                                };
                                                                tokWa = tokWa + 1 + tokWa->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokWa);
                                                                if (tokWa->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokWa = tokWa + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokWa == tokWaEnd);
                                                    } break;
                                                    };
                                                    tokFlags = tokFlags + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokFlags == tokFlagsEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__CACHE_POLICY: {
                                            const TokenHeader *tokCachePolicy = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokCachePolicyEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokCachePolicy < tokCachePolicyEnd) {
                                                if (false == tokCachePolicy->flags.flag4IsVariableLength) {
                                                    switch (tokCachePolicy->id) {
                                                    default:
                                                        if (tokCachePolicy->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_TEXTURE_INFO_REC__ANONYMOUS4927__USAGE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.CachePolicy.Usage = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.CachePolicy.Usage)>(*tokCachePolicy);
                                                    } break;
                                                    };
                                                    tokCachePolicy = tokCachePolicy + 1 + tokCachePolicy->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCachePolicy);
                                                    if (tokCachePolicy->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokCachePolicy = tokCachePolicy + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokCachePolicy == tokCachePolicyEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__MSAA: {
                                            const TokenHeader *tokMSAA = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokMSAAEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokMSAA < tokMSAAEnd) {
                                                if (false == tokMSAA->flags.flag4IsVariableLength) {
                                                    switch (tokMSAA->id) {
                                                    default:
                                                        if (tokMSAA->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_RESOURCE_MSAA_INFO_REC__SAMPLE_PATTERN: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.MSAA.SamplePattern = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.MSAA.SamplePattern)>(*tokMSAA);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_MSAA_INFO_REC__NUM_SAMPLES: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.MSAA.NumSamples = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.MSAA.NumSamples)>(*tokMSAA);
                                                    } break;
                                                    };
                                                    tokMSAA = tokMSAA + 1 + tokMSAA->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokMSAA);
                                                    if (tokMSAA->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokMSAA = tokMSAA + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokMSAA == tokMSAAEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__ALIGNMENT: {
                                            const TokenHeader *tokAlignment = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokAlignmentEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokAlignment < tokAlignmentEnd) {
                                                if (false == tokAlignment->flags.flag4IsVariableLength) {
                                                    switch (tokAlignment->id) {
                                                    default:
                                                        if (tokAlignment->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_RESOURCE_ALIGNMENT_REC__ARRAY_SPACING_SINGLE_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.ArraySpacingSingleLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.ArraySpacingSingleLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__BASE_ALIGNMENT: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.BaseAlignment = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.BaseAlignment)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__HALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.HAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.HAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__VALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.VAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.VAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__DALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.DAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.DAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__MIP_TAIL_START_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.MipTailStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.MipTailStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_START_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_WIDTH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipWidth)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.PackedMipHeight)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__QPITCH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Alignment.QPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Alignment.QPitch)>(*tokAlignment);
                                                    } break;
                                                    };
                                                    tokAlignment = tokAlignment + 1 + tokAlignment->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokAlignment);
                                                    if (tokAlignment->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokAlignment = tokAlignment + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokAlignment == tokAlignmentEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__OFFSET_INFO: {
                                            const TokenHeader *tokOffsetInfo = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokOffsetInfo < tokOffsetInfoEnd) {
                                                if (false == tokOffsetInfo->flags.flag4IsVariableLength) {
                                                    if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokOffsetInfo = tokOffsetInfo + 1 + tokOffsetInfo->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOffsetInfo);
                                                    switch (tokOffsetInfo->id) {
                                                    default:
                                                        if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE3DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture3DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture3DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture3DOffsetInfo < tokTexture3DOffsetInfoEnd) {
                                                            if (false == tokTexture3DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture3DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__MIP0SLICE_PITCH: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch)>(*tokTexture3DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture3DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + 1 + tokTexture3DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture3DOffsetInfo);
                                                                if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture3DOffsetInfo == tokTexture3DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE2DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture2DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture2DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture2DOffsetInfo < tokTexture2DOffsetInfoEnd) {
                                                            if (false == tokTexture2DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture2DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_LOCK: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_RENDER: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Texture2DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + 1 + tokTexture2DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture2DOffsetInfo);
                                                                if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture2DOffsetInfo == tokTexture2DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__PLANE: {
                                                        const TokenHeader *tokPlane = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokPlaneEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokPlane < tokPlaneEnd) {
                                                            if (false == tokPlane->flags.flag4IsVariableLength) {
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ARRAY_QPITCH: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.ArrayQPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.ArrayQPitch)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__X: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.X)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.X, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.X), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.X));
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__Y: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.Y)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.Y, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.Y), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.Y));
                                                                } break;
                                                                case TOK_FBD_GMM_PLANAR_OFFSET_INFO_REC__NO_OF_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.NoOfPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.NoOfPlanes)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBB_GMM_PLANAR_OFFSET_INFO_REC__IS_TILE_ALIGNED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.IsTileAlignedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.IsTileAlignedPlanes)>(*tokPlane);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + 1 + tokPlane->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlane);
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FS_GMM_PLANAR_OFFSET_INFO_REC__UN_ALIGNED: {
                                                                    const TokenHeader *tokUnAligned = varLen->getValue<TokenHeader>();
                                                                    const TokenHeader *tokUnAlignedEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                                    while (tokUnAligned < tokUnAlignedEnd) {
                                                                        if (false == tokUnAligned->flags.flag4IsVariableLength) {
                                                                            switch (tokUnAligned->id) {
                                                                            default:
                                                                                if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                    return false;
                                                                                }
                                                                                break;
                                                                            case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ANONYMOUS1851__HEIGHT: {
                                                                                auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValue<char>();
                                                                                auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValueSizeInBytes();
                                                                                if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.UnAligned.Height)) {
                                                                                    return false;
                                                                                }
                                                                                WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.UnAligned.Height, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.UnAligned.Height), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSurf.OffsetInfo.Plane.UnAligned.Height));
                                                                            } break;
                                                                            };
                                                                            tokUnAligned = tokUnAligned + 1 + tokUnAligned->valueDwordCount;
                                                                        } else {
                                                                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokUnAligned);
                                                                            if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                return false;
                                                                            }
                                                                            tokUnAligned = tokUnAligned + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                                        }
                                                                    }
                                                                    WCH_ASSERT(tokUnAligned == tokUnAlignedEnd);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokPlane == tokPlaneEnd);
                                                    } break;
                                                    };
                                                    tokOffsetInfo = tokOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokOffsetInfo == tokOffsetInfoEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__S3D: {
                                            const TokenHeader *tokS3d = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokS3dEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokS3d < tokS3dEnd) {
                                                if (false == tokS3d->flags.flag4IsVariableLength) {
                                                    switch (tokS3d->id) {
                                                    default:
                                                        if (tokS3d->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__DISPLAY_MODE_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.DisplayModeHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.DisplayModeHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__NUM_BLANK_ACTIVE_LINES: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.NumBlankActiveLines = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.NumBlankActiveLines)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__RFRAME_OFFSET: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.RFrameOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.RFrameOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__BLANK_AREA_OFFSET: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.BlankAreaOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.BlankAreaOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.TallBufferHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.TallBufferHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_SIZE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.TallBufferSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.TallBufferSize)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBC_GMM_S3D_INFO_REC__IS_RFRAME: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.S3d.IsRFrame = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.S3d.IsRFrame)>(*tokS3d);
                                                    } break;
                                                    };
                                                    tokS3d = tokS3d + 1 + tokS3d->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokS3d);
                                                    if (tokS3d->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokS3d = tokS3d + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokS3d == tokS3dEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__SEGMENT_OVERRIDE: {
                                            const TokenHeader *tokSegmentOverride = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSegmentOverrideEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSegmentOverride < tokSegmentOverrideEnd) {
                                                if (false == tokSegmentOverride->flags.flag4IsVariableLength) {
                                                    switch (tokSegmentOverride->id) {
                                                    default:
                                                        if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__SEG1: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.SegmentOverride.Seg1 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.SegmentOverride.Seg1)>(*tokSegmentOverride);
                                                    } break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__EVICT: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.SegmentOverride.Evict = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.SegmentOverride.Evict)>(*tokSegmentOverride);
                                                    } break;
                                                    };
                                                    tokSegmentOverride = tokSegmentOverride + 1 + tokSegmentOverride->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSegmentOverride);
                                                    if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokSegmentOverride = tokSegmentOverride + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSegmentOverride == tokSegmentOverrideEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__PLATFORM: {
#if _DEBUG || _RELEASE_INTERNAL
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__EXISTING_SYS_MEM: {
                                            const TokenHeader *tokExistingSysMem = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokExistingSysMemEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokExistingSysMem < tokExistingSysMemEnd) {
                                                if (false == tokExistingSysMem->flags.flag4IsVariableLength) {
                                                    switch (tokExistingSysMem->id) {
                                                    default:
                                                        if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_GMM_ALLOCATED: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.ExistingSysMem.IsGmmAllocated = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.ExistingSysMem.IsGmmAllocated)>(*tokExistingSysMem);
                                                    } break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_PAGE_ALIGNED: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.ExistingSysMem.IsPageAligned = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.ExistingSysMem.IsPageAligned)>(*tokExistingSysMem);
                                                    } break;
                                                    };
                                                    tokExistingSysMem = tokExistingSysMem + 1 + tokExistingSysMem->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokExistingSysMem);
                                                    if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokExistingSysMem = tokExistingSysMem + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokExistingSysMem == tokExistingSysMemEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC____PLATFORM: {
#if !(_DEBUG || _RELEASE_INTERNAL)
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSurf.__Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSurf.__Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        };
                                        tokAuxSurf = tokAuxSurf + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokAuxSurf == tokAuxSurfEnd);
                            } break;
                            case TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__AUX_SEC_SURF: {
                                const TokenHeader *tokAuxSecSurf = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokAuxSecSurfEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokAuxSecSurf < tokAuxSecSurfEnd) {
                                    if (false == tokAuxSecSurf->flags.flag4IsVariableLength) {
                                        switch (tokAuxSecSurf->id) {
                                        default:
                                            if (tokAuxSecSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TYPE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.Type = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Type)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__FORMAT: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.Format = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Format)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BITS_PER_PIXEL: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.BitsPerPixel = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.BitsPerPixel)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__BASE_WIDTH: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.BaseWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.BaseWidth)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__BASE_HEIGHT: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.BaseHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.BaseHeight)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__DEPTH: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Depth)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAX_LOD: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.MaxLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.MaxLod)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__ARRAY_SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.ArraySize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.ArraySize)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CP_TAG: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.CpTag = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.CpTag)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_MODE: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokAuxSecSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokAuxSecSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcMode)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.MmcMode, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcMode), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcMode));
                                        } break;
                                        case TOK_FBC_GMM_TEXTURE_INFO_REC__MMC_HINT: {
                                            auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokAuxSecSurf).getValue<char>();
                                            auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokAuxSecSurf).getValueSizeInBytes();
                                            if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcHint)) {
                                                return false;
                                            }
                                            WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.MmcHint, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcHint), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.MmcHint));
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__PITCH: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.Pitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Pitch)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__OVERRIDE_PITCH: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.OverridePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OverridePitch)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.Size = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Size)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__CCSIZE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.CCSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.CCSize)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__UNPADDED_SIZE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.UnpaddedSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.UnpaddedSize)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBQ_GMM_TEXTURE_INFO_REC__SIZE_REPORT_TO_OS: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.SizeReportToOS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.SizeReportToOS)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FE_GMM_TEXTURE_INFO_REC__TILE_MODE: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.TileMode = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.TileMode)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__CCSMODE_ALIGN: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.CCSModeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.CCSModeAlign)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__LEGACY_FLAGS: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.LegacyFlags = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.LegacyFlags)>(*tokAuxSecSurf);
                                        } break;
                                        case TOK_FBD_GMM_TEXTURE_INFO_REC__MAXIMUM_RENAMING_LIST_LENGTH: {
                                            dst.GmmResourceInfoCommon.AuxSecSurf.MaximumRenamingListLength = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.MaximumRenamingListLength)>(*tokAuxSecSurf);
                                        } break;
                                        };
                                        tokAuxSecSurf = tokAuxSecSurf + 1 + tokAuxSecSurf->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokAuxSecSurf);
                                        switch (tokAuxSecSurf->id) {
                                        default:
                                            if (tokAuxSecSurf->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__FLAGS: {
                                            const TokenHeader *tokFlags = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokFlagsEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokFlags < tokFlagsEnd) {
                                                if (false == tokFlags->flags.flag4IsVariableLength) {
                                                    if (tokFlags->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokFlags = tokFlags + 1 + tokFlags->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokFlags);
                                                    switch (tokFlags->id) {
                                                    default:
                                                        if (tokFlags->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__GPU: {
                                                        const TokenHeader *tokGpu = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokGpuEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokGpu < tokGpuEnd) {
                                                            if (false == tokGpu->flags.flag4IsVariableLength) {
                                                                switch (tokGpu->id) {
                                                                default:
                                                                    if (tokGpu->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CAMERA_CAPTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.CameraCapture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.CameraCapture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.CCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.CCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_DISCARD: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorDiscard = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorDiscard)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorSeparation = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorSeparation)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__COLOR_SEPARATION_RGBX: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorSeparationRGBX = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ColorSeparationRGBX)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__CONSTANT: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Constant = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Constant)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__DEPTH: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Depth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Depth)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.FlipChain = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.FlipChain)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__FLIP_CHAIN_PREFERRED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.FlipChainPreferred = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.FlipChainPreferred)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HISTORY_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.HistoryBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.HistoryBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__HI_Z: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.HiZ = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.HiZ)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDEX: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Index = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Index)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INDIRECT_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.IndirectClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.IndirectClearColor)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INSTRUCTION_FLAT: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.InstructionFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.InstructionFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__INTERLACED_SCAN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.InterlacedScan = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.InterlacedScan)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MMC: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MMC = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MMC)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__MOTION_COMP: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MotionComp = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.MotionComp)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__NO_RESTRICTION: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.NoRestriction = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.NoRestriction)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__OVERLAY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Overlay = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Overlay)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PRESENTABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Presentable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Presentable)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__PROCEDURAL_TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ProceduralTexture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ProceduralTexture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__QUERY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Query = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Query)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__RENDER_TARGET: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.RenderTarget = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.RenderTarget)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.S3d = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.S3d)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__S3D_DX: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.S3dDx = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.S3dDx)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_NON_PACKED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__S3dNonPacked = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__S3dNonPacked)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____S3D_WIDI: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__S3dWidi = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__S3dWidi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SCRATCH_FLAT: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ScratchFlat = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.ScratchFlat)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__SEPARATE_STENCIL: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.SeparateStencil = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.SeparateStencil)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.State = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.State)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STATE_DX9CONSTANT_BUFFER: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.StateDx9ConstantBuffer = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.StateDx9ConstantBuffer)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__STREAM: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Stream = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Stream)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXT_API: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TextApi = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TextApi)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TEXTURE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Texture = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Texture)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILED_RESOURCE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TiledResource = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TiledResource)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__TILE_POOL: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TilePool = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.TilePool)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__UNIFIED_AUX_SURFACE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.UnifiedAuxSurface = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.UnifiedAuxSurface)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VERTEX: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Vertex = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Vertex)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739__VIDEO: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Video = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.Video)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_XCCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaTileXCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaTileXCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_TILE_YCCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaTileYCcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaTileYCcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____MSAA_TILE_MCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__MsaaTileMcs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__MsaaTileMcs)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____NON_MSAA_LINEAR_CCS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaLinearCCS = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__NonMsaaLinearCCS)>(*tokGpu);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS1739____REMAINING: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__Remaining = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Gpu.__Remaining)>(*tokGpu);
                                                                } break;
                                                                };
                                                                tokGpu = tokGpu + 1 + tokGpu->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokGpu);
                                                                if (tokGpu->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokGpu = tokGpu + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokGpu == tokGpuEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__INFO: {
                                                        const TokenHeader *tokInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokInfo < tokInfoEnd) {
                                                            if (false == tokInfo->flags.flag4IsVariableLength) {
                                                                switch (tokInfo->id) {
                                                                default:
                                                                    if (tokInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ALLOW_VIRTUAL_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.AllowVirtualPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.AllowVirtualPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__BIG_PAGE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.BigPage = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.BigPage)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CACHEABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Cacheable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Cacheable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CONTIG_PHYS_MEMORY_FORI_DART: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ContigPhysMemoryForiDART = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ContigPhysMemoryForiDART)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__CORNER_TEXEL_MODE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.CornerTexelMode = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.CornerTexelMode)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__EXISTING_SYS_MEM: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ExistingSysMem = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ExistingSysMem)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__FORCE_RESIDENCY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ForceResidency = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.ForceResidency)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GFDT: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Gfdt = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Gfdt)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__GTT_MAP_TYPE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.GttMapType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.GttMapType)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__HARDWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.HardwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.HardwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__KERNEL_MODE_MAPPED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.KernelModeMapped = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.KernelModeMapped)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_BELOW: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LayoutBelow = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LayoutBelow)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LAYOUT_MONO: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LayoutMono = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LayoutMono)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.LocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__LINEAR: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Linear = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Linear)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__MEDIA_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.MediaCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.MediaCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_OPTIMIZATION_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NoOptimizationPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NoOptimizationPadding)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NO_PHYS_MEMORY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NoPhysMemory = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NoPhysMemory)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NOT_LOCKABLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NotLockable = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NotLockable)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__NON_LOCAL_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NonLocalOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.NonLocalOnly)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.StdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.StdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__PSEUDO_STD_SWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.PseudoStdSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.PseudoStdSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__UNDEFINED64KBSWIZZLE: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Undefined64KBSwizzle = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Undefined64KBSwizzle)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__REDECRIBED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.RedecribedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.RedecribedPlanes)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__RENDER_COMPRESSED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.RenderCompressed = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.RenderCompressed)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__ROTATED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Rotated = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Rotated)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SHARED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Shared = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Shared)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SOFTWARE_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.SoftwareProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.SoftwareProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__SVM: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.SVM)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE4: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Tile4 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Tile4)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILE64: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Tile64 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.Tile64)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_W: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledW = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledW)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_X: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledX = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledX)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_Y: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledY = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledY)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YF: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledYf = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledYf)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__TILED_YS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledYs = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.TiledYs)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__WDDM_PROTECTED: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.WddmProtected = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.WddmProtected)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797__XADAPTER: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.XAdapter = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.XAdapter)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PREALLOCATED_RES_INFO: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.__PreallocatedResInfo = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.__PreallocatedResInfo)>(*tokInfo);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS6797____PRE_WDDM2SVM: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.__PreWddm2SVM = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Info.__PreWddm2SVM)>(*tokInfo);
                                                                } break;
                                                                };
                                                                tokInfo = tokInfo + 1 + tokInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokInfo);
                                                                if (tokInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokInfo = tokInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokInfo == tokInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_RESOURCE_FLAG_REC__WA: {
                                                        const TokenHeader *tokWa = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokWaEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokWa < tokWaEnd) {
                                                            if (false == tokWa->flags.flag4IsVariableLength) {
                                                                switch (tokWa->id) {
                                                                default:
                                                                    if (tokWa->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__GTMFX2ND_LEVEL_BATCH_RING_SIZE_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.GTMfx2ndLevelBatchRingSizeAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_MPR_ROW_STORE32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ILKNeedAvcMprRowStore32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__ILKNEED_AVC_DMV_BUFFER32KALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ILKNeedAvcDmvBuffer32KAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_BUFFER_SAMPLER_PADDING: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.NoBufferSamplerPadding = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.NoBufferSamplerPadding)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__NO_LEGACY_PLANAR_LINEAR_VIDEO_RESTRICTIONS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.NoLegacyPlanarLinearVideoRestrictions)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__CHVASTC_SKIP_VIRTUAL_MIPS: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.CHVAstcSkipVirtualMips = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.CHVAstcSkipVirtualMips)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_PACKED_MIP_TAIL: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisablePackedMipTail = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisablePackedMipTail)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521____FORCE_OTHER_HVALIGN4: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.__ForceOtherHVALIGN4 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.__ForceOtherHVALIGN4)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_CLEAR_COLOR: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisableDisplayCcsClearColor = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisableDisplayCcsClearColor)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DISABLE_DISPLAY_CCS_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisableDisplayCcsCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DisableDisplayCcsCompression)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__PRE_GEN12FAST_CLEAR_ONLY: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.PreGen12FastClearOnly = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.PreGen12FastClearOnly)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__FORCE_STD_ALLOC_ALIGN: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ForceStdAllocAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.ForceStdAllocAlign)>(*tokWa);
                                                                } break;
                                                                case TOK_FBD_GMM_RESOURCE_FLAG_REC__ANONYMOUS12521__DENIABLE_LOCAL_ONLY_FOR_COMPRESSION: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DeniableLocalOnlyForCompression = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Flags.Wa.DeniableLocalOnlyForCompression)>(*tokWa);
                                                                } break;
                                                                };
                                                                tokWa = tokWa + 1 + tokWa->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokWa);
                                                                if (tokWa->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokWa = tokWa + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokWa == tokWaEnd);
                                                    } break;
                                                    };
                                                    tokFlags = tokFlags + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokFlags == tokFlagsEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__CACHE_POLICY: {
                                            const TokenHeader *tokCachePolicy = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokCachePolicyEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokCachePolicy < tokCachePolicyEnd) {
                                                if (false == tokCachePolicy->flags.flag4IsVariableLength) {
                                                    switch (tokCachePolicy->id) {
                                                    default:
                                                        if (tokCachePolicy->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_TEXTURE_INFO_REC__ANONYMOUS4927__USAGE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.CachePolicy.Usage = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.CachePolicy.Usage)>(*tokCachePolicy);
                                                    } break;
                                                    };
                                                    tokCachePolicy = tokCachePolicy + 1 + tokCachePolicy->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokCachePolicy);
                                                    if (tokCachePolicy->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokCachePolicy = tokCachePolicy + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokCachePolicy == tokCachePolicyEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__MSAA: {
                                            const TokenHeader *tokMSAA = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokMSAAEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokMSAA < tokMSAAEnd) {
                                                if (false == tokMSAA->flags.flag4IsVariableLength) {
                                                    switch (tokMSAA->id) {
                                                    default:
                                                        if (tokMSAA->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_GMM_RESOURCE_MSAA_INFO_REC__SAMPLE_PATTERN: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.MSAA.SamplePattern = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.MSAA.SamplePattern)>(*tokMSAA);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_MSAA_INFO_REC__NUM_SAMPLES: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.MSAA.NumSamples = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.MSAA.NumSamples)>(*tokMSAA);
                                                    } break;
                                                    };
                                                    tokMSAA = tokMSAA + 1 + tokMSAA->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokMSAA);
                                                    if (tokMSAA->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokMSAA = tokMSAA + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokMSAA == tokMSAAEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__ALIGNMENT: {
                                            const TokenHeader *tokAlignment = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokAlignmentEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokAlignment < tokAlignmentEnd) {
                                                if (false == tokAlignment->flags.flag4IsVariableLength) {
                                                    switch (tokAlignment->id) {
                                                    default:
                                                        if (tokAlignment->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_RESOURCE_ALIGNMENT_REC__ARRAY_SPACING_SINGLE_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.ArraySpacingSingleLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.ArraySpacingSingleLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__BASE_ALIGNMENT: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.BaseAlignment = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.BaseAlignment)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__HALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.HAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.HAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__VALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.VAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.VAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__DALIGN: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.DAlign = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.DAlign)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__MIP_TAIL_START_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.MipTailStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.MipTailStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_START_LOD: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipStartLod = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipStartLod)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_WIDTH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipWidth = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipWidth)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__PACKED_MIP_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.PackedMipHeight)>(*tokAlignment);
                                                    } break;
                                                    case TOK_FBD_GMM_RESOURCE_ALIGNMENT_REC__QPITCH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.QPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Alignment.QPitch)>(*tokAlignment);
                                                    } break;
                                                    };
                                                    tokAlignment = tokAlignment + 1 + tokAlignment->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokAlignment);
                                                    if (tokAlignment->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokAlignment = tokAlignment + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokAlignment == tokAlignmentEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__OFFSET_INFO: {
                                            const TokenHeader *tokOffsetInfo = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokOffsetInfo < tokOffsetInfoEnd) {
                                                if (false == tokOffsetInfo->flags.flag4IsVariableLength) {
                                                    if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokOffsetInfo = tokOffsetInfo + 1 + tokOffsetInfo->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokOffsetInfo);
                                                    switch (tokOffsetInfo->id) {
                                                    default:
                                                        if (tokOffsetInfo->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE3DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture3DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture3DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture3DOffsetInfo < tokTexture3DOffsetInfoEnd) {
                                                            if (false == tokTexture3DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture3DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__MIP0SLICE_PITCH: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Mip0SlicePitch)>(*tokTexture3DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_3D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture3DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture3DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + 1 + tokTexture3DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture3DOffsetInfo);
                                                                if (tokTexture3DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture3DOffsetInfo = tokTexture3DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture3DOffsetInfo == tokTexture3DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__TEXTURE2DOFFSET_INFO: {
                                                        const TokenHeader *tokTexture2DOffsetInfo = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokTexture2DOffsetInfoEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokTexture2DOffsetInfo < tokTexture2DOffsetInfoEnd) {
                                                            if (false == tokTexture2DOffsetInfo->flags.flag4IsVariableLength) {
                                                                switch (tokTexture2DOffsetInfo->id) {
                                                                default:
                                                                    if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_LOCK: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchLock)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__ARRAY_QPITCH_RENDER: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.ArrayQPitchRender)>(*tokTexture2DOffsetInfo);
                                                                } break;
                                                                case TOK_FBQ_GMM_2D_TEXTURE_OFFSET_INFO_REC__OFFSET: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokTexture2DOffsetInfo).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.Offset)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.Offset, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.Offset), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Texture2DOffsetInfo.Offset));
                                                                } break;
                                                                };
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + 1 + tokTexture2DOffsetInfo->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokTexture2DOffsetInfo);
                                                                if (tokTexture2DOffsetInfo->flags.flag3IsMandatory) {
                                                                    return false;
                                                                }
                                                                tokTexture2DOffsetInfo = tokTexture2DOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokTexture2DOffsetInfo == tokTexture2DOffsetInfoEnd);
                                                    } break;
                                                    case TOK_FS_GMM_OFFSET_INFO_REC__ANONYMOUS3429__PLANE: {
                                                        const TokenHeader *tokPlane = varLen->getValue<TokenHeader>();
                                                        const TokenHeader *tokPlaneEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                        while (tokPlane < tokPlaneEnd) {
                                                            if (false == tokPlane->flags.flag4IsVariableLength) {
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ARRAY_QPITCH: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.ArrayQPitch = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.ArrayQPitch)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__X: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.X)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.X, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.X), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.X));
                                                                } break;
                                                                case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__Y: {
                                                                    auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValue<char>();
                                                                    auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokPlane).getValueSizeInBytes();
                                                                    if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.Y)) {
                                                                        return false;
                                                                    }
                                                                    WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.Y, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.Y), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.Y));
                                                                } break;
                                                                case TOK_FBD_GMM_PLANAR_OFFSET_INFO_REC__NO_OF_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.NoOfPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.NoOfPlanes)>(*tokPlane);
                                                                } break;
                                                                case TOK_FBB_GMM_PLANAR_OFFSET_INFO_REC__IS_TILE_ALIGNED_PLANES: {
                                                                    dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.IsTileAlignedPlanes = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.IsTileAlignedPlanes)>(*tokPlane);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + 1 + tokPlane->valueDwordCount;
                                                            } else {
                                                                auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlane);
                                                                switch (tokPlane->id) {
                                                                default:
                                                                    if (tokPlane->flags.flag3IsMandatory) {
                                                                        return false;
                                                                    }
                                                                    break;
                                                                case TOK_FS_GMM_PLANAR_OFFSET_INFO_REC__UN_ALIGNED: {
                                                                    const TokenHeader *tokUnAligned = varLen->getValue<TokenHeader>();
                                                                    const TokenHeader *tokUnAlignedEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                                                    while (tokUnAligned < tokUnAlignedEnd) {
                                                                        if (false == tokUnAligned->flags.flag4IsVariableLength) {
                                                                            switch (tokUnAligned->id) {
                                                                            default:
                                                                                if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                    return false;
                                                                                }
                                                                                break;
                                                                            case TOK_FBQ_GMM_PLANAR_OFFSET_INFO_REC__ANONYMOUS1851__HEIGHT: {
                                                                                auto srcData = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValue<char>();
                                                                                auto srcSize = reinterpret_cast<const TokenArray<1> &>(*tokUnAligned).getValueSizeInBytes();
                                                                                if (srcSize < sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.UnAligned.Height)) {
                                                                                    return false;
                                                                                }
                                                                                WCH_SAFE_COPY(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.UnAligned.Height, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.UnAligned.Height), srcData, sizeof(dst.GmmResourceInfoCommon.AuxSecSurf.OffsetInfo.Plane.UnAligned.Height));
                                                                            } break;
                                                                            };
                                                                            tokUnAligned = tokUnAligned + 1 + tokUnAligned->valueDwordCount;
                                                                        } else {
                                                                            auto varLen = reinterpret_cast<const TokenVariableLength *>(tokUnAligned);
                                                                            if (tokUnAligned->flags.flag3IsMandatory) {
                                                                                return false;
                                                                            }
                                                                            tokUnAligned = tokUnAligned + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                                        }
                                                                    }
                                                                    WCH_ASSERT(tokUnAligned == tokUnAlignedEnd);
                                                                } break;
                                                                };
                                                                tokPlane = tokPlane + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                            }
                                                        }
                                                        WCH_ASSERT(tokPlane == tokPlaneEnd);
                                                    } break;
                                                    };
                                                    tokOffsetInfo = tokOffsetInfo + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokOffsetInfo == tokOffsetInfoEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__S3D: {
                                            const TokenHeader *tokS3d = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokS3dEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokS3d < tokS3dEnd) {
                                                if (false == tokS3d->flags.flag4IsVariableLength) {
                                                    switch (tokS3d->id) {
                                                    default:
                                                        if (tokS3d->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__DISPLAY_MODE_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.DisplayModeHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.DisplayModeHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__NUM_BLANK_ACTIVE_LINES: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.NumBlankActiveLines = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.NumBlankActiveLines)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__RFRAME_OFFSET: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.RFrameOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.RFrameOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__BLANK_AREA_OFFSET: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.BlankAreaOffset = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.BlankAreaOffset)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_HEIGHT: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.TallBufferHeight = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.TallBufferHeight)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBD_GMM_S3D_INFO_REC__TALL_BUFFER_SIZE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.TallBufferSize = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.TallBufferSize)>(*tokS3d);
                                                    } break;
                                                    case TOK_FBC_GMM_S3D_INFO_REC__IS_RFRAME: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.S3d.IsRFrame = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.S3d.IsRFrame)>(*tokS3d);
                                                    } break;
                                                    };
                                                    tokS3d = tokS3d + 1 + tokS3d->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokS3d);
                                                    if (tokS3d->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokS3d = tokS3d + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokS3d == tokS3dEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__SEGMENT_OVERRIDE: {
                                            const TokenHeader *tokSegmentOverride = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokSegmentOverrideEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokSegmentOverride < tokSegmentOverrideEnd) {
                                                if (false == tokSegmentOverride->flags.flag4IsVariableLength) {
                                                    switch (tokSegmentOverride->id) {
                                                    default:
                                                        if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__SEG1: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.SegmentOverride.Seg1 = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.SegmentOverride.Seg1)>(*tokSegmentOverride);
                                                    } break;
                                                    case TOK_FBD_GMM_TEXTURE_INFO_REC__ANONYMOUS6185__EVICT: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.SegmentOverride.Evict = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.SegmentOverride.Evict)>(*tokSegmentOverride);
                                                    } break;
                                                    };
                                                    tokSegmentOverride = tokSegmentOverride + 1 + tokSegmentOverride->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokSegmentOverride);
                                                    if (tokSegmentOverride->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokSegmentOverride = tokSegmentOverride + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokSegmentOverride == tokSegmentOverrideEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__PLATFORM: {
#if _DEBUG || _RELEASE_INTERNAL
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC__EXISTING_SYS_MEM: {
                                            const TokenHeader *tokExistingSysMem = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokExistingSysMemEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokExistingSysMem < tokExistingSysMemEnd) {
                                                if (false == tokExistingSysMem->flags.flag4IsVariableLength) {
                                                    switch (tokExistingSysMem->id) {
                                                    default:
                                                        if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_GMM_ALLOCATED: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.ExistingSysMem.IsGmmAllocated = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.ExistingSysMem.IsGmmAllocated)>(*tokExistingSysMem);
                                                    } break;
                                                    case TOK_FBC_GMM_TEXTURE_INFO_REC__ANONYMOUS6590__IS_PAGE_ALIGNED: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.ExistingSysMem.IsPageAligned = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.ExistingSysMem.IsPageAligned)>(*tokExistingSysMem);
                                                    } break;
                                                    };
                                                    tokExistingSysMem = tokExistingSysMem + 1 + tokExistingSysMem->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokExistingSysMem);
                                                    if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokExistingSysMem = tokExistingSysMem + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokExistingSysMem == tokExistingSysMemEnd);
                                        } break;
                                        case TOK_FS_GMM_TEXTURE_INFO_REC____PLATFORM: {
#if !(_DEBUG || _RELEASE_INTERNAL)
                                            const TokenHeader *tokPlatform = varLen->getValue<TokenHeader>();
                                            const TokenHeader *tokPlatformEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                            while (tokPlatform < tokPlatformEnd) {
                                                if (false == tokPlatform->flags.flag4IsVariableLength) {
                                                    switch (tokPlatform->id) {
                                                    default:
                                                        if (tokPlatform->flags.flag3IsMandatory) {
                                                            return false;
                                                        }
                                                        break;
                                                    case TOK_FE_PLATFORM_STR__E_PRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PCHPRODUCT_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.ePCHProductFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.ePCHProductFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_DISPLAY_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eDisplayCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eDisplayCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_RENDER_CORE_FAMILY: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eRenderCoreFamily = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eRenderCoreFamily)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_PLATFORM_TYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.ePlatformType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.ePlatformType)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usDeviceID = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usDeviceID)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usRevId = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usRevId)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_DEVICE_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usDeviceID_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usDeviceID_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FBW_PLATFORM_STR__US_REV_ID_PCH: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usRevId_PCH = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.usRevId_PCH)>(*tokPlatform);
                                                    } break;
                                                    case TOK_FE_PLATFORM_STR__E_GTTYPE: {
                                                        dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eGTType = readTokValue<decltype(dst.GmmResourceInfoCommon.AuxSecSurf.__Platform.eGTType)>(*tokPlatform);
                                                    } break;
                                                    };
                                                    tokPlatform = tokPlatform + 1 + tokPlatform->valueDwordCount;
                                                } else {
                                                    auto varLen = reinterpret_cast<const TokenVariableLength *>(tokPlatform);
                                                    if (tokPlatform->flags.flag3IsMandatory) {
                                                        return false;
                                                    }
                                                    tokPlatform = tokPlatform + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                                }
                                            }
                                            WCH_ASSERT(tokPlatform == tokPlatformEnd);
#endif
                                        } break;
                                        };
                                        tokAuxSecSurf = tokAuxSecSurf + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokAuxSecSurf == tokAuxSecSurfEnd);
                            } break;
                            case TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__EXISTING_SYS_MEM: {
                                const TokenHeader *tokExistingSysMem = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokExistingSysMemEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokExistingSysMem < tokExistingSysMemEnd) {
                                    if (false == tokExistingSysMem->flags.flag4IsVariableLength) {
                                        switch (tokExistingSysMem->id) {
                                        default:
                                            if (tokExistingSysMem->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_EXISTING_SYS_MEM: {
                                            dst.GmmResourceInfoCommon.ExistingSysMem.pExistingSysMem = readTokValue<decltype(dst.GmmResourceInfoCommon.ExistingSysMem.pExistingSysMem)>(*tokExistingSysMem);
                                        } break;
                                        case TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_VIRT_ADDRESS: {
                                            dst.GmmResourceInfoCommon.ExistingSysMem.pVirtAddress = readTokValue<decltype(dst.GmmResourceInfoCommon.ExistingSysMem.pVirtAddress)>(*tokExistingSysMem);
                                        } break;
                                        case TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__P_GFX_ALIGNED_VIRT_ADDRESS: {
                                            dst.GmmResourceInfoCommon.ExistingSysMem.pGfxAlignedVirtAddress = readTokValue<decltype(dst.GmmResourceInfoCommon.ExistingSysMem.pGfxAlignedVirtAddress)>(*tokExistingSysMem);
                                        } break;
                                        case TOK_FBQ_GMM_EXISTING_SYS_MEM_REC__SIZE: {
                                            dst.GmmResourceInfoCommon.ExistingSysMem.Size = readTokValue<decltype(dst.GmmResourceInfoCommon.ExistingSysMem.Size)>(*tokExistingSysMem);
                                        } break;
                                        case TOK_FBC_GMM_EXISTING_SYS_MEM_REC__IS_GMM_ALLOCATED: {
                                            dst.GmmResourceInfoCommon.ExistingSysMem.IsGmmAllocated = readTokValue<decltype(dst.GmmResourceInfoCommon.ExistingSysMem.IsGmmAllocated)>(*tokExistingSysMem);
                                        } break;
                                        };
                                        tokExistingSysMem = tokExistingSysMem + 1 + tokExistingSysMem->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokExistingSysMem);
                                        if (tokExistingSysMem->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokExistingSysMem = tokExistingSysMem + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokExistingSysMem == tokExistingSysMemEnd);
                            } break;
                            case TOK_FS_GMM_RESOURCE_INFO_COMMON_STRUCT__MULTI_TILE_ARCH: {
                                const TokenHeader *tokMultiTileArch = varLen->getValue<TokenHeader>();
                                const TokenHeader *tokMultiTileArchEnd = varLen->getValue<TokenHeader>() + varLen->valueLengthInBytes / sizeof(TokenHeader);
                                while (tokMultiTileArch < tokMultiTileArchEnd) {
                                    if (false == tokMultiTileArch->flags.flag4IsVariableLength) {
                                        switch (tokMultiTileArch->id) {
                                        default:
                                            if (tokMultiTileArch->flags.flag3IsMandatory) {
                                                return false;
                                            }
                                            break;
                                        case TOK_FBC_GMM_MULTI_TILE_ARCH_REC__ENABLE: {
                                            dst.GmmResourceInfoCommon.MultiTileArch.Enable = readTokValue<decltype(dst.GmmResourceInfoCommon.MultiTileArch.Enable)>(*tokMultiTileArch);
                                        } break;
                                        case TOK_FBC_GMM_MULTI_TILE_ARCH_REC__TILE_INSTANCED: {
                                            dst.GmmResourceInfoCommon.MultiTileArch.TileInstanced = readTokValue<decltype(dst.GmmResourceInfoCommon.MultiTileArch.TileInstanced)>(*tokMultiTileArch);
                                        } break;
                                        case TOK_FBC_GMM_MULTI_TILE_ARCH_REC__GPU_VA_MAPPING_SET: {
                                            dst.GmmResourceInfoCommon.MultiTileArch.GpuVaMappingSet = readTokValue<decltype(dst.GmmResourceInfoCommon.MultiTileArch.GpuVaMappingSet)>(*tokMultiTileArch);
                                        } break;
                                        case TOK_FBC_GMM_MULTI_TILE_ARCH_REC__LOCAL_MEM_ELIGIBILITY_SET: {
                                            dst.GmmResourceInfoCommon.MultiTileArch.LocalMemEligibilitySet = readTokValue<decltype(dst.GmmResourceInfoCommon.MultiTileArch.LocalMemEligibilitySet)>(*tokMultiTileArch);
                                        } break;
                                        case TOK_FBC_GMM_MULTI_TILE_ARCH_REC__LOCAL_MEM_PREFERRED_SET: {
                                            dst.GmmResourceInfoCommon.MultiTileArch.LocalMemPreferredSet = readTokValue<decltype(dst.GmmResourceInfoCommon.MultiTileArch.LocalMemPreferredSet)>(*tokMultiTileArch);
                                        } break;
                                        };
                                        tokMultiTileArch = tokMultiTileArch + 1 + tokMultiTileArch->valueDwordCount;
                                    } else {
                                        auto varLen = reinterpret_cast<const TokenVariableLength *>(tokMultiTileArch);
                                        if (tokMultiTileArch->flags.flag3IsMandatory) {
                                            return false;
                                        }
                                        tokMultiTileArch = tokMultiTileArch + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                                    }
                                }
                                WCH_ASSERT(tokMultiTileArch == tokMultiTileArchEnd);
                            } break;
                            };
                            tokGmmResourceInfoCommon = tokGmmResourceInfoCommon + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
                        }
                    }
                    WCH_ASSERT(tokGmmResourceInfoCommon == tokGmmResourceInfoCommonEnd);
                } break;
                };
                tok = tok + sizeof(TokenVariableLength) / sizeof(uint32_t) + varLen->valuePaddedSizeInDwords;
            }
        }
        WCH_ASSERT(tok == srcTokensEnd);
        return true;
    }
};
