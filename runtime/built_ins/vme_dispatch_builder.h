/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/accelerators/intel_accelerator.h"
#include "runtime/accelerators/intel_motion_estimation.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/helpers/dispatch_info_builder.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"

namespace NEO {
class VmeBuiltinDispatchInfoBuilder : public BuiltinDispatchInfoBuilder {
  public:
    VmeBuiltinDispatchInfoBuilder(BuiltIns &kernelsLib, Context &context, Device &device, EBuiltInOps::Type builtinOp,
                                  const char *kernelName)
        : BuiltinDispatchInfoBuilder(kernelsLib) {
        populate(context, device, builtinOp,
                 mediaKernelsBuildOptions,
                 kernelName, vmeKernel);
        widthArgNum = vmeKernel->getKernelInfo().getArgNumByName("width");
        heightArgNum = vmeKernel->getKernelInfo().getArgNumByName("height");
        strideArgNum = vmeKernel->getKernelInfo().getArgNumByName("stride");
        acceleratorArgNum = vmeKernel->getKernelInfo().getArgNumByName("accelerator");
        srcImgArgNum = vmeKernel->getKernelInfo().getArgNumByName("srcImg");
        refImgArgNum = vmeKernel->getKernelInfo().getArgNumByName("refImg");
        motionVectorBufferArgNum = vmeKernel->getKernelInfo().getArgNumByName("motion_vector_buffer");
        predictionMotionVectorBufferArgNum = vmeKernel->getKernelInfo().getArgNumByName("prediction_motion_vector_buffer");
        residualsArgNum = vmeKernel->getKernelInfo().getArgNumByName("residuals");
    }

    void getBlkTraits(const Vec3<size_t> &inGws, size_t &gwWidthInBlk, size_t &gwHeightInBlk) const {
        const size_t vmeMacroBlockWidth = 16;
        const size_t vmeMacroBlockHeight = 16;
        gwWidthInBlk = Math::divideAndRoundUp(inGws.x, vmeMacroBlockWidth);
        gwHeightInBlk = Math::divideAndRoundUp(inGws.y, vmeMacroBlockHeight);
    }

    bool buildDispatchInfos(MultiDispatchInfo &multiDispatchInfo, Kernel *kern,
                            const uint32_t inDim, const Vec3<size_t> &inGws, const Vec3<size_t> &inLws, const Vec3<size_t> &inOffset) const override {
        if (kern == nullptr) {
            return false;
        }

        size_t gwWidthInBlk = 0;
        size_t gwHeightInBlk = 0;
        getBlkTraits(inGws, gwWidthInBlk, gwHeightInBlk);

        cl_int height = (cl_int)gwHeightInBlk;
        cl_int width = (cl_int)gwWidthInBlk;
        cl_int stride = height;
        size_t numThreadsX = gwWidthInBlk;
        const size_t simdWidth = vmeKernel->getKernelInfo().getMaxSimdSize();
        stride = (height * width + (cl_int)numThreadsX - 1) / (cl_int)numThreadsX;

        // update implicit args
        vmeKernel->setArg(heightArgNum, sizeof(height), &height);
        vmeKernel->setArg(widthArgNum, sizeof(width), &width);
        vmeKernel->setArg(strideArgNum, sizeof(stride), &stride);

        // Update global work size to force macro-block to HW thread execution model
        Vec3<size_t> gws = {numThreadsX * simdWidth, 1, 1};
        Vec3<size_t> lws = {vmeKernel->getKernelInfo().reqdWorkGroupSize[0], 1, 1};

        DispatchInfoBuilder<SplitDispatch::Dim::d2D, SplitDispatch::SplitMode::NoSplit> builder;
        builder.setDispatchGeometry(gws, lws, inOffset, gws, lws);
        builder.setKernel(vmeKernel);
        builder.bake(multiDispatchInfo);
        return true;
    }

    bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
        DEBUG_BREAK_IF(!((argIndex != widthArgNum) && (argIndex != heightArgNum) && (argIndex != strideArgNum)));
        if ((argIndex == acceleratorArgNum) && (argVal == nullptr)) {
            err = CL_INVALID_ACCELERATOR_INTEL;
            return false;
        }
        err = vmeKernel->setArg(argIndex, argSize, argVal);
        return false;
    }

    cl_int validateDispatch(Kernel *kernel, uint32_t inworkDim, const Vec3<size_t> &inGws, const Vec3<size_t> &inLws, const Vec3<size_t> &inOffset) const override {
        if (inworkDim != 2) {
            return CL_INVALID_WORK_DIMENSION;
        }

        size_t gwWidthInBlk = 0;
        size_t gwHeightInBlk = 0;
        getBlkTraits(inGws, gwWidthInBlk, gwHeightInBlk);

        size_t BlkNum = gwWidthInBlk * gwHeightInBlk;
        size_t BlkMul = 1;
        IntelAccelerator *accelerator = castToObject<IntelAccelerator>((cl_accelerator_intel)vmeKernel->getKernelArg(acceleratorArgNum));
        if (accelerator == nullptr) {
            return CL_INVALID_KERNEL_ARGS; // accelerator was not set
        }
        DEBUG_BREAK_IF(accelerator->getDescriptorSize() != sizeof(cl_motion_estimation_desc_intel));
        const cl_motion_estimation_desc_intel *acceleratorDesc = reinterpret_cast<const cl_motion_estimation_desc_intel *>(accelerator->getDescriptor());
        switch (acceleratorDesc->mb_block_type) {
        case CL_ME_MB_TYPE_8x8_INTEL:
            BlkMul = 4;
            break;
        case CL_ME_MB_TYPE_4x4_INTEL:
            BlkMul = 16;
            break;
        default:
            break;
        }

        return validateVmeDispatch(inGws, inOffset, BlkNum, BlkMul);
    }

    // notes on corner cases :
    // * if arg not available in kernels - returns true
    // * if arg set to nullptr - returns true
    bool validateBufferSize(int32_t bufferArgNum, size_t minimumSizeExpected) const {
        if (bufferArgNum == -1) {
            return true;
        }

        auto buff = castToObject<Buffer>((cl_mem)vmeKernel->getKernelArg(bufferArgNum));
        if (buff == nullptr) {
            return true;
        }

        size_t bufferSize = buff->getSize();
        if (bufferSize < minimumSizeExpected) {
            return false;
        }

        return true;
    }

    template <typename EnumBaseType>
    bool validateEnumVal(EnumBaseType val) const {
        return false;
    }

    template <typename EnumBaseType, typename ExpectedValType, typename... ExpectedValsTypes>
    bool validateEnumVal(EnumBaseType val, ExpectedValType expectedVal, ExpectedValsTypes... expVals) const {
        return (val == static_cast<EnumBaseType>(expectedVal)) || validateEnumVal<EnumBaseType, ExpectedValsTypes...>(val, expVals...);
    }

    // notes on corner cases :
    // * if arg not available in kernels - returns true
    template <typename EnumBaseType, typename... ExpectedValsTypes>
    bool validateEnumArg(int32_t argNum, ExpectedValsTypes... expVals) const {
        if (argNum == -1) {
            return true;
        }

        EnumBaseType val = this->getKernelArgByValValue<EnumBaseType>(static_cast<uint32_t>(argNum));
        return validateEnumVal<EnumBaseType, ExpectedValsTypes...>(val, expVals...);
    }

    template <typename RetType>
    RetType getKernelArgByValValue(uint32_t argNum) const {
        auto &kai = vmeKernel->getKernelInfo().kernelArgInfo[argNum];
        DEBUG_BREAK_IF(kai.kernelArgPatchInfoVector.size() != 1);
        const KernelArgPatchInfo &patchInfo = kai.kernelArgPatchInfoVector[0];
        DEBUG_BREAK_IF(sizeof(RetType) > patchInfo.size);
        return *(RetType *)(vmeKernel->getCrossThreadData() + patchInfo.crossthreadOffset);
    }

