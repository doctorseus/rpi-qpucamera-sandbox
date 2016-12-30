#ifndef MMAL_VIDEO_ENCODING_H
#define MMAL_VIDEO_ENCODING_H

#include <stdbool.h>
#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

typedef struct mmal_video_encoding_settings_s {
    int width;
    int height;
    int framerate;
    FILE *output_file;
    
    uint32_t input_buffer_size;
    uint32_t input_buffer_num;
    bool inline_vectors;
} mmal_video_encoding_settings_t;

typedef struct mmal_video_encoding_handle_s {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *encoder_input;
    MMAL_PORT_T *encoder_output;
    
    MMAL_POOL_T *encoder_input_pool;
    MMAL_POOL_T *encoder_output_pool;

    mmal_video_encoding_settings_t settings;
} mmal_video_encoding_handle_t;

MMAL_STATUS_T mmal_video_encoding_create(mmal_video_encoding_handle_t *handle);
void mmal_video_encoding_destroy(mmal_video_encoding_handle_t *handle);

#endif

