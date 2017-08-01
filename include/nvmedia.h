/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved. All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 */

/**
 * \mainpage NvMedia API for Tegra
 *
 * \section intro Introduction
 *
 * The NvMedia provides
 * a complete solution for decoding, post-processing, compositing,
 * and displaying compressed or uncompressed video streams. These
 * video streams may be combined (composited) with graphics content.
 * It was specifically designed along with other media formats to support
 * Blu-Ray and DVD-Video playback.
 *
 * @warning The API content represents the set of APIs you can use directly.
 * Some APIs are not documented and we advise against using them directly.
 * Using undocumented APIs can lead to incompatability when upgrading to later
 * releases.
 *
 * \section video_decoder_usage Video Decoder Usage
 *
 * NvMedia is a slice-level API. Put another way, NvMedia implementations accept
 * "slice" data from the bitstream, and perform all required processing of
 * those slices (e.g VLD decoding, IDCT, motion compensation, in-loop
 * deblocking, etc.).
 *
 * The client application is responsible for:
 *
 * - Extracting the slices from the bitstream (e.g. parsing/demultiplexing
 *   container formats, scanning the data to determine slice start positions
 *   and slice sizes).
 * - Parsing various bitstream headers/structures (e.g. sequence header,
 *   sequence parameter set, picture parameter set, entry point structures,
 *   etc.) Various fields from the parsed header structures needs to be
 *   provided to NvMedia alongside the slice bitstream in a "picture info"
 *   structure.
 * - Surface management (e.g. H.264 DPB processing, display re-ordering)
 *
 * The exact data that should be passed to NvMedia is detailed below for each
 * supported format:
 *
 * \subsection bitstream_mpeg1_mpeg2 MPEG-1 and MPEG-2
 *
 * Include all slices beginning with start codes 0x00000101 through
 * 0x000001AF. The slice start code must be included for all slices.
 *
 * \subsection bitstream_h264 H.264
 *
 * Include all NALs with nal_unit_type of 1 or 5 (coded slice of non-IDR/IDR
 * picture respectively). The complete slice start code (including 0x000001
 * prefix) must be included for all slices, even when the prefix is not
 * included in the bitstream.
 *
 * Note that if desired:
 *
 * - The slice start code prefix may be included in a separate bitstream
 *   buffer array entry to the actual slice data extracted from the bitstream.
 * - Multiple bitstream buffer array entries (e.g. one per slice) may point at
 *   the same physical data storage for the slice start code prefix.
 *
 * \subsection bitstream_vc1_sp_mp VC-1 Simple and Main Profile
 *
 * VC-1 simple/main profile bitstreams always consist of a single slice per
 * picture, and do not use start codes to delimit pictures. Instead, the
 * container format must indicate where each picture begins/ends.
 *
 * As such, no slice start codes should be included in the data passed to
 * NvMedia; simply pass in the exact data from the bitstream.
 *
 * Header information contained in the bitstream should be parsed by the
 * application and passed to NvMedia using the "picture info" data structure;
 * this header information explicitly must not be included in the bitstream
 * data passed to NvMedia for this encoding format.
 *
 * \subsection bitstream_vc1_ap VC-1 Advanced Profile
 *
 * Include all slices beginning with start codes 0x0000010D (frame),
 * 0x0000010C (field) or 0x0000010B (slice). The slice start code should be
 * included in all cases.
 *
 * Some VC-1 advanced profile streams do not contain slice start codes; again,
 * the container format must indicate where picture data begins and ends. In
 * this case, pictures are assumed to be progressive and to contain a single
 * slice. It is highly recommended that applications detect this condition,
 * and add the missing start codes to the bitstream passed to NvMedia. However,
 * NvMedia implementations must allow bitstreams with missing start codes, and
 * act as if a 0x0000010D (frame) start code had been present.
 *
 * Note that pictures containing multiple slices, or interlace streams, must
 * contain a complete set of slice start codes in the original bitstream;
 * without them, it is not possible to correctly parse and decode the stream.
 *
 * The bitstream passed to NvMedia should contain all original emulation
 * prevention bytes present in the original bitstream; do not remove these
 * from the bitstream.
 *
 * \subsection bitstream_mpeg4part2 MPEG-4 Part 2 and DivX
 *
 * Include all slices beginning with start codes 0x000001B6. The slice start
 * code must be included for all slices.
 *
 * \section software_decoder_usage Using Software Video Decoders
 *
 * NvMedia provides two functions to support software video decoders.
 * - The decoder can obtain a mapped YUV video surface by calling \ref NvMediaVideoSurfaceLock.
 * During the call NvMedia waits while the surface is being used by the internal engines so
 * this is a blocking call. Once the function returns the surface can be used by the software
 * decoder. The returned parameters are the mapped memory YUV pointers and the associated pitches.
 * It also returns the width and height in terms of luma pixels.
 * - When the decoder finished filling up the surfaces it calls \ref NvMediaVideoSurfaceUnlock.
 * This tells NvMedia that the surface can be used the internal engines.
 *
 * \section video_mixer_usage Video Mixer Usage
 *
 * \subsection layer_struct Layer Structure
 *
 * \ref NvMediaVideoMixerRender supports 5 layers of images and these are composited
 * in the following order:
 * - Background
 * - Primary video
 * - Secondary video
 * - Graphics 0
 * - Graphics 1
 *
 * The presence of these layers are determined at the video mixer creation with
 * the combinations of following features:
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_BACKGROUND_PRESENT
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_SECONDARY_VIDEO_PRESENT
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_RGBA_PRESENT
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_I8_PRESENT
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_RGBA_PRESENT
 *  - \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_I8_PRESENT
 *
 * Note:
 * - Primary video is always present.
 * - Only one feature can be defined for a graphics layer, either RGBA or I8.
 *
 * \subsection layer_usage Layer usage for different media formats:
 *
 *  <table border>
 *  <tr>
 *     <td><b> Layer </b></td>
 *     <td><b> Blu-Ray BDMV Mode </b></td>
 *     <td><b> Blu-Ray BD-J Mode</b></td>
 *     <td><b> DVD-Video </b></td>
 *  </tr>
 *  <tr>
 *     <td><b> Background </b></td>
 *     <td> <center> --- </center> </td>
 *     <td> Background </td>
 *     <td> <center> --- </center> </td>
 *  </tr>
 *  <tr>
 *     <td><b> Primary Video </b></td>
 *     <td> Primary Video </td>
 *     <td> Primary Video </td>
 *     <td> Video </td>
 *  </tr>
 *  <tr>
 *     <td><b> Secondary Video </b></td>
 *     <td> Secondary Video </td>
 *     <td> Secondary Video </td>
 *     <td> <center> --- </center> </td>
 *  </tr>
 *  <tr>
 *     <td><b> Graphics 0 </b></td>
 *     <td> Presentation Graphics </td>
 *     <td> Presentation Graphics </td>
 *     <td> Sub-picture </td>
 *  </tr>
 *  <tr>
 *     <td><b> Graphics 1 </b></td>
 *     <td> Interactive Graphics </td>
 *     <td> Java Graphics </td>
 *     <td> <center> --- </center> </td>
 *  </tr>
 *  </table>
 *
 * \subsection background_layer Background Layer
 *
 * Background layer is an optional layer mainly used with Blu-Ray BD-J mode.
 * It can display a solid color or an RGB graphics image. \ref NvMediaBackgroundType
 * is used to describe the background layer. \b type determines the whether the layer is
 * a solid color (\ref NVMEDIA_BACKGROUND_TYPE_SOLID_COLOR) or an RGBA surface
 * (\ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS).
 * In case of solid color mode the \b backgroundColor determines the color.
 * In case of graphics mode a surface allocated in the main memory determines the content.
 * - \b sourceSurface defines the memory region as the source. Each pixel is stored
 * as an RGBA pixel but the A (alpha) component is not used. Each 32-bit value has R, G, B,
 * and A bytes packed from LSB to MSB.
 * - \b pitch Pitch is the distance between the memory addresses that represent the
 * beginning of one bitmap line and the beginning of the next bitmap line. It is measured in bytes.
 * Generally for a main memory allocated RGBA surface the value is 4 * width of the surface.
 * - \b srcRect determines which portion of the source surface is considered for display. This rectangle
 * gets zoomed to the full background surface.
 * - \b updateRectangleList is a NULL terminated list of pointers to rectangles that determine
 * what portion of the source surface is copied to the background surface. Since each list
 * element adds an extra copy loop, it is recommended that the list size is kept at minimum.
 *
 * \subsection primary_video_layer Primary Video Layer
 *
 * Primary video layer is the main video playback layer. This is used as primary
 * video for Blu-Ray and as normal video for DVD-Video. \ref NvMediaPrimaryVideo is
 * used to describe this layer.
 * - \b pictureStructure, \b next, \b current, \b previous and \b previous2 are describing
 * the type and video surfaces to be used. See section \ref video_surf_params.
 * - \b srcRect determines which portion of the source video surface is used.
 * This rectangle from the source gets zoomed into \b dstRect.
 * - \b dstRect determines the rectangle where the primary video is going to be rendered.
 * The position of this rectangle is relative to the destination surface.
 * The destination surface size is determined at \ref NvMediaVideoMixer creation.
 *
 * \subsection secondary_video_layer Secondary Video Layer
 *
 * This layer is used as the secondary (PiP) video layer for Blu-Ray. \ref NvMediaSecondaryVideo is
 * used to describe this layer.
 * - \b pictureStructure, \b next, \b current, \b previous, \b previous2 and \b srcRect
 * parameters are same as described in \ref primary_video_layer.
 * - \b dstRect determines the rectangle where the secondary video is going to be rendered.
 * \n Note: The position of this rectangle is relative to destination rectangle of the \b primary \b video.
 * - \b lumaKeyUsed determines whether luma keying is applied to this layer.
 * - \b lumaKey If luma keying is applied then this value determines the luma threshold.
 * If the luma value of a pixel is below or equal to the threshold the pixel will be transparent.
 *
 * \subsection graphics_layer Graphics Layers
 *
 * These layers are used as graphics (Presentation, Interactive, Java) layers for Blu-Ray
 * or sub-picture layer for DVD-Video. \ref NvMediaGraphics is used to describe this layer.
 * The source of this layer can be either an 8-bit indexed or a 32-bit RGBA surface.
 * For 8-bit indexed surface each pixel is stored as one byte, for RGBA surface each pixel
 * is stored as a 32-bit value having R, G, B, and A bytes packed from LSB to MSB.
 *
 * - \b sourceSurface defines the memory region as the source. This surface is
 * allocated in the main memory.
 * - \b pitch Pitch is the distance between the memory addresses that represent the
 * beginning of one bitmap line and the beginning of the next bitmap line. It is measured in bytes.
 * Generally for a main memory allocated RGBA surface the value is 4 * width of the surface.
 * - \b palette If the graphics layer was created with the \b NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_X_I8_PRESENT
 * feature flag then a valid palette has to be supplied. See \ref palette_api.
 * - \b srcRect determines which portion of the source surface is considered for display. This rectangle
 * gets zoomed to the full graphics surface.
 * - \b updateRectangleList is a NULL terminated list of pointers to rectangles that determine
 * what portion of the source surface is copied to the graphics surface. Since each list
 * element adds an extra copy loop, it is recommended that the list size is kept at minimum.
 *
 * \subsection video_surface_content NvMediaVideoSurface Content
 *
 * Each \ref NvMediaVideoSurface "NvMediaVideoSurface" is expected to contain an
 * entire frame's-worth of data, irrespective of whether an interlaced of
 * progressive sequence is being decoded.
 *
 * Depending on the exact encoding structure of the compressed video stream,
 * the application may need to call \ref NvMediaVideoDecoderRender twice to fill a
 * single \ref NvMediaVideoSurface "NvMediaVideoSurface". When the stream contains an
 * encoded progressive frame, or a "frame coded" interlaced field-pair, a
 * single \ref NvMediaVideoDecoderRender call will fill the entire surface. When the
 * stream contains separately encoded interlaced fields, two
 * \ref NvMediaVideoDecoderRender calls will be required; one for the top field, and
 * one for the bottom field.
 *
 * Note: When \ref NvMediaVideoDecoderRender renders an interlaced
 * field, this operation will not disturb the content of the other field in
 * the surface.
 *
 * \subsection video_surf_params Video Surface Parameters
 *
 * The canonical usage is to call \ref NvMediaVideoMixerRender once for decoded
 * field, in display order, to yield one post-processed frame for display.
 *
 * For each call to \ref NvMediaVideoMixerRender, the field to be processed should
 * be provided as the \b current parameter.
 *
 * To enable operation of advanced de-interlacing algorithms and/or
 * post-processing algorithms, some past and/or future surfaces should be
 * provided as context. These are provided as the \b previous2, \b previous and
 * \b next parameters.
 *
 * The \ref NvMediaVideoMixerRender parameter \b pictureStructure applies
 * to \b current. The picture structure for the other surfaces
 * will be automatically derived from that for the current picture. The
 * derivation algorithm is extremely simple; the concatenated list
 * past/current/future is simply assumed to have an alternating top/bottom
 * pattern throughout.
 *
 * In other words, the concatenated list of past/current/future frames simply
 * forms a window that slides through the sequence of decoded fields.
 *
 * Here is a full reference of the required fields for different de-intelacing modes:
 *
 *  <table border>
 *  <tr>
 *     <td><b> De-interlacing mode </b></td>
 *     <td><b> Picture structure </b></td>
 *     <td><b><center> previous2 </center></b></td>
 *     <td><b><center> previous </center></b></td>
 *     <td><b><center> current </center></b></td>
 *     <td><b><center> next  </center></b></td>
 *  </tr>
 *  <tr>
 *     <td><b> Progressive </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_FRAME </center> </td>
 *     <td><center> NULL </center></td>
 *     <td><center> NULL </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> NULL </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Bob </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD \n NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD </center> </td>
 *     <td><center> NULL </center></td>
 *     <td><center> NULL </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> NULL </center> </td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Half-rate) \n Top Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Half-rate) \n Bottom Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD </center> </td>
 *     <td><center> Past </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Full-rate) \n Top Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD </center> </td>
 *     <td><center> Past </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Full-rate) \n Top Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Future </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Full-rate) \n Bottom Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *  </tr>
 *  <tr>
 *     <td><b> Advanced1 (Full-rate) \n Bottom Field First </b></td>
 *     <td><center> NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD </center></td>
 *     <td><center> Past </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Current </center></td>
 *     <td><center> Future </center></td>
 *  </tr>
 *  </table>
 *
 * Specific examples for different de-interlacing types are presented below.
 *
 * \subsection deint Deinterlacing
 * If pictureStructure is not \ref NVMEDIA_PICTURE_STRUCTURE_FRAME, deinterlacing
 * will be performed.  Bob deinterlacing is always available but
 * Advanced1 deinterlacing (\ref NVMEDIA_DEINTERLACE_TYPE_ADVANCED1) will
 * be used only if the following conditions are met:
 *   - The NvMediaVideoMixer must be created with the combination of
 *      \ref NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_PRIMARY_DEINTERLACING and
 *      \ref NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_SECONDARY_DEINTERLACING flag.
 *   - The primaryDeinterlaceType or secondaryDeinterlaceType attributes must be set to
 *      \ref NVMEDIA_DEINTERLACE_TYPE_ADVANCED1
 *   - All 4 source fields must be presented to the \ref NvMediaVideoMixer "NvMediaVideoMixer:"
 *      next, current, previous and previous2.
 *
 * \subsection deint_weave Weave De-interlacing
 *
 * Weave de-interlacing is the act of interleaving the lines of two temporally
 * adjacent fields to form a frame for display.
 *
 * To disable de-interlacing for progressive streams, simply specify
 * \b current as \ref NVMEDIA_PICTURE_STRUCTURE_FRAME;
 * no de-interlacing will be applied.
 *
 * Weave de-interlacing for interlaced streams is identical to disabling
 * de-interlacing, as describe immediately above, because each
 * \ref NvMediaVideoSurface already contains an entire frame's worth (i.e. two
 * fields) of picture data.
 *
 * Weave de-interlacing produces one output frame for each input frame. The
 * application should make one \ref NvMediaVideoMixerRender call per pair of
 * decoded fields, or per decoded frame.
 *
 * Weave de-interlacing requires no entries in the past/future lists.
 *
 * \subsection deint_bob Bob De-interlacing
 *
 * Bob de-interlacing is the act of vertically scaling a single field to the
 * size of a single frame.
 *
 * To achieve bob de-interlacing, simply provide a single field as
 * \b current, and set \b pictureStructure
 * appropriately, to indicate whether a top or bottom field was provided.
 *
 * Inverse telecine is disabled when using bob de-interlacing.
 *
 * Bob de-interlacing produces one output frame for each input field. The
 * application should make one \ref NvMediaVideoMixerRender call per decoded
 * field.
 *
 * Bob de-interlacing requires no entries in the past/future lists.
 *
 * Bob de-interlacing is the default when no advanced method is requested and
 * enabled. Advanced de-interlacing algorithms may fall back to bob e.g. when
 * required past/future fields are missing.
 *
 * \subsection deint_advanced1 Advanced1 De-interlacing
 *
 * This algorithm uses various advanced processing on the pixels of both the
 * current and various past/future fields in order to determine how best to
 * de-interlacing individual portions of the image.
 *
 * Advanced de-interlacing produces one output frame for each input field. The
 * application should make one \ref NvMediaVideoMixerRender call per decoded
 * field.
 *
 * Advanced de-interlacing requires entries in the past/future lists.
 *
 * \subsection deint_rate De-interlacing Rate
 *
 * For all de-interlacing algorithms except weave, a choice may be made to
 * call \ref NvMediaVideoMixerRender for either each decoded field, or every
 * second decoded field.
 *
 * If \ref NvMediaVideoMixerRender is called for every decoded field, the
 * generated post-processed frame rate is equal to the decoded field rate.
 * Put another way, the generated post-processed nominal field rate is equal
 * to 2x the decoded field rate. This is standard practice.
 *
 * If \ref NvMediaVideoMixerRender is called for every second decoded field (say
 * every top field), the generated post-processed frame rate is half to the
 * decoded field rate. This mode of operation is thus referred to as
 * "half-rate".
 *
 * Recall that the concatenation of past/current/future surface lists simply
 * forms a window into the stream of decoded fields. To achieve standard
 * de-interlacing, the window is slid through the list of decoded fields one
 * field at a time, and a call is made to \ref NvMediaVideoMixerRender for each
 * movement of the window. To achieve half-rate de-interlacing, the window is
 * slid through the list of decoded fields two fields at a time, and a
 * call is made to \ref NvMediaVideoMixerRender for each movement of the window.
 */

