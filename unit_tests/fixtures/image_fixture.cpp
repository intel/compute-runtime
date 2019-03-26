/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/image_fixture.h"

#include "unit_tests/mocks/mock_context.h"

using NEO::MockContext;

static const size_t imageWidth = 7;
static const size_t imageHeight = 9;
static const size_t imageDepth = 11;
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
    1,
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

NEO::Context *Image1dDefaults::context = nullptr;
