/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"
#include "level_zero/zet_intel_gpu_metric_export.h"

#include <map>

constexpr uint32_t indentLevelIncrement = 5;

#define SHOW(indent) LOG(LogLevel::INFO) << std::string(indent, ' ')
#define SHOW_CSTRING_OFFSET(parameter, indent) \
    {                                          \
        SHOW(indent) << #parameter << " : ";   \
        showCstringOffset(parameter, 1);       \
        SHOW(indent) << "\n";                  \
    }

#define SHOW_EQUATION(parameter, indent)       \
    {                                          \
        SHOW(indent) << #parameter << " : \n"; \
        showEquation(parameter, indent);       \
        SHOW(indent) << "\n";                  \
    }

#define SHOW_UINT32_T(parameter, indent)                               \
    {                                                                  \
        SHOW(indent) << #parameter << " : " << parameter << std::endl; \
    }

namespace ZelloMetricsUtility {

class MetricOaExporter01Logger {

    uintptr_t startAddress;
    MetricOaExporter01Logger(){};

    void showMetricApiVersion(zet_intel_metric_df_gpu_apiversion_0_1_t *apiVersion, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        indent += indentLevelIncrement;

        SHOW_UINT32_T(apiVersion->majorNumber, indent);
        SHOW_UINT32_T(apiVersion->minorNumber, indent);
        SHOW_UINT32_T(apiVersion->buildNumber, indent);
    }

    void showCstringOffset(cstring_offset_t *cStringOffset, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;
        auto cString = zet_intel_metric_df_gpu_offset_to_ptr(cstring_offset_t, *cStringOffset, startAddress);
        if (cString != nullptr) {
            SHOW(indent) << std::string(cString);
        }
    }

    void showByteArray(zet_intel_metric_df_gpu_byte_array_0_1_t *byteArray, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;
        uint8_t *baseAddr = zet_intel_metric_df_gpu_offset_to_ptr(uint8_offset_t, byteArray->data, startAddress);
        for (auto index = 0u; index < byteArray->size; index++) {
            SHOW(indent) << "[" << index << "]:" << static_cast<uint32_t>(baseAddr[index]) << std::endl;
        }
    }

    void showTypedValue(zet_intel_metric_df_gpu_typed_value_0_1_t *typedValue, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        SHOW(indent) << "typedValue->valueType: ";
        switch (typedValue->valueType) {
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32 | " << typedValue->valueUInt32 << std::endl;
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64 | " << typedValue->valueUInt64 << std::endl;
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT  | " << typedValue->valueFloat << std::endl;
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL  | " << typedValue->valueBool << std::endl;
            break;
        case ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING | ";
            SHOW_CSTRING_OFFSET(&typedValue->valueCString, indent);
            break;

        case ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY | "
                         << "\n";
            showByteArray(&typedValue->valueByteArray, indent);
            SHOW(indent) << std::endl;
            break;

        default:
            EXPECT(0);
            break;
        }
    }

    void showAdapterId(zet_intel_metric_df_gpu_adapter_id_0_1_t *adapterId, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        indent += indentLevelIncrement;

        switch (adapterId->type) {
        case ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_LUID:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_LUID | ";
            SHOW(indent) << "adapterId->luid.lowPart : " << adapterId->luid.lowPart << " | ";
            SHOW(indent) << "adapterId->luid.highPart : " << adapterId->luid.highPart << std::endl;
            break;

        case ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_MAJOR_MINOR:
            SHOW(indent) << "ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_MAJOR_MINOR | ";
            SHOW(indent) << "adapterId->majorMinor.major : " << adapterId->majorMinor.major << " | ";
            SHOW(indent) << "adapterId->majorMinor.minor : " << adapterId->majorMinor.minor << std::endl;
            break;

        default:
            EXPECT(0);
            break;
        }
    }

