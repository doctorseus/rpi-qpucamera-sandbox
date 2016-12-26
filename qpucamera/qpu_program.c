#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h> // exit(.)
#include "qpu_program.h"
#include "mailbox.h"

bool qpu_program_create(qpu_program_handle_t *handle, int mb) {
    handle->mb = mb;
    
    // Get host info
    struct GPU_FFT_HOST host;
    if (gpu_fft_get_host_info(&host)) {
        fprintf(stderr, "QPU fetch of host information (Rpi version, etc.) failed.\n");
        return false;
    }

    printf("Host:\n");
    printf("mem_flg=%x\n", host.mem_flg);
    printf("mem_map=%x\n", host.mem_map);
    printf("peri_addr=%x\n", host.peri_addr);
    printf("peri_size=%x\n", host.peri_size);
    
    // Allocate GPU memory
    unsigned int size = sizeof(qpu_program_mmap_t);
    
    handle->mem_handle = mem_alloc(handle->mb, size, 4096, host.mem_flg);
    if (!handle->mem_handle) {
        fprintf(stderr, "Unable to allocate %d bytes of GPU memory", size);
        return false;
    }

    // Lock memory
    unsigned int ptr = mem_lock(handle->mb, handle->mem_handle);

    // Map memory into ARM (GPU MEMORY LEAK HERE IF mapmem FAILS!)
    void *arm_ptr = mapmem(BUS_TO_PHYS(ptr + host.mem_map), size);
    

    // Map struct to memory
    qpu_program_mmap_t *arm_map = (qpu_program_mmap_t *)arm_ptr;
    memset(arm_map, 0x0, sizeof(qpu_program_mmap_t));

    unsigned vc_uniforms = ptr + offsetof(qpu_program_mmap_t, uniforms);
    unsigned vc_code = ptr + offsetof(qpu_program_mmap_t, code);
    unsigned vc_msg = ptr + offsetof(qpu_program_mmap_t, msg);
        

    // Set pointers
    arm_map->msg[0] = vc_uniforms;
    arm_map->msg[1] = vc_code;


    handle->vc_msg = vc_msg;
    handle->arm_mem_size = size;
    handle->arm_mem_map = arm_map;


    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
    
    return true;    
}

void qpu_program_load_code(qpu_program_handle_t *handle, unsigned int *code, int words) {
    // Copy shader code to memory
    memcpy(handle->arm_mem_map->code, code, words * sizeof(unsigned int));
}

static int load_file(const char *fname, unsigned int* buffer, int len) {
    FILE *in = fopen(fname, "r");
    if (!in) {
        fprintf(stderr, "Failed to open %s.\n", fname);
        return -1;
    }
    size_t items = fread(buffer, sizeof(unsigned int), len, in);
    fclose(in);
    return items;
}

void qpu_program_load_file(qpu_program_handle_t *handle, char *filename) {
    unsigned int qpu_code[MAX_CODE_SIZE];
    int code_words = load_file(filename, qpu_code, MAX_CODE_SIZE);
    printf("Loaded kernel from %s, code_words=%d\n", filename, code_words);
    qpu_program_load_code(handle, qpu_code, code_words);
}

void qpu_program_execute(qpu_program_handle_t *handle) {
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
    handle->arm_mem_map->uniforms[2] = handle->vc_msg;
    handle->arm_mem_map->uniforms[1] = handle->vc_msg;
    handle->arm_mem_map->uniforms[0] = handle->vc_msg;
    
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
    
    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
}

void qpu_program_destroy(qpu_program_handle_t *handle) {
    // Unmap ARM memory
    unmapmem(handle->arm_mem_map, handle->arm_mem_size);
    // Free GPU memory
    mem_free(handle->mb, handle->mem_handle);
}
