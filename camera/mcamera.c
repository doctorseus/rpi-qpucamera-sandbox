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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "bcm_host.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

#include "RaspiCamControl.h"

// Defines for vc mem mapping
#include "mailbox.h"
#include <linux/ioctl.h>
#define VC_MEM_IOC_MAGIC 'v'
#define VC_MEM_IOC_MEM_PHYS_ADDR    _IOR( VC_MEM_IOC_MAGIC, 0, unsigned long )
#define VC_MEM_IOC_MEM_SIZE         _IOR( VC_MEM_IOC_MAGIC, 1, uint32_t )
#define VC_MEM_IOC_MEM_BASE         _IOR( VC_MEM_IOC_MAGIC, 2, uint32_t )
#define VC_MEM_IOC_MEM_LOAD         _IOR( VC_MEM_IOC_MAGIC, 3, uint32_t )

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

struct {
    int mailboxfd;
    int vcfd;
    volatile uint32_t * vcmem;
} app;

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

        printf("size=%d, data=%p\n", buf->length, buf->data);
        printf("data_buf[0]=%d\n", buf->data[0]);
        
        // VC only
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        printf("handle=%x\n", vc_handle);
        
        
        unsigned int busp = mem_lock(app.mailboxfd, vc_handle);
        
        char *vcmem = mapmem(BUS_TO_PHYS(busp), buf->length);
        printf("data_gpu[0]=%d\n", vcmem[0]);
        
        unmapmem(vcmem, buf->length);
        
        /*
        unsigned int ad = (busp & 0x3FFFFFFF);
        printf("lock busp=%x\n", busp);
        printf("lock busp=%x\n", (busp & 0x3FFFFFFF));
        printf("lock busp=%x\n", ad);
        printf("data=%d\n", app.vcmem[ad]);
        */
        
        unsigned int buss = mem_unlock(app.mailboxfd, vc_handle);
        printf("unlock status=%x\n", buss);
        
        /*
        size=1382400, data=0x73f5c000
        handle=13
        lock busp=fd08a000
        unlock status=0
        frame received
        size=1382400, data=0x73e0a000
        handle=21
        lock busp=fcf36000
        unlock status=0
        frame received
        size=1382400, data=0x740ae000
        handle=6
        lock busp=fd1dd000
        unlock status=0
        frame received
        
        Address Perm   Offset Device  Inode  Size  Rss Pss Referenced Anonymous Shared_Hugetlb Private_Hugetlb Swap SwapPss Locked Mapping
        73f0a000 rw-s 3cf36000  00:06   1153  1352    0   0          0         0              0               0    0       0      0 vcsm
        7405c000 rw-s 3d08a000  00:06   1153  1352    0   0          0         0              0               0    0       0      0 vcsm
        741ae000 rw-s 3d1dd000  00:06   1153  1352    0   0          0         0              0               0    0       0      0 vcsm

        fd08a000 = 0000000011111101000010001010000000000000
        3d08a000 = 0000000000111101000010001010000000000000 = 1023975424
                              11101110000000000000000000000
        c0000000 = 0000000011000000000000000000000000000000
                   0000000000100000000000000000000000000000
                             111111111111111111111111111111
                             
                111100111100110110000000000000
                111101000010001010000000000000
                111101000111011101000000000000
                111100000000000000000000000000
                000001111111111111111111111111
                
        VC_MEM_IOC_MEM_PHYS_ADDR = 00000000
        VC_MEM_IOC_MEM_SIZE = 3f000000
        VC_MEM_IOC_MEM_BASE = 3dc00000 = 1035993088
        VC_MEM_IOC_MEM_LOAD = 3dc00000
        */
        
        
        // VCSM only
        /*
        unsigned int vcsm_handle = vcsm_usr_handle(buf->data);
        printf("handle=%x\n", vcsm_handle);
        printf("lock p=%p\n", vcsm_lock(vcsm_handle));
        vcsm_unlock_hdl(vcsm_handle);
        printf("unlock\n");
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
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    //format->encoding = MMAL_ENCODING_OPAQUE;
    //format->encoding_variant = MMAL_ENCODING_I420;
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
    format->encoding = MMAL_ENCODING_I420;
    format->encoding_variant = MMAL_ENCODING_I420;
    //format->encoding = MMAL_ENCODING_OPAQUE;
    //format->encoding_variant = MMAL_ENCODING_I420;
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

// Space at the end of memory we assume is holding code and fixed start.elf buffers
#define VC_MEM_IMAGE 18706228

static int is_qpu_end(volatile uint32_t *inst) {
	return (inst[0] == 0x009e7000) && (inst[1] == 0x300009e7) 
		&& (inst[2] == 0x009e7000) && (inst[3] == 0x100009e7)
		&& (inst[4] == 0x009e7000) && ((inst[5] == 0x100009e7) || (inst[5] == 0x500009e7));
}

static int vcdev_open() {
    //app.vcmem = mapmem(BUS_TO_PHYS(ptr->vc+host.mem_map), size);
    
    /*
    int fd = open("/dev/vc-mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Unable to open /dev/vc-mem, run as sudo\n");
        exit(EXIT_FAILURE);
    }

    unsigned long address, size, base, load;
    ioctl(fd, VC_MEM_IOC_MEM_PHYS_ADDR, &address);
    ioctl(fd, VC_MEM_IOC_MEM_SIZE, &size);
    ioctl(fd, VC_MEM_IOC_MEM_BASE, &base);
    ioctl(fd, VC_MEM_IOC_MEM_LOAD, &load);


	volatile uint32_t *vc = (volatile uint32_t *)mmap( 0, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (vc == (uint32_t *)-1) {
		printf("mmap failed %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

    printf("VC_MEM_IOC_MEM_PHYS_ADDR = %08x\n", address);
    printf("VC_MEM_IOC_MEM_SIZE = %08x\n", size);
    printf("VC_MEM_IOC_MEM_BASE = %08x\n", base);
    printf("VC_MEM_IOC_MEM_LOAD = %08x\n", load);
    printf("vc = %08x\n", vc);

    app.vcmem = vc;
    */
    //printf("Scanning for QPU code fragments...\n");

    /*
    for (int i = 0; i < (size-VC_MEM_IMAGE)/4; i++) {
        if (is_qpu_end(&vc[i])) {
            printf("%08x:", i*4);
            for (int j=0; j<4; j++) {
                printf(" %08x %08x", vc[i+j*2], vc[i+j*2+1]);
            }
            printf("\n");
        }
    }
    */

    int fd=0;
    return fd;
}

int main(int argc, char **argv)
{
    bcm_host_init();

    app.vcfd = vcdev_open();
    app.mailboxfd = mbox_open();

    mmal_camera_create();

    time_t tstop = time(NULL) + 2;
    while (time(NULL) < tstop) {
        //wait 5 seconds
        if(mmal_camera_read_frame()){
            printf("frame received\n");
        }
    }

    mmal_camera_destroy();

    mbox_close(app.mailboxfd);

    return EXIT_SUCCESS;
}