/**
 * \file nvmedia.h
 * \brief The Core NvMedia API
 *
 * This file contains the \ref api_core "Core API".
 */

#ifndef _NVMEDIA_H
#define _NVMEDIA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>

/**
 * \defgroup api_core Core API
 *
 * Encompasses all NvMedia functionality
 *
 * @{
 */

/**
 * \defgroup basic_api Basic NvMedia types and structures
 *
 * Defines basic types used throughout the NvMedia API.
 * @{
 */

/** \brief Major Version number */
#define NVMEDIA_VERSION_MAJOR   1
/** \brief Minor Version number */
#define NVMEDIA_VERSION_MINOR   25

/** \hideinitializer \brief A true \ref NvMediaBool value */
#define NVMEDIA_TRUE  (0 == 0)
/** \hideinitializer \brief A false \ref NvMediaBool value */
#define NVMEDIA_FALSE (0 == 1)

/**
 * \brief A boolean value, holding \ref NVMEDIA_TRUE or \ref
 * NVMEDIA_FALSE.
 */
typedef int NvMediaBool;

/**
 * \brief Media time (timespec as defined by the POSIX specification)
 */
typedef struct timespec NvMediaTime;

/**
 * \brief A constant RGBA color.
 *
 * Note that the components are stored as float values in the
 * range 0.0...1.0 rather than format-specific integer values.
 * This allows NvMediaColor values to be independent from the exact
 * surface format(s) in use.
 */
typedef struct {
    /*! Red color component */
    float red;
    /*! Green color component */
    float green;
    /*! Blue color component */
    float blue;
    /*! Alpha color component */
    float alpha;
} NvMediaColor;

/**
 * \brief A rectangular region of a surface.
 *
 * The co-ordinates are top-left inclusive, bottom-right
 * exclusive.
 *
 * The NvMedia co-ordinate system has its origin at the top-left
 * of a surface, with x and y components increasing right and
 * down.
 */
typedef struct {
    /*! Left X co-ordinate. Inclusive. */
    unsigned short x0;
    /*! Top Y co-ordinate. Inclusive. */
    unsigned short y0;
    /*! Right X co-ordinate. Exclusive. */
    unsigned short x1;
    /*! Bottom Y co-ordinate. Exclusive. */
    unsigned short y1;
} NvMediaRect;

/**
 * \hideinitializer
 * \brief The set of all possible error codes.
 */
typedef enum {
    /** \hideinitializer The operation completed successfully; no error. */
    NVMEDIA_STATUS_OK = 0,
    /** Bad parameter was passed. */
    NVMEDIA_STATUS_BAD_PARAMETER,
    /** Operation has not finished yet. */
    NVMEDIA_STATUS_PENDING,
    /** Operation timed out. */
    NVMEDIA_STATUS_TIMED_OUT,
    /** Out of memory. */
    NVMEDIA_STATUS_OUT_OF_MEMORY,
    /** Not initialized. */
    NVMEDIA_STATUS_NOT_INITIALIZED,
    /** Not supported. */
    NVMEDIA_STATUS_NOT_SUPPORTED,
    /** A catch-all error, used when no other error code applies. */
    NVMEDIA_STATUS_ERROR,
    /** No operation is pending. */
    NVMEDIA_STATUS_NONE_PENDING,
    /** Insufficient buffering. */
    NVMEDIA_STATUS_INSUFFICIENT_BUFFERING
} NvMediaStatus;

/*@}*/

/**
 * \defgroup devive_api Device
 *
 * Manages NvMediaDevice objects, which are the root of the Nvmedia object
 * system.
 *
 * The NvMediaDevice is the root of the NvMedia object system. Using a
 * NvMediaDevice object, all other object types may be created. See
 * the sections describing those other object types for details
 * on object creation.
 * @{
 */

/**
 * \brief  An opaque handle representing a NvMediaDevice object.
 */
typedef void NvMediaDevice;

/**
 * \brief Create a NvMediaDevice.
 * \return device The new device's handle or NULL if unsuccessful.
 */
NvMediaDevice *
NvMediaDeviceCreate(
    void
);

/**
 * \brief Destroy a NvMediaDevice.
 * \param[in] device The device to destroy.
 * \return void
 */
void
NvMediaDeviceDestroy(
    NvMediaDevice *device
);

/*@}*/

/**
 * \defgroup surface_api Video Surface
 *
 * Defines and manages objects for defining video RAM surfaces.
 *
 * \ref NvMediaVideoSurface "NvMediaVideoSurface"s are video RAM surfaces
 * storing YCrCb or RGBA data. They may be used as sources for the video mixer and
 * they may be used as rendering targets/references by the video decoder.
 * \ref NvMediaVideoSurface "NvMediaVideoSurface"s are created with
 * \ref NvMediaVideoSurfaceCreate() and destroyed with
 * \ref NvMediaVideoSurfaceDestroy().
 * @{
 */
/**
 * \brief The set of surface types for \ref NvMediaVideoSurface
 * "NvMediaVideoSurface"s
 */
typedef enum {
  /** YV12 surface type (also referred as 4:2:0 format) */
  NvMediaSurfaceType_YV12,
  /** YV16 surface type (also referred as 4:2:2 format) */
  NvMediaSurfaceType_YV16,
  /** YV24 surface type (also referred as 4:4:4 format) */
  NvMediaSurfaceType_YV24,
  /** Special YV16 surface type used for video capture */
  NvMediaSurfaceType_YV16x2,
  /** R8G8B8A8 surface type used for video capture */
  NvMediaSurfaceType_R8G8B8A8,
  /** R8G8B8A8 surface type used for video rendering.
      Surfaces that are interacting with EGL Stream functions has to be in this format */
  NvMediaSurfaceType_R8G8B8A8_BottomOrigin
} NvMediaSurfaceType;

/**
 * \brief A handle representing a video surface object.
 */
typedef struct {
    /*! Surface type. */
    NvMediaSurfaceType type;
    /*! Surface width */
    unsigned int width;
    /*! Surface height */
    unsigned int height;
    /*! Surface tag that can be used by the application. NvMedia
        does not change the value of this member. */
    void *tag;
} NvMediaVideoSurface;

/**
 * \brief A handle representing a video surface map.
 * Corresponding members of this structure are filled by \ref NvMediaVideoSurfaceLock.
 * For different surface types different members are filled as described here:
 *  <table border>
 *  <tr>
 *     <td><b> Surface type </b></td>
 *     <td><b> Filled members </b></td>
 *  </tr>
 *  <tr>
 *     <td><b> \ref NvMediaSurfaceType_YV12 \n \ref NvMediaSurfaceType_YV16 \n \ref NvMediaSurfaceType_YV24 </b></td>
 *     <td> lumaWidth, lumaHeight, pY, pitchY, pU, pitchU, pV, pitchV </td>
 *  </tr>
 *  <tr>
 *     <td><b> \ref NvMediaSurfaceType_YV16x2 </b></td>
 *     <td> lumaWidth, lumaHeight, pY, pitchY, pU, pitchU, pV, pitchV, pY2, pitchY2, pU2, pitchU2, pV2, pitchV2
 *          \n Member names ending with 2 refer to the second field of that surface. </td>
 *  </tr>
 *  <tr>
 *     <td><b> \ref NvMediaSurfaceType_R8G8B8A8 </b></td>
 *     <td> lumaWidth, lumaHeight, pRGBA, pitchRGBA </td>
 *  </tr>
 *  <tr>
 *     <td><b> \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin </b></td>
 *     <td> lumaWidth, lumaHeight, pRGBA, pitchRGBA </td>
 *  </tr>
 *  </table>
 */
typedef struct {
    /*! Surface width in luma pixels*/
    unsigned int lumaWidth;
    /*! Surface height in luma pixels */
    unsigned int lumaHeight;
    /*! Y surface pointer */
    unsigned char *pY;
    /*! Y surface pitch */
    unsigned int pitchY;
    /*! U surface pointer */
    unsigned char *pU;
    /*! U surface pitch */
    unsigned int pitchU;
    /*! V surface pointer */
    unsigned char *pV;
    /*! V surface pitch */
    unsigned int pitchV;
    /*! Y2 surface pointer */
    unsigned char *pY2;
    /*! Y2 surface pitch */
    unsigned int pitchY2;
    /*! U2 surface pointer */
    unsigned char *pU2;
    /*! U2 surface pitch */
    unsigned int pitchU2;
    /*! V2 surface pointer */
    unsigned char *pV2;
    /*! V2 surface pitch */
    unsigned int pitchV2;
    /*! RGBA surface pointer */
    unsigned char *pRGBA;
    /*! RGBA surface pitch */
    unsigned int pitchRGBA;
} NvMediaVideoSurfaceMap;

/**
 * \brief Allocate a video surface object. Upon creation, the contents are in an
 * undefined state.
 * \param[in] device The device's resource manager connection is used for the allocation.
 * \param[in] type Type of the surface. The supported types are:
 * \n \ref NvMediaSurfaceType_YV12
 * \n \ref NvMediaSurfaceType_YV16
 * \n \ref NvMediaSurfaceType_YV24
 * \n \ref NvMediaSurfaceType_YV16x2
 * \n \ref NvMediaSurfaceType_R8G8B8A8
 * \n \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin
 * \param[in] width Width of the surface
 * \param[in] height Height of the surface.
 * \return \ref NvMediaVideoSurface The new surfaces's handle or NULL if unsuccessful.
 */
NvMediaVideoSurface *
NvMediaVideoSurfaceCreate(
    NvMediaDevice *device,
    NvMediaSurfaceType type,
    unsigned short width,
    unsigned short height
);

/**
 * \brief Destroy a video surface object.
 * \param[in] surface The video surface to destroy.
 * \return void
 */
void
NvMediaVideoSurfaceDestroy(
    NvMediaVideoSurface *surface
);

/**
 * Locks a video surface and returns the associated mapped pointers
 * pointing to the Y, U and V surfaces that a software video decoder
 * can fill up.
 * \param[in] surface Video surface object
 * \param[out] surfaceMap Surface pointers and pitches
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaVideoSurfaceLock(
    NvMediaVideoSurface *surface,
    NvMediaVideoSurfaceMap *surfaceMap
);

/**
 * Unlocks a video surface. Internal engines cannot use a surface
 * until it is locked.
 * \param[in] surface Video surface object
 * \return void
 */
void
NvMediaVideoSurfaceUnlock(
    NvMediaVideoSurface *surface
);

/**
 * \brief NvMediaVideoSurfacePutBits reads a client memory buffer and
 *       writes the content into an NvMedia video surface.
 * \param[out] videoSurface
 *       Destination NvMediaVideoSurface type surface. The surface must be locked using
 *       \ref NvMediaVideoSurfaceLock prior to calling this function.
 * \param[in] dstRect
 *       Structure containing co-ordinates of the rectangle in the destination surface
 *       to which the client surface is to be copied. Setting dstRect to NULL implies
 *       rectangle of full surface size.
 * \param[in] srcPntrs
 *       Array of pointers to the client surface planes
 * \param[in] srcPitches
 *       Array of pitch values for the client surface planes
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameters is invalid.
 * \n \ref NVMEDIA_STATUS_ERROR if videoSurface is not locked.
 */
NvMediaStatus
NvMediaVideoSurfacePutBits(
    NvMediaVideoSurface *videoSurface,
    NvMediaRect *dstRect,
    void **srcPntrs,
    unsigned int *srcPitches
);

/**
 * \brief NvMediaVideoSurfaceGetBits reads an NvMedia video surface and
 *        writes the content into a client memory buffer.
 * \param[in] videoSurface
 *       Source NvMediaVideoSurface type surface. The surface must be locked using
 *       \ref NvMediaVideoSurfaceLock prior to calling this function.
 * \param[in] srcRect
 *       Structure containing co-ordinates of the rectangle in the source surface
 *       from which the client surface is to be copied. Setting srcRect to NULL
 *       implies rectangle of full surface size.
 * \param[out] dstPntrs
 *       Array of pointers to the destination surface planes
 * \param[in] dstPitches
 *       Array of pitch values for the destination surface planes
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameters is invalid.
 * \n \ref NVMEDIA_STATUS_ERROR if videoSurface is not locked.
 */
NvMediaStatus
NvMediaVideoSurfaceGetBits(
    NvMediaVideoSurface *videoSurface,
    NvMediaRect *srcRect,
    void **dstPntrs,
    unsigned int *dstPitches
);

/*@}*/

/**
 * \defgroup palette_api Palette
 * Defines and manages objects that provide device-specific RGBA color palettes.
 * @{
 */

/**
 * \brief A handle representing a palette object.
 */
typedef void NvMediaPalette;

/**
 * \brief Create a 256 element RGBA palette suitable for use with
 * \ref NvMediaVideoMixer.
 * \param[in] device The device that will contain the palette.
 * \return NvMediaPalette The new palette's handle or NULL if unsuccessful.
 */
NvMediaPalette *
NvMediaPaletteCreate(
    NvMediaDevice *device
);

/**
 * \brief Destroy a palette created by \ref NvMediaPaletteCreate.
 * \param[in] palette The palette to be destroyed.
 * \return void
 */
void
NvMediaPaletteDestroy(
  NvMediaPalette *palette
);

/**
 * \brief Upload the palette data.
 * \param[in] device Required for the HW channel used for the upload.
 * \param[in] palette The \ref NvMediaPalette being updated.
 * \param[in] rgba A 256-element array of unsigned int RGBA values. Each 32-bit
 * value has R, G, B, and A bytes packed from LSB to MSB.
 * \return void
 */

void
NvMediaPaletteLoad(
   NvMediaDevice *device,
   NvMediaPalette *palette,
   unsigned int *rgba
);

/*@}*/

/**
 * \defgroup videooutput_api Video Output
 *
 * Declares and manages objects for defining the locations of the composed surfaces.
 *
 * The NvMediaVideoOutput object determines where the composed surfaces
 * are going to be displayed.
 *
 * @{
 */

/**
 * \hideinitializer
 * \brief Mask for video output device 0
 */
#define NVMEDIA_OUTPUT_DEVICE_0     (1<<0)

/**
 * \hideinitializer
 * \brief Mask for video output device 1
 */
#define NVMEDIA_OUTPUT_DEVICE_1     (1<<1)

/**
 * \hideinitializer
 * \brief Mask for video output device 2
 */
#define NVMEDIA_OUTPUT_DEVICE_2     (1<<2)

/**
 * \brief Determines the type of video output.
 *
 */
typedef enum {
    /** Display overlay */
    NvMediaVideoOutputType_Overlay,
    /** KD manager */
    NvMediaVideoOutputType_KD,
    /** KD manager for RGB and Overlay for YUV surfaces */
    NvMediaVideoOutputType_KDandOverlay,
    /** Display on YUV overlay */
    NvMediaVideoOutputType_OverlayYUV,
    /** Display on RGB overlay */
    NvMediaVideoOutputType_OverlayRGB
} NvMediaVideoOutputType;

/**
 * \brief Determines the output device if the output type is \ref NvMediaVideoOutputType_Overlay.
 */
typedef enum {
    /*! LVDS panel */
    NvMediaVideoOutputDevice_LVDS,
    /*! HDMI monitor */
    NvMediaVideoOutputDevice_HDMI,
    /*! VGA monitor */
    NvMediaVideoOutputDevice_VGA,
    /*! Display 0 */
    NvMediaVideoOutputDevice_Display0,
    /*! Display 1 */
    NvMediaVideoOutputDevice_Display1
} NvMediaVideoOutputDevice;

/**
 * \brief Video output object created by \ref NvMediaVideoOutputCreate.
 */
typedef struct {
    /*! Video output type */
    NvMediaVideoOutputType outputType;
    /*! Video output device */
    NvMediaVideoOutputDevice outputDevice;
    /*! Display width */
    unsigned int width;
    /*! Display height */
    unsigned int height;
    /*! Display refresh rate in Hertz */
    float refreshRate;
} NvMediaVideoOutput;

/**
 * \brief Sets the preferences for the video output creation.
 *  Note: Depending on the video output type these preferences might not be met
 *  and the created video output parameters might be different.
 */
typedef struct {
    /*! Display width */
    unsigned int width;
    /*! Display height */
    unsigned int height;
    /*! Display refresh rate in Hertz */
    float refreshRate;
} NvMediaVideoOutputPreferences;

/**
 * \brief Device parameters returned by \ref NvMediaVideoOutputDevicesQuery function.
 */
typedef struct {
    /*! Video output device */
    NvMediaVideoOutputDevice outputDevice;
    /*! Display identification */
    unsigned int displayId;
    /*! Set to NVMEDIA_TRUE if the device is already enabled (initialized) by the system */
    NvMediaBool enabled;
    /*! Display width */
    unsigned int width;
    /*! Display height */
    unsigned int height;
    /*! Display refresh rate in Hertz */
    float refreshRate;
} NvMediaVideoOutputDeviceParams;

