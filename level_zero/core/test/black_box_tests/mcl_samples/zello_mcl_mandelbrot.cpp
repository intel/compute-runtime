/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/black_box_tests/mcl_samples/zello_mcl_test_helper.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>

namespace MclTestMandelbrot {
const char *mandelbrotModuleSource = R"OpenCLC(
float2 getCoords(float2 start, float2 end, int2 pixel, int2 size) {
    float2 scale=(float2)(fabs(start.x - end.x), fabs(start.y - end.y));

    float2 coords;
    coords.x = (float)pixel.x / (float)size.x;
    coords.x = coords.x * scale.x + start.x;

    coords.y = (float)pixel.y / (float)size.y;
    coords.y = coords.y * scale.y + start.y;

    return coords;
}

float2 mandelbrotF(float2 z, const float2 c) {
    return (float2)(z.x * z.x - z.y * z.y + c.x, 2 * z.x * z.y + c.y);
}

__kernel void mandelbrot(__global uchar4 *image, float2 start, float2 end) {
    int2 pixelPos = (int2)(get_global_id(0), get_global_id(1));
    int2 size = (int2)(get_global_size(0), get_global_size(1));

    const uint maxIterations = 32;
    const float2 c = getCoords(start, end, pixelPos, size);
    float2 z = (float2)(0.0f, 0.0f);

    uint i = 0;
    while(i < maxIterations && z.x * z.x + z.y * z.y < 4.0f) { // |z|<2 -> R^2+I^2<4
        z = mandelbrotF(z, c);
        i++;
    }
    i -= 1;

    size_t idx = pixelPos.x + pixelPos.y * size.x;
    image[idx]=(uchar4)(i * 8, i * 4, 0, 0);
}

)OpenCLC";

const char *mclFilename = "MandelbrotSet.mcl";
const char *bitmapFilename = "mandelbrot.bmp";
const char *moduleDumpFilename = "mandelbrotmodule";
bool performIoOperations = false;
bool dumpImageToFile = false;

} // namespace MclTestMandelbrot

void getCoords(float *start, float *end, size_t *pixel, size_t *size, float scaleX, float scaleY, float *coords) {
    coords[0] = (float)pixel[0] / (float)size[0];
    coords[0] = coords[0] * scaleX + start[0];

    coords[1] = (float)pixel[1] / (float)size[1];
    coords[1] = coords[1] * scaleY + start[1];
}

bool verifyMandelbrot(unsigned char *input, const size_t imageSize, size_t height, size_t width, size_t elemsPerPixel, float *start, float *end) {
    constexpr uint32_t maxIterations = 32;

    float scaleX = std::abs(end[0] - start[0]);
    float scaleY = std::abs(end[1] - start[1]);

    size_t imageDimensions[2] = {width, height};
    unsigned char *generatedImage = new unsigned char[imageSize];
    std::memset(generatedImage, 0x0, imageSize);

    for (size_t posY = 0; posY < height; posY++) {
        for (size_t posX = 0; posX < width; posX++) {
            size_t pixel[2] = {posX, posY};
            float coords[2] = {0.0f, 0.0f};
            getCoords(start, end, pixel, imageDimensions, scaleX, scaleY, coords);

            float zValue[2] = {0.0f, 0.0f};
            uint32_t i = 0;

            while (i < maxIterations && (zValue[0] * zValue[0] + zValue[1] * zValue[1] < 4.0f)) {
                float newValueZ = zValue[0] * zValue[0] - zValue[1] * zValue[1] + coords[0];
                float newValueY = 2 * zValue[0] * zValue[1] + coords[1];
                zValue[0] = newValueZ;
                zValue[1] = newValueY;
                i++;
            }
            i--;
            size_t imageIdx = posX * elemsPerPixel + posY * width * elemsPerPixel;
            generatedImage[imageIdx] = i * 8;
            generatedImage[imageIdx + 1] = i * 4;
            generatedImage[imageIdx + 2] = 0;
            generatedImage[imageIdx + 3] = 0;
        }
    }

    bool valid = true;

    for (size_t byteIdx = 0; byteIdx < imageSize; byteIdx++) {
        if (generatedImage[byteIdx] != input[byteIdx]) {
            if (LevelZeroBlackBoxTests::verbose) {
                uint32_t generated = generatedImage[byteIdx];
                uint32_t gpu = input[byteIdx];
                std::cout << "invalid byte at " << byteIdx
                          << " cpu value " << std::hex << generated
                          << " gpu value " << std::hex << gpu << std::dec << std::endl;
            }
            valid = false;
            break;
        }
    }

    delete[] generatedImage;
    return valid;
}

