/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/metrics/metric_oa_export_data.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/metrics/metric_oa_enumeration_imp.h"
#include "level_zero/tools/source/metrics/metric_oa_source.h"

#include <algorithm>
#include <map>

namespace L0 {

HeapUsageTracker::HeapUsageTracker(uintptr_t startAddress,
                                   uintptr_t endAddress,
                                   OperationMode mode) : currentAddress(startAddress),
                                                         endAddress(endAddress),
                                                         operationMode(mode) {
    currentOffset = sizeof(zet_intel_metric_df_gpu_export_data_format_t);
}

template <typename T>
std::pair<T *, ptrdiff_t> HeapUsageTracker::allocate(uint64_t count) {
    const auto size = count * sizeof(T);
    std::pair<T *, ptrdiff_t> returnVal{};
    if (operationMode == OperationModeTrackAndAllocate) {
        UNRECOVERABLE_IF(currentAddress + size > endAddress);
        returnVal = {reinterpret_cast<T *>(currentAddress), currentOffset};
        currentAddress += size;
    } else {
        returnVal = {nullptr, currentOffset};
    }
    currentOffset += size;
    return returnVal;
}

size_t HeapUsageTracker::getUsedBytes() {
    return static_cast<size_t>(currentOffset - sizeof(zet_intel_metric_df_gpu_export_data_format_t));
}

MetricOaExporter01::MetricOaExporter01(MetricsDiscovery::IMetricsDevice_1_5 &mdDevice,
                                       MetricsDiscovery::IAdapter_1_9 &mdAdapter,
                                       MetricsDiscovery::IMetricSet_1_5 &mdMetricSet,
                                       MetricsDiscovery::IConcurrentGroup_1_5 &mdConcurrentGroup,
                                       HeapUsageTracker &heapUsageTracker) : mdDevice(mdDevice), adapter(mdAdapter),
                                                                             mdMetricSet(mdMetricSet),
                                                                             concurrentGroup(mdConcurrentGroup),
                                                                             heapUsageTracker(heapUsageTracker) {
    if (heapUsageTracker.getOperationMode() == HeapUsageTracker::OperationModeTrackAndAllocate) {
        currOperationMode = OperationModeGetExportData;
    } else {
        currOperationMode = OperationModeGetHeapSize;
    }
}

template <typename T>
void MetricOaExporter01::assignUnaligned(T *unAlignedData, T *alignedData) {
    memcpy_s(unAlignedData, sizeof(T), alignedData, sizeof(T));
}

void MetricOaExporter01::updateMetricApiVersion(zet_intel_metric_df_gpu_apiversion_0_1_t *apiVersion) {

    MetricsDiscovery::TMetricsDeviceParams_1_2 *metricsDeviceParams = mdDevice.GetParams();
    DEBUG_BREAK_IF(metricsDeviceParams == nullptr);
    assignUnaligned(&apiVersion->majorNumber, &metricsDeviceParams->Version.MajorNumber);
    assignUnaligned(&apiVersion->minorNumber, &metricsDeviceParams->Version.MinorNumber);
    assignUnaligned(&apiVersion->buildNumber, &metricsDeviceParams->Version.BuildNumber);
}

void MetricOaExporter01::assignCstringOffset(cstring_offset_t *cStringOffset, const char *stringValue) {
    if (stringValue == nullptr) {
        cstring_offset_t offset = ZET_INTEL_GPU_METRIC_INVALID_OFFSET;
        assignUnaligned(cStringOffset, &offset);
        return;
    }

    auto bytesNecessary = strnlen_s(stringValue, 512u) + 1;
    auto [address, offset] = heapUsageTracker.allocate<uint8_t>(bytesNecessary);
    if (currOperationMode == OperationModeGetExportData) {
        strcpy_s(reinterpret_cast<char *>(address), bytesNecessary, stringValue);
        assignUnaligned(cStringOffset, &offset);
    }
}

void MetricOaExporter01::assignByteArray(zet_intel_metric_df_gpu_byte_array_0_1_t *byteArray, MetricsDiscovery::TByteArray_1_0 *mdByteArray) {
    auto bytesNecessary = mdByteArray->Size;
    auto [address, offset] = heapUsageTracker.allocate<uint8_t>(bytesNecessary);
    if (currOperationMode == OperationModeGetExportData) {
        memcpy_s(address, bytesNecessary, mdByteArray->Data, bytesNecessary);
        assignUnaligned(&byteArray->data, &offset);
        assignUnaligned(&byteArray->size, &bytesNecessary);
    }
}

ze_result_t MetricOaExporter01::assignTypedValue(zet_intel_metric_df_gpu_typed_value_0_1_t *typedValue, MetricsDiscovery::TTypedValue_1_0 &mdTypedValue) {

    auto type = ZET_INTEL_METRIC_DF_VALUE_TYPE_FORCE_UINT32;
    switch (mdTypedValue.ValueType) {
    case MetricsDiscovery::VALUE_TYPE_UINT32:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32;
        assignUnaligned(&typedValue->valueUInt32, &mdTypedValue.ValueUInt32);
        break;

    case MetricsDiscovery::VALUE_TYPE_UINT64:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64;
        assignUnaligned(&typedValue->valueUInt64, &mdTypedValue.ValueUInt64);
        break;

    case MetricsDiscovery::VALUE_TYPE_FLOAT:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT;
        assignUnaligned(&typedValue->valueFloat, &mdTypedValue.ValueFloat);
        break;

    case MetricsDiscovery::VALUE_TYPE_BOOL:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL;
        assignUnaligned(&typedValue->valueBool, &mdTypedValue.ValueBool);
        break;

    case MetricsDiscovery::VALUE_TYPE_CSTRING:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING;
        assignCstringOffset(&typedValue->valueCString, mdTypedValue.ValueCString);
        break;

    case MetricsDiscovery::VALUE_TYPE_BYTEARRAY:
        type = ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY;
        assignByteArray(&typedValue->valueByteArray, mdTypedValue.ValueByteArray);
        break;

    default:
        METRICS_LOG_ERR("Unknown Typed value 0x%x returning 0x%x",
                        static_cast<uint32_t>(mdTypedValue.ValueType),
                        ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }
    assignUnaligned(&typedValue->valueType, &type);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignAdapterId(zet_intel_metric_df_gpu_adapter_id_0_1_t *adapterId, MetricsDiscovery::TAdapterId_1_6 *mAdapterId) {

    auto type = ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_FORCE_UINT32;
    switch (mAdapterId->Type) {
    case MetricsDiscovery::ADAPTER_ID_TYPE_LUID:
        type = ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_LUID;
        assignUnaligned(&adapterId->luid.lowPart, &mAdapterId->Luid.LowPart);
        assignUnaligned(&adapterId->luid.highPart, &mAdapterId->Luid.HighPart);

        break;

    case MetricsDiscovery::ADAPTER_ID_TYPE_MAJOR_MINOR:
        type = ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_MAJOR_MINOR;
        assignUnaligned(&adapterId->majorMinor.major, &mAdapterId->MajorMinor.Major);
        assignUnaligned(&adapterId->majorMinor.minor, &mAdapterId->MajorMinor.Minor);
        break;

    default:
        METRICS_LOG_ERR("Unknown Adapter Type 0x%x, returning 0x%x",
                        static_cast<uint32_t>(mAdapterId->Type),
                        ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }
    assignUnaligned(&adapterId->type, &type);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::getEquationOperation(zet_intel_metric_df_gpu_equation_operation_t &equationOperation,
                                                     const MetricsDiscovery::TEquationOperation mdEquationOperation) {
    std::map<MetricsDiscovery::TEquationOperation, zet_intel_metric_df_gpu_equation_operation_t> equationOperationMap = {
        {MetricsDiscovery::EQUATION_OPER_RSHIFT, ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT},
        {MetricsDiscovery::EQUATION_OPER_LSHIFT, ZET_INTEL_METRIC_DF_EQUATION_OPER_LSHIFT},
        {MetricsDiscovery::EQUATION_OPER_AND, ZET_INTEL_METRIC_DF_EQUATION_OPER_AND},
        {MetricsDiscovery::EQUATION_OPER_OR, ZET_INTEL_METRIC_DF_EQUATION_OPER_OR},
        {MetricsDiscovery::EQUATION_OPER_XOR, ZET_INTEL_METRIC_DF_EQUATION_OPER_XOR},
        {MetricsDiscovery::EQUATION_OPER_XNOR, ZET_INTEL_METRIC_DF_EQUATION_OPER_XNOR},
        {MetricsDiscovery::EQUATION_OPER_AND_L, ZET_INTEL_METRIC_DF_EQUATION_OPER_AND_L},
        {MetricsDiscovery::EQUATION_OPER_EQUALS, ZET_INTEL_METRIC_DF_EQUATION_OPER_EQUALS},
        {MetricsDiscovery::EQUATION_OPER_UADD, ZET_INTEL_METRIC_DF_EQUATION_OPER_UADD},
        {MetricsDiscovery::EQUATION_OPER_USUB, ZET_INTEL_METRIC_DF_EQUATION_OPER_USUB},
        {MetricsDiscovery::EQUATION_OPER_UMUL, ZET_INTEL_METRIC_DF_EQUATION_OPER_UMUL},
        {MetricsDiscovery::EQUATION_OPER_UDIV, ZET_INTEL_METRIC_DF_EQUATION_OPER_UDIV},
        {MetricsDiscovery::EQUATION_OPER_FADD, ZET_INTEL_METRIC_DF_EQUATION_OPER_FADD},
        {MetricsDiscovery::EQUATION_OPER_FSUB, ZET_INTEL_METRIC_DF_EQUATION_OPER_FSUB},
        {MetricsDiscovery::EQUATION_OPER_FMUL, ZET_INTEL_METRIC_DF_EQUATION_OPER_FMUL},
        {MetricsDiscovery::EQUATION_OPER_FDIV, ZET_INTEL_METRIC_DF_EQUATION_OPER_FDIV},
        {MetricsDiscovery::EQUATION_OPER_UGT, ZET_INTEL_METRIC_DF_EQUATION_OPER_UGT},
        {MetricsDiscovery::EQUATION_OPER_ULT, ZET_INTEL_METRIC_DF_EQUATION_OPER_ULT},
        {MetricsDiscovery::EQUATION_OPER_UGTE, ZET_INTEL_METRIC_DF_EQUATION_OPER_UGTE},
        {MetricsDiscovery::EQUATION_OPER_ULTE, ZET_INTEL_METRIC_DF_EQUATION_OPER_ULTE},
        {MetricsDiscovery::EQUATION_OPER_FGT, ZET_INTEL_METRIC_DF_EQUATION_OPER_FGT},
        {MetricsDiscovery::EQUATION_OPER_FLT, ZET_INTEL_METRIC_DF_EQUATION_OPER_FLT},
        {MetricsDiscovery::EQUATION_OPER_FGTE, ZET_INTEL_METRIC_DF_EQUATION_OPER_FGTE},
        {MetricsDiscovery::EQUATION_OPER_FLTE, ZET_INTEL_METRIC_DF_EQUATION_OPER_FLTE},
        {MetricsDiscovery::EQUATION_OPER_UMIN, ZET_INTEL_METRIC_DF_EQUATION_OPER_UMIN},
        {MetricsDiscovery::EQUATION_OPER_UMAX, ZET_INTEL_METRIC_DF_EQUATION_OPER_UMAX},
        {MetricsDiscovery::EQUATION_OPER_FMIN, ZET_INTEL_METRIC_DF_EQUATION_OPER_FMIN},
        {MetricsDiscovery::EQUATION_OPER_FMAX, ZET_INTEL_METRIC_DF_EQUATION_OPER_FMAX},
    };

    if (equationOperationMap.find(mdEquationOperation) == equationOperationMap.end()) {
        METRICS_LOG_ERR("Unknown Equation Operation 0x%x, returning 0x%x",
                        static_cast<uint32_t>(mdEquationOperation),
                        ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    assignUnaligned(&equationOperation, &equationOperationMap[mdEquationOperation]);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignEquation(zet_intel_metric_df_gpu_equation_0_1_t &equation, MetricsDiscovery::IEquation_1_0 *mdEquation) {

    if (mdEquation == nullptr) {
        equation.elementCount = 0;
        equation.elements = ZET_INTEL_GPU_METRIC_INVALID_OFFSET;
        return ZE_RESULT_SUCCESS;
    }

    const auto elementCount = mdEquation->GetEquationElementsCount();
    auto [elementBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_equation_element_0_1_t>(elementCount);

    for (auto elemIdx = 0u; elemIdx < elementCount; elemIdx++) {

        zet_intel_metric_df_gpu_equation_element_0_1_t currElement{};
        auto mdElement = mdEquation->GetEquationElement(elemIdx);
        auto status = ZE_RESULT_SUCCESS;
        zet_intel_metric_df_gpu_equation_element_type_t type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_FORCE_UINT32;
        assignCstringOffset(&currElement.symbolName, mdElement->SymbolName);

        switch (mdElement->Type) {
        case MetricsDiscovery::EQUATION_ELEM_OPERATION:
            status = getEquationOperation(currElement.operation, mdElement->Operation);
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION;
            break;

        case MetricsDiscovery::EQUATION_ELEM_RD_BITFIELD:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            assignUnaligned(&currElement.readParams.bitOffset, &mdElement->ReadParams.BitOffset);
            assignUnaligned(&currElement.readParams.bitsCount, &mdElement->ReadParams.BitsCount);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT8:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT16:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT32:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_UINT64:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_FLOAT:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            break;
        case MetricsDiscovery::EQUATION_ELEM_RD_40BIT_CNTR:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR;
            assignUnaligned(&currElement.readParams.byteOffset, &mdElement->ReadParams.ByteOffset);
            assignUnaligned(&currElement.readParams.byteOffsetExt, &mdElement->ReadParams.ByteOffsetExt);
            break;

        case MetricsDiscovery::EQUATION_ELEM_IMM_UINT64:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64;
            assignUnaligned(&currElement.immediateUInt64, &mdElement->ImmediateUInt64);

            break;
        case MetricsDiscovery::EQUATION_ELEM_IMM_FLOAT:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT;
            assignUnaligned(&currElement.immediateFloat, &mdElement->ImmediateFloat);
            break;

        case MetricsDiscovery::EQUATION_ELEM_SELF_COUNTER_VALUE:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE;
            break;

        case MetricsDiscovery::EQUATION_ELEM_GLOBAL_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL;
            break;
        case MetricsDiscovery::EQUATION_ELEM_LOCAL_COUNTER_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL;
            break;
        case MetricsDiscovery::EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL;
            break;

        case MetricsDiscovery::EQUATION_ELEM_LOCAL_METRIC_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL;
            break;
        case MetricsDiscovery::EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL;
            break;
        case MetricsDiscovery::EQUATION_ELEM_INFORMATION_SYMBOL:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL;
            break;

        case MetricsDiscovery::EQUATION_ELEM_STD_NORM_GPU_DURATION:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION;
            break;
        case MetricsDiscovery::EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION;
            break;

        case MetricsDiscovery::EQUATION_ELEM_MASK:
            type = ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK;
            assignByteArray(&currElement.mask, &mdElement->Mask);
            break;

        default:
            METRICS_LOG_ERR("Unknown Equation Element Type 0x%x returning 0x%x",
                            static_cast<uint32_t>(mdElement->Type),
                            ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
            return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
        }
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        assignUnaligned(&currElement.type, &type);
        if (currOperationMode == OperationModeGetExportData) {
            assignUnaligned(&elementBase[elemIdx], &currElement);
        }
    }

    equation.elementCount = elementCount;
    equation.elements = offset;

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::getDeltaFunction(zet_intel_metric_df_gpu_delta_function_0_1_t &deltaFunction, MetricsDiscovery::TDeltaFunction_1_0 &mdDeltaFunction) {

    std::map<MetricsDiscovery::TDeltaFunctionType, zet_intel_metric_df_gpu_delta_function_type_t> deltaFunctionMap = {
        {MetricsDiscovery::DELTA_FUNCTION_NULL, ZET_INTEL_METRIC_DF_DELTA_FUNCTION_NULL},
        {MetricsDiscovery::DELTA_N_BITS, ZET_INTEL_METRIC_DF_DELTA_N_BITS},
        {MetricsDiscovery::DELTA_BOOL_OR, ZET_INTEL_METRIC_DF_DELTA_BOOL_OR},
        {MetricsDiscovery::DELTA_BOOL_XOR, ZET_INTEL_METRIC_DF_DELTA_BOOL_XOR},
        {MetricsDiscovery::DELTA_GET_PREVIOUS, ZET_INTEL_METRIC_DF_DELTA_GET_PREVIOUS},
        {MetricsDiscovery::DELTA_GET_LAST, ZET_INTEL_METRIC_DF_DELTA_GET_LAST},
        {MetricsDiscovery::DELTA_NS_TIME, ZET_INTEL_METRIC_DF_DELTA_NS_TIME},
    };

    if (deltaFunctionMap.find(mdDeltaFunction.FunctionType) == deltaFunctionMap.end()) {
        METRICS_LOG_ERR("Error: Unknown Delta Function Type 0x%x returning 0x%x",
                        static_cast<uint32_t>(mdDeltaFunction.FunctionType), ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }
    assignUnaligned(&deltaFunction.bitsCount, &mdDeltaFunction.BitsCount);
    assignUnaligned(&deltaFunction.functionType, &deltaFunctionMap[mdDeltaFunction.FunctionType]);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::getInformationType(zet_intel_metric_df_gpu_information_type_t &infoType, const MetricsDiscovery::TInformationType mdInfoType) {

    std::map<MetricsDiscovery::TInformationType, zet_intel_metric_df_gpu_information_type_t> informationTypeMap = {
        {MetricsDiscovery::INFORMATION_TYPE_REPORT_REASON, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_REPORT_REASON},
        {MetricsDiscovery::INFORMATION_TYPE_VALUE, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_VALUE},
        {MetricsDiscovery::INFORMATION_TYPE_FLAG, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_FLAG},
        {MetricsDiscovery::INFORMATION_TYPE_TIMESTAMP, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_TIMESTAMP},
        {MetricsDiscovery::INFORMATION_TYPE_CONTEXT_ID_TAG, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_CONTEXT_ID_TAG},
        {MetricsDiscovery::INFORMATION_TYPE_SAMPLE_PHASE, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_SAMPLE_PHASE},
        {MetricsDiscovery::INFORMATION_TYPE_GPU_NODE, ZET_INTEL_METRIC_DF_INFORMATION_TYPE_GPU_NODE}};

    if (informationTypeMap.find(mdInfoType) == informationTypeMap.end()) {
        METRICS_LOG_ERR("Error: Unknown Information Type 0x%x returning 0x%x",
                        static_cast<uint32_t>(mdInfoType), ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    assignUnaligned(&infoType, &informationTypeMap[mdInfoType]);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignInformationParams(zet_intel_metric_df_gpu_information_params_0_1_t *infoParams, MetricsDiscovery::TInformationParams_1_0 *mdInfoParams) {

    assignUnaligned(&infoParams->idInSet, &mdInfoParams->IdInSet);
    assignCstringOffset(&infoParams->symbolName, mdInfoParams->SymbolName);
    assignCstringOffset(&infoParams->shortName, mdInfoParams->ShortName);
    assignCstringOffset(&infoParams->groupName, mdInfoParams->GroupName);
    assignCstringOffset(&infoParams->longName, mdInfoParams->LongName);
    assignUnaligned(&infoParams->apiMask, &mdInfoParams->ApiMask);

    auto status = getInformationType(infoParams->infoType, mdInfoParams->InfoType);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    assignCstringOffset(&infoParams->infoUnits, mdInfoParams->InfoUnits);
    status = assignEquation(infoParams->ioReadEquation, mdInfoParams->IoReadEquation);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = assignEquation(infoParams->queryReadEquation, mdInfoParams->QueryReadEquation);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = getDeltaFunction(infoParams->overflowFunction, mdInfoParams->OverflowFunction);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    return status;
}

ze_result_t MetricOaExporter01::assignIoMeaurementInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *ioMeasurementInformationOffset) {
    MetricsDiscovery::TConcurrentGroupParams_1_0 *mdConcGroupParams = concurrentGroup.GetParams();
    DEBUG_BREAK_IF(mdConcGroupParams == nullptr);
    const auto count = mdConcGroupParams->IoMeasurementInformationCount;
    auto [informationParamsBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_information_params_0_1_t>(count);
    zet_intel_metric_df_gpu_information_params_0_1_t paramsForSizeCalculation{};

    for (auto index = 0u; index < count; index++) {

        MetricsDiscovery::IInformation_1_0 *info = concurrentGroup.GetIoMeasurementInformation(index);
        DEBUG_BREAK_IF(info == nullptr);
        MetricsDiscovery::TInformationParams_1_0 *infoParams = info->GetParams();
        DEBUG_BREAK_IF(infoParams == nullptr);
        auto status = assignInformationParams(&paramsForSizeCalculation, infoParams);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        if (currOperationMode == OperationModeGetExportData) {
            assignUnaligned(&informationParamsBase[index], &paramsForSizeCalculation);
        }
    }

    assignUnaligned(ioMeasurementInformationOffset, &offset);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignIoGpuContextInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *ioGpuContextInformationOffset) {
    MetricsDiscovery::TConcurrentGroupParams_1_0 *mdConcGroupParams = concurrentGroup.GetParams();
    const auto count = mdConcGroupParams->IoGpuContextInformationCount;
    auto [informationParamsBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_information_params_0_1_t>(count);
    zet_intel_metric_df_gpu_information_params_0_1_t paramsForSizeCalculation{};

    for (auto index = 0u; index < count; index++) {
        MetricsDiscovery::IInformation_1_0 *info = concurrentGroup.GetIoGpuContextInformation(index);
        DEBUG_BREAK_IF(info == nullptr);
        MetricsDiscovery::TInformationParams_1_0 *infoParams = info->GetParams();
        DEBUG_BREAK_IF(infoParams == nullptr);
        auto status = assignInformationParams(&paramsForSizeCalculation, infoParams);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        if (currOperationMode == OperationModeGetExportData) {
            assignUnaligned(&informationParamsBase[index], &paramsForSizeCalculation);
        }
    }

    assignUnaligned(ioGpuContextInformationOffset, &offset);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignMetricSetInformationOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *metricSetInfoParamsOffset) {
    auto count = mdMetricSet.GetParams()->InformationCount;
    auto [informationParamsBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_information_params_0_1_t>(count);
    zet_intel_metric_df_gpu_information_params_0_1_t paramsForSizeCalculation{};
    for (auto index = 0u; index < count; index++) {

        MetricsDiscovery::IInformation_1_0 *info = mdMetricSet.GetInformation(index);
        DEBUG_BREAK_IF(info == nullptr);
        MetricsDiscovery::TInformationParams_1_0 *infoParams = info->GetParams();
        DEBUG_BREAK_IF(infoParams == nullptr);
        auto status = assignInformationParams(&paramsForSizeCalculation, infoParams);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        if (currOperationMode == OperationModeGetExportData) {
            assignUnaligned<zet_intel_metric_df_gpu_information_params_0_1_t>(&informationParamsBase[index], &paramsForSizeCalculation);
        }
    }
    assignUnaligned<zet_intel_metric_df_gpu_information_params_0_1_offset_t>(metricSetInfoParamsOffset, &offset);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::getMetricType(zet_intel_metric_df_gpu_metric_type_t &metricType, const MetricsDiscovery::TMetricType mdMetricType) {
    std::map<MetricsDiscovery::TMetricType, zet_intel_metric_df_gpu_metric_type_t> metricTypeMap = {
        {MetricsDiscovery::METRIC_TYPE_DURATION, ZET_INTEL_METRIC_DF_METRIC_TYPE_DURATION},
        {MetricsDiscovery::METRIC_TYPE_EVENT, ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT},
        {MetricsDiscovery::METRIC_TYPE_EVENT_WITH_RANGE, ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT_WITH_RANGE},
        {MetricsDiscovery::METRIC_TYPE_THROUGHPUT, ZET_INTEL_METRIC_DF_METRIC_TYPE_THROUGHPUT},
        {MetricsDiscovery::METRIC_TYPE_TIMESTAMP, ZET_INTEL_METRIC_DF_METRIC_TYPE_TIMESTAMP},
        {MetricsDiscovery::METRIC_TYPE_FLAG, ZET_INTEL_METRIC_DF_METRIC_TYPE_FLAG},
        {MetricsDiscovery::METRIC_TYPE_RATIO, ZET_INTEL_METRIC_DF_METRIC_TYPE_RATIO},
        {MetricsDiscovery::METRIC_TYPE_RAW, ZET_INTEL_METRIC_DF_METRIC_TYPE_RAW},
    };

    if (metricTypeMap.find(mdMetricType) == metricTypeMap.end()) {
        METRICS_LOG_ERR("Error: Unknown Metric Type 0x%x returning 0x%x",
                        static_cast<uint32_t>(mdMetricType), ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    assignUnaligned(&metricType, &metricTypeMap[mdMetricType]);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::getMetricResultType(zet_intel_metric_df_gpu_metric_result_type_t &resltType, const MetricsDiscovery::TMetricResultType mdResultType) {
    std::map<MetricsDiscovery::TMetricResultType, zet_intel_metric_df_gpu_metric_result_type_t> resultTypeMap = {
        {MetricsDiscovery::RESULT_UINT32, ZET_INTEL_METRIC_DF_RESULT_UINT32},
        {MetricsDiscovery::RESULT_UINT64, ZET_INTEL_METRIC_DF_RESULT_UINT64},
        {MetricsDiscovery::RESULT_BOOL, ZET_INTEL_METRIC_DF_RESULT_BOOL},
        {MetricsDiscovery::RESULT_FLOAT, ZET_INTEL_METRIC_DF_RESULT_FLOAT},
    };

    if (resultTypeMap.find(mdResultType) == resultTypeMap.end()) {
        METRICS_LOG_ERR("Error: Unknown Metric Result Type 0x%x returning 0x%x",
                        static_cast<uint32_t>(mdResultType), ZE_RESULT_ERROR_UNSUPPORTED_VERSION);
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    }

    assignUnaligned(&resltType, &resultTypeMap[mdResultType]);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricOaExporter01::assignMetricParamsOffset(zet_intel_metric_df_gpu_metric_params_0_1_offset_t *metricParamsOffset) {
    auto count = mdMetricSet.GetParams()->MetricsCount;
    auto [metricParamsBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_metric_params_0_1_t>(count);

    for (auto index = 0u; index < count; index++) {

        zet_intel_metric_df_gpu_metric_params_0_1_t paramsForSizeCalculation{};
        MetricsDiscovery::IMetric_1_0 *metric = mdMetricSet.GetMetric(index);
        DEBUG_BREAK_IF(metric == nullptr);
        MetricsDiscovery::TMetricParams_1_0 *mdMetricParams = metric->GetParams();
        DEBUG_BREAK_IF(mdMetricParams == nullptr);

        assignUnaligned(&paramsForSizeCalculation.idInSet, &mdMetricParams->IdInSet);
        assignCstringOffset(&paramsForSizeCalculation.symbolName, mdMetricParams->SymbolName);
        assignCstringOffset(&paramsForSizeCalculation.shortName, mdMetricParams->ShortName);
        assignCstringOffset(&paramsForSizeCalculation.groupName, mdMetricParams->GroupName);
        assignCstringOffset(&paramsForSizeCalculation.longName, mdMetricParams->LongName);
        assignUnaligned(&paramsForSizeCalculation.apiMask, &mdMetricParams->ApiMask);
        assignCstringOffset(&paramsForSizeCalculation.metricResultUnits, mdMetricParams->MetricResultUnits);
        assignUnaligned(&paramsForSizeCalculation.usageFlagsMask, &mdMetricParams->UsageFlagsMask);
        auto status = getMetricResultType(paramsForSizeCalculation.resultType, mdMetricParams->ResultType);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = getMetricType(paramsForSizeCalculation.metricType, mdMetricParams->MetricType);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = assignEquation(paramsForSizeCalculation.ioReadEquation, mdMetricParams->IoReadEquation);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = assignEquation(paramsForSizeCalculation.queryReadEquation, mdMetricParams->QueryReadEquation);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = getDeltaFunction(paramsForSizeCalculation.deltaFunction, mdMetricParams->DeltaFunction);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = assignEquation(paramsForSizeCalculation.normEquation, mdMetricParams->NormEquation);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        status = assignEquation(paramsForSizeCalculation.maxValueEquation, mdMetricParams->MaxValueEquation);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }

        if (currOperationMode == OperationModeGetExportData) {
            assignUnaligned<zet_intel_metric_df_gpu_metric_params_0_1_t>(&metricParamsBase[index], &paramsForSizeCalculation);
        }
    }

    assignUnaligned(metricParamsOffset, &offset);
    return ZE_RESULT_SUCCESS;
}

void MetricOaExporter01::updateConcurrentGroupParams(zet_intel_metric_df_gpu_concurrent_group_params_0_1_t *concGroupParams) {
    MetricsDiscovery::TConcurrentGroupParams_1_0 *mdConcGroupParams = concurrentGroup.GetParams();
    DEBUG_BREAK_IF(mdConcGroupParams == nullptr);
    assignCstringOffset(&concGroupParams->symbolName, mdConcGroupParams->SymbolName);
    assignCstringOffset(&concGroupParams->description, mdConcGroupParams->Description);
    assignUnaligned(&concGroupParams->ioMeasurementInformationCount, &mdConcGroupParams->IoMeasurementInformationCount);
    assignUnaligned(&concGroupParams->ioGpuContextInformationCount, &mdConcGroupParams->IoGpuContextInformationCount);
}

ze_result_t MetricOaExporter01::updateConcurrentGroup(zet_intel_metric_df_gpu_concurrent_group_0_1_t *concGroup) {
    updateConcurrentGroupParams(&concGroup->params);
    zet_intel_metric_df_gpu_information_params_0_1_offset_t offset;
    auto status = assignIoMeaurementInformationOffset(&offset);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    assignUnaligned(&concGroup->ioMeasurementInformation, &offset);
    status = assignIoGpuContextInformationOffset(&offset);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    assignUnaligned(&concGroup->ioGpuContextInformation, &offset);
    return status;
}

void MetricOaExporter01::updateMetricSetParams(zet_intel_metric_df_gpu_metric_set_params_0_1_t *params) {
    MetricsDiscovery::TMetricSetParams_1_0 *mdMetricParams = mdMetricSet.GetParams();
    DEBUG_BREAK_IF(mdMetricParams == nullptr);
    assignCstringOffset(&params->symbolName, mdMetricParams->SymbolName);
    assignCstringOffset(&params->shortName, mdMetricParams->ShortName);
    assignUnaligned(&params->apiMask, &mdMetricParams->ApiMask);
    assignUnaligned(&params->metricsCount, &mdMetricParams->MetricsCount);
    assignUnaligned(&params->informationCount, &mdMetricParams->InformationCount);

    MetricsDiscovery::TApiVersion_1_0 *apiVersion = &mdDevice.GetParams()->Version;
    DEBUG_BREAK_IF(apiVersion == nullptr);
    if (apiVersion->MajorNumber > 1 || (apiVersion->MajorNumber == 1 && apiVersion->MinorNumber >= 11)) {
        MetricsDiscovery::TMetricSetParams_1_11 *mdMetricSetParamsLatest = static_cast<MetricsDiscovery::TMetricSetParams_1_11 *>(mdMetricParams);
        assignCstringOffset(&params->availabilityEquation, mdMetricSetParamsLatest->AvailabilityEquation);
    } else {
        auto equation = ZET_INTEL_GPU_METRIC_INVALID_OFFSET;
        assignUnaligned(&params->availabilityEquation, &equation);
    }
}

ze_result_t MetricOaExporter01::updateMetricSet(zet_intel_metric_df_gpu_metric_set_0_1_t *metricSet) {
    updateMetricSetParams(&metricSet->params);
    auto status = assignMetricSetInformationOffset(&metricSet->informationParams);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    return assignMetricParamsOffset(&metricSet->metricParams);
}

ze_result_t MetricOaExporter01::updateAdapterParams(zet_intel_metric_df_gpu_adapter_params_0_1_t *adapterParams) {

    auto mdAdapterParams = const_cast<MetricsDiscovery::TAdapterParams_1_9 *>(adapter.GetParams());
    DEBUG_BREAK_IF(mdAdapterParams == nullptr);
    assignUnaligned(&adapterParams->busNumber, &mdAdapterParams->BusNumber);
    assignUnaligned(&adapterParams->deviceNumber, &mdAdapterParams->DeviceNumber);
    assignUnaligned(&adapterParams->functionNumber, &mdAdapterParams->FunctionNumber);
    assignUnaligned(&adapterParams->domainNumber, &mdAdapterParams->DomainNumber);
    return assignAdapterId(&adapterParams->systemId, &mdAdapterParams->SystemId);
}

ze_result_t MetricOaExporter01::updateGlobalSymbolOffsetAndValues(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t *globalSymbolsOffset) {

    MetricsDiscovery::TMetricsDeviceParams_1_2 *pMetricsDeviceParams = mdDevice.GetParams();
    DEBUG_BREAK_IF(pMetricsDeviceParams == nullptr);

    const auto globalSymbolsCount = pMetricsDeviceParams->GlobalSymbolsCount;
    auto [globalSymbolsBase, offset] = heapUsageTracker.allocate<zet_intel_metric_df_gpu_global_symbol_0_1_t>(globalSymbolsCount);
    zet_intel_metric_df_gpu_global_symbol_0_1_t symbolForSizeCalculation;
    zet_intel_metric_df_gpu_global_symbol_0_1_t *currSymbol = &symbolForSizeCalculation;
    for (uint32_t index = 0; index < globalSymbolsCount; index++) {
        if (currOperationMode != OperationModeGetHeapSize) {
            currSymbol = &globalSymbolsBase[index];
        }
        MetricsDiscovery::TGlobalSymbol_1_0 *symbol = mdDevice.GetGlobalSymbol(index);
        DEBUG_BREAK_IF(symbol == nullptr);
        assignCstringOffset(&currSymbol->symbolName, symbol->SymbolName);
        const auto status = assignTypedValue(&currSymbol->symbolTypedValue, symbol->SymbolTypedValue);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
    }
    assignUnaligned(globalSymbolsOffset, &offset);
    return ZE_RESULT_SUCCESS;
}

void MetricOaExporter01::updateMetricsDeviceParams(zet_intel_metric_df_gpu_metrics_device_params_0_1_t *deviceParams) {

    updateMetricApiVersion(&deviceParams->version);
    MetricsDiscovery::TMetricsDeviceParams_1_2 *pMetricsDeviceParams = mdDevice.GetParams();
    DEBUG_BREAK_IF(pMetricsDeviceParams == nullptr);
    assignUnaligned(&deviceParams->globalSymbolsCount, &pMetricsDeviceParams->GlobalSymbolsCount);
}

ze_result_t MetricOaExporter01::getExportData(zet_intel_metric_df_gpu_metric_oa_calc_0_1_t *oaCalcData) {
    updateMetricsDeviceParams(&oaCalcData->deviceParams);
    ze_result_t status = updateGlobalSymbolOffsetAndValues(&oaCalcData->globalSymbols);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = updateAdapterParams(&oaCalcData->adapterParams);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = updateConcurrentGroup(&oaCalcData->concurrentGroup);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    return updateMetricSet(&oaCalcData->metricSet);
}

ze_result_t OaMetricGroupImp::getExportDataHeapSize(size_t &exportDataHeapSize) {

    if (cachedExportDataHeapSize != 0) {
        exportDataHeapSize = cachedExportDataHeapSize;
        return ZE_RESULT_SUCCESS;
    }

    HeapUsageTracker memoryTracker(0, 0, HeapUsageTracker::OperationModeTrackOnly);

    OaMetricSourceImp *metricSource = getMetricSource();
    MetricsDiscovery::IMetricsDevice_1_5 *mdDevice = metricSource->getMetricEnumeration().getMdapiDevice();
    MetricsDiscovery::IAdapter_1_9 *mdAdapter = metricSource->getMetricEnumeration().getMdapiAdapter();

    MetricOaExporter01 exporter01(*mdDevice, *mdAdapter, *pReferenceMetricSet, *pReferenceConcurrentGroup, memoryTracker);
    zet_intel_metric_df_gpu_export_data_format_t exportData{};
    const auto status = exporter01.getExportData(&exportData.format01.oaData);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    cachedExportDataHeapSize = exportDataHeapSize = memoryTracker.getUsedBytes();
    return ZE_RESULT_SUCCESS;
}

ze_result_t OaMetricGroupImp::getExportData(const uint8_t *pRawData,
                                            size_t rawDataSize,
                                            size_t *pExportDataSize,
                                            uint8_t *pExportData) {
    if (metricGroups.size() != 0) {
        auto metricGroupSubDevice = MetricGroup::fromHandle(metricGroups[0]);
        return metricGroupSubDevice->getExportData(pRawData, rawDataSize, pExportDataSize, pExportData);
    }

    size_t requiredHeapSize = 0;
    ze_result_t status = getExportDataHeapSize(requiredHeapSize);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    const size_t expectedExportDataSize = sizeof(zet_intel_metric_df_gpu_export_data_format_t) + requiredHeapSize + rawDataSize;
    if (*pExportDataSize == 0u) {
        *pExportDataSize = expectedExportDataSize;
        return ZE_RESULT_SUCCESS;
    }

    if (*pExportDataSize < expectedExportDataSize) {
        METRICS_LOG_ERR("Error:Incorrect Size Passed returning 0x%x", ZE_RESULT_ERROR_INVALID_SIZE);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    uintptr_t startHeapAddress = reinterpret_cast<uintptr_t>(pExportData + sizeof(zet_intel_metric_df_gpu_export_data_format_t));
    uintptr_t endHeapAddress = startHeapAddress + requiredHeapSize;

    HeapUsageTracker memoryTracker(startHeapAddress, endHeapAddress);

    OaMetricSourceImp *metricSource = getMetricSource();
    MetricsDiscovery::IMetricsDevice_1_5 *mdDevice = metricSource->getMetricEnumeration().getMdapiDevice();
    MetricsDiscovery::IAdapter_1_9 *mdAdapter = metricSource->getMetricEnumeration().getMdapiAdapter();
    MetricOaExporter01 exporter01(*mdDevice, *mdAdapter, *pReferenceMetricSet, *pReferenceConcurrentGroup, memoryTracker);
    zet_intel_metric_df_gpu_export_data_format_t *exportData = reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(pExportData);

    // read and update the export data
    status = exporter01.getExportData(&exportData->format01.oaData);
    if (status != ZE_RESULT_SUCCESS) {
        METRICS_LOG_ERR("Error: ExportData_0_1 Failed returning 0x%x", status);
        return status;
    }

    DEBUG_BREAK_IF(memoryTracker.getUsedBytes() != requiredHeapSize);

    // Update header after updating the export data
    exportData->header.type = ZET_INTEL_METRIC_DF_SOURCE_TYPE_OA;
    exportData->header.version.major = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MAJOR;
    exportData->header.version.minor = ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MINOR;
    exportData->header.rawDataOffset = sizeof(zet_intel_metric_df_gpu_export_data_format_t) + memoryTracker.getUsedBytes();
    exportData->header.rawDataSize = rawDataSize;

    // Append the rawData
    memcpy_s(reinterpret_cast<void *>(endHeapAddress), expectedExportDataSize - exportData->header.rawDataOffset, pRawData, rawDataSize);

    return ZE_RESULT_SUCCESS;
}

} // namespace L0