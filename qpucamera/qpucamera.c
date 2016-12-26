#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"
#include "qpu_program.h"
#include "qpu.h"
#include "mailbox.h"
#include "tga.h"

#define MAX_CODE_SIZE   8192
#define BUFFER_SIZE     16*64

typedef struct qpu_memory_map_s {
    unsigned int code[MAX_CODE_SIZE];
    unsigned int uniforms[3];
    unsigned int msg[2];
    unsigned int inputs[BUFFER_SIZE];
    unsigned int results[BUFFER_SIZE];
} qpu_memory_map_t;

typedef struct qpu_handle_s {
    int mb;
    unsigned int mem_handle;
    unsigned int vc_msg;
    unsigned int arm_mem_size;
    qpu_memory_map_t *arm_mem_map;
} qpu_handle_t;

static bool stop = false;
static unsigned int frames = 0;

unsigned int vcsm_vc_hdl_from_ptr( void *usr_ptr );
void qpu_execute(qpu_handle_t *handle, unsigned int frameptr);

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

static bool camera_read_frame(mmal_camera_handle_t *camera_handle, qpu_handle_t *qpu_handle) {
    MMAL_BUFFER_HEADER_T* buf;
    if(buf = mmal_queue_get(camera_handle->video_queue)){
        //mmal_buffer_header_mem_lock(buf);



        printf("size=%d, data=%p\n", buf->length, buf->data);
        printf("data_buf[0]=%d\n", buf->data[0]);
            
        if(frames >= 15){
            
        unsigned int vc_handle = vcsm_vc_hdl_from_ptr(buf->data);
        unsigned int frameptr = mem_lock(qpu_handle->mb, vc_handle);
        qpu_execute(qpu_handle, frameptr);
        mem_unlock(qpu_handle->mb, vc_handle);


        // Write output debug image
        int image_width = 1280;
        int image_height = 720;
        int image_size = image_width * image_height * 4;
        char image_buffer[image_size];
        FILE* output_file = fopen("output.tga", "w");
        camera_data_to_rgba(buf->data, buf->length, image_buffer, image_size);
        write_tga(output_file, image_width, image_height, image_buffer, image_size);
        fclose(output_file);

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

int load_file(const char *fname, unsigned int* buffer, int len) {
    FILE *in = fopen(fname, "r");
    if (!in) {
        fprintf(stderr, "Failed to open %s.\n", fname);
        exit(EXIT_FAILURE);
    }
    size_t items = fread(buffer, sizeof(unsigned int), len, in);
    fclose(in);
    return items;
}

void qpu_init_kernel(qpu_handle_t *handle, char *filename) {
    unsigned int qpu_code[MAX_CODE_SIZE];
    int code_words = load_file(filename, qpu_code, MAX_CODE_SIZE);
    printf("Loaded kernel from %s, code_words=%d\n", filename, code_words);
    
    // Get host info
    struct GPU_FFT_HOST host;
    if (gpu_fft_get_host_info(&host)) {
        fprintf(stderr, "QPU fetch of host information (Rpi version, etc.) failed.\n");
        exit(EXIT_FAILURE);
    }
    
    printf("Host:\n");
    printf("mem_flg=%x\n", host.mem_flg);
    printf("mem_map=%x\n", host.mem_map);
    printf("peri_addr=%x\n", host.peri_addr);
    printf("peri_size=%x\n", host.peri_size);
    
    // Allocate GPU memory
    unsigned int size = 1024 * 1024;
    handle->mem_handle = mem_alloc(handle->mb, size, 4096, host.mem_flg);
    if (!handle->mem_handle) {
        fprintf(stderr, "Unable to allocate %d bytes of GPU memory", size);
        exit(EXIT_FAILURE);
    }
    
    // Lock memory
    unsigned int ptr = mem_lock(handle->mb, handle->mem_handle);
    
    // Map memory into ARM (GPU MEMORY LEAK HERE IF mapmem FAILS!)
    void *arm_ptr = mapmem(BUS_TO_PHYS(ptr + host.mem_map), size);

    // Map struct to memory
    qpu_memory_map_t *arm_map = (qpu_memory_map_t *)arm_ptr;
    memset(arm_map, 0x0, sizeof(qpu_memory_map_t));
    unsigned vc_uniforms = ptr + offsetof(qpu_memory_map_t, uniforms);
    unsigned vc_code = ptr + offsetof(qpu_memory_map_t, code);
    unsigned vc_msg = ptr + offsetof(qpu_memory_map_t, msg);
    unsigned vc_inputs = ptr + offsetof(qpu_memory_map_t, inputs);
    unsigned vc_results = ptr + offsetof(qpu_memory_map_t, results);
        
    // Copy shader code to memory
    memcpy(arm_map->code, qpu_code, code_words * sizeof(unsigned int));
    
    // Set pointers
    arm_map->uniforms[0] = BUFFER_SIZE;
    arm_map->uniforms[1] = vc_inputs;
    arm_map->uniforms[2] = vc_results;
    arm_map->msg[0] = vc_uniforms;
    arm_map->msg[1] = vc_code;
    
    for(int i = 0; i < BUFFER_SIZE; i++)
        arm_map->inputs[i] = (i+1)*16 | 0xee000000;

    handle->vc_msg = vc_msg;
    handle->arm_mem_size = size;
    handle->arm_mem_map = arm_map;

    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
}

void qpu_execute(qpu_handle_t *handle, unsigned int frameptr) {
    struct timespec start, stop;
    double accum;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);


    // Enable QPU
    if (qpu_enable(handle->mb, 1)) {
        fprintf(stderr, "QPU enable failed.\n");
        exit(EXIT_FAILURE);
    }
    
    // Lock memory
    mem_lock(handle->mb, handle->mem_handle);
    
    // Set pointers to camera frame
    handle->arm_mem_map->uniforms[1] = frameptr;
    
    unsigned ret = execute_qpu(handle->mb, 1, handle->vc_msg, GPU_FFT_NO_FLUSH, GPU_FFT_TIMEOUT);
    
    // Disable QPU
    if (qpu_enable(handle->mb, 0)) {
        fprintf(stderr, "QPU enable failed.\n");
        exit(EXIT_FAILURE);
    }
    
    // Time and output
    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);   
    accum = 1000 / (( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec ) / 1000000.0);
    printf( "fps = %lf\n", accum);
    
    printf("ret=%x\n", ret);    
    for (int j=0; j < BUFFER_SIZE; j++) {
        printf("%08x ", handle->arm_mem_map->results[j]);
        if((j+1) % 16 == 0)
            printf("\n");
    }
    
    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
}

void qpu_destroy(qpu_handle_t *handle){
    // Unmap ARM memory
    unmapmem(handle->arm_mem_map, handle->arm_mem_size);
    // Free GPU memory
    mem_free(handle->mb, handle->mem_handle);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <kernel.bin>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    bcm_host_init();
    int mb = mbox_open();
      

    qpu_program_handle_t qpu_program_handle;
    qpu_program_create(&qpu_program_handle, mb);
    qpu_program_load_file(&qpu_program_handle, argv[1]);
    
    unsigned int uniforms[3];
    uniforms[0] = 0x73e0a000;
    uniforms[1] = 0x73e0a000;
    uniforms[2] = 0x73e0a000;
    qpu_program_execute(&qpu_program_handle, uniforms, 3);
    
    qpu_program_destroy(&qpu_program_handle);
    printf("EXIT\n");
    return 0;

    
    qpu_handle_t qpu_handle;
    qpu_handle.mb = mb;
    qpu_init_kernel(&qpu_handle, argv[1]);

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
    qpu_destroy(&qpu_handle);


    return EXIT_SUCCESS;
}