/**
 * \brief Sets video output position and size. This function has to be called after
 * \ref NvMediaVideoOutputCreate but before \ref NvMediaVideoMixerCreate.
 * \param[in] output Video output
 * \param[in] position The rectangle where the video is going to be rendered
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoOutputSetPosition(
   NvMediaVideoOutput *output,
   NvMediaRect *position
);

/**
 * \brief Queries the display system and returns the parameters of all display devices.
 *        If outputParams is NULL then only outputDevices is returned and this way the
 *        client can query the number of devices, allocate memory for the descriptors
 *        and call this function again with the properly allocated outputParams buffer.
 * \param[out] outputDevices Set by the function to the number of display devices.
 * \param[out] outputParams This parameters is used as an array and the function
 *             fills it up with outputDevices number of entries. It is the client's
 *             responsibility to allocate sufficient memory. If NULL no data is returned.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoOutputDevicesQuery(
   int *outputDevices,
   NvMediaVideoOutputDeviceParams *outputParams
);

/**
 * \brief Creates a video output
 * \param[in] outputType Video ouput type
 * \param[in] outputDevice Video output device
 * \param[in] outputPreference Desired video output parameters
 * \param[in] alreadyCreated Set to \ref NVMEDIA_TRUE if the video output is already created
 *            by another application. In this case NvMedia is not going to create a new one
 *            just reuse it.
 * \param[in] displayId If alreadyCreated is set to \ref NVMEDIA_TRUE then this indicates
 *            the already created video output id in case of Overlay.
 * \param[in] displayHandle If alreadyCreated is set to \ref NVMEDIA_TRUE then this indicates
 *            the already created display handle in case of KD.
 * \return \ref NvMediaVideoOutput The new video output's handle or NULL if unsuccessful.
 *            The width, hight and refreshRate members reflect the actual video output values.
 */
NvMediaVideoOutput *
NvMediaVideoOutputCreate(
   NvMediaVideoOutputType outputType,
   NvMediaVideoOutputDevice outputDevice,
   NvMediaVideoOutputPreferences *outputPreference,
   NvMediaBool alreadyCreated,
   unsigned int displayId,
   void *displayHandle
);

/**
 * \brief Destroy a video output created by \ref NvMediaVideoOutputCreate.
 * \param[in] output The video output to be destroyed.
 * \return void
 */
void
NvMediaVideoOutputDestroy(
    NvMediaVideoOutput *output
);

/*@}*/

/**
 * \defgroup mixer_api Video Mixer
 * Defines multiple objects for specifying video mix,
 * which is primarily for converting YUV data to RGB.
 * @{
 */

/**
 * \defgroup mixer_features Mixer features
 * Declares and manages objects for specifying mixer features.
 * @{
 */

/**
 * \hideinitializer
 * \brief Background is used in the video mixer.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_BACKGROUND_PRESENT              (1<<0)

/**
 * \hideinitializer
 * \brief Primary video is interlaced so it needs deiterlacing.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_PRIMARY_VIDEO_DEINTERLACING     (1<<1)

/**
 * \hideinitializer
 * \brief Secondary video is used in the video mixer.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_SECONDARY_VIDEO_PRESENT         (1<<2)

/**
 * \hideinitializer
 * \brief Secondary video is interlaced so it needs deiterlacing.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_SECONDARY_VIDEO_DEINTERLACING   (1<<3)

/**
 * \hideinitializer
 * \brief Graphics layer 0 is present and in RGBA format
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_RGBA_PRESENT         (1<<4)

/**
 * \hideinitializer
 * \brief Graphics layer 0 is present and in I8 format
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_I8_PRESENT           (1<<5)

/**
 * \hideinitializer
 * \brief Graphics layer 0 has premultiplied alpha
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_PREMULTIPLIED_ALPHA  (1<<6)

/**
 * \hideinitializer
 * \brief Graphics layer 1 is present and in RGBA format
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_RGBA_PRESENT         (1<<7)

/**
 * \hideinitializer
 * \brief Graphics layer 1 is present and in I8 format
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_I8_PRESENT           (1<<8)

/**
 * \hideinitializer
 * \brief Graphics layer 1 has premultiplied alpha
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_PREMULTIPLIED_ALPHA  (1<<9)

/**
 * \hideinitializer
 * \brief Enable support for advanced1 de-interlacing for Primary Video
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_PRIMARY_DEINTERLACING  (1<<10)

/**
 * \hideinitializer
 * \brief Enable support for advanced1 de-interlacing for Secondary Video
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_SECONDARY_DEINTERLACING (1<<11)

/**
 * \hideinitializer
 * \brief Enable DVD mixing mode. In this mode there can be only one video and
 * one graphics layer. Graphics layer can be either RGBA or I8. No secondary video
 * or background graphics are supported.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_DVD_MIXING_MODE                 (1<<12)

/**
 * \hideinitializer
 * \brief Set the video input surface type to be \ref NvMediaSurfaceType_YV16.
 * No graphics, secondary video or background graphics are supported with this surface type.
 * The sent video surface type must be \ref NvMediaSurfaceType_YV16.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_YV16         (1<<13)

/**
 * \hideinitializer
 * \brief Set the video input surface type to be \ref NvMediaSurfaceType_YV16x2.
 * No graphics, secondary video or background graphics are supported with this surface type.
 * The sent video surface type must be \ref NvMediaSurfaceType_YV16x2.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_YV16X2       (1<<14)

/**
 * \hideinitializer
 * \brief Set the video input surface type to be \ref NvMediaSurfaceType_YV24.
 * No graphics, secondary video or background graphics are supported with this surface type.
 * The sent video surface type must be \ref NvMediaSurfaceType_YV24.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_YV24         (1<<15)

/**
 * \hideinitializer
 * \brief Set the video input surface type to be \ref NvMediaSurfaceType_R8G8B8A8 or
 * \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin.
 * No graphics, secondary video or background graphics are supported with this surface type.
 * The sent video surface type must be \ref NvMediaSurfaceType_R8G8B8A8 or
 * \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_VIDEO_SURFACE_TYPE_R8G8B8A8     (1<<16)

/**
 * \hideinitializer
 * \brief Set the mixer to surface rendering mode. In this mode no output
 * device is needed and an output device cannot be bound to the mixer.
 * The output surface type must be \ref NvMediaSurfaceType_R8G8B8A8 or
 * \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin.
 */
#define NVMEDIA_VIDEO_MIXER_FEATURE_SURFACE_RENDERING               (1<<17)

/*@}*/

/**
 * \defgroup mixer_attributes Mixer attributes
 * Defines mixer attribute bit masks for constructing attribute masks.
 * @{
 */

/**
 * \hideinitializer
 * \brief Brightness
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_BRIGHTNESS                    (1<<0)
/**
 * \hideinitializer
 * \brief Contrast
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_CONTRAST                      (1<<1)
/**
 * \hideinitializer
 * \brief Sturation
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_SATURATION                    (1<<2)
/**
 * \hideinitializer
 * \brief Hue
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_HUE                           (1<<3)
/**
 * \hideinitializer
 * \brief Color Standard for Primary video
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_PRIMARY        (1<<4)
/**
 * \hideinitializer
 * \brief Color Standard for Primary video
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_SECONDARY      (1<<5)
/**
 * \hideinitializer
 * \brief Deintelacing type for Primary Video
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_PRIMARY      (1<<6)
/**
 * \hideinitializer
 * \brief Deintelacing type for Secondary Video
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_SECONDARY    (1<<7)
/**
 * \hideinitializer
 * \brief Graphics 0 destination rectangle
 */
#define NVMEDIA_VIDEO_MIXER_ATTRIBUTE_GRAPHICS0_DST_RECT            (1<<8)
/*@}*/

/**
 * \brief A handle representing a video mixer object.
 */
typedef void NvMediaVideoMixer;

/** \brief Color standard
 */
typedef enum {
/** \hideinitializer \brief ITU BT.601 color standard. */
    NVMEDIA_COLOR_STANDARD_ITUR_BT_601,
/** \hideinitializer \brief ITU BT.709 color standard. */
    NVMEDIA_COLOR_STANDARD_ITUR_BT_709,
/** \hideinitializer \brief SMTE 240M color standard. */
    NVMEDIA_COLOR_STANDARD_SMPTE_240M
} NvMediaColorStandard;

/** \brief Deinterlace type
 */
typedef enum {
/** \hideinitializer \brief BOB deinterlacing. */
    NVMEDIA_DEINTERLACE_TYPE_BOB = 0,
/** \hideinitializer \brief Advanced1 deinterlacing. */
    NVMEDIA_DEINTERLACE_TYPE_ADVANCED1 = 1
} NvMediaDeinterlaceType;

/** \brief Picture structure type
 */
typedef enum {
/** \hideinitializer \brief The picture is a field, and is the top field of the surface. */
    NVMEDIA_PICTURE_STRUCTURE_TOP_FIELD = 0x1,
/** \hideinitializer \brief The picture is a field, and is the bottom field of the surface. */
    NVMEDIA_PICTURE_STRUCTURE_BOTTOM_FIELD = 0x2,
/** \hideinitializer \brief The picture is a frame, and hence is the entire surface. */
    NVMEDIA_PICTURE_STRUCTURE_FRAME = 0x3
} NvMediaPictureStructure;

/** \brief Background type
 */
typedef enum {
/** \hideinitializer \brief background is a solid color. */
    NVMEDIA_BACKGROUND_TYPE_SOLID_COLOR,
/** \hideinitializer \brief background is graphics plane. */
    NVMEDIA_BACKGROUND_TYPE_GRAPHICS
} NvMediaBackgroundType;

/**
 * \brief The principle job of the video mixer is to convert YUV data to
 * RGB, composite graphics layers and perform other postprocessing at the same time, such as
 * deinterlacing.
 *
 * \param[in] device The \ref NvMediaDevice "device" this video mixer will use.
 * \param[in] mixerWidth Video mixer width
 * \param[in] mixerHeight Video mixer height
 * \param[in]  sourceAspectRatio Aspect ratio of the source primary video.
 * This determines how the video is going to be presented on the output surface.
 * In case of Blu-Ray this aspect ratio is always 16:9, for DVD it is either 16: or 4:3.
 * \param[in] primaryVideoWidth Primary video width
 * \param[in] primaryVideoHeight Primary video height
 * \param[in] secondaryVideoWidth Secondary video width if exists, otherwise set to 0
 * \param[in] secondaryVideoHeight Secondary video height if exists, otherwise set to 0
 * \param[in] graphics0Width Width of grapics layer 0 if exists, otherwise set to 0
 * \param[in] graphics0Height Height of grapics layer 0 if exists, otherwise set to 0
 * \param[in] graphics1Width Width of grapics layer 1 if exists, otherwise set to 0
 * \param[in] graphics1Height Height of grapics layer 1 if exists, otherwise set to 0
 * \param[in] features  This selects which features this \ref NvMediaVideoMixer will
 * support. This determines the internal scratch surfaces
 * NvMediaVideoMixerCreate will create up-front. At present,
 * the following features are supported and may be OR'd together:
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_BACKGROUND_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_PRIMARY_VIDEO_DEINTERLACING
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_SECONDARY_VIDEO_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_SECONDARY_VIDEO_DEINTERLACING
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_RGBA_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_0_I8_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_RGBA_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_1_I8_PRESENT
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_PRIMARY_DEINTERLACING
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_ADVANCED1_SECONDARY_DEINTERLACING
 *  \n \ref NVMEDIA_VIDEO_MIXER_FEATURE_SURFACE_RENDERING
 * \param[in] outputList A NULL terminated list of output devices created by
 * \ref NvMediaVideoOutputCreate. These devices will be associated with the mixer
 * and can be used for output rendering. If multiple devices are given then the order
 * determines the device ID that will be used as an input parameter for the
 * \ref NvMediaVideoMixerRender  and \ref NvMediaVideoMixerSetAttributes functions.
 * If the \ref NVMEDIA_VIDEO_MIXER_FEATURE_SURFACE_RENDERING mode is specified
 * not output device can be attached to the mixer.
 * \return \ref NvMediaVideoMixer The new video mixer's handle or NULL if unsuccessful.
 */
NvMediaVideoMixer *
NvMediaVideoMixerCreate(
    NvMediaDevice *device,
    unsigned short mixerWidth,
    unsigned short mixerHeight,
    float          sourceAspectRatio,
    unsigned short primaryVideoWidth,
    unsigned short primaryVideoHeight,
    unsigned short secondaryVideoWidth,
    unsigned short secondaryVideoHeight,
    unsigned short graphics0Width,
    unsigned short graphics0Height,
    unsigned short graphics1Width,
    unsigned short graphics1Height,
    unsigned int   features,
    NvMediaVideoOutput **outputList
);

/**
 * \brief Destroy a mixer created by \ref NvMediaVideoMixerCreate.
 * \param[in] mixer The mixer to be destroyed.
 * \return void
 */
void
NvMediaVideoMixerDestroy(
    NvMediaVideoMixer *mixer
);

/**
 * \brief Video mixer attributes
 */
typedef struct {
/** \brief A value clamped to between -0.5 and 0.5, initialized to
 * 0.0 at NvMediaVideoMixer creation.  The corresponding
 * attribute mask is \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_BRIGHTNESS.
 */
    float brightness;
/** \brief A value clamped to between 0.1 and 2.0, initialized to
 * 1.0 at NvMediaVideoMixer creation.  The corresponding
 * attribute mask is \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_CONTRAST.
 */
    float contrast;
/** \brief A value clamped to between 0.1 and 2.0, initialized to
 * 1.0 at NvMediaVideoMixer creation.  The corresponding
 * attribute mask is \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_SATURATION.
 */
    float saturation;
/** \brief A value clamped to between -PI and PI, initialized to
 * 0.0 at NvMediaVideoMixer creation.  The corresponding
 * attribute mask is \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_HUE.
 */
    float hue;
/** \brief Color standard for primary video. One of the following:
 * \n \ref NVMEDIA_COLOR_STANDARD_ITUR_BT_601 (default)
 * \n \ref NVMEDIA_COLOR_STANDARD_ITUR_BT_709
 * \n \ref NVMEDIA_COLOR_STANDARD_SMPTE_240M
 * \n The corresponding attribute mask is
 * \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_PRIMARY.
 */
    NvMediaColorStandard colorStandardPrimary;
/** \brief Color standard for secondary video. One of the following:
 * \n \ref NVMEDIA_COLOR_STANDARD_ITUR_BT_601 (default)
 * \n \ref NVMEDIA_COLOR_STANDARD_ITUR_BT_709
 * \n \ref NVMEDIA_COLOR_STANDARD_SMPTE_240M
 * \n The corresponding attribute mask is
 * \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_SECONDARY.
 */
    NvMediaColorStandard colorStandardSecondary;
/** \brief Deintelacing type for Primary Video. One of the following:
 * \n \ref NVMEDIA_DEINTERLACE_TYPE_BOB (default)
 * \n \ref NVMEDIA_DEINTERLACE_TYPE_ADVANCED1
 * \n The corresponding attribute mask is
 * \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_PRIMARY.
 */
    NvMediaDeinterlaceType primaryDeinterlaceType;
/** \brief Deintelacing type for Secondary Video. One of the following:
 * \n \ref NVMEDIA_DEINTERLACE_TYPE_BOB (default)
 * \n \ref NVMEDIA_DEINTERLACE_TYPE_ADVANCED1
 * \n The corresponding attribute mask is
 * \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_SECONDARY.
 */
    NvMediaDeinterlaceType secondaryDeinterlaceType;
/** \brief Destination rectangle for Graphics 0. This determines the
 * position of the graphics plane relative to the mixer plane.
 * This functionality is only valid when the mixer was created with
 * \ref NVMEDIA_VIDEO_MIXER_FEATURE_DVD_MIXING_MODE. Default rectangle is
 * the full mixer surface.
 * \n The corresponding attribute mask is
 * \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_GRAPHICS0_DST_RECT.
 */
    NvMediaRect graphics0DstRect;
} NvMediaVideoMixerAttributes;

/** \brief Change NvMediaVideoMixer attributes.
 *
 * \param[in] mixer The mixer object that will perform the
 *       mixing/rendering operation.
 * \param[in] outputDeviceMask Determines which output device is used for setting the attribute.
 *       The value is the binary OR of any combination of the following values. Each device is
 *       represented with a bit mask. The order of devices matches the order of video output
 *       devices listed in the outputList when the mixer was created.
 * \n \ref NVMEDIA_OUTPUT_DEVICE_0
 * \n \ref NVMEDIA_OUTPUT_DEVICE_1
 * \n \ref NVMEDIA_OUTPUT_DEVICE_2
 * \param[in] attributeMask Determines which attributes are set. The value
 *       can be any combination of the binary OR of the following attributes:
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_BRIGHTNESS
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_CONTRAST
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_SATURATION
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_HUE
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_PRIMARY
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_COLOR_STANDARD_SECONDARY
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_PRIMARY
 * \n \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_DEINTERLACE_TYPE_SECONDARY
 * \param[in] attributes A pointer to a structure that holds all the
 *        attributes but only those are used which are indicated in the
 *        attributeMask.
 */
void
NvMediaVideoMixerSetAttributes(
   NvMediaVideoMixer *mixer,
   unsigned int outputDeviceMask,
   unsigned int attributeMask,
   NvMediaVideoMixerAttributes *attributes
);