    void showEquationOperation(zet_intel_metric_df_gpu_equation_operation_t &equationOperation, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        std::map<zet_intel_metric_df_gpu_equation_operation_t, std::string> equationOperationMap = {
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT"},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_LSHIFT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_LSHIFT"},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_AND, "ZET_INTEL_METRIC_DF_EQUATION_OPER_AND   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_OR, "ZET_INTEL_METRIC_DF_EQUATION_OPER_OR    "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_XOR, "ZET_INTEL_METRIC_DF_EQUATION_OPER_XOR   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_XNOR, "ZET_INTEL_METRIC_DF_EQUATION_OPER_XNOR  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_AND_L, "ZET_INTEL_METRIC_DF_EQUATION_OPER_AND_L "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_EQUALS, "ZET_INTEL_METRIC_DF_EQUATION_OPER_EQUALS"},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UADD, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UADD  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_USUB, "ZET_INTEL_METRIC_DF_EQUATION_OPER_USUB  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UMUL, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UMUL  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UDIV, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UDIV  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FADD, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FADD  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FSUB, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FSUB  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FMUL, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FMUL  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FDIV, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FDIV  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UGT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UGT   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_ULT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_ULT   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UGTE, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UGTE  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_ULTE, "ZET_INTEL_METRIC_DF_EQUATION_OPER_ULTE  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FGT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FGT   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FLT, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FLT   "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FGTE, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FGTE  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FLTE, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FLTE  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UMIN, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UMIN  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_UMAX, "ZET_INTEL_METRIC_DF_EQUATION_OPER_UMAX  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FMIN, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FMIN  "},
            {ZET_INTEL_METRIC_DF_EQUATION_OPER_FMAX, "ZET_INTEL_METRIC_DF_EQUATION_OPER_FMAX  "},
        };

