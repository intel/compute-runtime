/*
 * Copyright (c) 2017, Intel Corporation
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

#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/mocks/mock_context.h"

using OCLRT::MockContext;

static const size_t imageWidth = 7;
static const size_t imageHeight = 7;
static const size_t imageDepth = 7;
static const size_t imageArray = imageDepth;

const cl_image_format Image1dDefaults::imageFormat = {
    CL_R,
    CL_FLOAT};

const cl_image_format LuminanceImage::imageFormat = {
    CL_LUMINANCE,
    CL_FLOAT};

const cl_image_desc Image1dDefaults::imageDesc = {
    CL_MEM_OBJECT_IMAGE1D,
    imageWidth,
    imageHeight,
    1,
    1,
    0,
    0,
    0,
    0,
    {nullptr}};

const cl_image_desc Image2dDefaults::imageDesc = {
    CL_MEM_OBJECT_IMAGE2D,
    imageWidth,
    imageHeight,
    1,
    1,
    0,
    0,
    0,
    0,
    {nullptr}};

const cl_image_desc Image3dDefaults::imageDesc = {
    CL_MEM_OBJECT_IMAGE3D,
    imageWidth,
    imageHeight,
    imageDepth,
    1,
    0,
    0,
    0,
    0,
    {nullptr}};

const cl_image_desc Image2dArrayDefaults::imageDesc = {
    CL_MEM_OBJECT_IMAGE2D_ARRAY,
    imageWidth,
    imageHeight,
    0,
    imageArray,
    0,
    0,
    0,
    0,
    {nullptr}};

const cl_image_desc Image1dArrayDefaults::imageDesc = {
    CL_MEM_OBJECT_IMAGE1D_ARRAY,
    imageWidth,
    0,
    0,
    imageArray,
    0,
    0,
    0,
    0,
    {nullptr}};

static float imageMemory[imageWidth * imageHeight * imageDepth] = {};

void *Image1dDefaults::hostPtr = imageMemory;

OCLRT::Context *Image1dDefaults::context = nullptr;
