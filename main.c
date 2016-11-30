/*
 * main.c
 * 
 * Copyright 2016  <pi@raspberrypi>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */


#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"

#include "RaspiCamControl.h"

typedef struct app_state_s {
    
} app_state_t;

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT    0
#define MMAL_CAMERA_VIDEO_PORT      1
#define MMAL_CAMERA_CAPTURE_PORT    2

typedef struct camera_settings_s {
    int width;
    int height;
    int framerate;
} camera_settings_t;

struct {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *preview_port;
    MMAL_PORT_T *video_port;
    MMAL_PORT_T *still_port;
    
    MMAL_QUEUE_T *video_queue;
    MMAL_POOL_T *video_pool;
    
    camera_settings_t settings;
    RASPICAM_CAMERA_PARAMETERS parameters;
} camera;

/**
 *  buffer header callback function for camera control
 *
 *  No actions taken in current version
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void mmal_camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED) {
        
        MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
        switch (param->hdr.id) {
            case MMAL_PARAMETER_CAMERA_SETTINGS: {
                MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*) param;
                printf("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
                    settings->exposure,
                    settings->analog_gain.num, settings->analog_gain.den,
                    settings->digital_gain.num, settings->digital_gain.den);
                printf("AWB R=%u/%u, B=%u/%u",
                    settings->awb_red_gain.num, settings->awb_red_gain.den,
                    settings->awb_blue_gain.num, settings->awb_blue_gain.den
                );
            }
            break;
        }
    } else if (buffer->cmd == MMAL_EVENT_ERROR) {
        printf("No data received from sensor. Check all connections, including the Sunny one on the camera board");
    } else {
        printf("Received unexpected camera control callback event, 0x%08x", buffer->cmd);   
    }
    
    mmal_buffer_header_release(buffer);
}

void mmal_camera_output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
	//to handle the user not reading frames, remove and return any pre-existing ones
	if(mmal_queue_length(camera.video_queue) >= 2)
	{
        MMAL_BUFFER_HEADER_T* existing_buffer;
		if(existing_buffer = mmal_queue_get(camera.video_queue))
		{
			mmal_buffer_header_release(existing_buffer);
			if (port->is_enabled)
			{
				MMAL_STATUS_T status;
				MMAL_BUFFER_HEADER_T *new_buffer;
				new_buffer = mmal_queue_get(camera.video_pool->queue);
				if (new_buffer)
                    status = mmal_port_send_buffer(port, new_buffer);
				if (!new_buffer || status != MMAL_SUCCESS) {
                    fprintf(stderr, "unable to return a buffer to the video port\n\n");   
                }
			}	
		}
	}

	//add the buffer to the output queue
	mmal_queue_put(camera.video_queue, buffer);
}

bool mmal_camera_read_frame(void) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(camera.video_queue)){
        //mmal_buffer_header_mem_lock(buf);

        printf("data=%p\n", buf->data);

        /*

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, cam_ytex);
        check();
        if(yimg != EGL_NO_IMAGE_KHR){
        eglDestroyImageKHR(GDisplay, yimg);
        yimg = EGL_NO_IMAGE_KHR;
        }
        yimg = eglCreateImageKHR(GDisplay, 
        EGL_NO_CONTEXT, 
        EGL_IMAGE_BRCM_MULTIMEDIA_Y, 
        (EGLClientBuffer) buf->data, 
        NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, yimg);
        check();

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, cam_utex);
        check();
        if(uimg != EGL_NO_IMAGE_KHR){
        eglDestroyImageKHR(GDisplay, uimg);
        uimg = EGL_NO_IMAGE_KHR;
        }
        uimg = eglCreateImageKHR(GDisplay, 
        EGL_NO_CONTEXT, 
        EGL_IMAGE_BRCM_MULTIMEDIA_U, 
        (EGLClientBuffer) buf->data, 
        NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, uimg);
        check();

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, cam_vtex);
        check();
        if(vimg != EGL_NO_IMAGE_KHR){
        eglDestroyImageKHR(GDisplay, vimg);
        vimg = EGL_NO_IMAGE_KHR;
        }
        vimg = eglCreateImageKHR(GDisplay, 
        EGL_NO_CONTEXT, 
        EGL_IMAGE_BRCM_MULTIMEDIA_V, 
        (EGLClientBuffer) buf->data, 
        NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, vimg);
        check();
        */

        //mmal_buffer_header_mem_unlock(buf);
        mmal_buffer_header_release(buf);

        if(camera.preview_port->is_enabled){
            MMAL_STATUS_T status;
            MMAL_BUFFER_HEADER_T *new_buffer;
            new_buffer = mmal_queue_get(camera.video_pool->queue);
            if (new_buffer)
                status = mmal_port_send_buffer(camera.preview_port, new_buffer);
            if (!new_buffer || status != MMAL_SUCCESS)
                printf("Unable to return a buffer to the video port\n\n");
        }

        return true;
    }else{
        return false; //no buffer received
    }
}

static void mmal_camera_init() {
    camera.settings.width = 1280;
    camera.settings.height = 720;
    camera.settings.framerate = 30;
}