        if (equationOperationMap.find(equationOperation) == equationOperationMap.end()) {
            EXPECT(0);
            return;
        }
        SHOW(indent) << "equationOperation : " << equationOperationMap[equationOperation];
    }

    void showEquation(zet_intel_metric_df_gpu_equation_0_1_t &equation, uint32_t indent = 1) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        if (equation.elementCount == 0) {
            SHOW(indent) << " equation.elementCount == 0" << std::endl;
            return;
        }

        SHOW(indent) << " equation.elementCount :" << equation.elementCount << std::endl;
        auto elementBase = zet_intel_metric_df_gpu_offset_to_ptr(
            zet_intel_metric_df_gpu_equation_element_0_1_offset_t,
            equation.elements,
            startAddress);
        for (auto elemIdx = 0u; elemIdx < equation.elementCount; elemIdx++) {

            zet_intel_metric_df_gpu_equation_element_0_1_t *currElement = &elementBase[elemIdx];

            auto tabbedIndent = indent + 5;

            switch (currElement->type) {
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION : " << std::endl;
                showEquationOperation(currElement->operation, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.bitOffset: " << currElement->readParams.bitOffset << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.bitsCount: " << currElement->readParams.bitsCount << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffset: " << currElement->readParams.byteOffset << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->readParams.byteOffsetExt: " << currElement->readParams.byteOffsetExt << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->immediateUInt64 : " << currElement->immediateUInt64 << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT: " << std::endl;
                SHOW(tabbedIndent + 5) << " currElement->immediateFloat : " << currElement->immediateFloat << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE: " << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL: " << std::endl;
                showEquationOperation(currElement->operation, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL: " << std::endl;
                showEquationOperation(currElement->operation, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL: " << std::endl;
                showEquationOperation(currElement->operation, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL: " << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL: " << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL: " << std::endl;
                SHOW_CSTRING_OFFSET(&currElement->symbolName, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION: " << std::endl;
                break;
            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION: " << std::endl;
                break;

            case ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK:
                SHOW(tabbedIndent) << "ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK: " << std::endl;
                showByteArray(&currElement->mask, tabbedIndent + 5);
                SHOW(tabbedIndent) << std::endl;
                break;

            default:
                EXPECT(0);
                break;
            }
        }
    }

    void showDeltaFunction(zet_intel_metric_df_gpu_delta_function_0_1_t &deltaFunction, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        SHOW(indent) << "deltaFunction.bitsCount: " << deltaFunction.bitsCount << std::endl;

        std::map<zet_intel_metric_df_gpu_delta_function_type_t, std::string> deltaFunctionMap = {
            {ZET_INTEL_METRIC_DF_DELTA_FUNCTION_NULL, "ZET_INTEL_METRIC_DF_DELTA_FUNCTION_NULL "},
            {ZET_INTEL_METRIC_DF_DELTA_N_BITS, "ZET_INTEL_METRIC_DF_DELTA_N_BITS        "},
            {ZET_INTEL_METRIC_DF_DELTA_BOOL_OR, "ZET_INTEL_METRIC_DF_DELTA_BOOL_OR       "},
            {ZET_INTEL_METRIC_DF_DELTA_BOOL_XOR, "ZET_INTEL_METRIC_DF_DELTA_BOOL_XOR      "},
            {ZET_INTEL_METRIC_DF_DELTA_GET_PREVIOUS, "ZET_INTEL_METRIC_DF_DELTA_GET_PREVIOUS  "},
            {ZET_INTEL_METRIC_DF_DELTA_GET_LAST, "ZET_INTEL_METRIC_DF_DELTA_GET_LAST      "},
            {ZET_INTEL_METRIC_DF_DELTA_NS_TIME, "ZET_INTEL_METRIC_DF_DELTA_NS_TIME       "},
        };

        if (deltaFunctionMap.find(deltaFunction.functionType) == deltaFunctionMap.end()) {
            EXPECT(0);
        }
        SHOW(indent) << "deltaFunction.functionType :" << deltaFunctionMap[deltaFunction.functionType] << std::endl;
    }

    ze_result_t showInformationType(zet_intel_metric_df_gpu_information_type_t &infoType, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;
        std::map<zet_intel_metric_df_gpu_information_type_t, std::string> informationTypeMap = {
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_REPORT_REASON, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_REPORT_REASON "},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_VALUE, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_VALUE         "},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_FLAG, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_FLAG          "},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_TIMESTAMP, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_TIMESTAMP     "},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_CONTEXT_ID_TAG, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_CONTEXT_ID_TAG"},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_SAMPLE_PHASE, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_SAMPLE_PHASE  "},
            {ZET_INTEL_METRIC_DF_INFORMATION_TYPE_GPU_NODE, "ZET_INTEL_METRIC_DF_INFORMATION_TYPE_GPU_NODE      "}};

        if (informationTypeMap.find(infoType) == informationTypeMap.end()) {
            EXPECT(0);
        }
        SHOW(indent) << "infoType :" << informationTypeMap[infoType] << std::endl;
        return ZE_RESULT_SUCCESS;
    }

    void showInformationParams(zet_intel_metric_df_gpu_information_params_0_1_t *infoParams, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        SHOW(indent) << "infoParams->idInSet :" << infoParams->idInSet << std::endl;
        SHOW_CSTRING_OFFSET(&infoParams->symbolName, indent);
        SHOW_CSTRING_OFFSET(&infoParams->shortName, indent);
        SHOW_CSTRING_OFFSET(&infoParams->groupName, indent);
        SHOW_CSTRING_OFFSET(&infoParams->longName, indent);
        SHOW(indent) << "infoParams->apiMask : " << infoParams->apiMask << std::endl;
        SHOW(indent) << "infoParams->infoType : ";
        showInformationType(infoParams->infoType, indent);
        SHOW(indent) << std::endl;
        SHOW_CSTRING_OFFSET(&infoParams->infoUnits, indent);
        SHOW_EQUATION(infoParams->ioReadEquation, indent);
        SHOW_EQUATION(infoParams->queryReadEquation, indent);
        SHOW(indent) << "infoParams->overflowFunction : ";
        showDeltaFunction(infoParams->overflowFunction, indent);
        SHOW(indent) << std::endl;
    }

    void showInformationParamsOffset(zet_intel_metric_df_gpu_information_params_0_1_offset_t *informationParamsOffset, uint32_t count, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        auto informationParamsBase = zet_intel_metric_df_gpu_offset_to_ptr(
            zet_intel_metric_df_gpu_information_params_0_1_offset_t,
            *informationParamsOffset,
            startAddress);
        zet_intel_metric_df_gpu_information_params_0_1_t *currInformationParams;

        for (auto index = 0u; index < count; index++) {
            currInformationParams = &informationParamsBase[index];
            showInformationParams(currInformationParams, indent);
        }
    }

    ze_result_t showMetricType(zet_intel_metric_df_gpu_metric_type_t &metricType, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        std::map<zet_intel_metric_df_gpu_metric_type_t, std::string> metricTypeMap = {
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_DURATION, "ZET_INTEL_METRIC_DF_METRIC_TYPE_DURATION"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT, "ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT_WITH_RANGE, "ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT_WITH_RANGE"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_THROUGHPUT, "ZET_INTEL_METRIC_DF_METRIC_TYPE_THROUGHPUT"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_TIMESTAMP, "ZET_INTEL_METRIC_DF_METRIC_TYPE_TIMESTAMP"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_FLAG, "ZET_INTEL_METRIC_DF_METRIC_TYPE_FLAG"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_RATIO, "ZET_INTEL_METRIC_DF_METRIC_TYPE_RATIO"},
            {ZET_INTEL_METRIC_DF_METRIC_TYPE_RAW, "ZET_INTEL_METRIC_DF_METRIC_TYPE_RAW"},
        };

        if (metricTypeMap.find(metricType) == metricTypeMap.end()) {
            EXPECT(0);
        }

        SHOW(indent) << "MetricType: " << metricTypeMap[metricType] << std::endl;
        return ZE_RESULT_SUCCESS;
    }

    void showMetricParamsOffset(zet_intel_metric_df_gpu_metric_params_0_1_offset_t *metricParamsOffset, uint32_t count, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;
        auto metricParamsBase = zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_metric_params_0_1_offset_t, *metricParamsOffset, startAddress);
        for (auto index = 0u; index < count; index++) {

            SHOW(indent) << "[" << index << "]:" << std::endl;
            auto tabbedIndent = indent + 5;
            auto currMetricParams = &metricParamsBase[index];

            SHOW_UINT32_T(currMetricParams->idInSet, tabbedIndent);
            SHOW_CSTRING_OFFSET(&currMetricParams->symbolName, tabbedIndent);
            SHOW_CSTRING_OFFSET(&currMetricParams->shortName, tabbedIndent);
            SHOW_CSTRING_OFFSET(&currMetricParams->groupName, tabbedIndent);
            SHOW_CSTRING_OFFSET(&currMetricParams->longName, tabbedIndent);

            SHOW(tabbedIndent) << "currMetricParams->apiMask: " << currMetricParams->apiMask << std::endl;

            SHOW_CSTRING_OFFSET(&currMetricParams->metricResultUnits, tabbedIndent);
            SHOW(tabbedIndent) << "currMetricParams->metricType: ";
            showMetricType(currMetricParams->metricType, 1);

            SHOW_EQUATION(currMetricParams->ioReadEquation, tabbedIndent);
            SHOW_EQUATION(currMetricParams->queryReadEquation, tabbedIndent);

            SHOW(tabbedIndent) << "currMetricParams->deltaFunction : ";
            showDeltaFunction(currMetricParams->deltaFunction, tabbedIndent);
            SHOW(tabbedIndent) << std::endl;

            SHOW_EQUATION(currMetricParams->normEquation, tabbedIndent);
            SHOW_EQUATION(currMetricParams->maxValueEquation, tabbedIndent);
        }
    }

    void showConcurrentGroupParams(zet_intel_metric_df_gpu_concurrent_group_params_0_1_t *concGroupParams, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        SHOW_CSTRING_OFFSET(&concGroupParams->symbolName, indent);
        SHOW_CSTRING_OFFSET(&concGroupParams->description, indent);
        SHOW_UINT32_T(concGroupParams->ioMeasurementInformationCount, indent);
        SHOW_UINT32_T(concGroupParams->ioGpuContextInformationCount, indent);
    }

    void showConcurrentGroup(zet_intel_metric_df_gpu_concurrent_group_0_1_t *concGroup, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        indent += indentLevelIncrement;

        showConcurrentGroupParams(&concGroup->params, indent);
        showInformationParamsOffset(&concGroup->ioMeasurementInformation, concGroup->params.ioMeasurementInformationCount, indent);
        showInformationParamsOffset(&concGroup->ioGpuContextInformation, concGroup->params.ioGpuContextInformationCount, indent);
    }

    void showMetricSetParams(zet_intel_metric_df_gpu_metric_set_params_0_1_t *params, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        SHOW_CSTRING_OFFSET(&params->symbolName, indent);
        SHOW_CSTRING_OFFSET(&params->shortName, indent);
        SHOW_UINT32_T(params->apiMask, indent);
        SHOW_UINT32_T(params->metricsCount, indent);
        SHOW_UINT32_T(params->informationCount, indent);
        if (params->availabilityEquation == 0) {
            SHOW(indent) << "params->availabilityEquation : nullptr \n";
        } else {
            SHOW_CSTRING_OFFSET(&params->availabilityEquation, indent);
        }
    }

    void showMetricSet(zet_intel_metric_df_gpu_metric_set_0_1_t *metricSet, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        showMetricSetParams(&metricSet->params, indent);
        showInformationParamsOffset(&metricSet->informationParams, metricSet->params.informationCount, indent);
        showMetricParamsOffset(&metricSet->metricParams, metricSet->params.metricsCount, indent);
    }

    void showAdapterParams(zet_intel_metric_df_gpu_adapter_params_0_1_t *adapterParams, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        indent += indentLevelIncrement;

        SHOW_UINT32_T(adapterParams->busNumber, indent);
        SHOW_UINT32_T(adapterParams->deviceNumber, indent);
        SHOW_UINT32_T(adapterParams->functionNumber, indent);
        SHOW_UINT32_T(adapterParams->domainNumber, indent);
        showAdapterId(&adapterParams->systemId, indent);
    }

    void showGlobalSymbolOffsetAndValues(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t *globalSymbolsOffset, uint32_t globalSymbolsCount, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        indent += indentLevelIncrement;
        zet_intel_metric_df_gpu_global_symbol_0_1_t *globalSymbolsBase =
            zet_intel_metric_df_gpu_offset_to_ptr(zet_intel_metric_df_gpu_global_symbol_0_1_offset_t, *globalSymbolsOffset, startAddress);
        for (uint32_t index = 0; index < globalSymbolsCount; index++) {
            auto currSymbol = &globalSymbolsBase[index];

            SHOW_CSTRING_OFFSET(&currSymbol->symbolName, indent);
            showTypedValue(&currSymbol->symbolTypedValue, indent);
        }
    }

    void showMetricsDeviceParams(zet_intel_metric_df_gpu_metrics_device_params_0_1_t *deviceParams, uint32_t indent) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        showMetricApiVersion(&deviceParams->version, indent);
        SHOW_UINT32_T(deviceParams->globalSymbolsCount, indent);
    }

  public:
    MetricOaExporter01Logger(uintptr_t startAddress) : startAddress(startAddress) {}

    void showExportData(zet_intel_metric_df_gpu_metric_oa_calc_0_1_t *oaCalcData) {
        LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

        showMetricsDeviceParams(&oaCalcData->deviceParams, indentLevelIncrement);
        showGlobalSymbolOffsetAndValues(&oaCalcData->globalSymbols, oaCalcData->deviceParams.globalSymbolsCount, 3);
        showAdapterParams(&oaCalcData->adapterParams, indentLevelIncrement);
        showConcurrentGroup(&oaCalcData->concurrentGroup, indentLevelIncrement);
        showMetricSet(&oaCalcData->metricSet, indentLevelIncrement);
    }
};