void generateBitmapImage(unsigned char *image, size_t h, size_t w, const std::string &fileName) {
    alignas(uint32_t) char fileHeader[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    alignas(uint32_t) char infoHeader[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0};
    char padding[3] = {0, 0, 0};

    const size_t paddingSize = (4 - (w * 3) % 4) % 4;
    size_t fileSize = 54 + (3 * w + paddingSize) * h;

    *reinterpret_cast<uint32_t *>(fileHeader + 2) = static_cast<uint32_t>(fileSize);
    *reinterpret_cast<uint32_t *>(infoHeader + 4) = static_cast<uint32_t>(w);
    *reinterpret_cast<uint32_t *>(infoHeader + 8) = static_cast<uint32_t>(h);

    std::ofstream file(fileName, std::iostream::binary);
    file.write(fileHeader, sizeof(fileHeader));
    file.write(infoHeader, sizeof(infoHeader));
    for (size_t i = 0; i < h; i++) {
        size_t invertedRow = h - 1 - i;
        for (size_t j = 0; j < w; j++) {
            file.write(reinterpret_cast<const char *>(image + w * 4 * invertedRow + j * 4), 3);
        }
        file.write(padding, paddingSize);
    }
}

void createMandelbrot(MclTests::ExecEnv *env, ze_module_handle_t module, std::vector<char> &mclBinaryBuffer) {
    ze_command_list_handle_t cmdList;
    MclTests::createMutableCmdList(env->context, env->device, cmdList, false, false);

    // Create vars
    zex_variable_handle_t varImage = MclTests::getVariable(cmdList, "image", env);
    zex_variable_handle_t varStart = MclTests::getVariable(cmdList, "start", env);
    zex_variable_handle_t varEnd = MclTests::getVariable(cmdList, "end", env);
    zex_variable_handle_t varGroupSize = MclTests::getVariable(cmdList, "groupSize", env);
    zex_variable_handle_t varGroupCount = MclTests::getVariable(cmdList, "groupCount", env);

    // Create Kernel and set var arguments
    ze_kernel_handle_t kernelMandelbrot;
    {
        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "mandelbrot";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernelMandelbrot));
    }
    SUCCESS_OR_TERMINATE(env->zexKernelSetArgumentVariableFunc(kernelMandelbrot, 0, varImage));
    SUCCESS_OR_TERMINATE(env->zexKernelSetArgumentVariableFunc(kernelMandelbrot, 1, varStart));
    SUCCESS_OR_TERMINATE(env->zexKernelSetArgumentVariableFunc(kernelMandelbrot, 2, varEnd));
    SUCCESS_OR_TERMINATE(env->zexKernelSetVariableGroupSizeFunc(kernelMandelbrot, varGroupSize));

    SUCCESS_OR_TERMINATE(env->zexCommandListAppendVariableLaunchKernelFunc(cmdList, kernelMandelbrot, varGroupCount, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    std::cout << "Encoding completed" << std::endl;

    if (MclTestMandelbrot::performIoOperations) {
        MclTests::saveMclProgram(cmdList, module, MclTestMandelbrot::mclFilename, env);
    } else {
        MclTests::getMclBinary(cmdList, module, mclBinaryBuffer, env);
    }

    std::cout << "Saving binary completed" << std::endl;

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernelMandelbrot));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
}

