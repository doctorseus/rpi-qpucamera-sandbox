#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"
#include "qpu_program.h"
#include "mailbox.h"
#include "tga.h"

unsigned int vcsm_vc_hdl_from_ptr( void *usr_ptr );

static void convert_i420_to_rgba(char *frame_buffer, char *image_buffer, unsigned int image_size);
static void dump_frame_to_tga(char *frame_buffer, unsigned int frame_size, int image_width, int image_height, char *filename);
static void dump_frame_to_file(char *frame_buffer, unsigned int frame_size, char *filename);

static void camera_progress_frame(qpu_program_handle_t *qpu_handle, unsigned int frameptr, bool debug_frame) {
    // Create output buffer
    qpu_buffer_handle_t output_buffer;
    qpu_buffer_create(&output_buffer, qpu_handle->mb, 1280*720, 4096);
    qpu_buffer_lock(&output_buffer);
    
    // Execute QPU program
    unsigned int uniforms[3];
    uniforms[0] = 1280*720;
    uniforms[1] = frameptr;
    uniforms[2] = output_buffer.mem_ptr;
    qpu_program_execute(qpu_handle, uniforms, 3);
    
    // Show output_buffer data and write output frame to file
    if(debug_frame) {
        char *output_buffer_char = output_buffer.arm_mem_ptr;
        for (int j=0; j < (16*4*64); j++) {
            printf("%02x", output_buffer_char[j]);
            if((j+1) % (16*4) == 0)
                printf("\n");
        }
        
        dump_frame_to_tga(output_buffer.arm_mem_ptr, 1280*720, 1280, 720, "output.tga");
    }
    
    qpu_buffer_unlock(&output_buffer);
    qpu_buffer_destroy(&output_buffer);
}

static bool camera_read_frame(mmal_camera_handle_t *camera_handle, qpu_program_handle_t *qpu_handle, bool debug_frame) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(camera_handle->video_queue)){
        
        // Get physical memory ptr and lock frame buffer
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        unsigned int frameptr = mem_lock(qpu_handle->mb, vc_handle);
        printf("Frame[size=%d, ptr=%lu]\n", buf->length, frameptr);
                
        // Write input camera frame to file if its a debug frame
        if(debug_frame) {
            dump_frame_to_tga(buf->data, buf->length, 1280, 720, "input.tga");
            dump_frame_to_file(buf->data, buf->length, "input.data");   
        }
        
        // Progress camera frame
        camera_progress_frame(qpu_handle, frameptr, debug_frame);
        
        // Unlock and release frame buffer
        mem_unlock(qpu_handle->mb, vc_handle);
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
        
    int frame_counter = 0;
    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop && frame_counter < 15) {
        // run for 5 seconds or 15 frames
        if(camera_read_frame(&camera_handle, &qpu_handle, frame_counter == 14)){
            frame_counter++;
        }
    }
    
    mmal_camera_destroy(&camera_handle);
    qpu_program_destroy(&qpu_handle);


    return EXIT_SUCCESS;
}

static void convert_i420_to_rgba(char *frame_buffer, char *image_buffer, unsigned int image_size) {
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

static void dump_frame_to_tga(char *frame_buffer, unsigned int frame_size, int image_width, int image_height, char *filename) {
    printf("> File[name=%s]\n", filename);
    int image_size = image_width * image_height * 4;
    char image_buffer[image_size];
    
    FILE* file = fopen(filename, "w");
    convert_i420_to_rgba(frame_buffer, image_buffer, image_size);
    write_tga(file, image_width, image_height, image_buffer, image_size);
    fclose(file);
}

static void dump_frame_to_file(char *frame_buffer, unsigned int frame_size, char *filename) {
    printf("> File[name=%s]\n", filename);
    FILE* file = fopen(filename, "w");
    fwrite(frame_buffer, sizeof(char), frame_size, file);
    fclose(file);
}
