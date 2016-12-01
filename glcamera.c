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

#include <GLES2/gl2.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include "EGL/eglext.h"
#include <EGL/eglext_brcm.h>

#define check() assert(glGetError() == 0)

typedef struct app_state_s {
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;

    int screen_width;
    int screen_height;
    
    
    GLuint buf_vertex;
    GLuint attr_vertex;
    GLuint shader_vshader;
    GLuint shader_fshader;
    GLuint shader_program;
    
    GLuint cam_ytex, cam_utex, cam_vtex;
    EGLImageKHR cam_yimg;
    EGLImageKHR cam_uimg;
    EGLImageKHR cam_vimg;
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

typedef struct camera_s {
    MMAL_COMPONENT_T *component;
    MMAL_PORT_T *preview_port;
    MMAL_PORT_T *video_port;
    MMAL_PORT_T *still_port;
    
    MMAL_QUEUE_T *video_queue;
    MMAL_POOL_T *video_pool;
    
    camera_settings_t settings;
    RASPICAM_CAMERA_PARAMETERS parameters;
} camera_t;

static app_state_t app;
static camera_t camera;

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

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, app.cam_ytex);
        check();
        if(app.cam_yimg != EGL_NO_IMAGE_KHR){
            eglDestroyImageKHR(app.egl_display, app.cam_yimg);
            app.cam_yimg = EGL_NO_IMAGE_KHR;
        }

        app.cam_yimg = eglCreateImageKHR(app.egl_display, EGL_NO_CONTEXT, EGL_IMAGE_BRCM_MULTIMEDIA_Y, (EGLClientBuffer) buf->data, NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, app.cam_yimg);
        check();
/*   
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, app.cam_utex);
        check();
        if(app.cam_uimg != EGL_NO_IMAGE_KHR){
            eglDestroyImageKHR(app.egl_display, app.cam_uimg);
            app.cam_uimg = EGL_NO_IMAGE_KHR;
        }
        
        app.cam_uimg = eglCreateImageKHR(app.egl_display, EGL_NO_CONTEXT, EGL_IMAGE_BRCM_MULTIMEDIA_U, (EGLClientBuffer) buf->data, NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, app.cam_uimg);
        check();
     
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, app.cam_vtex);
        check();
        if(app.cam_vimg != EGL_NO_IMAGE_KHR){
            eglDestroyImageKHR(app.egl_display, app.cam_vimg);
            app.cam_vimg = EGL_NO_IMAGE_KHR;
        }
        app.cam_vimg = eglCreateImageKHR(app.egl_display, EGL_NO_CONTEXT, EGL_IMAGE_BRCM_MULTIMEDIA_V, (EGLClientBuffer) buf->data, NULL);
        check();
        glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, app.cam_vimg);
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

static void ogl_init(app_state_t *app) {
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    static EGL_DISPMANX_WINDOW_T nativewindow;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    static const EGLint attribute_list[] = {
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLConfig config;

    // get an EGL display connection
    app->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(app->egl_display != EGL_NO_DISPLAY);
    check();

    // initialize the EGL display connection
    result = eglInitialize(app->egl_display, NULL, NULL);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(app->egl_display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    check();

    // create an EGL rendering context
    app->egl_context = eglCreateContext(app->egl_display, config, EGL_NO_CONTEXT, context_attributes);
    assert(app->egl_context != EGL_NO_CONTEXT);
    check();

    // create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, &app->screen_width, &app->screen_height);
    assert( success >= 0 );

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = app->screen_width;
    dst_rect.height = app->screen_height;
    
    // NO FULLSCREEN - Top right corner
    int rect_width = app->screen_width / 4;
    int rect_height = app->screen_height / 4;
    dst_rect.width = rect_width;
    dst_rect.height = rect_height;
    dst_rect.x = app->screen_width - rect_width;
    dst_rect.y = 0;
    
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = app->screen_width << 16;
    src_rect.height = app->screen_height << 16;        

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
    0/*layer*/, &dst_rect, 0/*src*/,
    &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

    nativewindow.element = dispman_element;
    nativewindow.width = app->screen_width;
    nativewindow.height = app->screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );

    check();

    app->egl_surface = eglCreateWindowSurface(app->egl_display, config, &nativewindow, NULL );
    assert(app->egl_surface != EGL_NO_SURFACE);
    check();

    // connect the context to the surface
    result = eglMakeCurrent(app->egl_display, app->egl_surface, app->egl_surface, app->egl_context);
    assert(EGL_FALSE != result);
    check();

    // Set background color and clear buffers
    glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
    glClear( GL_COLOR_BUFFER_BIT );

    check();
}

