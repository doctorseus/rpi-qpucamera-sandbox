#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h> // exit(.)
#include "qpu_program.h"
#include "mailbox.h"

bool qpu_program_create(qpu_program_handle_t *handle, int mb) {
    handle->mb = mb;
    
    // Allocate GPU memory
    unsigned int size = sizeof(qpu_program_mmap_t);
    qpu_buffer_create(&handle->buffer_handle, mb, size, 4096);
    
    // Lock memory
    unsigned int ptr = qpu_buffer_lock(&handle->buffer_handle);

    // Map struct to memory
    qpu_program_mmap_t *arm_map = (qpu_program_mmap_t *) handle->buffer_handle.arm_mem_ptr;
    memset(arm_map, 0x0, sizeof(qpu_program_mmap_t));

    unsigned vc_uniforms = ptr + offsetof(qpu_program_mmap_t, uniforms);
    unsigned vc_code = ptr + offsetof(qpu_program_mmap_t, code);
    unsigned vc_msg = ptr + offsetof(qpu_program_mmap_t, msg);

    // Set pointers
    arm_map->msg[0] = vc_uniforms;
    arm_map->msg[1] = vc_code;
    
    handle->vc_msg = vc_msg;
    handle->buffer_arm_mmap = arm_map;
    
    // Unlock memory
    qpu_buffer_unlock(&handle->buffer_handle);
    
    return true;    
}

void qpu_program_load_code(qpu_program_handle_t *handle, unsigned int *code, int words) {
    // Copy shader code to memory
    qpu_buffer_lock(&handle->buffer_handle);
    memcpy(handle->buffer_arm_mmap->code, code, words * sizeof(unsigned int));
    qpu_buffer_unlock(&handle->buffer_handle);
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

void qpu_program_execute(qpu_program_handle_t *handle, unsigned int *uniforms, int words) {
    struct timespec start, stop;
    double accum;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    
    // Enable QPU
    if (qpu_enable(handle->mb, 1)) {
        fprintf(stderr, "QPU enable failed.\n");
        exit(EXIT_FAILURE);
    }
    
    // Lock memory
    qpu_buffer_lock(&handle->buffer_handle);
    
    // Copy uniforms
    memcpy(handle->buffer_arm_mmap->uniforms, uniforms, words * sizeof(unsigned int));
    
    unsigned ret = execute_qpu(handle->mb, 1, handle->vc_msg, GPU_FFT_NO_FLUSH, GPU_FFT_TIMEOUT);
    
    // Disable QPU
    if (qpu_enable(handle->mb, 0)) {
        fprintf(stderr, "QPU enable failed.\n");
        exit(EXIT_FAILURE);
    }
    
    // Time and output
    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);   
    accum = 1000 / (( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec ) / 1000000.0);
    printf("> QPU[fps=%lf, ret=%x]\n", accum, ret);
    
    // Unlock memory
    qpu_buffer_unlock(&handle->buffer_handle);
}

void qpu_program_destroy(qpu_program_handle_t *handle) {
    qpu_buffer_destroy(&handle->buffer_handle);
}

bool qpu_buffer_create(qpu_buffer_handle_t *handle, int mb, unsigned size, unsigned align) {
    handle->mb = mb;
    handle->mem_size = size;
    
    // Get host info
    struct GPU_FFT_HOST host;
    if (gpu_fft_get_host_info(&host)) {
        fprintf(stderr, "QPU fetch of host information (Rpi version, etc.) failed.\n");
        return false;
    }
    
    // Allocate GPU memory    
    handle->mem_handle = mem_alloc(handle->mb, size, align, host.mem_flg);
    if (!handle->mem_handle) {
        fprintf(stderr, "Unable to allocate %d bytes of GPU memory", size);
        return false;
    }
    
    // Get physical memory pointer
    handle->mem_ptr = qpu_buffer_lock(handle);
    qpu_buffer_unlock(handle);
    
    // Map memory into ARM (GPU MEMORY LEAK HERE IF mapmem FAILS!)
    handle->arm_mem_ptr = mapmem(BUS_TO_PHYS(handle->mem_ptr + host.mem_map), size);
}

unsigned int qpu_buffer_lock(qpu_buffer_handle_t *handle) {
    // Lock memory
    return mem_lock(handle->mb, handle->mem_handle);
}

void qpu_buffer_unlock(qpu_buffer_handle_t *handle) {
    // Unlock memory
    mem_unlock(handle->mb, handle->mem_handle);
}

void qpu_buffer_destroy(qpu_buffer_handle_t *handle) {
    // Unmap ARM memory
    unmapmem(handle->arm_mem_ptr, handle->mem_size);
    // Free GPU memory
    mem_free(handle->mb, handle->mem_handle);
}