/**
 * \brief Bind an output to a mixer.
 * \param[in] mixer Mixer object
 * \param[in] output Video output
 * \param[in] outputDeviceMask Determines the mask that will refer to this
 *            to this output during mixer rendering calls.
 * \n \ref NVMEDIA_OUTPUT_DEVICE_0
 * \n \ref NVMEDIA_OUTPUT_DEVICE_1
 * \n \ref NVMEDIA_OUTPUT_DEVICE_2
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoMixerBindOutput(
     NvMediaVideoMixer *mixer,
     NvMediaVideoOutput *output,
     unsigned int outputDeviceMask
);

/**
 * \brief Unbind an output from a mixer.
 * \param[in] mixer Mixer object
 * \param[in] output Video output
 * \param[in] releaseList Points to an array of \ref NvMediaVideoSurface pointers filled by this
 *       function and terminated with NULL. During previous render call the mixer migth hold
 *       buffers and during unbind these buffers are released.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoMixerUnbindOutput(
     NvMediaVideoMixer *mixer,
     NvMediaVideoOutput *output,
     NvMediaVideoSurface **releaseList
);

/**
 * \brief Background descriptor for video mixing.
 */
typedef struct {
    /** \brief Background type. Possible values are:
     * \n \ref NVMEDIA_BACKGROUND_TYPE_SOLID_COLOR Background is a solid color
     * \n \ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS Background is a graphics image
     */
    NvMediaBackgroundType type;
    /** \brief Source surface data if the type is \ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS. */
    unsigned char *sourceSurface;
    /** \brief Source surface pitch if the type is \ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS. */
    unsigned int pitch;
    /** \brief Source rectangle if the type is \ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS. This rectangle gets scaled to mixer size.
        If NULL, a rectangle the full size of the background surface is implied.
    */
    NvMediaRect *srcRect;
    /** \brief A NULL terminated array of rectangles that need update if the type is \ref NVMEDIA_BACKGROUND_TYPE_GRAPHICS. */
    NvMediaRect **updateRectangleList;
    /** \brief Background color if the type is \ref NVMEDIA_BACKGROUND_TYPE_SOLID_COLOR. */
    NvMediaColor backgroundColor;
} NvMediaBackground;

/**
 * \brief Primary video descriptor for video mixing.
 */
typedef struct {
    /** \brief Picture structure */
    NvMediaPictureStructure pictureStructure;
    /** \brief Frame/field that follow the current frame/field, NULL if unavailable. */
    NvMediaVideoSurface *next;
    /** \brief Current frame/field. */
    NvMediaVideoSurface *current;
    /** \brief Frame/field prior to the current frame/field, NULL if unavailable */
    NvMediaVideoSurface *previous;
    /** \brief Frame/field prior to previous frame/field, NULL if unavailable. */
    NvMediaVideoSurface *previous2;
    /** \brief Source rectangle, If NULL, a rectangle the full size of the \ref NvMediaVideoSurface is implied. */
    NvMediaRect *srcRect;
    /** \brief Destination rectangle, If NULL, a rectangle the full size of the \ref NvMediaVideoMixer is implied.  */
    NvMediaRect *dstRect;
} NvMediaPrimaryVideo;

/**
 * \brief Secondary video descriptor for video mixing.
 */
typedef struct {
    /** \brief Picture structure */
    NvMediaPictureStructure pictureStructure;
    /** \brief Frame/field that follow the current frame/field, NULL if unavailable */
    NvMediaVideoSurface *next;
    /** \brief Current frame/field */
    NvMediaVideoSurface *current;
    /** \brief Frame/field prior to the current frame/field, NULL if unavailable */
    NvMediaVideoSurface *previous;
    /** \brief Frame/field prior to previous frame/field, NULL if unavailable. */
    NvMediaVideoSurface *previous2;
    /** \brief Source rectangle, If NULL, a rectangle the full size of the \ref NvMediaVideoSurface is implied. */
    NvMediaRect *srcRect;
    /** \brief Destination rectangle, If NULL, a rectangle the full size of the \ref NvMediaVideoMixer is implied.  */
    NvMediaRect *dstRect;
    /** \brief Luma keying is used. */
    NvMediaBool lumaKeyUsed;
    /** \brief Luma key. Pixels with luma values below or equal to this level will be transparent on the secondary video layer. */
    unsigned char lumaKey;
} NvMediaSecondaryVideo;

/**
 * \brief Graphics descriptor for video mixing.
 */
typedef struct {
    /** \brief Source surface data. */
    unsigned char *sourceSurface;
    /** \brief Source surface pitch. */
    unsigned int pitch;
    /** \brief A palette created by \ref NvMediaPaletteCreate if
     * NVMEDIA_VIDEO_MIXER_FEATURE_GRAPHICS_X_I8_PRESENT feature was specified at
     * video mixer creation.
     */
    NvMediaPalette *palette;
    /** \brief Source rectangle. This rectangle gets scaled to mixer size.
        If NULL, a rectangle the full size of the graphics surface is implied.
    */
    NvMediaRect *srcRect;
    /** \brief A NULL terminated array of rectangles that need update. */
    NvMediaRect **updateRectangleList;
} NvMediaGraphics;

/**
 * \brief Perform a video post-processing and compositing
 *        operation.
 * \param[in] mixer The mixer object that will perform the
 *       mixing/rendering operation.
 * \param[in] outputDeviceMask Determines which output device is used for setting the attribute.
 *       The value is the binary OR of any combination of the following values. Each device is
 *       represented with a bit mask. The order of devices matches the order of video output
 *       devices listed in the outputList when the mixer was created.
 * \n \ref NVMEDIA_OUTPUT_DEVICE_0
 * \n \ref NVMEDIA_OUTPUT_DEVICE_1
 * \n \ref NVMEDIA_OUTPUT_DEVICE_2
 * \param[in] background A background color or image. If set to any value other
 *       than NULL, the specific color or surface will be used as the first layer
 *       in the mixer's compositing process.
 * \param[in] primaryVideo Primary video descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the second layer
 *       in the mixer's compositing process.
 * \param[in] secondaryVideo Secondary video descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the third layer
 *       in the mixer's compositing process.
 * \param[in] graphics0 Graphics layer 0 descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the fourth layer
 *       in the mixer's compositing process.
 * \param[in] graphics1 Graphics layer 1 descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the fifth layer
 *       in the mixer's compositing process.
 * \param[in] releaseList Points to an array of \ref NvMediaVideoSurface pointers filled by this
 *       function and terminated with NULL.
 *       When \ref NvMediaVideoMixerRender function is called it might need to use some of the
 *       input video buffers for a longer time depending on the pipelining model.
 *       When the buffers are no longer needed they are released using this list.
 *       It is the application responsibility to allocate the memory for the list.
 *       The maximum size of the list is the number of possible surfaces passed to
 *       this function plus one for the NULL terminator.
 * \param[in] timeStamp Determines when the composited image is going to be displayed.
 *       This is a pointer to the time structure. NULL means the composited image
 *       is displayed as soon as possible.
 * \return \ref NvMediaStatus Status of the mixing operation. Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoMixerRender(
    NvMediaVideoMixer *mixer,
    unsigned int outputDeviceMask,
    NvMediaBackground *background,
    NvMediaPrimaryVideo *primaryVideo,
    NvMediaSecondaryVideo *secondaryVideo,
    NvMediaGraphics *graphics0,
    NvMediaGraphics *graphics1,
    NvMediaVideoSurface **releaseList,
    NvMediaTime *timeStamp
);

/**
 * \brief Perform a video post-processing and compositing
 *        operation to a surface
 * \param[in] mixer The mixer object that will perform the
 *       mixing/rendering operation.
 * \param[in] outputSurface Destination surface. This surface type must be
 *       \ref NvMediaSurfaceType_R8G8B8A8 or \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin type.
 * \param[in] background A background color or image. If set to any value other
 *       than NULL, the specific color or surface will be used as the first layer
 *       in the mixer's compositing process.
 * \param[in] primaryVideo Primary video descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the second layer
 *       in the mixer's compositing process.
 * \param[in] secondaryVideo Secondary video descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the third layer
 *       in the mixer's compositing process.
 * \param[in] graphics0 Graphics layer 0 descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the fourth layer
 *       in the mixer's compositing process.
 * \param[in] graphics1 Graphics layer 1 descriptor structure. If set to any value other
 *       than NULL, the specific video descriptor will be used as the fifth layer
 *       in the mixer's compositing process.
 * \return \ref NvMediaStatus Status of the mixing operation. Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoMixerRenderSurface(
    NvMediaVideoMixer *mixer,
    NvMediaVideoSurface *outputSurface,
    NvMediaBackground *background,
    NvMediaPrimaryVideo *primaryVideo,
    NvMediaSecondaryVideo *secondaryVideo,
    NvMediaGraphics *graphics0,
    NvMediaGraphics *graphics1
);
/*@}*/

/**
 * \defgroup decoder_api Video Decoder
 * Defines and manages objects that decode video.
 *
 * The NvMediaVideoDecoder object decodes compressed video data, writing
 * the results to a \ref NvMediaVideoSurface "NvMediaVideoSurface".
 *
 * A specific NvMedia implementation may support decoding multiple
 * types of compressed video data. However, NvMediaVideoDecoder objects
 * are able to decode a specific type of compressed video data.
 * This type must be specified during creation.
 *
 * @{
 */

/**
 * \brief Application data buffer containing compressed video
 *        data.
 */
typedef struct {
    /** A pointer to the bitstream data bytes */
    unsigned char *bitstream;
    /** The number of data bytes */
    unsigned int bitstreamBytes;
} NvMediaBitstreamBuffer;

/**
 * \brief A generic "picture information" pointer type.
 *
 * This type serves solely to document the expected usage of a
 * generic (void *) function parameter. In actual usage, the
 * application is expected to physically provide a pointer to an
 * instance of one of the "real" NvMediaPictureInfo* structures,
 * picking the type appropriate for the decoder object in
 * question.
 */
typedef void NvMediaPictureInfo;

/**
 * \brief Video codec type
 */
typedef enum {
    /** \brief H.264 codec */
    NVMEDIA_VIDEO_CODEC_H264,
    /** \brief VC-1 simple and main profile codec */
    NVMEDIA_VIDEO_CODEC_VC1,
    /** \brief VC-1 advanced profile codec */
    NVMEDIA_VIDEO_CODEC_VC1_ADVANCED,
    /** \brief MPEG1 codec */
    NVMEDIA_VIDEO_CODEC_MPEG1,
    /** \brief MPEG2 codec */
    NVMEDIA_VIDEO_CODEC_MPEG2,
    /** \brief MPEG4 Part 2 codec */
    NVMEDIA_VIDEO_CODEC_MPEG4,
    /** \brief MJPEG codec */
    NVMEDIA_VIDEO_CODEC_MJPEG
} NvMediaVideoCodec;

/**
 * \brief A handle representing a video decoder object.
 */
typedef struct {
    /** \brief Codec type */
    NvMediaVideoCodec codec;
    /** \brief Decoder width */
    unsigned short width;
    /** \brief Decoder height */
    unsigned short height;
    /** \brief Maximum number of reference pictures */
    unsigned short maxReferences;
} NvMediaVideoDecoder;

/** \brief Create a video decoder object.
 *
 * Create a \ref NvMediaVideoDecoder object for the specified codec.  Each
 * decoder object may be accessed by a separate thread.  The object
 * is to be destroyed with \ref NvMediaVideoDecoderDestroy().  All surfaces
 * used with the NvMediaVideoDecoder must be of one of the following type:
 * \n \ref NvMediaSurfaceType_YV12
 * \n \ref NvMediaSurfaceType_YV16
 * \n \ref NvMediaSurfaceType_YV24
 *
 * \param[in] codec Codec type. The following are supported:
 * \n \ref NVMEDIA_VIDEO_CODEC_H264
 * \n \ref NVMEDIA_VIDEO_CODEC_VC1
 * \n \ref NVMEDIA_VIDEO_CODEC_VC1_ADVANCED
 * \n \ref NVMEDIA_VIDEO_CODEC_MPEG1
 * \n \ref NVMEDIA_VIDEO_CODEC_MPEG2
 * \n \ref NVMEDIA_VIDEO_CODEC_MPEG4
 * \n \ref NVMEDIA_VIDEO_CODEC_MJPEG
 * \param[in] width Decoder width in luminance pixels.
 * \param[in] height Decoder height in luminance pixels.
 * \param[in] maxReferences The maximum number of reference frames used.
 * This will limit internal allocations.
 * \param[in] maxBitstreamSize The maximum size for bitstream.
 * This will limit internal allocations.
 * \param[in] inputBuffering How many frames can be in flight at any given
 * time. If this value is one, NvMediaVideoDecoderRender() will block until the
 * previous frame has finished decoding. If this is two, NvMediaVideoDecoderRender()
 * will block if two frames are pending but will not block if one is pending.
 * This value is clamped internally to between 1 and 8.
 * \return NvMediaVideoDecoder The new video decoder's handle or NULL if unsuccessful.
 */
NvMediaVideoDecoder *
NvMediaVideoDecoderCreate(
    NvMediaVideoCodec codec,
    unsigned short width,
    unsigned short height,
    unsigned short maxReferences,
    unsigned long maxBitstreamSize,
    unsigned char inputBuffering
);

/** \brief Destroy a video decoder object.
 * \param[in] decoder The decoder to be destroyed.
 * \return void
 */
void
NvMediaVideoDecoderDestroy(
   NvMediaVideoDecoder *decoder
);

/**
 * \brief Decode a compressed field/frame and render the result
 *        into a \ref NvMediaVideoSurface "NvMediaVideoSurface".
 * \param[in] decoder The decoder object that will perform the
 *       decode operation.
 * \param[in] target The video surface to render to.
 * \param[in] pictureInfo A (pointer to a) structure containing
 *       information about the picture to be decoded. Note that
 *       the appropriate type of NvMediaPictureInfo* structure must
 *       be provided to match to profile that the decoder was
 *       created for.
 * \param[in] numBitstreamBuffers The number of bitstream
 *       buffers containing compressed data for this picture.
 * \param[in] bitstreams An array of bitstream buffers.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER
 */
NvMediaStatus
NvMediaVideoDecoderRender(
    NvMediaVideoDecoder *decoder,
    NvMediaVideoSurface *target,
    NvMediaPictureInfo *pictureInfo,
    unsigned int numBitstreamBuffers,
    const NvMediaBitstreamBuffer *bitstreams
);

/**
 * \defgroup h264decoder_api H.264 structures
 * Provides structures for defining the H.264 reference frame.
 * @{
 */

/**
 * \brief Information about an H.264 reference frame
 *
 * Note: References to "copy of bitstream field" in the field descriptions
 * may refer to data literally parsed from the bitstream, or derived from
 * the bitstream using a mechanism described in the specification.
 */
typedef struct {
    /**
     * The surface that contains the reference image.
     * Set to NULL for unused entries.
     */
    NvMediaVideoSurface *surface;
    /** Is this a long term reference (else short term). */
    NvMediaBool         is_long_term;
    /**
     * Is the top field used as a reference.
     * Set to NVMEDIA_FALSE for unused entries.
     */
    NvMediaBool         top_is_reference;
    /**
     * Is the bottom field used as a reference.
     * Set to NVMEDIA_FALSE for unused entries.
     */
    NvMediaBool         bottom_is_reference;
    /** [0]: top, [1]: bottom */
    int                 field_order_cnt[2];
    /**
     * Copy of the H.264 bitstream field:
     * frame_num from slice_header for short-term references,
     * LongTermPicNum from decoding algorithm for long-term references.
     */
    unsigned short     FrameIdx;
} NvMediaReferenceFrameH264;

/**
 * \brief Picture parameter information for an H.264 picture.
 *
 * Note: The \ref referenceFrames array must contain the "DPB" as
 * defined by the H.264 specification. In particular, once a
 * reference frame has been decoded to a surface, that surface must
 * continue to appear in the DPB until no longer required to predict
 * any future frame. Once a surface is removed from the DPB, it can
 * no longer be used as a reference, unless decoded again.
 *
 * Also note that only surfaces previously generated using \ref
 * NvMediaVideoDecoderRender may be used as reference frames.
 *
 * Note: References to "copy of bitstream field" in the field descriptions
 * may refer to data literally parsed from the bitstream, or derived from
 * the bitstream using a mechanism described in the specification.
 */