static void ogl_resources_init(app_state_t *app) {    
    const GLchar *vshader_source =
    "attribute vec4 vertex;"
    "varying vec2 texcoord;"
    "void main(void) {"
    "   gl_Position = vertex;"
    "   texcoord = vertex.xy*0.5+0.5;"
    "}";

    const GLchar *fshader_source =
    "#extension GL_OES_EGL_image_external : require\n"
    "uniform samplerExternalOES cam_ytex;\n"
    "varying vec2 texcoord;"
    "void main(void) {"
    //"   gl_FragColor = vec4(texcoord.x, texcoord.y, 1.0, 1.0);"
    "   gl_FragColor = texture2D(cam_ytex, texcoord);\n"
    "}";
    
    // Compile vertex shader
    app->shader_vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(app->shader_vshader, 1, &vshader_source, 0);
    glCompileShader(app->shader_vshader);
    check();

    // Compile fragment shader
    app->shader_fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(app->shader_fshader, 1, &fshader_source, 0);
    glCompileShader(app->shader_fshader);
    check();

    // Link shader
    app->shader_program = glCreateProgram();
    glAttachShader(app->shader_program, app->shader_vshader);
    glAttachShader(app->shader_program, app->shader_fshader);
    glLinkProgram(app->shader_program);
    check();
    
    app->attr_vertex = glGetAttribLocation(app->shader_program, "vertex");
    
    // Vertex data
    static const GLfloat vertex_data[] = {
        -1.0,-1.0,1.0,1.0,
        1.0,-1.0,1.0,1.0,
        1.0,1.0,1.0,1.0,
        -1.0,1.0,1.0,1.0
    };
    
    // Upload vertex data to a buffer
    glGenBuffers(1, &app->buf_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, app->buf_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(app->attr_vertex, 4, GL_FLOAT, 0, 16, 0);
    glEnableVertexAttribArray(app->attr_vertex);
    check();
    
    // Prepare viewport
    glViewport(0, 0, app->screen_width, app->screen_height);
    check();
    
    // Prepare yuv textures
    glGenTextures(1, &app->cam_ytex);
    glGenTextures(1, &app->cam_utex);
    glGenTextures(1, &app->cam_vtex);
    
    app->cam_yimg = EGL_NO_IMAGE_KHR;
    app->cam_uimg = EGL_NO_IMAGE_KHR;
    app->cam_vimg = EGL_NO_IMAGE_KHR;
}

static void ogl_destroy(app_state_t *app) {
    eglDestroyContext(app->egl_display, app->egl_context);
    eglDestroySurface(app->egl_display, app->egl_surface);
    eglTerminate(app->egl_display);
}

static void ogl_draw(app_state_t *app){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    check();

    // draw frame START
    glBindBuffer(GL_ARRAY_BUFFER, app->buf_vertex);
    check();
    
    glUseProgram(app->shader_program);
    check();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, app->cam_ytex);
    check();

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    check();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // draw frame END

    glFlush();
    glFinish();
    check();

    eglSwapBuffers(app->egl_display, app->egl_surface);
    check();
}

int main(int argc, char **argv)
{    
    bcm_host_init();
    
    ogl_init(&app);
    ogl_resources_init(&app);

    mmal_camera_create();

    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop) {
        //wait 5 seconds
        if(mmal_camera_read_frame()){
            //printf("frame received\n");
        }
        
        ogl_draw(&app);
    }

    mmal_camera_destroy();

    ogl_destroy(&app);

    return EXIT_SUCCESS;
}