void runMandelbrot(MclTests::ExecEnv *env, std::vector<char> &mclBinaryBuffer, bool &success, bool aubMode) {
    constexpr size_t imageWidth = 128;
    constexpr size_t imageHeight = 128;
    constexpr size_t elemsPerPixel = 4;
    constexpr size_t imageSize = imageWidth * imageHeight * elemsPerPixel * sizeof(char);

    void *imageAlloc = nullptr;
    {
        ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
        hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;
        SUCCESS_OR_TERMINATE(zeMemAllocHost(env->context, &hostDesc, imageSize, 1, &imageAlloc));
        std::memset(imageAlloc, 0x0, imageSize);
    }
    float start[2] = {-2.0F, -1.12F};
    float end[2] = {0.47F, 1.12F};
    uint32_t groupSize[3] = {0x80, 0x8, 1};
    uint32_t groupCount[3] = {static_cast<uint32_t>(imageWidth) / groupSize[0], static_cast<uint32_t>(imageHeight) / groupSize[1], 1U};

    ArrayRef<const char> refProgramData;

    if (MclTestMandelbrot::performIoOperations) {
        std::ifstream mclProgramFile(MclTestMandelbrot::mclFilename, std::ios::binary | std::ios::ate);
        size_t fileSize = mclProgramFile.tellg();
        mclBinaryBuffer.resize(fileSize, 0);
        mclProgramFile.seekg(0);
        mclProgramFile.read(mclBinaryBuffer.data(), fileSize);
    }

    refProgramData = {mclBinaryBuffer.data(), mclBinaryBuffer.size()};

    auto program = std::unique_ptr<MclTests::MclProgram>(MclTests::MclProgram::loadProgramFromBinary(refProgramData, env));
    program->setVar("image", sizeof(void *), &imageAlloc);
    program->setVar("start", 2 * sizeof(float), start);
    program->setVar("end", 2 * sizeof(float), end);
    program->setVar("groupSize", 3 * sizeof(uint32_t), groupSize);
    program->setVar("groupCount", 3 * sizeof(uint32_t), groupCount);

    std::cout << "Decoding completed" << std::endl;

    // Execute
    auto cmdList = program->getCmdlist();
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(env->queue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(env->queue, std::numeric_limits<uint64_t>::max()));

    std::cout << "Execution completed" << std::endl;

    if (!aubMode) {
        auto image = reinterpret_cast<unsigned char *>(imageAlloc);
        success = verifyMandelbrot(image, imageSize, imageHeight, imageWidth, elemsPerPixel, start, end);
        std::cout << "Verification completed" << std::endl;

        if (MclTestMandelbrot::dumpImageToFile) {
            // Save bitmap image
            generateBitmapImage(image, imageWidth, imageHeight, MclTestMandelbrot::bitmapFilename);
        }
    } else {
        success = true;
        std::cout << "Verification in AUB mode disabled" << std::endl;
    }

    // Clean up
    SUCCESS_OR_TERMINATE(zeMemFree(env->context, imageAlloc));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello MCL Mandelbrot";

    MclTestMandelbrot::performIoOperations = !!LevelZeroBlackBoxTests::getParamValue(argc, argv, "-f", "--file-save", 0);
    MclTestMandelbrot::dumpImageToFile = !!LevelZeroBlackBoxTests::getParamValue(argc, argv, "-d", "--dump-image", 0);
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    const char *moduleBinaryFileName = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--module_binary", nullptr);
    std::vector<char> mclBinaryBuffer;

    auto env = std::unique_ptr<MclTests::ExecEnv>(MclTests::ExecEnv::create());

    const char *moduleDumpFilename = nullptr;
    if (MclTestMandelbrot::performIoOperations) {
        moduleDumpFilename = MclTestMandelbrot::moduleDumpFilename;
    }

    ze_module_handle_t module;
    if (moduleBinaryFileName != nullptr) {
        std::vector<uint8_t> moduleBinary;
        std::ifstream moduleBinaryFile(moduleBinaryFileName, std::ios::binary | std::ios::ate);
        size_t fileSize = moduleBinaryFile.tellg();
        moduleBinary.resize(fileSize, 0);
        moduleBinaryFile.seekg(0);
        moduleBinaryFile.read(reinterpret_cast<char *>(moduleBinary.data()), fileSize);

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = moduleBinary.data();
        moduleDesc.inputSize = moduleBinary.size();
        moduleDesc.pBuildFlags = MclTests::mclBuildOption.c_str();
        if (zeModuleCreate(env->context, env->device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = reinterpret_cast<char *>(malloc(szLog));
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            LevelZeroBlackBoxTests::printBuildLog(strLog);

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cerr << "\nZello MCL Test Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
    } else {
        MclTests::createModule(env->context, env->device, module, MclTestMandelbrot::mandelbrotModuleSource, moduleDumpFilename);
    }

    bool success = false;
    createMandelbrot(env.get(), module, mclBinaryBuffer);
    runMandelbrot(env.get(), mclBinaryBuffer, success, aubMode);

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    int mainRetCode = aubMode ? 0 : (success ? 0 : 1);
    LevelZeroBlackBoxTests::printResult(aubMode, success, blackBoxName);
    return mainRetCode;
}