typedef struct {
    /** [0]: top, [1]: bottom */
    int            field_order_cnt[2];
    /** Will the decoded frame be used as a reference later. */
    NvMediaBool    is_reference;

    /** Copy of the H.264 bitstream field. */
    unsigned short chroma_format_idc;
    /** Copy of the H.264 bitstream field. */
    unsigned short frame_num;
    /** Copy of the H.264 bitstream field. */
    unsigned char  field_pic_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  bottom_field_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  num_ref_frames;
    /** Copy of the H.264 bitstream field. */
    unsigned char  mb_adaptive_frame_field_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  constrained_intra_pred_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  weighted_pred_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  weighted_bipred_idc;
    /** Copy of the H.264 bitstream field. */
    unsigned char  frame_mbs_only_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  transform_8x8_mode_flag;
    /** Copy of the H.264 bitstream field. */
    char           chroma_qp_index_offset;
    /** Copy of the H.264 bitstream field. */
    char           second_chroma_qp_index_offset;
    /** Copy of the H.264 bitstream field. */
    char           pic_init_qp_minus26;
    /** Copy of the H.264 bitstream field. */
    unsigned char  num_ref_idx_l0_active_minus1;
    /** Copy of the H.264 bitstream field. */
    unsigned char  num_ref_idx_l1_active_minus1;
    /** Copy of the H.264 bitstream field. */
    unsigned char  log2_max_frame_num_minus4;
    /** Copy of the H.264 bitstream field. */
    unsigned char  pic_order_cnt_type;
    /** Copy of the H.264 bitstream field. */
    unsigned char  log2_max_pic_order_cnt_lsb_minus4;
    /** Copy of the H.264 bitstream field. */
    unsigned char  delta_pic_order_always_zero_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  direct_8x8_inference_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  entropy_coding_mode_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  pic_order_present_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  deblocking_filter_control_present_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  redundant_pic_cnt_present_flag;
    /** Copy of the H.264 bitstream field. */
    unsigned char  num_slice_groups_minus1;
    /** Copy of the H.264 bitstream field. */
    unsigned char  slice_group_map_type;
    /** Copy of the H.264 bitstream field. */
    unsigned int   slice_group_change_rate_minus1;
    /** Slice group map */
    unsigned char *slice_group_map;
    /** Copy of the H.264 bitstream field. */
    unsigned char fmo_aso_enable;
    /** Copy of the H.264 bitstream field. */
    unsigned char scaling_matrix_present;

    /** Copy of the H.264 bitstream field, converted to raster order. */
    unsigned char  scaling_lists_4x4[6][16];
    /** Copy of the H.264 bitstream field, converted to raster order. */
    unsigned char  scaling_lists_8x8[2][64];

    /** See \ref NvMediaPictureInfoH264 for instructions regarding this field. */
    NvMediaReferenceFrameH264 referenceFrames[16];
} NvMediaPictureInfoH264;

/*@}*/

/**
 * \defgroup mpeg1and2decoder_api MPEG1 & 2 structures
 * Provides a structure for defining the MPEG1 and MPEG2 picture parameter
 * information.
 * @{
 */

/**
 * \brief Picture parameter information for an MPEG 1 or MPEG 2
 *        picture.
 *
 * Note: References to "copy of bitstream field" in the field descriptions
 * may refer to data literally parsed from the bitstream, or derived from
 * the bitstream using a mechanism described in the specification.
 */
typedef struct {
    /**
     * Reference used by B and P frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *forward_reference;
    /**
     * Reference used by B frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *backward_reference;

    /** Copy of the MPEG bitstream field. */
    unsigned char picture_structure;
    /** Copy of the MPEG bitstream field. */
    unsigned char picture_coding_type;
    /** Copy of the MPEG bitstream field. */
    unsigned char intra_dc_precision;
    /** Copy of the MPEG bitstream field. */
    unsigned char frame_pred_frame_dct;
    /** Copy of the MPEG bitstream field. */
    unsigned char concealment_motion_vectors;
    /** Copy of the MPEG bitstream field. */
    unsigned char intra_vlc_format;
    /** Copy of the MPEG bitstream field. */
    unsigned char alternate_scan;
    /** Copy of the MPEG bitstream field. */
    unsigned char q_scale_type;
    /** Copy of the MPEG bitstream field. */
    unsigned char top_field_first;
    /** Copy of the MPEG-1 bitstream field. For MPEG-2, set to 0. */
    unsigned char full_pel_forward_vector;
    /** Copy of the MPEG-1 bitstream field. For MPEG-2, set to 0. */
    unsigned char full_pel_backward_vector;
    /**
     * Copy of the MPEG bitstream field.
     * For MPEG-1, fill both horizontal and vertical entries.
     */
    unsigned char f_code[2][2];
    /** Copy of the MPEG bitstream field, converted to raster order. */
    unsigned char intra_quantizer_matrix[64];
    /** Copy of the MPEG bitstream field, converted to raster order. */
    unsigned char non_intra_quantizer_matrix[64];
} NvMediaPictureInfoMPEG1Or2;

/*@}*/

/**
 * \defgroup mpeg4part2decoder_api MPEG4 Part 2 structures
 * Provides a structure for defining picture parameters for the
 * MPEG-4 Part 2 picture.
 * @{
 */

/**
 * \brief Picture parameter information for an MPEG-4 Part 2 picture.
 *
 * Note: References to "copy of bitstream field" in the field descriptions
 * may refer to data literally parsed from the bitstream, or derived from
 * the bitstream using a mechanism described in the specification.
 */
typedef struct {
    /**
     * Reference used by B and P frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *forward_reference;
    /**
     * Reference used by B frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *backward_reference;

    /** Copy of the bitstream field. */
    int            trd[2];
    /** Copy of the bitstream field. */
    int            trb[2];
    /** Copy of the bitstream field. */
    unsigned short vop_time_increment_resolution;
    /** Copy of the bitstream field. */
    unsigned char  vop_coding_type;
    /** Copy of the bitstream field. */
    unsigned char  vop_fcode_forward;
    /** Copy of the bitstream field. */
    unsigned char  vop_fcode_backward;
    /** Copy of the bitstream field. */
    unsigned char  resync_marker_disable;
    /** Copy of the bitstream field. */
    unsigned char  interlaced;
    /** Copy of the bitstream field. */
    unsigned char  quant_type;
    /** Copy of the bitstream field. */
    unsigned char  quarter_sample;
    /** Copy of the bitstream field. */
    unsigned char  short_video_header;
    /** Derived from vop_rounding_type bitstream field. */
    unsigned char  rounding_control;
    /** Copy of the bitstream field. */
    unsigned char  alternate_vertical_scan_flag;
    /** Copy of the bitstream field. */
    unsigned char  top_field_first;
    /** Copy of the bitstream field. */
    unsigned char  intra_quantizer_matrix[64];
    /** Copy of the bitstream field. */
    unsigned char  non_intra_quantizer_matrix[64];
    /** Copy of the bitstream field. */
    unsigned char  data_partitioned;
    /** Copy of the bitstream field. */
    unsigned char  reversible_vlc;

} NvMediaPictureInfoMPEG4Part2;

/*@}*/

/**
 * \defgroup vc1decoder_api VC1 structures
 * Defines a structure for defining picture information for a VC1 picture.
 * @{
 */

/**
 * \brief Picture parameter information for a VC1 picture.
 *
 * Note: References to "copy of bitstream field" in the field descriptions
 * may refer to data literally parsed from the bitstream, or derived from
 * the bitstream using a mechanism described in the specification.
 */
typedef struct {
    /**
     * Reference used by B and P frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *forward_reference;
    /**
     * Reference used by B frames.
     * Set to NULL when not used.
     */
    NvMediaVideoSurface *backward_reference;

    /** I=0, P=1, B=3, BI=4  from 7.1.1.4. */
    unsigned char  picture_type;
    /** Progressive=0, Frame-interlace=2, Field-interlace=3; see VC-1 7.1.1.15. */
    unsigned char  frame_coding_mode;
    /** Bottom field flag TopField=0 BottomField=1 */
    unsigned char  bottom_field_flag;


    /** Copy of the VC-1 bitstream field. See VC-1 6.1.5. */
    unsigned char postprocflag;
    /** Copy of the VC-1 bitstream field. See VC-1 6.1.8. */
    unsigned char pulldown;
    /** Copy of the VC-1 bitstream field. See VC-1 6.1.9. */
    unsigned char interlace;
    /** Copy of the VC-1 bitstream field. See VC-1 6.1.10. */
    unsigned char tfcntrflag;
    /** Copy of the VC-1 bitstream field. See VC-1 6.1.11. */
    unsigned char finterpflag;
    /** Copy of the VC-1 bitstream field. See VC-1 6.1.3. */
    unsigned char psf;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.8. */
    unsigned char dquant;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.3. */
    unsigned char panscan_flag;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.4. */
    unsigned char refdist_flag;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.11. */
    unsigned char quantizer;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.7. */
    unsigned char extended_mv;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.14. */
    unsigned char extended_dmv;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.10. */
    unsigned char overlap;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.9. */
    unsigned char vstransform;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.5. */
    unsigned char loopfilter;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.6. */
    unsigned char fastuvmc;
    /** Copy of the VC-1 bitstream field. See VC-1 6.12.15. */
    unsigned char range_mapy_flag;
    /** Copy of the VC-1 bitstream field. */
    unsigned char range_mapy;
    /** Copy of the VC-1 bitstream field. See VC-1 6.2.16. */
    unsigned char range_mapuv_flag;
    /** Copy of the VC-1 bitstream field. */
    unsigned char range_mapuv;

    /**
     * Copy of the VC-1 bitstream field. See VC-1 J.1.10.
     * Only used by simple and main profiles.
     */
    unsigned char multires;
    /**
     * Copy of the VC-1 bitstream field. See VC-1 J.1.16.
     * Only used by simple and main profiles.
     */
    unsigned char syncmarker;
    /**
     * VC-1 SP/MP range reduction control. See VC-1 J.1.17.
     * Only used by simple and main profiles.
     */
    unsigned char rangered;
    /**
     * Copy of the VC-1 bitstream field. See VC-1 7.1.13
     * Only used by simple and main profiles.
     */
    unsigned char rangeredfrm;
    /**
     * Copy of the VC-1 bitstream field. See VC-1 J.1.17.
     * Only used by simple and main profiles.
     */
    unsigned char maxbframes;
} NvMediaPictureInfoVC1;
/*@}*/
/*@}*/

/**
 * \defgroup capture_api Video Capture
 *
 * Captures uncompressed video data, writing
 * the results to a \ref NvMediaVideoSurface "NvMediaVideoSurface".
 *
 * A specific NvMedia implementation may support capturing multiple
 * types of uncompressed video data. However, NvMediaVideoCapture objects
 * are able to capture a specific type of uncompressed video data.
 * This type must be specified during creation.
 *
 * @{
 */

/**
 * \hideinitializer
 * \brief Minimun number of capture buffers
 */
#define NVMEDIA_MIN_CAPTURE_FRAME_BUFFERS  2
/**
 * \hideinitializer
 * \brief Maximum number of capture buffers
 */
#define NVMEDIA_MAX_CAPTURE_FRAME_BUFFERS  32

/**
 * \hideinitializer
 * \brief Infinite time-out for \ref NvMediaVideoCaptureGetFrame
 */
#define NVMEDIA_VIDEO_CAPTURE_TIMEOUT_INFINITE  0xFFFFFFFF

/**
 * \brief Determines the video capture interface type for CSI interface
 */
typedef enum {
    /*! Interface: CSI, port: A */
    NVMEDIA_VIDEO_CAPTURE_CSI_INTERFACE_TYPE_CSI_A,
    /*! Interface: CSI, port: B */
    NVMEDIA_VIDEO_CAPTURE_CSI_INTERFACE_TYPE_CSI_B,
    /*! Interface: CSI, port: AB */
    NVMEDIA_VIDEO_CAPTURE_CSI_INTERFACE_TYPE_CSI_AB
} NvMediaVideoCaptureInterfaceType;

/**
 * \brief Determines the video capture input format type
 */
typedef enum {
    /*! Input format: YUV 4:2:0 */
    NVMEDIA_VIDEO_CAPTURE_INPUT_FORMAT_TYPE_YUV420,
    /*! Input format: YUV 4:2:2 */
    NVMEDIA_VIDEO_CAPTURE_INPUT_FORMAT_TYPE_YUV422,
    /*! Input format: YUV 4:4:4 */
    NVMEDIA_VIDEO_CAPTURE_INPUT_FORMAT_TYPE_YUV444,
    /*! Input format: RGBA */
    NVMEDIA_VIDEO_CAPTURE_INPUT_FORMAT_TYPE_RGB888
} NvMediaVideoCaptureInputFormatType;

/**
 * \brief Video capture settings for CSI format
 */
typedef struct {
    /*! Interface type */
    NvMediaVideoCaptureInterfaceType interfaceType;
    /*! Input format */
    NvMediaVideoCaptureInputFormatType inputFormatType;
    /*! Capture width */
    unsigned short width;
    /*! Capture height */
    unsigned short height;
    /*! Horizontal start position */
    unsigned short startX;
    /*! Vertical start position */
    unsigned short startY;
    /*! Extra lines */
    unsigned short extraLines;
    /*! Interlaced format */
    NvMediaBool interlace;
    /*! Number of CSI interface lanes active */
    unsigned int interfaceLanes;
} NvMediaVideoCaptureSettings;

/**
 * \brief Determines the video capture interface and in case of VIP the capture format.
 */
typedef enum {
    /*! VIP capture interface: VIP, format: NTSC */
    NVMEDIA_VIDEO_CAPTURE_INTERFACE_FORMAT_VIP_NTSC,
    /*! VIP capture interface: VIP, format: PAL */
    NVMEDIA_VIDEO_CAPTURE_INTERFACE_FORMAT_VIP_PAL,
    /*! VIP capture interface: CSI */
    NVMEDIA_VIDEO_CAPTURE_INTERFACE_FORMAT_CSI
} NvMediaVideoCaptureInterfaceFormat;

/**
 * \brief Video capture object created by \ref NvMediaVideoCaptureCreate.
 */
typedef struct {
    /*! Capture interface format */
    NvMediaVideoCaptureInterfaceFormat interfaceFormat;
    /*! The surface type that will be returned upon capturing an image.
        \n Currently this is \ref NvMediaSurfaceType_YV16x2 for VIP capture and
        \ref NvMediaSurfaceType_R8G8B8A8 for CSI capture.
    */
    NvMediaSurfaceType surfaceType;
    /*! Width of the captured surface */
    unsigned short width;
    /*! Height of the captured surface */
    unsigned short height;
    /*! Number of frame buffers used to create the video capture object */
    unsigned char numBuffers;
} NvMediaVideoCapture;

/**
 * \brief Create a capture object used to capture varoius formats
 *        of analog or digital video input.
 *        into a \ref NvMediaVideoSurface "NvMediaVideoSurface".
 * \param[in] interfaceFormat This determines the interface
 *        of the capture device and in case of VIP the capture format.
 * \param[in] settings Determines the settings for the capture. Used only for the CSI interface.
 *        For VIP set it to NULL.
 * \param[in] numBuffers Number of frame buffers used in the internal
 *       ring.  Must be at least \ref NVMEDIA_MIN_CAPTURE_FRAME_BUFFERS and no
 *       more than \ref NVMEDIA_MAX_CAPTURE_FRAME_BUFFERS.
 * \return \ref NvMediaVideoCapture The new video capture's handle or NULL if unsuccessful.
 */
NvMediaVideoCapture *
NvMediaVideoCaptureCreate(
   NvMediaVideoCaptureInterfaceFormat interfaceFormat,
   NvMediaVideoCaptureSettings *settings,
   unsigned char numBuffers
);

/**
 * \brief Destroy a video capture created by \ref NvMediaVideoCaptureCreate.
 * \param[in] capture The video capture to be destroyed.
 * \return void
 */
void
NvMediaVideoCaptureDestroy(
    NvMediaVideoCapture *capture
);

/**
 * \brief Start a video capture. This assumes that all the setup related to
 * the capture hadrware has been done.
 * \param[in] capture The video capture to be started.
 * \return void
 */
void
NvMediaVideoCaptureStart(
    NvMediaVideoCapture *capture
);

/**
 * \brief Stop a video capture.
 * \param[in] capture The video capture to be stopped.
 * \return void
 */
void
NvMediaVideoCaptureStop(
    NvMediaVideoCapture *capture
);

/**
 * \brief Get the video surfaces used in NvMediaVideoCapture object.
 * \param[in] capture The video capture to be used.
 * \param[in] surfaces Array of \ref NvMediaVideoSurface * to store the \ref NvMediaVideoSurface
 *       pointers used for capturing frames.
 * \param[in] countInOut a pointer to the maximum count to store \ref NvMediaVideoSurface *
 *       in surfaces. When the dereferenced value of countInOut is greater
 *       than the numBuffers passed to \ref NvMediaVideoCaptureCreate, the value of
 *       numBuffers will be stored in countInOut.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameter is NULL.
 */
NvMediaStatus
NvMediaVideoCaptureListVideoSurfaces(
   NvMediaVideoCapture *capture,
   NvMediaVideoSurface *surfaces[],
   unsigned char *countInOut
);

/**
 * \brief Get a captured frame.
 * This function will block until a frame is available
 * or until the timeout (in milliseconds) has been reached.  To block
 * without a timeout specify \ref NVMEDIA_VIDEO_CAPTURE_TIMEOUT_INFINITE.
 * The returned \ref NvMediaVideoSurface should be passed back to the \ref NvMediaVideoCapture
 * object using \ref NvMediaVideoCaptureReturnFrame after it has been processed.
 * NvMediaVideoCaptureGetFrame returns NULL if capture is not running (was
 * stopped or never started) or if there are insufficient buffers in
 * the internal pool, meaning that too few NvMediaVideoSurfaces have
 * been returned to the capture object. When NvMediaVideoCaptureGetFrame
 * returns a \ref NvMediaVideoSurface, that surface is idle and ready
 * for immediate use. Each \ref NvMediaVideoSurface returned by
 * NvMediaVideoCaptureGetFrame corresponds to two buffers removed from the
 * internal capture pool. There must be at least two buffers left
 * in the pool for NvMediaVideoCaptureGetFrame to succeed, so if one
 * allocated the NvMediaVideoCapture object with only two buffers, the
 * NvMediaVideoSurface returned by NvMediaVideoCaptureGetFrame must be
 * returned via \ref NvMediaVideoCaptureReturnFrame before NvMediaVideoCaptureGetFrame
 * can succeed again.
 * \param[in] capture The video capture to be used.
 * \param[in] millisecondTimeout Time-out in milliseconds
 * \return \ref NvMediaVideoSurface Captured video surface's handle or NULL if unsuccessful.
 */
NvMediaVideoSurface *
NvMediaVideoCaptureGetFrame(
    NvMediaVideoCapture *capture,
    unsigned int millisecondTimeout
);

