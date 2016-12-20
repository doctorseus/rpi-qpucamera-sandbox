#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/time.h>

#include <bcm_host.h>
#include "mailbox.h"
#include "qpu.h"

#define MAX_CODE_SIZE   8192

static unsigned int qpu_code[MAX_CODE_SIZE];

#define BUFFER_SIZE     16*64

struct memory_map {
    unsigned int code[MAX_CODE_SIZE];
    unsigned int uniforms[3];       // 2 parameters per QPU
                                    // first address is the input value
                                    // for the program to add to
                                    // second is the address of the
                                    // result buffer
    unsigned int msg[2];
    unsigned int inputs[BUFFER_SIZE];
    unsigned int results[BUFFER_SIZE];       // result buffer for the QPU to
                                    // write into
};

int loadShaderCode(const char *fname, unsigned int* buffer, int len) {
    FILE *in = fopen(fname, "r");
    if (!in) {
        fprintf(stderr, "Failed to open %s.\n", fname);
        exit(EXIT_FAILURE);
    }
    size_t items = fread(buffer, sizeof(unsigned int), len, in);
    fclose(in);
    return items;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <code .bin>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Load shader
    int code_words = loadShaderCode(argv[1], qpu_code, MAX_CODE_SIZE);

    // Init system
    bcm_host_init();
    int mb = mbox_open();

    // Get host info
    struct GPU_FFT_HOST host;
    if (gpu_fft_get_host_info(&host)){
        fprintf(stderr, "QPU fetch of host information (Rpi version, etc.) failed.\n");
        exit(EXIT_FAILURE);
    }
    
    // Enable QPU
    if (qpu_enable(mb, 1)) {
        fprintf(stderr, "QPU enable failed.\n");
        exit(EXIT_FAILURE);
    }
    printf("QPU enabled.\n");

    printf("Host:\n");
    printf("mem_flg=%x\n", host.mem_flg);
    printf("mem_map=%x\n", host.mem_map);
    printf("peri_addr=%x\n", host.peri_addr);
    printf("peri_size=%x\n", host.peri_size);


    // Allocate GPU memory
    unsigned int size = 1024 * 1024;
    unsigned int handle = mem_alloc(mb, size, 4096, host.mem_flg);
    if (!handle) {
        fprintf(stderr, "Unable to allocate %d bytes of GPU memory", size);
        exit(EXIT_FAILURE);
    }
    
    // Lock memory
    unsigned int ptr = mem_lock(mb, handle);
    
    // Map memory into ARM (GPU MEMORY LEAK HERE IF mapmem FAILS!)
    void *arm_ptr = mapmem(BUS_TO_PHYS(ptr + host.mem_map), size);
    
    printf("GPU memory(addr=%p) locked\n", ptr);
    
    
    // Map struct to memory
    struct memory_map *arm_map = (struct memory_map *)arm_ptr;
    memset(arm_map, 0x0, sizeof(struct memory_map));
    unsigned vc_uniforms = ptr + offsetof(struct memory_map, uniforms);
    unsigned vc_code = ptr + offsetof(struct memory_map, code);
    unsigned vc_msg = ptr + offsetof(struct memory_map, msg);
    unsigned vc_inputs = ptr + offsetof(struct memory_map, inputs);
    unsigned vc_results = ptr + offsetof(struct memory_map, results);
    
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
    
    struct timespec start, stop;
    double accum;
    
    // Execute Shader
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    unsigned ret = execute_qpu(mb, 1, vc_msg, GPU_FFT_NO_FLUSH, GPU_FFT_TIMEOUT);
    clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
    
    accum = 1000 / (( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec ) / 1000000.0);
    printf( "fps = %lf\n", accum);
    
    printf("ret=%x\n", ret);
    /*
    for (int j=0; j < BUFFER_SIZE; j++) {
        printf("QPU, word %d: 0x%08x\n", j, arm_map->results[j]);
    }
    */
    
    for (int j=0; j < BUFFER_SIZE; j++) {
        printf("%08x ", arm_map->results[j]);
        if((j+1) % 16 == 0)
            printf("\n");
    }
    
    
    // Unmap memory
    unmapmem(arm_ptr, size);
    // Unlock memory
    mem_unlock(mb, handle);
    // Free memory
    mem_free(mb, handle);
    // Disable QPU
    qpu_enable(mb, 0);

    return EXIT_SUCCESS;
}
