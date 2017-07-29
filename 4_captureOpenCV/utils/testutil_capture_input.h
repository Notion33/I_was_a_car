/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef _TESTUTIL_CAPTURE_INPUT_H_
#define _TESTUTIL_CAPTURE_INPUT_H_

#include "testutil_i2c.h"

typedef enum
{
    I2C0,
    I2C1,
    I2C2,
    I2C3,
    I2C4
} I2cId;

typedef enum
{
    AnalogDevices_ADV7180,
    AnalogDevices_ADV7182,
    AnalogDevices_ADV7281,
    AnalogDevices_ADV7282,
    NationalSemi_DS90UR910Q,
    Toshiba_TC358743
} CaptureInputDeviceId;

typedef void * CaptureInputHandle;

typedef enum
{
    YUV_420,
    YUV_422,
    YUV_444,
    RGB
} FrameFormat;

typedef enum
{
    NTSC,
    PAL
} VideoStd;

typedef enum
{
    PROGRESSIVE,
    INTERLACED
} PictureStructure;

typedef struct
{
    unsigned int width;
    unsigned int height;
    union
    {
        struct
        {
            unsigned int lanes;
            FrameFormat format;
        } hdmi2csi;
        struct
        {
            VideoStd std;
            PictureStructure structure;
        } cvbs2csi;
        struct
        {
            VideoStd std;
        } vip;
    };
} CaptureInputConfigParams;

int
testutil_capture_input_open(
    I2cId i2cId,
    CaptureInputDeviceId deviceId,
    unsigned int captureMode,
    CaptureInputHandle *handle);

int
testutil_capture_input_configure(
    CaptureInputHandle handle,
    CaptureInputConfigParams *params);

int
testutil_capture_input_start(
    CaptureInputHandle handle);

int
testutil_capture_input_stop(
    CaptureInputHandle handle);

void
testutil_capture_input_close(
    CaptureInputHandle handle);

#endif /* _TESTUTIL_CAPTURE_INPUT_H_ */