/**
 * \brief Return a surface back to the video capture pool.
 * An error will be returned if the surface passed is not from the
 * NvMediaVideoCapture pool.
 * \param[in] capture The video capture to be used.
 * \param[in] surface Surface to be returned.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_ERROR
 */
NvMediaStatus
NvMediaVideoCaptureReturnFrame(
    NvMediaVideoCapture *capture,
    NvMediaVideoSurface *surface
);

/**
 * \brief Return extra lines data stored in a captured surface.
 *        Currently only NvMediaSurfaceType_R8G8B8A8 surface type
 *        is supported.
 * \param[in] capture The video capture to be used.
 * \param[in] surface Surface to get the extra lines from.
 * \param[in] extraBuf The buffer where the extra line data is stored.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameter is NULL
 * \n \ref NVMEDIA_STATUS_ERROR if the surface type is not supported
 */
NvMediaStatus
NvMediaVideoCaptureGetExtraLines(
    NvMediaVideoCapture *capture,
    NvMediaVideoSurface *surface,
    void *extraBuf
);
/*@}*/
/**
 * \defgroup encoder_api Video Encoder
 *
 * The NvMediaVideoEncoder object takes uncompressed video data and tuns it
 * into a codec specific bitstream. Currently only H.264 encoding is supported.
 *
 * @{
 */

/**
 * \defgroup h264_encoder_api H.264 encoder
 * @{
 */

/**
 * Input picture type
 */
typedef enum {
    /** Forward predicted */
    NVMEDIA_ENCODE_PIC_TYPE_P               = 0x0,
    /** Bi-directionally predicted picture */
    NVMEDIA_ENCODE_PIC_TYPE_B               = 0x01,
    /** Intra predicted picture */
    NVMEDIA_ENCODE_PIC_TYPE_I               = 0x02,
    /** IDR picture */
    NVMEDIA_ENCODE_PIC_TYPE_IDR             = 0x03,
    /** Bi-directionally predicted with only Intra MBs */
    NVMEDIA_ENCODE_PIC_TYPE_BI              = 0x04,
    /** Picture is skipped */
    NVMEDIA_ENCODE_PIC_TYPE_SKIPPED         = 0x05,
    /** First picture in intra refresh cycle */
    NVMEDIA_ENCODE_PIC_TYPE_INTRA_REFRESH   = 0x06,
    /** Picture type unknown */
    NVMEDIA_ENCODE_PIC_TYPE_UNKNOWN         = 0xFF
} NvMediaEncodePicType;

/**
 * Input slice type
 */
typedef enum {
    /** Slice type is same as picture type */
    NVMEDIA_ENCODE_SLICE_TYPE_DEFAULT         = 0x0,
    /** Intra predicted slice */
    NVMEDIA_ENCODE_SLICE_TYPE_I               = 0x02,
    /** Slice type unknown */
    NVMEDIA_ENCODE_SLICE_TYPE_UNKNOWN         = 0xFF
} NvMediaEncodeSliceType;

/**
 * Encoding profiles
 */
typedef enum {
    /** Automatic profile selection */
    NVMEDIA_ENCODE_PROFILE_AUTOSELECT  = 0,

    /** Baseline profile */
    NVMEDIA_ENCODE_PROFILE_BASELINE    = 66,
    /** Main profile */
    NVMEDIA_ENCODE_PROFILE_MAIN        = 77,
    /** Extended profile */
    NVMEDIA_ENCODE_PROFILE_EXTENDED    = 88,
    /** High profile */
    NVMEDIA_ENCODE_PROFILE_HIGH        = 100
} NvMediaEncodeProfile;

/**
 * Encoding levels
 */
typedef enum {
    /** Automatic level selection */
    NVMEDIA_ENCODE_LEVEL_AUTOSELECT         = 0,

    /** H.264 Level 1 */
    NVMEDIA_ENCODE_LEVEL_H264_1             = 10,
    /** H.264 Level 1b */
    NVMEDIA_ENCODE_LEVEL_H264_1b            = 9,
    /** H.264 Level 1.1 */
    NVMEDIA_ENCODE_LEVEL_H264_11            = 11,
    /** H.264 Level 1.2 */
    NVMEDIA_ENCODE_LEVEL_H264_12            = 12,
    /** H.264 Level 1.3 */
    NVMEDIA_ENCODE_LEVEL_H264_13            = 13,
    /** H.264 Level 2 */
    NVMEDIA_ENCODE_LEVEL_H264_2             = 20,
    /** H.264 Level 2.1 */
    NVMEDIA_ENCODE_LEVEL_H264_21            = 21,
    /** H.264 Level 2.2 */
    NVMEDIA_ENCODE_LEVEL_H264_22            = 22,
    /** H.264 Level 3 */
    NVMEDIA_ENCODE_LEVEL_H264_3             = 30,
    /** H.264 Level 3.1 */
    NVMEDIA_ENCODE_LEVEL_H264_31            = 31,
    /** H.264 Level 3.2 */
    NVMEDIA_ENCODE_LEVEL_H264_32            = 32,
    /** H.264 Level 4 */
    NVMEDIA_ENCODE_LEVEL_H264_4             = 40,
    /** H.264 Level 4.1 */
    NVMEDIA_ENCODE_LEVEL_H264_41            = 41,
    /** H.264 Level 4.2 */
    NVMEDIA_ENCODE_LEVEL_H264_42            = 42,
} NvMediaEncodeLevel;

/**
 * Encode Picture encode flags.
 */
typedef enum {
    /** Encode the current picture as an Intra picture */
    NVMEDIA_ENCODE_PIC_FLAG_FORCEINTRA         = 0x01,
    /** Encode the current picture as an IDR picture.
    This flag is only valid when Picture type decision is taken by the Encoder
    [NvMediaEncodeInitializeParams enablePTD is set to NVMEDIA_TRUE] */
    NVMEDIA_ENCODE_PIC_FLAG_FORCEIDR           = 0x02,
    /** Write the sequence and picture header in encoded bitstream of the current picture */
    NVMEDIA_ENCODE_PIC_FLAG_OUTPUT_SPSPPS      = 0x04,
    /** Indicates end of the input stream */
    NVMEDIA_ENCODE_PIC_FLAG_EOS                = 0x08,
    /** Indicates change in bitrate from current picture onwards */
    NVMEDIA_ENCODE_PIC_FLAG_DYN_BITRATE_CHANGE = 0x10,
    /** Indicates user forced constant QP Rate control from current picture onwards */
    NVMEDIA_ENCODE_PIC_FLAG_USER_FORCE_CONSTQP = 0x20,
    /** Indicates change in Rate control mode on the fly from current picture onwards */
    NVMEDIA_ENCODE_PIC_FLAG_DYN_RCMODE_CHANGE  = 0x40,
} NvMediaEncodePicFlags;

/**
 * Input surface rotation.
 */
typedef enum {
    /** No rotation */
    NVMEDIA_ENCODE_ROTATION_NONE               = 0x0,
    /** 90 degrees rotation */
    NVMEDIA_ENCODE_ROTATION_90                 = 0x1,
    /** 180 degrees rotation */
    NVMEDIA_ENCODE_ROTATION_180                = 0x2,
    /** 270 degrees rotation */
    NVMEDIA_ENCODE_ROTATION_270                = 0x3
} NvMediaEncodeRotation;

/**
 * Input surface mirroring.
 */
typedef enum {
    /** No mirroring */
    NVMEDIA_ENCODE_MIRRORING_NONE              = 0x0,
    /** Horizontal mirroring */
    NVMEDIA_ENCODE_MIRRORING_HORIZONTAL        = 0x1,
    /** Vertical mirroring */
    NVMEDIA_ENCODE_MIRRORING_VERTICAL          = 0x2,
    /** Horizontal and vertical mirroring */
    NVMEDIA_ENCODE_MIRRORING_BOTH              = 0x3
} NvMediaEncodeMirroring;

/**
 * H.264 entropy coding modes.
 */
typedef enum {
    /** Entropy coding mode is auto selected by the encoder driver */
    NVMEDIA_ENCODE_H264_ENTROPY_CODING_MODE_AUTOSELECT = 0x0,
    /** Entropy coding mode is CABAC */
    NVMEDIA_ENCODE_H264_ENTROPY_CODING_MODE_CABAC      = 0x1,
    /** Entropy coding mode is CAVLC */
    NVMEDIA_ENCODE_H264_ENTROPY_CODING_MODE_CAVLC      = 0x2
} NvMediaEncodeH264EntropyCodingMode;

/**
 * H.264 specific Bdirect modes
 */
typedef enum {
    /** BDirect mode is auto selected by the encoder driver */
    NVMEDIA_ENCODE_H264_BDIRECT_MODE_AUTOSELECT = 0x0,
    /** Disable BDirect mode */
    NVMEDIA_ENCODE_H264_BDIRECT_MODE_DISABLE    = 0x1,
    /** Temporal BDirect mode */
    NVMEDIA_ENCODE_H264_BDIRECT_MODE_TEMPORAL   = 0x2,
    /** Spatial BDirect mode */
    NVMEDIA_ENCODE_H264_BDIRECT_MODE_SPATIAL    = 0x3
} NvMediaEncodeH264BDirectMode;

/**
 * H.264 specific Adaptive Transform modes
 */
typedef enum {
    /** Adaptive Transform 8x8 mode is auto selected by the encoder driver*/
    NVMEDIA_ENCODE_H264_ADAPTIVE_TRANSFORM_AUTOSELECT = 0x0,
    /** Adaptive Transform 8x8 mode disabled */
    NVMEDIA_ENCODE_H264_ADAPTIVE_TRANSFORM_DISABLE    = 0x1,
    /** Adaptive Transform 8x8 mode should be used */
    NVMEDIA_ENCODE_H264_ADAPTIVE_TRANSFORM_ENABLE     = 0x2,
} NvMediaEncodeH264AdaptiveTransformMode;

/**
 * Stereo frame packing modes.
 */
typedef enum {
    /** No Stereo packing required */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_NONE             = 0x0,
    /** Checkerboard mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_CHECKERBOARD     = 0x1,
    /** Column Interleave mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_COLINTERLEAVE    = 0x2,
    /** Row Interleave mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_ROWINTERLEAVE    = 0x3,
    /** Side-by-side mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_SIDEBYSIDE       = 0x4,
    /** Top-Bottom mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_TOPBOTTOM        = 0x5,
    /** Frame Sequential mode for packing stereo frames */
    NVMEDIA_ENCODE_STEREO_PACKING_MODE_FRAMESEQ         = 0x6
} NvMediaEncodeStereoPackingMode;

/**
 * Motion estimation mode for H.264
 */
typedef enum {
    /** 16x16 motion estimation mode */
    NVMEDIA_ENCODE_MOTION_ESTIMATION_MODE_16x16        = 0x0,
    /** 16x8 motion estimation mode */
    NVMEDIA_ENCODE_MOTION_ESTIMATION_MODE_16x8         = 0x1,
    /** 8x16 motion estimation mode */
    NVMEDIA_ENCODE_MOTION_ESTIMATION_MODE_8x16         = 0x2,
    /** 8x8 motion estimation mode */
    NVMEDIA_ENCODE_MOTION_ESTIMATION_MODE_8x8          = 0x3
} NvMediaEncodeH264MotionEstimationMode;

/**
 * Motion prediction exclusion flags for H.264
 */
typedef enum {
    /** Disable Intra 4x4 vertical prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_VERTICAL_PREDICTION               = (1 << 0),
    /** Disable Intra 4x4 horizontal prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_HORIZONTAL_PREDICTION             = (1 << 1),
    /** Disable Intra 4x4 DC prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_DC_PREDICTION                     = (1 << 2),
    /** Disable Intra 4x4 diagonal down left prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_DIAGONAL_DOWN_LEFT_PREDICTION     = (1 << 3),
    /** Disable Intra 4x4 diagonal down right prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_DIAGONAL_DOWN_RIGHT_PREDICTION    = (1 << 4),
    /** Disable Intra 4x4 vertical right prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_VERTICAL_RIGHT_PREDICTION         = (1 << 5),
    /** Disable Intra 4x4 horizontal down prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_HORIZONTAL_DOWN_PREDICTION        = (1 << 6),
    /** Disable Intra 4x4 vertical left prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_VERTICAL_LEFT_PREDICTION          = (1 << 7),
    /** Disable Intra 4x4 horizontal up prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_4x4_HORIZONTAL_UP_PREDICTION          = (1 << 8),

    /** Disable Intra 8x8 vertical prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_VERTICAL_PREDICTION               = (1 << 9),
    /** Disable Intra 8x8 horizontal prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_HORIZONTAL_PREDICTION             = (1 << 10),
    /** Disable Intra 8x8 DC prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_DC_PREDICTION                     = (1 << 11),
    /** Disable Intra 8x8 diagonal down left prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_DIAGONAL_DOWN_LEFT_PREDICTION     = (1 << 12),
    /** Disable Intra 8x8 diagonal down right prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_DIAGONAL_DOWN_RIGHT_PREDICTION    = (1 << 13),
    /** Disable Intra 8x8 vertical right prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_VERTICAL_RIGHT_PREDICTION         = (1 << 14),
    /** Disable Intra 8x8 horizontal down prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_HORIZONTAL_DOWN_PREDICTION        = (1 << 15),
    /** Disable Intra 8x8 vertical left prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_VERTICAL_LEFT_PREDICTION          = (1 << 16),
    /** Disable Intra 8x8 horizontal up prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_8x8_HORIZONTAL_UP_PREDICTION          = (1 << 17),

    /** Disable Intra 16x16 vertical prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_16x16_VERTICAL_PREDICTION             = (1 << 18),
    /** Disable Intra 16x16 horizontal prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_16x16_HORIZONTAL_PREDICTION           = (1 << 19),
    /** Disable Intra 16x16 DC prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_16x16_DC_PREDICTION                   = (1 << 20),
    /** Disable Intra 16x16 plane prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_16x16_PLANE_PREDICTION                = (1 << 21),

    /** Disable Intra chroma vertical prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_CHROMA_VERTICAL_PREDICTION            = (1 << 22),
    /** Disable Intra chroma horizontal prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_CHROMA_HORIZONTAL_PREDICTION          = (1 << 23),
    /** Disable Intra chroma DC prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_CHROMA_DC_PREDICTION                  = (1 << 24),
    /** Disable Intra chroma plane prediction */
    NVMEDIA_ENCODE_DISABLE_INTRA_CHROMA_PLANE_PREDICTION               = (1 << 25)
} NvMediaEncodeH264MotionPredictionExclusionFlags;

/**
 * Specifies the frequency of the writing of Sequence and Picture parameters for H.264
 */
typedef enum {
    /** Repeating of SPS/PPS is disabled */
    NVMEDIA_ENCODE_SPSPPS_REPEAT_DISABLED          = 0x0,
    /** SPS/PPS is repeated for every intra frame */
    NVMEDIA_ENCODE_SPSPPS_REPEAT_INTRA_FRAMES      = 0x1,
    /** SPS/PPS is repeated for every IDR frame */
    NVMEDIA_ENCODE_SPSPPS_REPEAT_IDR_FRAMES        = 0x2
} NvMediaEncodeH264SPSPPSRepeatMode;

/**
 * H264 Video Usability Info parameters
 */
typedef struct {
    /** If set to NVMEDIA_TRUE, it specifies that the overscanInfo is present */
    NvMediaBool     overscanInfoPresentFlag;
    /** Specifies the overscan info(as defined in Annex E of the ITU-T Specification). */
    unsigned int    overscanInfo;
    /** If set to NVMEDIA_TRUE, it specifies  that the videoFormat, videoFullRangeFlag and colourDescriptionPresentFlag are present. */
    NvMediaBool     videoSignalTypePresentFlag;
    /** Specifies the source video format(as defined in Annex E of the ITU-T Specification).*/
    unsigned int    videoFormat;
    /** Specifies the output range of the luma and chroma samples(as defined in Annex E of the ITU-T Specification). */
    unsigned int    videoFullRangeFlag;
    /** If set to NVMEDIA_TRUE, it specifies that the colourPrimaries, transferCharacteristics and colourMatrix are present. */
    NvMediaBool     colourDescriptionPresentFlag;
    /** Specifies color primaries for converting to RGB(as defined in Annex E of the ITU-T Specification) */
    unsigned int    colourPrimaries;
    /** Specifies the opto-electronic transfer characteristics to use (as defined in Annex E of the ITU-T Specification) */
    unsigned int    transferCharacteristics;
    /** Specifies the matrix coefficients used in deriving the luma and chroma from the RGB primaries (as defined in Annex E of the ITU-T Specification). */
    unsigned int    colourMatrix;
} NvMediaEncodeConfigH264VUIParams;

/**
 * External motion vector hint counts per block type.
 */
typedef struct {
    /** Specifies the number of candidates per 16x16 block. */
    unsigned int   numCandsPerBlk16x16;
    /** Specifies the number of candidates per 16x8 block. */
    unsigned int   numCandsPerBlk16x8;
    /** Specifies the number of candidates per 8x16 block. */
    unsigned int   numCandsPerBlk8x16;
    /** Specifies the number of candidates per 8x8 block. */
    unsigned int   numCandsPerBlk8x8;
} NvMediaEncodeExternalMeHintCountsPerBlocktype;

/**
 * External Motion Vector hint structure.
 */
