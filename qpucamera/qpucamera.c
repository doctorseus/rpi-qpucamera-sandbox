#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"
#include "qpu_program.h"
#include "mailbox.h"
#include "tga.h"

static bool stop = false;
static unsigned int frames = 0;

unsigned int vcsm_vc_hdl_from_ptr( void *usr_ptr );

static void camera_data_to_rgba(char *frame_buffer, unsigned int frame_size, char *image_buffer, unsigned int image_size) {
    uint8_t* in = frame_buffer;
    uint8_t* out = image_buffer;
    uint8_t* end = image_buffer + image_size;

    while (out < end) {
        out[0] = in[0];
        out[1] = in[0];
        out[2] = in[0];
        out[3] = 0xff;
        in += 1;
        out += 4;
    }
}

static void camera_progress_frame(qpu_program_handle_t *qpu_handle, unsigned int frameptr) {
    qpu_buffer_handle_t input_buffer;
    qpu_buffer_create(&input_buffer, qpu_handle->mb, 1280*720, 4096);
    qpu_buffer_handle_t output_buffer;
    qpu_buffer_create(&output_buffer, qpu_handle->mb, 1280*720, 4096);
    
    qpu_buffer_lock(&input_buffer);
    qpu_buffer_lock(&output_buffer);
    
    char *input_buffer_int32 = input_buffer.arm_mem_ptr;
    char c = 0;
    bool s = true;
    for (int j=0; j < 1280*720; j++) {
        input_buffer_int32[j] = c;
        if(s){
            if(c == 0xff){
                s = false;
                c--;
            }else{
                c++;
            }
        }else{
            if(c == 0x00){
                s = true;
                c++;
            }else{
                c--;
            }
        }
    }

    unsigned int uniforms[3];
    uniforms[0] = 1280*720;
    uniforms[1] = frameptr; //input_buffer.mem_ptr; 
    uniforms[2] = output_buffer.mem_ptr;
    qpu_program_execute(qpu_handle, uniforms, 3);

    char *output_buffer_char = output_buffer.arm_mem_ptr;
    for (int j=0; j < (16*4*64); j++) {
        printf("%02x ", output_buffer_char[j]);
        if((j+1) % (16*4) == 0)
            printf("\n");
    }
    /*
    unsigned int *output_buffer_int32 = output_buffer.arm_mem_ptr;
    for (int j=0; j < (16*64); j++) {
        printf("%08x ", output_buffer_int32[j]);
        if((j+1) % 16 == 0)
            printf("\n");
    }
    */

    // Write output debug image
    int image_width = 1280;
    int image_height = 720;
    int image_size = image_width * image_height * 4;
    char image_buffer[image_size];
    FILE* output_file = fopen("output.tga", "w");
    camera_data_to_rgba(output_buffer.arm_mem_ptr, 1280*720, image_buffer, image_size);
    write_tga(output_file, image_width, image_height, image_buffer, image_size);
    fclose(output_file);
    // END - Write output debug image
    
    qpu_buffer_unlock(&input_buffer);
    qpu_buffer_destroy(&input_buffer);
    qpu_buffer_unlock(&output_buffer);
    qpu_buffer_destroy(&output_buffer);
}

static bool camera_read_frame(mmal_camera_handle_t *camera_handle, qpu_program_handle_t *qpu_handle) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(camera_handle->video_queue)){
        //mmal_buffer_header_mem_lock(buf);

        printf("size=%d, data=%p\n", buf->length, buf->data);
        printf("data_buf[0]=%d\n", buf->data[0]);
            
        if(frames >= 15){
            
        // Get physical memory ptr and lock frame buffer
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        unsigned int frameptr = mem_lock(qpu_handle->mb, vc_handle);
        
        camera_progress_frame(qpu_handle, frameptr);
        
        // Write output debug image
        int image_width = 1280;
        int image_height = 720;
        int image_size = image_width * image_height * 4;
        char image_buffer[image_size];
        FILE* output_file = fopen("input.tga", "w");
        camera_data_to_rgba(buf->data, buf->length, image_buffer, image_size);
        write_tga(output_file, image_width, image_height, image_buffer, image_size);
        fclose(output_file);
        
        FILE* buffer_file = fopen("input.data", "w");
        fwrite(buf->data, sizeof(char), buf->length, buffer_file);
        fclose(buffer_file);
        // END - Write output debug image

        // Unlock frame buffer
        mem_unlock(qpu_handle->mb, vc_handle);


        stop = true;
        }
        frames++;

        //mmal_buffer_header_mem_unlock(buf);
        mmal_buffer_header_release(buf);

        if(camera_handle->preview_port->is_enabled){
            MMAL_STATUS_T status;
            MMAL_BUFFER_HEADER_T *new_buffer;
            new_buffer = mmal_queue_get(camera_handle->video_pool->queue);
            if (new_buffer)
                status = mmal_port_send_buffer(camera_handle->preview_port, new_buffer);
            if (!new_buffer || status != MMAL_SUCCESS)
                fprintf(stderr, "Unable to return a buffer to the video port\n\n");
        }

        return true;
    }else{
        return false;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <kernel.bin>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    bcm_host_init();
    int mb = mbox_open();
      
    qpu_program_handle_t qpu_handle;
    qpu_program_create(&qpu_handle, mb);
    qpu_program_load_file(&qpu_handle, argv[1]);

    mmal_camera_handle_t camera_handle;
    mmal_camera_init(&camera_handle);
    mmal_camera_create(&camera_handle);
        
    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop && !stop) {
        //wait 5 seconds
        if(camera_read_frame(&camera_handle, &qpu_handle)){
            printf("frame received\n");
        }
    }
    
    mmal_camera_destroy(&camera_handle);
    qpu_program_destroy(&qpu_handle);


    return EXIT_SUCCESS;
}
