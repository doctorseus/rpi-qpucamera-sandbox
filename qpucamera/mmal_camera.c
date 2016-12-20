#include <stdio.h>
#include "mmal_camera.h"

void mmal_camera_init(mmal_camera_handle_t *handle) {
    memset(handle, 0x0, sizeof(mmal_camera_handle_t));
    
    handle->settings.width = 1280;
    handle->settings.height = 720;
    handle->settings.framerate = 30;
}

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

static void mmal_camera_output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    mmal_camera_handle_t *handle = buffer->user_data;
	//to handle the user not reading frames, remove and return any pre-existing ones
	if(mmal_queue_length(handle->video_queue) >= 2)
	{
        fprintf(stderr, "WARNING: dropped frame...\n");
        MMAL_BUFFER_HEADER_T* existing_buffer;
		if(existing_buffer = mmal_queue_get(handle->video_queue))
		{
			mmal_buffer_header_release(existing_buffer);
			if (port->is_enabled)
			{
				MMAL_STATUS_T status;
				MMAL_BUFFER_HEADER_T *new_buffer;
				new_buffer = mmal_queue_get(handle->video_pool->queue);
				if (new_buffer)
                    status = mmal_port_send_buffer(port, new_buffer);
				if (!new_buffer || status != MMAL_SUCCESS) {
                    fprintf(stderr, "unable to return a buffer to the video port\n\n");   
                }
			}	
		}
	}

	//add the buffer to the output queue
	mmal_queue_put(handle->video_queue, buffer);
}

MMAL_STATUS_T mmal_camera_create(mmal_camera_handle_t *handle) {
    MMAL_ES_FORMAT_T *format;
    MMAL_STATUS_T status;

    //create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &handle->component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "mmal_component_create failed\n");
        goto error;
    }

    if (!handle->component->output_num) {
        status = MMAL_ENOSYS;
        fprintf(stderr, "camera doesn't have output ports\n");
        goto error;
    }

    handle->preview_port = handle->component->output[MMAL_CAMERA_PREVIEW_PORT];
    handle->video_port = handle->component->output[MMAL_CAMERA_VIDEO_PORT];
    handle->still_port = handle->component->output[MMAL_CAMERA_CAPTURE_PORT];
    
    //enable the camera, and tell it its control callback function
    status = mmal_port_enable(handle->component->control, mmal_camera_control_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to enable control port : error %d\n", status);
        goto error;
    }

    //set camera parameters.
    MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
    {
        { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
        .max_stills_w = handle->settings.width,
        .max_stills_h = handle->settings.height,
        .stills_yuv422 = 0,
        .one_shot_stills = 0,
        .max_preview_video_w = handle->settings.width,
        .max_preview_video_h = handle->settings.height,
        .num_preview_video_frames = 3,
        .stills_capture_circular_buffer_height = 0,
        .fast_preview_resume = 0,
        .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
    };
    
    status = mmal_port_parameter_set(handle->component->control, &cam_config.hdr);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to set camera parameters : error %d\n", status);
        goto error;
    }
    
    // setup preview port format
    format = handle->preview_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    //format->encoding = MMAL_ENCODING_OPAQUE;
    //format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = handle->settings.width,
    format->es->video.height = handle->settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = handle->settings.width,
    format->es->video.crop.height = handle->settings.height,
    format->es->video.frame_rate.num = handle->settings.framerate,
    format->es->video.frame_rate.den = 1;

    status = mmal_port_format_commit(handle->preview_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set preview port format : error %d\n", status);
        goto error;
    }

    //setup video port format
    format = handle->video_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = handle->settings.width,
    format->es->video.height = handle->settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = handle->settings.width,
    format->es->video.crop.height = handle->settings.height,
    format->es->video.frame_rate.num = handle->settings.framerate,
    format->es->video.frame_rate.den = 1;
    
    status = mmal_port_format_commit(handle->video_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set video port format : error %d\n", status);
        goto error;
    }

    //setup still port format
    format = handle->still_port->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    //format->encoding = MMAL_ENCODING_OPAQUE;
    //format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = handle->settings.width,
    format->es->video.height = handle->settings.height,
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = handle->settings.width,
    format->es->video.crop.height = handle->settings.height,
    format->es->video.frame_rate.num = 1;
    format->es->video.frame_rate.den = 1;
    
    status = mmal_port_format_commit(handle->still_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "couldn't set still port format : error %d\n", status);
        goto error;
    }

    status = mmal_port_parameter_set_boolean(handle->preview_port, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "failed to enable zero copy on camera video port\n");
        goto error;
    }

    status = mmal_port_format_commit(handle->preview_port);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera format couldn't be set\n");
        goto error;
    }
    
    
    // a pool of opaque buffer handles must be allocated in the client.
    handle->preview_port->buffer_num = 3;
    handle->preview_port->buffer_size = handle->preview_port->buffer_size_recommended;

    // Pool + queue to hold preview frames
    handle->video_pool = (MMAL_POOL_T*) mmal_port_pool_create(handle->preview_port, handle->preview_port->buffer_num, handle->preview_port->buffer_size);
    if (!handle->video_pool) {
        status = MMAL_ENOMEM;
        fprintf(stderr, "error allocating camera video pool. Buffer num: %d Buffer size: %d\n", handle->preview_port->buffer_num, handle->preview_port->buffer_size);
        goto error;
    }

    // Place filled buffers from the preview port in a queue to render 
    handle->video_queue = mmal_queue_create();
    if (!handle->video_queue) {
        status = MMAL_ENOMEM;
        fprintf(stderr, "error allocating video buffer queue\n");
        goto error;
    }

    // Enable preview port callback
    status = mmal_port_enable(handle->preview_port, mmal_camera_output_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to enable preview port : error %d\n", status);
        goto error;
    }

    // Set up the camera_parameters to default
    raspicamcontrol_set_defaults(&handle->parameters);
    //apply all camera parameters
    raspicamcontrol_set_all_parameters(handle->component, &handle->parameters);

    // Enable camera/component
    status = mmal_component_enable(handle->component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "camera component couldn't be enabled");
        goto error;
    }

    //send all the buffers in our pool to the video port ready for use
    {
        int num = mmal_queue_length(handle->video_pool->queue);
        int q;
        for (q=0;q<num;q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(handle->video_pool->queue);
            if (!buffer) {
                fprintf(stderr, "unable to get a required buffer %d from pool queue\n\n", q);
                goto error;
            } else if (mmal_port_send_buffer(handle->preview_port, buffer)!= MMAL_SUCCESS) {
                fprintf(stderr, "unable to send a buffer to port (%d)\n\n", q);
                goto error;
            }
            // Set user_data pointer to handle
            buffer->user_data = handle;
        }
    }

    return MMAL_SUCCESS;

    error:
    if (handle->component)
        mmal_component_destroy(handle->component);

    return status;
}

void mmal_camera_destroy(mmal_camera_handle_t *handle) {
    if (handle->component)
        mmal_component_destroy(handle->component);
}
