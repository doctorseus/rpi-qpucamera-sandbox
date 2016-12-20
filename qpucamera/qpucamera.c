#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"

static bool camera_read_frame(mmal_camera_handle_t *handle) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(handle->video_queue)){
        //mmal_buffer_header_mem_lock(buf);

        printf("size=%d, data=%p\n", buf->length, buf->data);
        printf("data_buf[0]=%d\n", buf->data[0]);
            
        /*
        // VC only
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        printf("handle=%x\n", vc_handle);
        
        
        unsigned int busp = mem_lock(app.mailboxfd, vc_handle);
        
        char *vcmem = mapmem(BUS_TO_PHYS(busp), buf->length);
        printf("data_gpu[0]=%d\n", vcmem[0]);
        
        unmapmem(vcmem, buf->length);
        
        unsigned int buss = mem_unlock(app.mailboxfd, vc_handle);
        printf("unlock status=%x\n", buss);
        */

        //mmal_buffer_header_mem_unlock(buf);
        mmal_buffer_header_release(buf);

        if(handle->preview_port->is_enabled){
            MMAL_STATUS_T status;
            MMAL_BUFFER_HEADER_T *new_buffer;
            new_buffer = mmal_queue_get(handle->video_pool->queue);
            if (new_buffer)
                status = mmal_port_send_buffer(handle->preview_port, new_buffer);
            if (!new_buffer || status != MMAL_SUCCESS)
                fprintf(stderr, "Unable to return a buffer to the video port\n\n");
        }

        return true;
    }else{
        return false; //no buffer received
    }
}

int main(int argc, char **argv)
{
    bcm_host_init();
    
    mmal_camera_handle_t camera_handle;
    
    mmal_camera_init(&camera_handle);
    mmal_camera_create(&camera_handle);
    
    time_t tstop = time(NULL) + 2;
    while (time(NULL) < tstop) {
        //wait 5 seconds
        if(camera_read_frame(&camera_handle)){
            printf("frame received\n");
        }
    }
    
    mmal_camera_destroy(&camera_handle);

    return EXIT_SUCCESS;
}