typedef struct {
    /** Specifies the x component of integer pixel MV (relative to current MB) S12.0. */
    int mvx         : 12;
    /** Specifies the y component of integer pixel MV (relative to current MB) S10.0 .*/
    int mvy         : 10;
    /** Specifies the reference index (31=invalid). Current we support only 1 reference frame per direction for external hints, so refidx must be 0. */
    int refidx      : 5;
    /** Specifies the direction of motion estimation . 0=L0 1=L1.*/
    int dir         : 1;
    /** Specifies the block partition type. 0=16x16 1=16x8 2=8x16 3=8x8 (blocks in partition must be consecutive).*/
    int partType    : 2;
    /** Set to NVMEDIA_TRUE for the last MV of (sub) partition  */
    int lastofPart  : 1;
    /** Set to NVMEDIA_TRUE for the last MV of macroblock. */
    int lastOfMB    : 1;
} NvMediaEncodeExternalMEHint;

/**
 * H264 encoder configuration parameters
 */
typedef struct {
    /** Set to NVMEDIA_TRUE to enable stereo MVC*/
    NvMediaBool enableStereoMVC;
    /** Set to NVMEDIA_TRUE to enable hierarchical PFrames */
    NvMediaBool hierarchicalPFrames;
    /** Set to NVMEDIA_TRUE to enable hierarchical BFrames */
    NvMediaBool hierarchicalBFrames;
    /** Set to NVMEDIA_TRUE to write SEI buffering period syntax in the bitstream */
    NvMediaBool outputBufferingPeriodSEI;
    /** Set to NVMEDIA_TRUE to write SEI picture timing syntax in the bitstream */
    NvMediaBool outputPictureTimingSEI;
    /** Set to NVMEDIA_TRUE to write access unit delimiter syntax in bitstream */
    NvMediaBool outputAUD;
    /** Set to NVMEDIA_TRUE to disable writing of Sequence and Picture parameter info in bitstream */
    NvMediaBool disableSPSPPS;
    /** Set to NVMEDIA_TRUE to enable writing of frame packing arrangement SEI messages to bitstream */
    NvMediaBool outputFramePackingSEI;
    /** Set to NVMEDIA_TRUE to enable writing of recovery point SEI message */
    NvMediaBool outputRecoveryPointSEI;
    /** Set to NVMEDIA_TRUE to enable gradual decoder refresh or intra refresh. If the GOP structure uses B frames this will be ignored */
    NvMediaBool enableIntraRefresh;
    /** Set to NVMEDIA_TRUE to enable dynamic slice mode. Client must specify max slice size using NvMediaEncodeConfigH264 maxSliceSizeInBytes field. */
    NvMediaBool enableDynamicSliceMode;
    /** Set this to NVMEDIA_TRUE to enable constrainedFrame encoding where each slice in the constarined picture is independent of other slices. */
    NvMediaBool enableConstrainedEncoding;
    /** Specifies the frequency of the writing of Sequence and Picture parameters */
    NvMediaEncodeH264SPSPPSRepeatMode repeatSPSPPS;
    /** Specifies the encoding profile. Client is recommended to set this to NVMEDIA_ENCODE_PROFILE_AUTOSELECT in order to enable the NvMedia Encode interface to select the correct profile. */
    unsigned int profile;
    /** Specifies the encoding level. Client is recommended to set this to NVMEDIA_ENCODE_LEVEL_AUTOSELECT in order to enable the NvMedia Encode interface to select the correct level. */
    unsigned int level;
    /** Specifies the IDR interval. If not set, this is made equal to gopLength in NvMediaEncodeConfig. Low latency application client can set IDR interval to \ref NVMEDIA_ENCODE_INFINITE_GOPLENGTH so that IDR frames are not inserted automatically. */
    unsigned int idrPeriod;
    /** Set to 1 less than the number of slices desired per frame */
    unsigned int numSliceCountMinus1;
    /** Specifies the deblocking filter mode. Permissible value range: [0,2] */
    unsigned int disableDeblockingFilterIDC;
    /** Specifies the SPS id of the sequence header. Currently reserved and must be set to 0. */
    unsigned int spsId;
    /** Specifies the PPS id of the picture header. Currently reserved and must be set to 0. */
    unsigned int ppsId;
    /** Specifies the AdaptiveTransform Mode. */
    NvMediaEncodeH264AdaptiveTransformMode      adaptiveTransformMode;
    /** Specifies the BDirect mode. */
    NvMediaEncodeH264BDirectMode                bdirectMode;
    /** Specifies the entropy coding mode. */
    NvMediaEncodeH264EntropyCodingMode          entropyCodingMode;
    /** Specifies the stereo frame packing mode which is to be signalled in frame packing arrangement SEI */
    NvMediaEncodeStereoPackingMode              stereoMode;
    /** Specifies the interval between successive intra refresh if enableIntrarefresh is set and one time intraRefresh configuration is desired.
    When this is specified only first IDR will be encoded and no more key frames will be encoded. Client should set pictureType = \ref NVMEDIA_ENCODE_PIC_TYPE_INTRA_REFRESH
    for first picture of every intra refresh period. */
    unsigned int intraRefreshPeriod;
    /** Specifies the number of frames over which intra refresh will happen */
    unsigned int intraRefreshCnt;
    /** Specifies the max slice size in bytes for dynamic slice mode. Client must set NvMediaEncodeConfigH264 enableDynamicSliceMode to NVMEDIA_TRUE to use max slice size in bytes. */
    unsigned int maxSliceSizeInBytes;
    /** Specifies the DPB size used for encoding. Setting it to 0 will let driver use the default dpb size.
        The low latency application which wants to invalidate reference frame as an error resilience tool
        is recommended to use a large DPB size so that the encoder can keep old reference frames which can be used if recent
        frames are invalidated. */
    unsigned int maxNumRefFrames;
    /** Specifies the maximum number of frames for log2_max_frame_num_minus4.
        Setting it to 0 will let the driver use the default frame number. Valid range is 1 to 65536 */
    unsigned int maxFrameNumber;
    /** Specifies the H264 video usability info pamameters */
    NvMediaEncodeConfigH264VUIParams       h264VUIParameters;
    /** Specifies the motion estimation mode if NvMediaEncodeInitializeParams enableMotionEstimation is set to NVMEDIA_TRUE */
    NvMediaEncodeH264MotionEstimationMode  motionEstimationMode;
    /** Specifies bit-wise OR`ed exclusion flags for motion prediction. See NvMediaEncodeH264MotionPredictionExclusionFlags enum. */
    unsigned int motionPredictionExclusionFlags;
} NvMediaEncodeConfigH264;

/**
 * Codec-specific encoder configuration parameters to be set during initialization.
 */
typedef union {
    /** Specifies the H.264-specific encoder configuration */
    NvMediaEncodeConfigH264 h264Config;
} NvMediaEncodeCodecConfig;

/**
 * MVC specific enc pic params. sent on a per frame basis.
 */
typedef struct {
    /** Specifies the view ID associated with the current input view. */
    unsigned int viewID;
    /** Specifies the temporal ID associated with the current input view. */
    unsigned int temporalID;
    /** Specifies the priority ID associated with the current input view. Reserved and ignored by the NvMedia Encode interface. */
    unsigned int priorityID;
} NvMediaEncodePicParamsMVC;

/**
 * H264 extension  picture parameters
 */
typedef union {
    /** Specifies the MVC picture parameters. */
    NvMediaEncodePicParamsMVC  mvcPicParams;
} NvMediaEncodePicParamsH264Ext;

/**
 *  User SEI message
 */
typedef struct {
    /** SEI payload size in bytes. SEI payload must be byte aligned, as described in Annex D */
    unsigned int payloadSize;
    /** SEI payload types and syntax can be found in Annex D of the H.264 Specification. */
    unsigned int payloadType;
    /** pointer to user data */
    unsigned char *payload;
} NvMediaEncodeH264SEIPayload;

/**
 * H264 specific encoder picture params. Sent on a per frame basis.
 */
typedef struct {
    /** Specifies the display POC syntax This is required to be set if client is handling the picture type decision. */
    unsigned int displayPOCSyntax;
    /** Specifies the frameNum syntax. This is required to be set if client is handling the picture type decision. */
    unsigned int frameNumSyntax;
    /** Specifies the H264 extension config parameters using this config. */
    NvMediaEncodePicParamsH264Ext h264ExtPicParams;
    /** Set to NVMEDIA_TRUE for a reference picture. This is ignored if NvMediaEncodeInitializeParams enablePTD is set to NVMEDIA_TRUE. */
    NvMediaBool refPicFlag;
    /** Forces an intra refresh with duration equal to intraRefreshFrameCnt.
        When outputRecoveryPointSEI is set this is value is used for recovery_frame_cnt in recovery point SEI message
        forceIntraRefreshWithFrameCnt cannot be used if B frames are used in the GOP structure specified */
    unsigned int forceIntraRefreshWithFrameCnt;
    /** Set to NVMEDIA_TRUE if client wants to encode this frame with each slice completely independent of other slices in the frame.
        NvMediaEncodeInitializeParams enableConstrainedEncoding should be set to NVMEDIA_TRUE */
    NvMediaBool constrainedFrame;
    /** Array which specifies the slice type used to force intra slice for a particular slice.
        Client should allocate array of size numSliceCountMinus1 + 1 where numSliceCountMinus1 is specified in field of NvMediaEncodeConfigH264
        Array element with index n corresponds to nth slice. To force a particular slice to intra client should set corresponding array element to \ref NVMEDIA_ENCODE_SLICE_TYPE_I
        all other array elements should be set to \ref NVMEDIA_ENCODE_SLICE_TYPE_DEFAULT */
    unsigned char* sliceTypeData;
    /** Client should set this to the number of elements allocated in sliceTypeData array. If sliceTypeData is NULL then this should be set to 0 */
    unsigned int sliceTypeArrayCnt;
    /** Specifies the number of elements allocated in  seiPayloadArray array. */
    unsigned int seiPayloadArrayCnt;
    /** Array of SEI payloads which will be inserted for this frame. */
    NvMediaEncodeH264SEIPayload *seiPayloadArray;
} NvMediaEncodePicParamsH264;
/*@}*/

/**
 * \hideinitializer
 * \brief Infinite time-out for \ref NvMediaVideoEncoderBitsAvailable
 */
#define NVMEDIA_VIDEO_ENCODER_TIMEOUT_INFINITE  0xFFFFFFFF

/**
 * \hideinitializer
 * \brief Infinite GOP lenght so that keyframes are not inserted automatically
 */
#define NVMEDIA_ENCODE_INFINITE_GOPLENGTH       0xFFFFFFFF

/**
 * Motion vector precisions
 */
typedef enum {
    /** FullPel  motion vector precision */
    NVMEDIA_ENCODE_MV_PRECISION_FULL_PEL    = 0x01,
    /** HalfPel motion vector precision */
    NVMEDIA_ENCODE_MV_PRECISION_HALF_PEL    = 0x02,
    /** QuarterPel motion vector precision */
    NVMEDIA_ENCODE_MV_PRECISION_QUARTER_PEL = 0x03
} NvMediaEncodeMVPrecision;

/**
 * QP (Quantization Parameter) value for frames
 */
typedef struct {
    /** QP value for P frames */
    unsigned int qpInterP;
    /** QP value for B frames */
    unsigned int qpInterB;
    /** QP value for Intra frames */
    unsigned int qpIntra;
} NvMediaEncodeQP;

/**
 * Rate Control Modes
 */
typedef enum
{
    /** Default rate control mode */
    NVMEDIA_ENCODE_PARAMS_RC_DEFAULT      = 0x0,
    /** Constant QP mode */
    NVMEDIA_ENCODE_PARAMS_RC_CONSTQP      = 0x1,
    /** Variable bitrate mode */
    NVMEDIA_ENCODE_PARAMS_RC_VBR          = 0x2,
    /** Constant bitrate mode */
    NVMEDIA_ENCODE_PARAMS_RC_CBR          = 0x3,
    /** Variable bitrate mode with MinQP */
    NVMEDIA_ENCODE_PARAMS_RC_VBR_MINQP    = 0x4
} NvMediaEncodeParamsRCMode;

/**
 * Rate Control Configuration Paramters
 */
 typedef struct {
    /** Specifies the rate control mode. */
    NvMediaEncodeParamsRCMode   rateControlMode;
    /** Specifies the initial QP to be used for encoding, these values would be used for all frames if in Constant QP mode. */
    NvMediaEncodeQP             constQP;
    /** Specifies the average bitrate(in bits/sec) used for encoding. */
    unsigned int                averageBitRate;
    /** Specifies the maximum bitrate for the encoded output. This is used for VBR and ignored for CBR mode. */
    unsigned int                maxBitRate;
    /** Specifies the VBV(HRD) buffer size. in bits. Set 0 to use the default VBV  buffer size. */
    unsigned int                vbvBufferSize;
    /** Specifies the VBV(HRD) initial delay in bits. Set 0 to use the default VBV  initial delay .*/
    unsigned int                vbvInitialDelay;
    /** Set this to NVMEDIA_TRUE if minimum QP used for rate control. */
    NvMediaBool                 enableMinQP;
    /** Set this to NVMEDIA_TRUE if maximum QP used for rate control. */
    NvMediaBool                 enableMaxQP;
    /** Specifies the minimum QP used for rate control. Client must set NvMediaEncodeRCParams enableMinQP to NVMEDIA_TRUE. */
    NvMediaEncodeQP             minQP;
    /** Specifies the maximum QP used for rate control. Client must set NvMediaEncodeRCParams enableMaxQP to NVMEDIA_TRUE. */
    NvMediaEncodeQP             maxQP;
 } NvMediaEncodeRCParams;

/**
 * Encoder configuration parameters to be set during initialization.
 */
typedef struct {
    /** Specifies the number of pictures in one GOP. Low latency application client can set goplength to \ref NVMEDIA_ENCODE_INFINITE_GOPLENGTH so that keyframes are not inserted automatically. */
    unsigned int             gopLength;
    /** Specifies the GOP pattern as follows: frameIntervalP = 0: I, 1: IPP, 2: IBP, 3: IBBP  If goplength is set to \ref NVMEDIA_ENCODE_INFINITE_GOPLENGTH frameIntervalP should be set to 1. */
    int                      frameIntervalP;
    /** Set this to NVMEDIA_TRUE to enable monochrome encoding for this session. */
    NvMediaBool              monoChromeEncoding;
    /** Specifies the desired motion vector prediction precision. */
    NvMediaEncodeMVPrecision mvPrecision;
    /** Specifies the rate control parameters for the current encoding session. */
    NvMediaEncodeRCParams    rcParams;
    /** Specifies the codec specific config parameters through this union. */
    NvMediaEncodeCodecConfig encodeCodecConfig;
} NvMediaEncodeConfig;

/**
 * Encode Initialization parameters.
 */
typedef struct {
    /** Specifies the encode width.*/
    unsigned int encodeWidth;
    /** Specifies the encode height.*/
    unsigned int encodeHeight;
    /** Specifies the display aspect ratio width. Set to 0 for automatic selection */
    unsigned int darWidth;
    /** Specifies the display aspect ratio height. Set to 0 for automatic selection */
    unsigned int darHeight;
    /** Specifies the numerator for frame rate used for encoding in frames per second ( Frame rate = frameRateNum / frameRateDen ). */
    unsigned int frameRateNum;
    /** Specifies the denominator for frame rate used for encoding in frames per second ( Frame rate = frameRateNum / frameRateDen ). */
    unsigned int frameRateDen;
    /** Set this to NVMEDIA_TRUE to enable motion estimation mode. */
    NvMediaBool  enableMotionEstimation;
    /** Set this to NVMEDIA_TRUE to enable the Picture Type Decision is be taken by the NvMedia Encode interface. */
    NvMediaBool  enablePTD;
    /** Specifies the rotation of the input surface. */
    NvMediaEncodeRotation rotation;
    /** Specifies the mirroring of the input surface. */
    NvMediaEncodeMirroring mirroring;
    /** Set to NVMEDIA_TRUE to enable external ME hints for the current frame. Currently this feature is supported only if NvMediaEncodeInitializeParams enablePTD is set to NV_FALSE or frameIntervalP = 1 (i.e no B frames). */
    NvMediaBool  enableExternalMEHints;
    /** Specifies the advanced codec specific structure. */
    NvMediaEncodeConfig* encodeConfig;
    /** If Client wants to pass external motion vectors in \ref NvMediaEncodePicParams meExternalHints buffer it must specify the maximum number of hint candidates per block per direction for the encode session.
        The NvMediaEncodeInitializeParams maxMEHintCountsPerBlock[0] is for L0 predictors and NvMediaEncodeInitializeParams maxMEHintCountsPerBlock[1] is for L1 predictors.
        This client must also set NvMediaEncodeInitializeParams enableExternalMEHints to NVMEDIA_TRUE. */
    NvMediaEncodeExternalMeHintCountsPerBlocktype  maxMEHintCountsPerBlock[2];
} NvMediaEncodeInitializeParams;

/**
 * Codec specific per-picture encoding parameters.
 */
typedef union {
    /** H264 encode picture params */
    NvMediaEncodePicParamsH264 h264PicParams;
} NvMediaEncodeCodecPicParams;

/**
 * Encoding parameters that need to be sent on a per frame basis.
 */
