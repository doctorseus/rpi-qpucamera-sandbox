#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"
#include "mmal_video_encoding.h"
#include "qpu_program.h"
#include "mailbox.h"
#include "tga.h"

//#define RECORD_VIDEO

unsigned int vcsm_vc_hdl_from_ptr( void *usr_ptr );

static void convert_i420_to_rgba(char *frame_buffer, char *image_buffer, unsigned int image_size);
static void dump_frame_to_tga(char *frame_buffer, unsigned int frame_size, int image_width, int image_height, char *filename);
static void dump_frame_to_file(char *frame_buffer, unsigned int frame_size, char *filename);

typedef union int_to_float_to_int
{
    unsigned char uc[4];
    char c[4];
    unsigned short us[2];
    short s[2];
    unsigned int ui;
    int i;
    float f;
} int_to_float_to_int;

static void camera_progress_frame(mmal_video_encoding_handle_t *encoding_handle, qpu_program_handle_t *qpu_handle, unsigned int frameptr, bool debug_frame) {
    // Create output buffer
    qpu_buffer_handle_t output_buffer;
    qpu_buffer_create(&output_buffer, qpu_handle->mb, 1280*720*2, 4096);
    qpu_buffer_lock(&output_buffer);
    
    // Execute QPU program
    unsigned int uniforms[8];
    
    int_to_float_to_int fstep;
    uniforms[0] = 1280;
    uniforms[1] = 720;
    fstep.f = 1.0f/1280.0f;
    uniforms[2] = fstep.ui;
    fstep.f = 1.0f/720.0f;
    uniforms[3] = fstep.ui;
    
    uniforms[4] = frameptr;
    uniforms[5] = output_buffer.mem_ptr;
    

    
    unsigned int tex_base_ptr = frameptr >> 12;
    //uniforms[6] = 0x00000000 | (tex_base_ptr & 0xfffff) << 12 | (0 & 0x1) << 8 | (5 & 0xf) << 4;
    //uniforms[7] = 0x00000000 | (720 & 0x7ff) << 20 | (1280 & 0x7ff) << 8 | (1 & 0x1) << 7 | (1 & 0x3) << 4;
    unsigned int tex_type = 17;
    uniforms[6] = 0x00000000 | (tex_base_ptr & 0xfffff) << 12 | (0 & 0x1) << 8 | (tex_type & 0xf) << 4;
    uniforms[7] = 0x00000000 | (720 & 0x7ff) << 20 | (1280 & 0x7ff) << 8 | (1 & 0x1) << 7 | (1 & 0x3) << 4 | ((tex_type >> 4) & 0x1) << 31;
    
    
    // 10101010101010101010 10 1 0 1010 1010
    // 1 01010101010 1 01010101010 1 010 10 10
    qpu_program_execute(qpu_handle, uniforms, 8);
    
    // Show output_buffer data and write output frame to file
    if(debug_frame) {
        char *output_buffer_char = output_buffer.arm_mem_ptr;
        for (int j=0; j < (16*4*64); j++) {
            printf("%02x", output_buffer_char[j]);
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
        
        dump_frame_to_tga(output_buffer.arm_mem_ptr, 1280*720, 1280, 720, "output.tga");
    }
    
    /*
    // Send output buffer to video encoder
    MMAL_BUFFER_HEADER_T *encoding_buffer = mmal_queue_wait(encoding_handle->encoder_input_pool->queue);
    if (encoding_buffer) {
        mmal_buffer_header_mem_lock(encoding_buffer);

        encoding_buffer->length = 1382400;
        memset(encoding_buffer->data, 0x80, 1382400); // Reset UV planes to get a w/b video
        memcpy(encoding_buffer->data, output_buffer.arm_mem_ptr, 1280*720);

        mmal_buffer_header_mem_unlock(encoding_buffer);
        fprintf(stderr, "- mmal_port_send_buffer\n");
        if (mmal_port_send_buffer(encoding_handle->encoder_input, encoding_buffer) != MMAL_SUCCESS) {
            fprintf(stderr, "ERROR: Unable to send buffer \n");
        }
    }
    */
        
    qpu_buffer_unlock(&output_buffer);
    qpu_buffer_destroy(&output_buffer);
}

static bool camera_read_frame(mmal_camera_handle_t *camera_handle, mmal_video_encoding_handle_t *encoding_handle, qpu_program_handle_t *qpu_handle, bool debug_frame) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(camera_handle->video_queue)){
        
        // Get physical memory ptr and lock frame buffer
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        unsigned int frameptr = mem_lock(qpu_handle->mb, vc_handle);
        printf("Frame[size=%d, ptr=%08x]\n", buf->length, frameptr);
                
        // Write input camera frame to file if its a debug frame
        if(debug_frame) {
            dump_frame_to_tga(buf->data, buf->length, 1280, 720, "input.tga");
            dump_frame_to_file(buf->data, buf->length, "input.data");   
        }
        
        /*   
        // Send camera frame to video encoder
        MMAL_BUFFER_HEADER_T *output_buffer = mmal_queue_wait(encoding_handle->encoder_input_pool->queue);
        if (output_buffer) {
            mmal_buffer_header_mem_lock(output_buffer);

            output_buffer->length = buf->length;
            memcpy(output_buffer->data, buf->data, buf->length);

            mmal_buffer_header_mem_unlock(output_buffer);
            fprintf(stderr, "- mmal_port_send_buffer\n");
            if (mmal_port_send_buffer(encoding_handle->encoder_input, output_buffer) != MMAL_SUCCESS) {
                fprintf(stderr, "ERROR: Unable to send buffer \n");
            }
        }
        */
        
        // Progress camera frame
        camera_progress_frame(encoding_handle, qpu_handle, frameptr, debug_frame);
        
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
    
    mmal_video_encoding_handle_t encoding_handle;
    // mmal_video_encoding_create(&encoding_handle);

    mmal_camera_handle_t camera_handle;
    mmal_camera_init(&camera_handle);
    mmal_camera_create(&camera_handle);
        
    int frame_counter = 0;
    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop && frame_counter < 15) {
        // run for 5 seconds or 15 frames
        if(camera_read_frame(&camera_handle, &encoding_handle, &qpu_handle, frame_counter == 14)){
            frame_counter++;
        }
    }
    
    mmal_camera_destroy(&camera_handle);
    // mmal_video_encoding_destroy(&encoding_handle);
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
