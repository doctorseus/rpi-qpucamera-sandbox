#include <stdio.h>
#include <stdbool.h>
#include "mmal_video_encoding.h"

static void mmal_video_encoding_init(mmal_video_encoding_handle_t *handle) {
    memset(handle, 0x0, sizeof(mmal_video_encoding_handle_t));
    
    handle->settings.width = 1280;
    handle->settings.height = 720;
    handle->settings.framerate = 30;
    handle->settings.output_file = fopen("video.h264", "w");
    handle->settings.input_buffer_size = 1382400;
    handle->settings.input_buffer_num = 3;
    handle->settings.inline_vectors = false;
}

static void encoder_input_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    fprintf(stderr, "> %s\n", __func__);
    mmal_buffer_header_release(buffer);
}

static void encode_frame_h264(mmal_video_encoding_handle_t *handle, MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    fprintf(stderr, "dataLength=%d\n", buffer->length);
    
    if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
        fprintf(stderr, "HEADER\n");
        
        // Write headers
        if(handle->settings.output_file)
            fwrite(buffer->data, sizeof(char), buffer->length, handle->settings.output_file);
    } else if((buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)) {
        fprintf(stderr, "MOTION\n");
    } else {
        fprintf(stderr, "DATA\n");
        
        if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) {
            fprintf(stderr, "#KEYFRAME\n");
        }
        
        if(buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
            fprintf(stderr, "#END\n");
        }
        
        // Write frame data (no iframe support yet)
        if(handle->settings.output_file)
            fwrite(buffer->data, sizeof(char), buffer->length, handle->settings.output_file);
    }
    
    /*
    if(buffer->length > 0 && !w) {
        FILE *imgfile = fopen("debugimg.jpg", "w");
        fwrite(buffer->data, sizeof(char), buffer->length, imgfile);
        fclose(imgfile);   
        w = true;
    }
    */
}

static void encoder_output_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer) {
    fprintf(stderr, "< %s\n", __func__);
    mmal_video_encoding_handle_t *handle = buffer->user_data;
    mmal_buffer_header_mem_lock(buffer);
    
    encode_frame_h264(handle, port, buffer);
    
    mmal_buffer_header_mem_unlock(buffer);
    mmal_buffer_header_release(buffer);
    if (port->is_enabled) {
        MMAL_STATUS_T status;
        MMAL_BUFFER_HEADER_T *new_buffer;
        new_buffer = mmal_queue_get(handle->encoder_output_pool->queue);
        if (new_buffer)
            status = mmal_port_send_buffer(port, new_buffer);
        if (!new_buffer || status != MMAL_SUCCESS) {
            fprintf(stderr, "unable to return a buffer to the encoder port\n\n");   
        }
    }
}