    cl_int validateImages(Vec3<size_t> inputRegion, Vec3<size_t> offset) const {
        Image *srcImg = castToObject<Image>((cl_mem)vmeKernel->getKernelArg(srcImgArgNum));
        Image *refImg = castToObject<Image>((cl_mem)vmeKernel->getKernelArg(refImgArgNum));

        if ((srcImg == nullptr) || (refImg == nullptr)) {
            return CL_INVALID_KERNEL_ARGS;
        }

        for (Image *img : {srcImg, refImg}) {
            const cl_image_format &imgFormat = img->getImageFormat();
            if ((imgFormat.image_channel_order != CL_R) || (imgFormat.image_channel_data_type != CL_UNORM_INT8)) {
                return CL_INVALID_IMAGE_FORMAT_DESCRIPTOR;
            }

            if (false == img->isTiledAllocation()) {
                //VME only works with tiled images.
                return CL_OUT_OF_RESOURCES;
            }
        }

        {
            const cl_image_desc &srcImgDesc = srcImg->getImageDesc();

            size_t srcImageWidth = srcImgDesc.image_width;
            size_t srcImageHeight = srcImgDesc.image_height;
            if (((inputRegion.x + offset.x) > srcImageWidth) ||
                ((inputRegion.y + offset.y) > srcImageHeight)) {
                return CL_INVALID_IMAGE_SIZE;
            }
        }

        return CL_SUCCESS;
    }

    virtual cl_int validateVmeDispatch(Vec3<size_t> inputRegion, Vec3<size_t> offset, size_t blkNum, size_t blkMul) const {
        {
            cl_int imageValidationStatus = validateImages(inputRegion, offset);
            if (imageValidationStatus != CL_SUCCESS) {
                return imageValidationStatus;
            }
        }

        size_t numPredictors = 1;
        std::pair<int32_t, size_t> bufferRequirements[] = {
            std::make_pair(motionVectorBufferArgNum, (blkNum * blkMul * 2 * sizeof(cl_short))),
            std::make_pair(predictionMotionVectorBufferArgNum, (blkNum * numPredictors * 2 * sizeof(cl_short))),
            std::make_pair(residualsArgNum, (blkNum * blkMul * sizeof(cl_ushort)))};
        for (const auto &req : bufferRequirements) {
            if (false == validateBufferSize(req.first, req.second)) {
                return CL_INVALID_BUFFER_SIZE;
            }
        }

        return CL_SUCCESS;
    }

  protected:
    uint32_t heightArgNum;
    uint32_t widthArgNum;
    uint32_t strideArgNum;
    uint32_t acceleratorArgNum;
    uint32_t srcImgArgNum;
    uint32_t refImgArgNum;
    int32_t motionVectorBufferArgNum;
    int32_t predictionMotionVectorBufferArgNum;
    int32_t residualsArgNum;
    Kernel *vmeKernel;
};

template <>
class BuiltInOp<EBuiltInOps::VmeBlockMotionEstimateIntel> : public VmeBuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : VmeBuiltinDispatchInfoBuilder(kernelsLib, context, device,
                                        EBuiltInOps::VmeBlockMotionEstimateIntel, "block_motion_estimate_intel") {
    }
};