static MMAL_STATUS_T mmal_camera_create() {
    MMAL_ES_FORMAT_T *format;
    MMAL_STATUS_T status;

    //init
    mmal_camera_init();

    //create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera.component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "mmal_component_create failed\n");
        goto error;
    }
    
    if (!camera.component->output_num) {
        status = MMAL_ENOSYS;
        fprintf(stderr, "camera doesn't have output ports\n");
        goto error;
    }
    
    camera.preview_port = camera.component->output[MMAL_CAMERA_PREVIEW_PORT];
    camera.video_port = camera.component->output[MMAL_CAMERA_VIDEO_PORT];
    camera.still_port = camera.component->output[MMAL_CAMERA_CAPTURE_PORT];
    
    //enable the camera, and tell it its control callback function
    status = mmal_port_enable(camera.component->control, mmal_camera_control_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to enable control port : error %d\n", status);
        goto error;
    }
        
    //set camera parameters.
    MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
    {
        { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
        .max_stills_w = camera.settings.width,
        .max_stills_h = camera.settings.height,
        .stills_yuv422 = 0,
        .one_shot_stills = 0,
        .max_preview_video_w = camera.settings.width,
        .max_preview_video_h = camera.settings.height,
        .num_preview_video_frames = 3,
        .stills_capture_circular_buffer_height = 0,
        .fast_preview_resume = 0,
        .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
    };
    
    status = mmal_port_parameter_set(camera.component->control, &cam_config.hdr);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to set camera parameters : error %d\n", status);
        goto error;
    }
    
    // setup preview port format
    format = camera.preview_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = camera.settings.width,
    format->es->video.height = camera.settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = camera.settings.width,
    format->es->video.crop.height = camera.settings.height,
    format->es->video.frame_rate.num = camera.settings.framerate,
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(camera.preview_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set preview port format : error %d\n", status);
        goto error;
    }

    //setup video port format
    format = camera.video_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = camera.settings.width,
    format->es->video.height = camera.settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = camera.settings.width,
    format->es->video.crop.height = camera.settings.height,
    format->es->video.frame_rate.num = camera.settings.framerate,
    format->es->video.frame_rate.den = 1;
    
    status = mmal_port_format_commit(camera.video_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set video port format : error %d\n", status);
        goto error;
    }

    //setup still port format
    format = camera.still_port->format;
    format->encoding = MMAL_ENCODING_OPAQUE;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = camera.settings.width,
    format->es->video.height = camera.settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = camera.settings.width,
    format->es->video.crop.height = camera.settings.height,
    format->es->video.frame_rate.num = 1;
    format->es->video.frame_rate.den = 1;
    
    status = mmal_port_format_commit(camera.still_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set still port format : error %d\n", status);
        goto error;
    }

    status = mmal_port_parameter_set_boolean(camera.preview_port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "failed to enable zero copy on camera video port\n");
        goto error;
    }

    status = mmal_port_format_commit(camera.preview_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera format couldn't be set\n");
        goto error;
    }
    
    
    // a pool of opaque buffer handles must be allocated in the client.
    camera.preview_port->buffer_num = 3;
    camera.preview_port->buffer_size = camera.preview_port->buffer_size_recommended;

    // Pool + queue to hold preview frames
    camera.video_pool = (MMAL_POOL_T*) mmal_port_pool_create(camera.preview_port, camera.preview_port->buffer_num, camera.preview_port->buffer_size);
    if (!camera.video_pool) {
        status = MMAL_ENOMEM;
        fprintf(stderr, "error allocating camera video pool. Buffer num: %d Buffer size: %d\n", camera.preview_port->buffer_num, camera.preview_port->buffer_size);
        goto error;
    }

    // Place filled buffers from the preview port in a queue to render 
    camera.video_queue = mmal_queue_create();
    if (!camera.video_queue) {
        status = MMAL_ENOMEM;
        fprintf(stderr, "error allocating video buffer queue\n");
        goto error;
    }

    // Enable preview port callback
    status = mmal_port_enable(camera.preview_port, mmal_camera_output_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to enable preview port : error %d\n", status);
        goto error;
    }

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&camera.parameters);
    //apply all camera parameters
    raspicamcontrol_set_all_parameters(camera.component, &camera.parameters);

    // Enable camera/component
    status = mmal_component_enable(camera.component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera component couldn't be enabled");
        goto error;
    }

    //send all the buffers in our pool to the video port ready for use
    {
        int num = mmal_queue_length(camera.video_pool->queue);
        int q;
        for (q=0;q<num;q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(camera.video_pool->queue);
            if (!buffer) {
                fprintf(stderr, "unable to get a required buffer %d from pool queue\n\n", q);
                goto error;
            } else if (mmal_port_send_buffer(camera.preview_port, buffer)!= MMAL_SUCCESS) {
                fprintf(stderr, "unable to send a buffer to port (%d)\n\n", q);
                goto error;
            }
        }
    }

    return MMAL_SUCCESS;

    error:
    if (camera.component)
        mmal_component_destroy(camera.component);

    return status;
}

static void mmal_camera_destroy() {
    if (camera.component)
        mmal_component_destroy(camera.component);
}

int main(int argc, char **argv)
{
    app_state_t app;
    
    bcm_host_init();

    mmal_camera_create();

    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop) {
        //wait 5 seconds
        if(mmal_camera_read_frame()){
            printf("frame received\n");
        }
    }

    mmal_camera_destroy();

    return EXIT_SUCCESS;
}

