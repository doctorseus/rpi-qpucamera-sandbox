#include <stdio.h>
#include <stdbool.h>
#include "bcm_host.h"
#include "mmal_camera.h"
#include "qpu.h"
#include "mailbox.h"

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
} qpu_handle_t;

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
    
    printf("GPU memory(addr=%p) locked\n", ptr);
    
    
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
    
    // Unmap memory
    unmapmem(arm_ptr, size);
    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
}

void qpu_destroy(qpu_handle_t *handle){
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
    
    qpu_handle_t qpu_handle;
    qpu_handle.mb = mb;
    qpu_init_kernel(&qpu_handle, argv[1]);
    

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
    qpu_destroy(&qpu_handle);


    return EXIT_SUCCESS;
}
