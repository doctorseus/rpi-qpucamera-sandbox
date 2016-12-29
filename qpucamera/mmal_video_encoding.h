#ifndef MMAL_VIDEO_ENCODING_H
#define MMAL_VIDEO_ENCODING_H

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

typedef struct mmal_video_encoding_handle_s {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *encoder_input;
    MMAL_PORT_T *encoder_output;
    
    MMAL_POOL_T *encoder_input_pool;
    MMAL_POOL_T *encoder_output_pool;

} mmal_video_encoding_handle_t;

MMAL_STATUS_T mmal_video_encoding_create(mmal_video_encoding_handle_t *handle);
void mmal_video_encoding_destroy(mmal_video_encoding_handle_t *handle);

/*
#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

#include "RaspiCamControl.h"

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT    0
#define MMAL_CAMERA_VIDEO_PORT      1
#define MMAL_CAMERA_CAPTURE_PORT    2

typedef struct camera_settings_s {
    int width;
    int height;
    int framerate;
} camera_settings_t;

typedef struct mmal_camera_handle_s {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *preview_port;
    MMAL_PORT_T *video_port;
    MMAL_PORT_T *still_port;
    
    MMAL_QUEUE_T *video_queue;
    MMAL_POOL_T *video_pool;
    
    camera_settings_t settings;
    RASPICAM_CAMERA_PARAMETERS parameters;
} mmal_camera_handle_t;

void mmal_camera_init(mmal_camera_handle_t *handle);
MMAL_STATUS_T mmal_camera_create(mmal_camera_handle_t *handle);
void mmal_camera_destroy(mmal_camera_handle_t *handle);
*/

#endif