class AdvancedVmeBuiltinDispatchInfoBuilder : public VmeBuiltinDispatchInfoBuilder {
  public:
    AdvancedVmeBuiltinDispatchInfoBuilder(BuiltIns &kernelsLib, Context &context, Device &device, EBuiltInOps::Type builtinOp,
                                          const char *kernelName)
        : VmeBuiltinDispatchInfoBuilder(kernelsLib, context, device, builtinOp,
                                        kernelName) {
        flagsArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("flags");
        intraSrcImgArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("intraSrcImg");
        skipBlockTypeArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("skip_block_type");
        searchCostPenaltyArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("search_cost_penalty");
        searchCostPrecisionArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("search_cost_precision");
        bidirWeightArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("bidir_weight");
        predictorsBufferArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("predictors_buffer");
        countMotionVectorBufferArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("count_motion_vector_buffer");
        skipMotionVectorBufferArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("skip_motion_vector_buffer");
        intraSearchPredictorModesArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("intra_search_predictor_modes");
        skipResidualsArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("skip_residuals");
        intraResidualsArgNum = this->vmeKernel->getKernelInfo().getArgNumByName("intra_residuals");
    }

    bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
        DEBUG_BREAK_IF(argIndex == intraSrcImgArgNum);
        if (argIndex == this->srcImgArgNum) {
            // rebind also as media block image
            this->vmeKernel->setArg(intraSrcImgArgNum, argSize, argVal);
        }
        return VmeBuiltinDispatchInfoBuilder::setExplicitArg(argIndex, argSize, argVal, err);
    }

    virtual bool isBidirKernel() const {
        return false;
    }

    bool validateFlags(uint32_t &outSkipBlockType) const {
        uint32_t flagsVal = VmeBuiltinDispatchInfoBuilder::template getKernelArgByValValue<uint32_t>(flagsArgNum);

        if ((flagsVal & CL_ME_CHROMA_INTRA_PREDICT_ENABLED_INTEL) == CL_ME_CHROMA_INTRA_PREDICT_ENABLED_INTEL) {
            return false;
        }

        if (flagsVal == CL_ME_SKIP_BLOCK_TYPE_16x16_INTEL) {
            outSkipBlockType = CL_ME_MB_TYPE_16x16_INTEL;
        } else if ((flagsVal & CL_ME_SKIP_BLOCK_TYPE_8x8_INTEL) == CL_ME_SKIP_BLOCK_TYPE_8x8_INTEL) {
            outSkipBlockType = CL_ME_MB_TYPE_8x8_INTEL;
        }

        return true;
    }

    bool validateSkipBlockTypeArg(uint32_t &outSkipBlockType) const {
        if (skipBlockTypeArgNum == -1) {
            return true;
        }

        outSkipBlockType = VmeBuiltinDispatchInfoBuilder::template getKernelArgByValValue<uint32_t>(static_cast<uint32_t>(skipBlockTypeArgNum));

        switch (outSkipBlockType) {
        case CL_ME_MB_TYPE_16x16_INTEL:
            break;
        case CL_ME_MB_TYPE_8x8_INTEL:
            break;
        default:
            return false;
            ;
        }
        return true;
    }

    size_t getIntraSearchPredictorModesBuffExpSize(size_t blkNum) const {
        // vector size is 22 - 1 (16x16 luma block) +  4 (8x8 luma block) + 16 (4x4 luma block) + 1 (8x8 chroma block)
        int VectorSize = 22;
        size_t intraSearchPredictorModesBuffExpSize = blkNum * VectorSize;
        return intraSearchPredictorModesBuffExpSize;
    }

    size_t getSkipMotionVectorBufferExpSize(uint32_t skipBlockType, size_t blkNum) const {
        // vector size is either 1 (16x16 block) or 4 (8x8 block)
        // 0 to 8 skip MVs per MB
        // may be null if all MBs in frame have 0 skip check MVs in which case VME skip checks are not performed
        // layout assumes 4 (for bidir) or 8 (otherwise) skip check MVs per MB
        // row-major block layout; all MVs for a block are contiguous
        // buffer size depends on the block and frame size .
        int vectorSize = (skipBlockType == CL_ME_MB_TYPE_16x16_INTEL) ? 1 : 4;
        int numChecks = (isBidirKernel() ? 4 : 8);
        size_t skipMotionVectorBufferExpSize = blkNum * numChecks * vectorSize * 2 * sizeof(cl_short);
        return skipMotionVectorBufferExpSize;
    }

    size_t getSkipResidualsBuffExpSize(uint32_t skipBlockType, size_t blkNum) const {
        /*  output buffer of vectors of unsigned short SAD adjusted values corresponding to the input skip check MVs
            may be null if skip_motion_vector_buffer is null
            vector size is either 1 (16x16 block) or 4 (8x8 block)
            0 to 8 skip check residuals per MB
            layout always assumes 8 skip check residuals per MB
            row major block layout; all MVs for a block are contiguous
            buffer size depends on the block and frame size  */
        int vectorSize = 1;
        switch (skipBlockType) {
        case CL_ME_MB_TYPE_16x16_INTEL:
            vectorSize = 1;
            break;
        case CL_ME_MB_TYPE_8x8_INTEL:
            vectorSize = 4;
            break;
        default:
            break;
        };

        int numChecks = (isBidirKernel() ? 4 : 8);
        size_t skipResidualsBuffExpSize = blkNum * vectorSize * numChecks * sizeof(cl_ushort);
        return skipResidualsBuffExpSize;
    }

    size_t getIntraResidualsBuffExpSize(size_t blkNum) const {
        /*  output buffer of vectors of  unsigned short SAD adjusted values
            may be null in which case the intra residuals corresponding not returned
            vector size is 4 - 1 (16x16 luma block) +  1 (8x8 luma block) + 1 (4x4  luma block) + 1 (8x8 chroma block)
            1 vector per MB
            buffer size depends on the frame size  */
        int vectorSize = 4;
        size_t intraResidualsBuffExpSize = (blkNum * sizeof(cl_ushort) * vectorSize);
        return intraResidualsBuffExpSize;
    }

    size_t getPredictorsBufferExpSize(size_t blkNum) const {
        size_t numPredictors = 8;
        size_t predictorsBufferExpSize = (blkNum * numPredictors * 2 * sizeof(cl_short));
        return predictorsBufferExpSize;
    }

    cl_int validateVmeDispatch(Vec3<size_t> inputRegion, Vec3<size_t> offset, size_t blkNum, size_t blkMul) const override {
        cl_int basicVmeValidationStatus = VmeBuiltinDispatchInfoBuilder::validateVmeDispatch(inputRegion, offset, blkNum, blkMul);
        if (basicVmeValidationStatus != CL_SUCCESS) {
            return basicVmeValidationStatus;
        }

        uint32_t skipBlockType = CL_ME_MB_TYPE_16x16_INTEL;
        if (false == validateFlags(skipBlockType)) {
            return CL_INVALID_KERNEL_ARGS;
        }

        if (false == validateSkipBlockTypeArg(skipBlockType)) {
            return CL_OUT_OF_RESOURCES;
        }

        if (false == VmeBuiltinDispatchInfoBuilder::template validateEnumArg<uint32_t>(searchCostPenaltyArgNum, CL_ME_COST_PENALTY_NONE_INTEL, CL_ME_COST_PENALTY_LOW_INTEL, CL_ME_COST_PENALTY_NORMAL_INTEL,
                                                                                       CL_ME_COST_PENALTY_HIGH_INTEL)) {
            return CL_OUT_OF_RESOURCES;
        }

        if (false == VmeBuiltinDispatchInfoBuilder::template validateEnumArg<uint32_t>(searchCostPrecisionArgNum, CL_ME_COST_PRECISION_QPEL_INTEL, CL_ME_COST_PRECISION_HPEL_INTEL, CL_ME_COST_PRECISION_PEL_INTEL,
                                                                                       CL_ME_COST_PRECISION_DPEL_INTEL)) {
            return CL_OUT_OF_RESOURCES;
        }

        if (false == VmeBuiltinDispatchInfoBuilder::template validateEnumArg<uint8_t>(bidirWeightArgNum, 0, CL_ME_BIDIR_WEIGHT_QUARTER_INTEL, CL_ME_BIDIR_WEIGHT_THIRD_INTEL, CL_ME_BIDIR_WEIGHT_HALF_INTEL,
                                                                                      CL_ME_BIDIR_WEIGHT_TWO_THIRD_INTEL, CL_ME_BIDIR_WEIGHT_THREE_QUARTER_INTEL)) {
            return CL_INVALID_KERNEL_ARGS;
        }

        std::pair<int32_t, size_t> bufferRequirements[] = {
            std::make_pair(countMotionVectorBufferArgNum, (blkNum * 2 * sizeof(cl_short))),
            std::make_pair(skipMotionVectorBufferArgNum, getSkipMotionVectorBufferExpSize(skipBlockType, blkNum)),
            std::make_pair(intraSearchPredictorModesArgNum, getIntraSearchPredictorModesBuffExpSize(blkNum)),
            std::make_pair(skipResidualsArgNum, getSkipResidualsBuffExpSize(skipBlockType, blkNum)),
            std::make_pair(intraResidualsArgNum, getIntraResidualsBuffExpSize(blkNum)),
            std::make_pair(predictorsBufferArgNum, getPredictorsBufferExpSize(blkNum))};
        for (const auto &req : bufferRequirements) {
            if (false == this->validateBufferSize(req.first, req.second)) {
                return CL_INVALID_BUFFER_SIZE;
            }
        }

        return CL_SUCCESS;
    }

  protected:
    uint32_t flagsArgNum;
    int32_t skipBlockTypeArgNum;
    uint32_t searchCostPenaltyArgNum;
    uint32_t searchCostPrecisionArgNum;
    int32_t bidirWeightArgNum;
    int32_t predictorsBufferArgNum;
    uint32_t countMotionVectorBufferArgNum;
    uint32_t skipMotionVectorBufferArgNum;
    uint32_t intraSearchPredictorModesArgNum;
    uint32_t skipResidualsArgNum;
    uint32_t intraResidualsArgNum;
    uint32_t intraSrcImgArgNum;
};

