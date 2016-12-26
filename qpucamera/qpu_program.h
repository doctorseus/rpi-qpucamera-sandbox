#ifndef QPU_PROGRAM_H
#define QPU_PROGRAM_H

#include "qpu.h"
#include <stdbool.h>

#define MAX_CODE_SIZE   8192
#define MAX_UNIFORMS    3

typedef struct qpu_program_mmap_s {
    unsigned int code[MAX_CODE_SIZE];
    unsigned int uniforms[MAX_UNIFORMS];
    unsigned int msg[2];
} qpu_program_mmap_t;

typedef struct qpu_program_handle_s {
    int mb;
    
    unsigned int mem_handle;    // GPU memory handle
    
    unsigned int arm_mem_size;          // ARM memory size
    qpu_program_mmap_t *arm_mem_map;    // ARM memory ptr
    
    unsigned int vc_msg;                // Mailbox message ptr
} qpu_program_handle_t;

bool qpu_program_create(qpu_program_handle_t *handle, int mailboxfd);
void qpu_program_load_code(qpu_program_handle_t *handle, unsigned int *code, int words);
void qpu_program_load_file(qpu_program_handle_t *handle, char *filename);
void qpu_program_execute(qpu_program_handle_t *handle);
void qpu_program_destroy(qpu_program_handle_t *handle);

#endif