typedef struct {
    /** Specifies bit-wise OR`ed encode pic flags. See NvMediaEncodePicFlags enum. */
    unsigned int encodePicFlags;
    /** Specifies the frame index associated with the input frame [optional]. */
    unsigned int frameIdx;
    /** Specifies presentation timestamp associated with the input picture. */
    unsigned long long inputTimeStamp;
    /** Specifies duration of the input picture */
    unsigned long long inputDuration;
    /** Specifies input picture type. Client required to be set explicitly by the client if the client has not set NvMediaEncodeInitializeParams enablePTD to NVMEDIA_TRUE while calling NvInitializeEncoder. */
    NvMediaEncodePicType pictureType;
    /** Specifies the codec specific per-picture encoding parameters. */
    NvMediaEncodeCodecPicParams codecPicParams;
    /** Specifies the rate control parameters for the current encoding session. */
    NvMediaEncodeRCParams rcParams;
    /** Specifies the number of hint candidates per block per direction for the current frame. meHintCountsPerBlock[0] is for L0 predictors and meHintCountsPerBlock[1] is for L1 predictors.
        The candidate count in NvMediaEncodeCodecPicParams meHintCountsPerBlock[lx] must never exceed NvMediaEncodeInitializeParams maxMEHintCountsPerBlock[lx] provided during encoder intialization. */
    NvMediaEncodeExternalMeHintCountsPerBlocktype  meHintCountsPerBlock[2];
    /** Specifies the pointer to ME external hints for the current frame. The size of ME hint buffer should be equal to number of macroblocks multiplied by the total number of candidates per macroblock.
        The total number of candidates per MB per direction = 1*meHintCountsPerBlock[Lx].numCandsPerBlk16x16 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk16x8 + 2*meHintCountsPerBlock[Lx].numCandsPerBlk8x8
        + 4*meHintCountsPerBlock[Lx].numCandsPerBlk8x8. For frames using bidirectional ME , the total number of candidates for single macroblock is sum of total number of candidates per MB for each direction (L0 and L1) */
    NvMediaEncodeExternalMEHint *meExternalHints;
} NvMediaEncodePicParams;

/**
 * Encode Stats structure.
 */
typedef struct {
    /** Picture type of encoded picture. */
    NvMediaEncodePicType picType;
    /** Offsets of each slice. */
    unsigned int sliceOffsets[32];
    /** Picture number. */
    unsigned int picIdx;
    /** Timestamp of the encoded picture. This value is copy of the corresponding inputTimeStamp. */
    unsigned long long picTimeStamp;
} NvMediaEncodeStat;

/**
 * Blocking type
 */
typedef enum {
    /** Never blocks */
    NVMEDIA_ENCODE_BLOCKING_TYPE_NEVER,
    /** Block only when operation is pending */
    NVMEDIA_ENCODE_BLOCKING_TYPE_IF_PENDING,
    /** Always blocks */
    NVMEDIA_ENCODE_BLOCKING_TYPE_ALWAYS
} NvMediaBlockingType;

/**
 * \brief Video encoder object created by \ref NvMediaVideoEncoderCreate.
 */
typedef struct {
    /** Codec type */
    NvMediaVideoCodec codec;
    /** Input surface format */
    NvMediaSurfaceType inputFormat;
} NvMediaVideoEncoder;

/**
 * \brief Create an encoder object capable of turning a stream of surfaces
 * of the "inputFormat" into a bitstream of the specified "codec".
 * Surfaces are feed to the encoder with \ref NvMediaVideoEncoderFeedFrame
 * and bitstream buffers are retrieved with \ref NvMediaVideoEncoderGetBits.
 * \param[in] device The \ref NvMediaDevice "device" this video encoder will use.
 * \param[in] codec Currently only \ref NVMEDIA_VIDEO_CODEC_H264 is supported.
 * \param[in] initParams Specify encode parameters.
 * \param[in] inputFormat May be one of the following:
 *      \n \ref NvMediaSurfaceType_R8G8B8A8
 *      \n \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin
 *      \n \ref NvMediaSurfaceType_YV12
 * \param[in] maxInputBuffering
 *      This determines how many frames may be in the encode pipeline
 *      at any time.  For example, if maxInputBuffering==1,
 *      \ref NvMediaVideoEncoderFeedFrame will block until the previous
 *      encode has completed.  If maxInputBuffering==2,
 *      \ref NvMediaVideoEncoderFeedFrame will accept one frame while another
 *      is still being encoded by the hardware, but will block if two
 *      are still being encoded.  maxInputBuffering will be clamped between
 *      1 and 16.  This field is ignored for YUV inputs which don't require
 *      a pre-processing pipeline before the encode hardware.
 * \param[in] maxOutputBuffering
 *      This determines how many frames of encoded bitstream can be held
 *      by the NvMediaVideoEncoder before it must be retrieved using
 *      NvMediaVideoEncoderGetBits().  This number should be greater than
 *      or equal to maxInputBuffering and is clamped to between
 *      maxInputBuffering and 16.  If maxOutputBuffering frames worth
 *      of encoded bitstream are yet unretrived by \ref NvMediaVideoEncoderGetBits
 *      \ref NvMediaVideoEncoderFeedFrame will return
 *      \ref NVMEDIA_STATUS_INSUFFICIENT_BUFFERING.  One or more frames need to
 *      be retrived with NvMediaVideoEncoderGetBits() before frame feeding
 *      can continue.
 * \param[in] optionalDevice
 *      Should be NULL under normal circumstances.  If not NULL, NvMedia will
 *      use the 3D engine (specified by the NvMediaDevice) instead of the 2D
 *      engine for RGB to YUV conversions during the
 *      \ref NvMediaVideoEncoderFeedFrame function.  In this case,
 *      \ref NvMediaVideoEncoderFeedFrame is bound to the NvMediaDevice; this
 *      has thread saftey consequences.  For example, if the same NvMediaDevice
 *      is used for both the Encoder and VideoMixer, the two should be
 *      called from the same thread.  If the Encoder and VideoMixer are
 *      to be used in separate threads they should use different NvMediaDevices.
 * \return \ref NvMediaVideoEncoder The new video encoder's handle or NULL if unsuccessful.
 */
NvMediaVideoEncoder *
NvMediaVideoEncoderCreate(
    NvMediaDevice *device,
    NvMediaVideoCodec codec,
    NvMediaEncodeInitializeParams *initParams,
    NvMediaSurfaceType inputFormat,
    unsigned char maxInputBuffering,
    unsigned char maxOutputBuffering,
    NvMediaDevice *optionalDevice
);

/**
 * \brief Destroy an NvMediaEncoder
 * \param[in] encoder The encoder to destroy.
 * \return void
 */
void NvMediaVideoEncoderDestroy(NvMediaVideoEncoder *encoder);

/**
 * \brief Encode the specified "frame".  NvMediaVideoEncoderFeedFrame
 *  returns \ref NVMEDIA_STATUS_INSUFFICIENT_BUFFERING if \ref NvMediaVideoEncoderGetBits
 *  has not been called frequently enough and the maximum internal
 *  bitstream buffering (determined by "maxOutputBuffering" passed to
 *  \ref NvMediaVideoEncoderCreate) has been exhausted.
 * \param[in] encoder The encoder to use.
 * \param[in] frame
 *      This must be of the same sourceType as the NvMediaEncoder.
 *      There is no limit on the size of this surface.  The
 *      source rectangle specified by "sourceRect" will be scaled
 *      to the stream dimensions.
 * \param[in] sourceRect
 *       This rectangle on the source will be scaled to the stream
 *       dimensions.  If NULL, a rectangle the full size of the source
 *       surface is implied.  The source may be flipped horizontally
 *       or vertically by swapping the left and right or top and bottom
 *       coordinates. This parameter is valid only for RGB type input
 *       surfaces.
 * \param[in] refFrame
 *      This must be of the same sourceType as the NvMediaEncoder.
 *      This surface is used as the reference frame if the encoder was
 *      created for motion estimation. Otherwise set this to NULL.
 * \param[in] picParams Picture parameters used for the frame.
 * \return \ref NvMediaVideoEncoder The new video encoder's handle or NULL if unsuccessful.
 */
NvMediaStatus
NvMediaVideoEncoderFeedFrame(
    NvMediaVideoEncoder *encoder,
    NvMediaVideoSurface *frame,
    NvMediaRect *sourceRect,
    NvMediaVideoSurface *refFrame,
    NvMediaEncodePicParams *picParams
);

/**
 * \brief Set the encoder configuration. These go into effect only at
 * the start of the next GOP.
 * \param[in] encoder The encoder to use.
 * \param[in] configuration Configuration data.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameter is NULL.
 */
NvMediaStatus
NvMediaVideoEncoderSetConfiguration(
    NvMediaVideoEncoder *encoder,
    NvMediaEncodeConfig *configuration
);

/**
 * \brief NvMediaVideoEncoderGetBits returns a frame's worth of bitstream
 *  into the provided "buffer".  "numBytes" returns the size of this
 *  bitstream.  It is safe to call this function from a separate thread.
 *  The return value and behavior is the same as that of
 *  \ref NvMediaVideoEncoderBitsAvailable when called with \ref NVMEDIA_ENCODE_BLOCKING_TYPE_NEVER
 *  except that when \ref NVMEDIA_STATUS_OK is returned, the "buffer" will be
 *  filled in addition to the "numBytes".
 * \param[in] encoder The encoder to use.
 * \param[in] stats Upon succesful completion if not NULL, stats returns
 *       encoding statistics.
 * \param[in] numBytes
 *       Returns the size of the filled bitstream.
 * \param[in] buffer
 *      The buffer to be filled with the encoded data.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameter is NULL.
 * \n \ref NVMEDIA_STATUS_PENDING if an encode is in progress but not yet completed.
 * \n \ref NVMEDIA_STATUS_NONE_PENDING if no encode is in progress.
 */
NvMediaStatus
NvMediaVideoEncoderGetBits(
    NvMediaVideoEncoder *encoder,
    NvMediaEncodeStat *stats,
    unsigned int *numBytes,
    void *buffer
);

/**
 * \brief NvMediaVideoEncoderBitsAvailable returns the encode status
 *  and number of bytes available for the next frame (if any).
 *  The specific behavior depends on the specified \ref NvMediaBlockingType.
 *  It is safe to call this function from a separate thread.
 * \param[in] encoder The encoder to use.
 * \param[in] numBytesAvailable
 *      The number of bytes available in the next encoded frame.  This
 *      is valid only when the return value is \ref NVMEDIA_STATUS_OK.
 * \param[in] blockingType
 *      The following are supported blocking types:
 * \n \ref NVMEDIA_ENCODE_BLOCKING_TYPE_NEVER
 *        This type never blocks so "millisecondTimeout" is ignored.
 *        The following are possible return values:  \ref NVMEDIA_STATUS_OK
 *        \ref NVMEDIA_STATUS_PENDING or \ref NVMEDIA_STATUS_NONE_PENDING.
 * \n
 * \n \ref NVMEDIA_ENCODE_BLOCKING_TYPE_IF_PENDING
 *        Same as \ref NVMEDIA_ENCODE_BLOCKING_TYPE_NEVER except that \ref NVMEDIA_STATUS_PENDING
 *        will never be returned.  If an encode is pending this function
 *        will block until the status changes to \ref NVMEDIA_STATUS_OK or until the
 *        timeout has occurred.  Possible return values: \ref NVMEDIA_STATUS_OK,
 *        \ref NVMEDIA_STATUS_NONE_PENDING, \ref NVMEDIA_STATUS_TIMED_OUT.
 * \n
 * \n \ref NVMEDIA_ENCODE_BLOCKING_TYPE_ALWAYS
 *         This function will return only \ref NVMEDIA_STATUS_OK or
 *         \ref NVMEDIA_STATUS_TIMED_OUT.  It will block until those conditions.
 * \param[in] millisecondTimeout
 *       Timeout in milliseconds or \ref NVMEDIA_VIDEO_ENCODER_TIMEOUT_INFINITE
 *       if a timeout is not desired.
 * \return \ref NvMediaStatus The completion status of the operation.
 * Possible values are:
 * \n \ref NVMEDIA_STATUS_OK
 * \n \ref NVMEDIA_STATUS_BAD_PARAMETER if any of the input parameter is NULL.
 * \n \ref NVMEDIA_STATUS_PENDING if an encode is in progress but not yet completed.
 * \n \ref NVMEDIA_STATUS_NONE_PENDING if no encode is in progress.
 * \n \ref NVMEDIA_STATUS_NONE_PENDING if no encode is in progress.
 * \n \ref NVMEDIA_STATUS_TIMED_OUT if the operation timed out.
 */
NvMediaStatus
NvMediaVideoEncoderBitsAvailable(
    NvMediaVideoEncoder *encoder,
    unsigned int *numBytesAvailable,
    NvMediaBlockingType blockingType,
    unsigned int millisecondTimeout
);

/*@}*/
/*@}*/
/**
 * \defgroup history_core History
 * Provides change history for the NvMedia API.
 *
 * \section history_core Version History
 *
 * <b> Version 1.25 </b> April 10, 2013
 * - Added \ref NvMediaSurfaceType_R8G8B8A8_BottomOrigin surface type
 *
 * <b> Version 1.24 </b> March 12, 2013
 * - Added \ref NvMediaVideoMixerRenderSurface API
 * - Added \ref NVMEDIA_VIDEO_MIXER_FEATURE_SURFACE_RENDERING mixer flag
 * - Added \ref NvMediaVideoSurfacePutBits and \ref NvMediaVideoSurfaceGetBits APIs
 * - Added \ref NvMediaVideoOutputDevicesQuery API
 * - Removed EGL and X11 output types
 *
 * <b> Version 1.23 </b> October 26, 2012
 * - Added video encoder APIs
 *
 * <b> Version 1.22 </b> October 25, 2012
 * - Added bottom_field_flag field to \ref NvMediaPictureInfoVC1 structure
 *
 * <b> Version 1.21 </b> September 26, 2012
 * - Added \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_KD_DISPLAYABLE_ID attribute
 *
 * <b> Version 1.20 </b> August 16, 2012
 * - Added video capture APIs
 * - Added \ref NvMediaSurfaceType_R8G8B8A8 and \ref NvMediaSurfaceType_YV16x2 surface types
 *
 * <b> Version 1.19 </b> June 26, 2012
 * - Added \ref NvMediaVideoMixerBindOutput and \ref NvMediaVideoMixerUnbindOutput APIs
 * - Added \ref NvMediaVideoOutputDevice_Display0 and \ref NvMediaVideoOutputDevice_Display1 output
 * - Added \ref NVMEDIA_VIDEO_MIXER_ATTRIBUTE_GRAPHICS0_DST_RECT attribute
 *
 * <b> Version 1.18 </b> June 26, 2012
 * - Created nvmedia_audio.h from nvmedia.h
 *
 * <b> Version 1.17 </b> February 24, 2012
 * - Added \ref NvMediaVideoOutputType_OverlayYUV video output type
 * - Added \ref NvMediaVideoOutputType_OverlayRGB video output type
 *
 * <b> Version 1.14 </b> January 20, 2012
 * - Added \ref NvMediaVideoOutputSetPosition API
 * - Added \ref NvMediaVideoOutputType_KDandOverlay video output type
 *
 * <b> Version 1.13 </b> January 19, 2012
 * - Added fmo_aso_enable and scaling_matrix_present to \ref NvMediaPictureInfoH264
 *
 * <b> Version 1.12 </b> January 5, 2012
 * - Added \ref NVMEDIA_VIDEO_MIXER_FEATURE_DVD_MIXING_MODE mixer feature
 *
 * <b> Version 1.9 </b> September 19, 2011
 * - Added chroma_format_idc field to \ref NvMediaPictureInfoH264 structure
 * - Added full implemtation of multiple outputs
 *
 * <b> Version 1.8 </b> September 13, 2011
 * - Added \ref NvMediaVideoOutputCreate and \ref NvMediaVideoOutputDestroy
 *   APIs and modified all rendering APIs to accomodate multiple outputs
 * - Added a tag to \ref NvMediaVideoSurface structure for application use
 *
 * <b> Version 1.7 </b> September 8, 2011
 * - Split NVMEDIA_VIDEO_CODEC_MPEG1or2 into two defines:
 *   \n \ref NVMEDIA_VIDEO_CODEC_MPEG1 and \ref NVMEDIA_VIDEO_CODEC_MPEG2
 *
 * <b> Version 1.6 </b> August 30, 2011
 * - Added releaseList parameter to \ref NvMediaVideoMixerRender API
 *
 * <b> Version 1.5 </b> August 16, 2011
 * - Added MJPEG decoding support
 * - Added \ref NvMediaSurfaceType_YV16 and \ref NvMediaSurfaceType_YV24 surface types
 *
 * <b> Version 1.4 </b> July 25, 2011
 * - Added MPEG4 decoding support
 * - Added H.264 parameters to support FMO (Flexible Macroblock Ordering)
 * - Changed NvMediaTime from pointer to struct type
 *
 * <b> Version 1.3 </b> May 25, 2011
 * - Added premultiplied alpha attributes
 *
 * <b> Version 1.2 </b> March 16, 2011
 * - Added Primary and secondary video geometry to \ref NvMediaVideoMixerCreate
 * - Added sourceAspectRatio to \ref NvMediaVideoMixerCreate
 * - Added Hue as a new mixer attribute
 * - Changed the type of \ref NvMediaDevice, \ref NvMediaPalette and \ref NvMediaVideoMixer to void
 * - Changed the srcRect member to pointer type for \ref NvMediaBackground and \ref NvMediaGraphics
 * - Changed reference video surface types in codec specific structures to pointers
 * - Changed brightness, contrast, saturation and hue operating ranges to reflect actual hardware capablity
 * - Added rangeredfrm field to \ref NvMediaPictureInfoVC1 structure
 * - Typo fixes
 *
 * <b> Version 1.1 </b> February 9, 2011
 * - Version numbering added
 * - Surface lock and unlock functions added
 * - Surface mapping structure added
 * - Software video decoder usage explanation added
 * - updateRectangleList type changed to pointer-pointer
 *
 * <b> Version 1.0 </b> February 8, 2011
 * - Initial release
 *
 */
/*@}*/

#ifdef __cplusplus
};     /* extern "C" */
#endif

#endif /* _NVMEDIA_H */