template <>
class BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel> : public AdvancedVmeBuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : AdvancedVmeBuiltinDispatchInfoBuilder(kernelsLib, context, device, EBuiltInOps::VmeBlockAdvancedMotionEstimateCheckIntel,
                                                "block_advanced_motion_estimate_check_intel") {
    }

    cl_int validateVmeDispatch(Vec3<size_t> inputRegion, Vec3<size_t> offset,
                               size_t gwWidthInBlk, size_t gwHeightInBlk) const override {
        cl_int basicAdvVmeValidationStatus = AdvancedVmeBuiltinDispatchInfoBuilder::validateVmeDispatch(inputRegion, offset, gwWidthInBlk, gwHeightInBlk);
        if (basicAdvVmeValidationStatus != CL_SUCCESS) {
            return basicAdvVmeValidationStatus;
        }

        auto countMotionVectorBuff = castToObject<Buffer>((cl_mem)this->vmeKernel->getKernelArg(this->countMotionVectorBufferArgNum));
        if (countMotionVectorBuff == nullptr) {
            return CL_INVALID_BUFFER_SIZE;
        }

        return CL_SUCCESS;
    }
};

template <>
class BuiltInOp<EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel> : public AdvancedVmeBuiltinDispatchInfoBuilder {
  public:
    BuiltInOp(BuiltIns &kernelsLib, Context &context, Device &device)
        : AdvancedVmeBuiltinDispatchInfoBuilder(kernelsLib, context, device, EBuiltInOps::VmeBlockAdvancedMotionEstimateBidirectionalCheckIntel,
                                                "block_advanced_motion_estimate_bidirectional_check_intel") {
    }

    bool isBidirKernel() const override {
        return true;
    }
};
} // namespace NEO
