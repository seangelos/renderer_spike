#ifndef _PLUGIN_ADDRESS_H
#define _PLUGIN_ADDRESS_H

#include <stdint.h>
#include <stdarg.h>

#define PLUGIN_BASE_ADDR  0x10000000 /* Address of shared memory in spike address space. */
#define PLUGIN_CMD_OFFSET 0x08b00000 /* Offset in shared memory where messages are sent. */
#define PLUGIN_FS_OFFSET  0x08c00000 /* Offset in shared memory where fragment shader is loaded. */
#define PLUGIN_VS_OFFSET  0x09100000 /* Offset in shared memory where vertex shader is loaded. */

typedef uint64_t command_t;

/* Command types */

#define PLUGIN_CMD_VS       1 /* Run vertex shader on spike, 2 arguments (see plugin_vertex_shader). */
#define PLUGIN_CMD_FS       2 /* Run fragment shader on spike, 3 arguments (see plugin_fragment_shader). */
#define PLUGIN_CMD_DRAW     4 /* Tell spike to store the given value on the given offset in the framebuffer, 2 arguments (see plugin_draw). */
#define PLUGIN_CMD_FBADDR   8 /* Send framebuffer address to spike, 1 argument. */
#define PLUGIN_CMD_STOP    16 /* Tell spike to terminate, 0 arguments. */

#define PLUGIN_CMD_READY   32 /* Signal that execution on spike is finished, variable number of arguments. 
                               * After PLUGIN_CMD_VS: 4 arguments, which form the return value of the vertex shader.
                               * After PLUGIN_CMD_FS: 5 arguments, where the first is the value of discard and the rest
                               * are the return value of the fragment shader.
                               * In any other case, no arguments are sent with this command.
                               */


/* Write a message to shared memory.
 * base_addr : address of shared memory
 * command   : command to send 
 * argc      : number of arguments to be written 
 * All arguments given are written after the command with 8-byte alignment. */
static void send_msg(void* base_addr, command_t command, int argc, ...)
{
    volatile uint64_t* cmd_addr = (uint64_t*) ((uint8_t*) base_addr + PLUGIN_CMD_OFFSET);

    va_list args;
    va_start(args, argc);
    for (int i = 0; i < argc; ++i)
        cmd_addr[i + 1] = va_arg(args, uint64_t);
    va_end(args);

    *cmd_addr = command;
}

/* Wait for a command and write the arguments received in the given buffer.
 * base_addr : address of shared memory
 * command   : binary or'd commands to wait for
 * args      : buffer to write arguments received into. Buffer length should be
 *             enough to hold the arguments for the command received. 
 *             If this is NULL, don't write arguments. */
static command_t recv_msg(void* base_addr, command_t command, uint64_t* args)
{
    volatile uint64_t* cmd_addr = (uint64_t*) ((uint8_t*) base_addr + PLUGIN_CMD_OFFSET);
    while (!(*cmd_addr & command)) { /* wait until command is returned */ }

    if (args) {
        int argc = 0;
        switch (*cmd_addr) {
        case PLUGIN_CMD_FBADDR:
            argc = 1;
            break;
        case PLUGIN_CMD_DRAW:
        case PLUGIN_CMD_VS:
            argc = 2;
            break;
        case PLUGIN_CMD_FS:
            argc = 3;
            break;
        case PLUGIN_CMD_READY:
            argc = 5;
            break;
        }
        for (int i = 0; i < argc; ++i)
            args[i] = cmd_addr[i + 1];
    }

    return *cmd_addr;
}

#endif /* _PLUGIN_ADDRESS_H */