static void showExportHeader(zet_intel_metric_df_gpu_header_t &header, uint32_t indent = 1) {
    LOG(LogLevel::DEBUG) << "@line: " << __LINE__ << " of function " << __FUNCTION__ << std::endl;

    std::map<zet_intel_metric_df_gpu_metric_source_type_t, std::string> sourceTypeMap =
        {
            {ZET_INTEL_METRIC_DF_SOURCE_TYPE_OA, "ZET_INTEL_METRIC_DF_SOURCE_TYPE_OA"}};

    if (sourceTypeMap.find(header.type) == sourceTypeMap.end()) {
        EXPECT(0);
        return;
    }
    SHOW(indent) << "sourcetype : " << sourceTypeMap[header.type] << "\n";
    SHOW_UINT32_T(header.version.major, indent);
    SHOW_UINT32_T(header.version.minor, indent);
    SHOW_UINT32_T(header.rawDataOffset, indent);
    SHOW_UINT32_T(header.rawDataSize, indent);
}

void showMetricsExportData(uint8_t *pExportData, size_t exportDataSize) {

    zet_intel_metric_df_gpu_export_data_format_t *exportData =
        reinterpret_cast<zet_intel_metric_df_gpu_export_data_format_t *>(pExportData);

    showExportHeader(exportData->header);
    MetricOaExporter01Logger exportLogger10(reinterpret_cast<uintptr_t>(pExportData));
    exportLogger10.showExportData(&exportData->format01.oaData);
}

} // namespace ZelloMetricsUtility