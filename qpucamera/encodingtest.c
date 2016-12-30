#include <stdio.h>
#include <time.h>
#include "bcm_host.h"
#include "mmal_video_encoding.h"

int main(int argc, char **argv) {
    int MAX_SIZE = 1382400;
    int input_size = 0;
    char input_buffer[MAX_SIZE];
    
    FILE *input_file = fopen("input.data", "rb");
    if(input_file != NULL) {
        input_size = fread(input_buffer, sizeof(char), MAX_SIZE, input_file);
        fprintf(stderr, "Read input, %d bytes\n", input_size);
        fclose(input_file);
    }
    
    bcm_host_init();
    
    mmal_video_encoding_handle_t handle;
    mmal_video_encoding_create(&handle);
    
    time_t tstop = time(NULL) + 5;
    while (time(NULL) < tstop) {
        // run for 5 seconds        
        MMAL_BUFFER_HEADER_T *output_buffer = mmal_queue_get(handle.encoder_input_pool->queue);
        if (output_buffer) {
            
            mmal_buffer_header_mem_lock(output_buffer);

            output_buffer->length = input_size;
            memcpy(output_buffer->data, input_buffer, input_size);
            
            mmal_buffer_header_mem_unlock(output_buffer);
            
            fprintf(stderr, "- mmal_port_send_buffer\n");
            if (mmal_port_send_buffer(handle.encoder_input, output_buffer) != MMAL_SUCCESS) {
                fprintf(stderr, "ERROR: Unable to send buffer \n");
            }
        } else {
            //fprintf(stderr, "ERROR: mmal_queue_get (%d)\n", output_buffer);
        }
        
        // sleep for 33 ms -> 30 FPS
        usleep(33000);
    }
    
    mmal_video_encoding_destroy(&handle);
    
    fprintf(stderr, "EXIT\n");
    return 0;
}