MMAL_STATUS_T mmal_video_encoding_create(mmal_video_encoding_handle_t *handle) {
    MMAL_STATUS_T status;

    mmal_video_encoding_init(handle);
    
    // create the component
    status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER, &handle->component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "mmal_component_create failed\n");
        goto error;
    }
    
    printf("input_num: %d\n", handle->component->input_num);
    printf("output_num: %d\n", handle->component->output_num);
    
    if (!handle->component->input_num || !handle->component->output_num) {
        status = MMAL_ENOSYS;
        fprintf(stderr, "video encoder doesn't have input/output ports");
        goto error;
    }
    

    handle->encoder_input  = handle->component->input[0];
    handle->encoder_output = handle->component->output[0];
    
    // Set input format
    MMAL_ES_FORMAT_T *format = handle->encoder_input->format;
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    format->es->video.width = handle->settings.width;
    format->es->video.height = handle->settings.height;
    format->es->video.crop.x = 0;
    format->es->video.crop.y = 0;
    format->es->video.crop.width = handle->settings.width;
    format->es->video.crop.height = handle->settings.height;
    format->es->video.frame_rate.num = handle->settings.framerate;
    format->es->video.frame_rate.den = 1;
    
    // same format on input and output
    mmal_format_copy(handle->encoder_output->format, handle->encoder_input->format);
    
    handle->encoder_input->buffer_size = handle->settings.input_buffer_size;//handle->encoder_input->buffer_size_recommended;
    handle->encoder_input->buffer_num = handle->settings.input_buffer_num; // handle->encoder_input->buffer_num_recommended;
    
    // Commit the port changes to the input port
    status = mmal_port_format_commit(handle->encoder_input);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to set format on video encoder input port");
        goto error;
    }
    
    
    // H.264
    handle->encoder_output->format->encoding = MMAL_ENCODING_H264;
    handle->encoder_output->format->bitrate  = 25000000; // 25MBit/s
    
    // MJPEG (seems not to work for high framerates)
    //handle->encoder_output->format->encoding = MMAL_ENCODING_MJPEG;
    //handle->encoder_output->format->bitrate = 2000000;

    // Set output buffer
    handle->encoder_output->buffer_size = handle->encoder_output->buffer_size_recommended;
    handle->encoder_output->buffer_num = handle->encoder_output->buffer_num_recommended;

    // We need to set the frame rate on output to 0, to ensure it gets
    // updated correctly from the input framerate when port connected
    //handle->encoder_output->format->es->video.frame_rate.num = 0;
    handle->encoder_output->format->es->video.frame_rate.num = handle->settings.framerate; // Try to set it fix framerate
    handle->encoder_output->format->es->video.frame_rate.den = 1;

    // Commit the port changes to the output port
    status = mmal_port_format_commit(handle->encoder_output);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "unable to set format on video encoder output port");
        goto error;
    }
    
    printf("encoder_input->buffer_size: %d\n", handle->encoder_input->buffer_size);
    printf("encoder_input->buffer_num: %d\n", handle->encoder_input->buffer_num);
    printf("encoder_output->buffer_size: %d\n", handle->encoder_output->buffer_size);
    printf("encoder_output->buffer_num: %d\n", handle->encoder_output->buffer_num);
    
    if(handle->settings.inline_vectors) {
        // Enable optional inline vectors (for motion analysis)
        status = mmal_port_parameter_set_boolean(handle->encoder_output, MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS, true);
        if (status != MMAL_SUCCESS) {
            fprintf(stderr, "Error: unable to set MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS on output port (%u)\n", status);
            goto error;
        }  
    }

    // Create input buffer pool
    handle->encoder_input_pool = (MMAL_POOL_T *) mmal_port_pool_create(handle->encoder_input, handle->encoder_input->buffer_num, handle->encoder_input->buffer_size);
    status = mmal_port_enable(handle->encoder_input, encoder_input_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Error: unable to enable encoder input port (%u)\n", status);
        goto error;
    }
    printf("- encoder input pool has been created\n");
    
    // Create output buffer pool
    handle->encoder_output_pool = (MMAL_POOL_T *) mmal_port_pool_create(handle->encoder_output, handle->encoder_output->buffer_num, handle->encoder_output->buffer_size);
    status = mmal_port_enable(handle->encoder_output, encoder_output_buffer_callback);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Error: unable to enable encoder output port (%u)\n", status);
        goto error;
    }
    printf("- encoder output pool has been created\n");
    
    //  Enable component
    status = mmal_component_enable(handle->component);
    if (status != MMAL_SUCCESS) {
        fprintf(stderr, "Unable to enable video encoder component");
        goto error;
    }
    
    // Send all the output buffers to the encoder port ready for use
    {
        int num = mmal_queue_length(handle->encoder_output_pool->queue);
        int q;
        for (q=0;q<num;q++) {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(handle->encoder_output_pool->queue);
            if (!buffer) {
                fprintf(stderr, "unable to get a required buffer %d from pool queue\n\n", q);
                goto error;
            } else if (mmal_port_send_buffer(handle->encoder_output, buffer)!= MMAL_SUCCESS) {
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

void mmal_video_encoding_destroy(mmal_video_encoding_handle_t *handle) {
    if(handle->settings.output_file)
        fclose(handle->settings.output_file);
    
    if (handle->component) {
        mmal_port_disable(handle->encoder_input);
        mmal_port_disable(handle->encoder_output);
        mmal_component_disable(handle->component);
        
        mmal_port_pool_destroy(handle->encoder_input, handle->encoder_input_pool);
        mmal_port_pool_destroy(handle->encoder_output, handle->encoder_output_pool);
        mmal_component_destroy(handle->component);
    }

}
