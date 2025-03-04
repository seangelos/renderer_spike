#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include "plugin_address.h"
#include "graphics.h"

// Update fbaddr CSR to the given value.
void update_fbaddr(unsigned char* addr) __attribute__ ((noinline));
// Draw the given pixel at the specified offset in the framebuffer.
void draw(uint32_t pixel, ptrdiff_t offset) __attribute__ ((noinline));

#define NO_STFB 1

// define this macro to disable usage of stfb instruction
#ifdef NO_STFB
static unsigned char* color_buffer = NULL;
#endif

// Loop forever, handling incoming commands
int main(void)
{
    send_msg((void*) PLUGIN_BASE_ADDR, PLUGIN_CMD_READY, 0);
    while (true) {
        uint64_t args[5] = { 0 };
        program_t *program = NULL;
        command_t cmd = recv_msg((void*) PLUGIN_BASE_ADDR, 
                        PLUGIN_CMD_FBADDR | PLUGIN_CMD_DRAW | PLUGIN_CMD_FS | PLUGIN_CMD_VS | PLUGIN_CMD_STOP,
                        args);
        switch (cmd) {
        case PLUGIN_CMD_FBADDR:
            update_fbaddr((unsigned char*) args[0]);
            send_msg((void*) PLUGIN_BASE_ADDR, PLUGIN_CMD_READY, 0);
            break;
        case PLUGIN_CMD_DRAW:
            draw((uint32_t) args[0], (ptrdiff_t) args[1]);
            send_msg((void*) PLUGIN_BASE_ADDR, PLUGIN_CMD_READY, 0);
            break;
        case PLUGIN_CMD_FS:
            program = (program_t*) args[0];
            int discard = (int) args[1];
            int backface = (int) args[2];
            vec4_t color = program->fragment_shader(program->shader_varyings,
                                                    program->shader_uniforms,
                                                    &discard,
                                                    backface);
            send_msg((void*) PLUGIN_BASE_ADDR, PLUGIN_CMD_READY, 5,
                        discard, 
                        * (int32_t*) &color.x, * (int32_t*) &color.y, 
                        * (int32_t*) &color.z, * (int32_t*) &color.w);
            break;
        case PLUGIN_CMD_VS:
            program = (program_t*) args[0];
            int i = (int) args[1];
            vec4_t rv = program->vertex_shader(program->shader_attribs[i],
                                                program->in_varyings[i],
                                                program->shader_uniforms);
            send_msg((void*) PLUGIN_BASE_ADDR, PLUGIN_CMD_READY, 4,
                        * (int32_t*) &rv.x, * (int32_t*) &rv.y, 
                        * (int32_t*) &rv.z, * (int32_t*) &rv.w);
            break;
        case PLUGIN_CMD_STOP:
            return 0;
        }
    }
}

void update_fbaddr(unsigned char* addr)
{
#ifndef NO_STFB
    __asm__("csrw 0x800, a0");
#else
    color_buffer = addr;
#endif
}

void draw(uint32_t pixel, ptrdiff_t offset)
{
#ifndef NO_STFB
    __asm__(".insn s 0xB, 0x0, a0, 0(a1)"); // use stfb custom command
#else
    memcpy(color_buffer + (4 * offset), &pixel, 4);
#endif
}
