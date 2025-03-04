#include "fbplugin.h"
#include "plugin_shaders.h"
#include "plugin_address.h"
#include "../renderer/renderer/shaders/blinn_shader.h"

#include <riscv/mmio_plugin.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include <thread>
#include <iostream>
#include <elf.h>

struct framebuffer_plugin;

framebuffer_plugin* fb_plugin = nullptr; // Ugly global variable needed to access plugin from C interface
extern "C" int renderer_main(int argc, char *argv[]);

struct framebuffer_plugin
{
	unsigned char* buffer;
	size_t buffer_size;
	std::vector<std::pair<void*, std::size_t>> allocations;
	std::thread rendererThread;

	framebuffer_plugin(const std::string& args)
	{
		std::cout << "plugin created with args: " << args << "\n";
		fb_plugin = this;
		buffer = (unsigned char*) std::malloc(0x9600000);
		buffer_size = 0;

		int argc = 3;
		char **argv = (char**) std::malloc(4 * sizeof(char*));
		for (int i = 0; i < 3; ++i)
			argv[i] = (char*) std::malloc(50);
		std::strcpy(argv[0], "./Viewer");
		std::strcpy(argv[1], "blinn");
		std::strcpy(argv[2], args.c_str()); // TODO: Fix inconsistent assertion failure when scene is other than "triangle" e.g. "mccree"
		argv[3] = nullptr;
		rendererThread = std::thread(renderer_main, argc, argv);
	}

	~framebuffer_plugin()
	{
		rendererThread.join();
		std::free(buffer);
		std::cout << "plugin destroyed..." << "\n";
	}

	bool load(reg_t offset, size_t len, uint8_t* bytes)
	{
		std::memcpy(bytes,  &buffer[offset], len);
#ifdef PLUGIN_DEBUG
    	std::printf("Load offset=%lx, len=%lx , value %x\n", offset, len, buffer[offset]);
#endif
		return true;
	}

	bool store(reg_t offset, size_t len, const uint8_t* bytes)
	{
		std::memcpy(&buffer[offset], bytes, len);
#ifdef PLUGIN_DEBUG
        std::printf("Store offset=%lx, len=%lx , value %x\n", offset, len, buffer[offset]);
#endif
		return true;
	}

	void* allocate(size_t requested_size)
	{
		if (buffer_size % 16 != 0)
			buffer_size += (16 - buffer_size % 16);
		void* allocation_address = &buffer[buffer_size];
		buffer_size += requested_size;

		std::printf("Allocated address %p, total allocation size is now %zu bytes.\n", allocation_address, buffer_size);

		return allocation_address;
	}

	void deallocate(void* ptr)
	{
		// Not implemented
	}

	vec4_t invoke_fragment_shader(program_t *program, int *discard, int backface)
	{
		uint64_t ret_args[5];
		structs_to_spike(program);

		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		send_msg(buffer, PLUGIN_CMD_FS, 3, program, *discard, backface);
		recv_msg(buffer, PLUGIN_CMD_READY, ret_args);

		structs_to_host(program);
		*discard = ret_args[0];
		vec4_t color;
		color.x = * (float*) &ret_args[1];
		color.y = * (float*) &ret_args[2];
		color.z = * (float*) &ret_args[3];
		color.w = * (float*) &ret_args[4];
		return color;
	}

	vec4_t invoke_vertex_shader(program_t *program, int i)
	{
		uint64_t ret_args[4];
		structs_to_spike(program);

		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		send_msg(buffer, PLUGIN_CMD_VS, 2, program, i);
		recv_msg(buffer, PLUGIN_CMD_READY, ret_args);

		structs_to_host(program);
		vec4_t rv;
		rv.x = * (float*) &ret_args[0];
		rv.y = * (float*) &ret_args[1];
		rv.z = * (float*) &ret_args[2];
		rv.w = * (float*) &ret_args[3];
		return rv;
	}

	void invoke_update_fbaddr(unsigned char* addr)
	{
		// convert to spike address space first
		if (addr)
			addr = (addr - buffer) + reinterpret_cast<unsigned char*>(PLUGIN_BASE_ADDR);
		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		send_msg(buffer, PLUGIN_CMD_FBADDR, 1, addr);
		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		printf("%s: Updated fbaddr to %p\n", __func__, addr);
	}

	void invoke_draw(uint32_t pixel, ptrdiff_t offset)
	{
		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		send_msg(buffer, PLUGIN_CMD_DRAW, 2, pixel, offset);
		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
	}

	void shutdown()
	{
		recv_msg(buffer, PLUGIN_CMD_READY, NULL);
		send_msg(buffer, PLUGIN_CMD_STOP, 0);
	}

	void* load_shader(const char* file_name, char sdr_type)
	{
		ptrdiff_t offset = PLUGIN_FS_OFFSET;
		if (sdr_type)
			offset = PLUGIN_VS_OFFSET;

		FILE* f = fopen(file_name, "rb");
		if (!f) {
			std::fprintf(stderr, "%s: I/O failure (failed to open file)\n", __func__);
			return nullptr;
		}

		// Read ELF header
		Elf64_Ehdr header;
		if (fread(&header, sizeof(header), 1, f) != 1) {
			fclose(f);
			std::fprintf(stderr, "%s: I/O failure (failed to read ELF header)\n", __func__);
			return nullptr;
		}
		if (header.e_ident[0] != ELFMAG0 || header.e_ident[1] != ELFMAG1 
		 || header.e_ident[2] != ELFMAG2 || header.e_ident[3] != ELFMAG3) {
			fclose(f);
			std::fprintf(stderr, "%s: File specified is not an ELF file.\n", __func__);
			return nullptr;
		 }

		// Copy entire binary into shared memory
		rewind(f);
		size_t rs = fread(buffer + offset, 1, 0x500000, f);
		if (!feof(f)) {
			fclose(f);
			std::fprintf(stderr, "%s: I/O failure (stream not EOF)\n", __func__);
			return nullptr;
		}
		fclose(f);
#ifdef PLUGIN_DEBUG
		std::printf("%s: Copied %zu bytes. e_entry = %lu\n", __func__, rs, header.e_entry);
#endif
		return buffer + offset + header.e_entry - 0x10000; // TODO: Try setting text segment to start at 0x0
	}

private:
	// Transform the given struct from host to spike address space.
	void structs_to_spike(program_t* & program)
	{
		auto conv_addr_to_spike = [this](unsigned char* addr) -> unsigned char* 
		{
			if (addr)
				addr = (addr - buffer) + reinterpret_cast<unsigned char*>(PLUGIN_BASE_ADDR);
			return addr;
		};

		// uniforms can contain pointers, so we need to convert them as well.
		blinn_uniforms_t* bup = (blinn_uniforms_t*) program->shader_uniforms;
		if (bup) {
			bup->joint_matrices = reinterpret_cast<mat4_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->joint_matrices)));
			bup->joint_n_matrices = reinterpret_cast<mat3_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->joint_n_matrices)));
			if (bup->shadow_map)
				bup->shadow_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->shadow_map->buffer)));
			bup->shadow_map = reinterpret_cast<texture_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->shadow_map)));
			if (bup->diffuse_map)
				bup->diffuse_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->diffuse_map->buffer)));
			bup->diffuse_map = reinterpret_cast<texture_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->diffuse_map)));
			if (bup->specular_map)
				bup->specular_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->specular_map->buffer)));
			bup->specular_map = reinterpret_cast<texture_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->specular_map)));
			if (bup->emission_map)
				bup->emission_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->emission_map->buffer)));
			bup->emission_map = reinterpret_cast<texture_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(bup->emission_map)));
		}
		bup = reinterpret_cast<blinn_uniforms_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->shader_uniforms)));

		// program members
		program->fragment_shader = reinterpret_cast<fragment_shader_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->fragment_shader)));
		program->vertex_shader = reinterpret_cast<vertex_shader_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->vertex_shader)));
		for (int i = 0; i < 3; ++i)
			program->shader_attribs[i] = conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->shader_attribs[i]));
		program->shader_varyings = conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->shader_varyings));
		program->shader_uniforms = conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->shader_uniforms));
		for (int i = 0; i < MAX_VARYINGS; ++i) {
			program->in_varyings[i] = conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->in_varyings[i]));
			program->out_varyings[i] = conv_addr_to_spike(reinterpret_cast<unsigned char*>(program->out_varyings[i]));
		}

		// Lastly, convert the address of the struct itself.
		program = reinterpret_cast<program_t*>(conv_addr_to_spike(reinterpret_cast<unsigned char*>(program)));
	}

	// Transform the addresses in the given struct from the spike to the host address space.
	void structs_to_host(program_t* & program)
	{
		auto conv_addr_to_host = [this](unsigned char* addr) -> unsigned char*
		{
			if (addr)
				addr = (addr - reinterpret_cast<unsigned char*>(PLUGIN_BASE_ADDR)) + buffer;
			return addr;
		};

		// First, convert the address of the struct itself.
		program = reinterpret_cast<program_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(program)));

		// uniforms can contain pointers, so we need to convert them as well.
		// blinn_uniforms_t* bup = (blinn_uniforms_t*) program->shader_uniforms;
		blinn_uniforms_t* bup = reinterpret_cast<blinn_uniforms_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(program->shader_uniforms)));
		if (bup) {
			bup->joint_matrices = reinterpret_cast<mat4_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->joint_matrices)));
			bup->joint_n_matrices = reinterpret_cast<mat3_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->joint_n_matrices)));
			bup->shadow_map = reinterpret_cast<texture_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->shadow_map)));
			if (bup->shadow_map)
				bup->shadow_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->shadow_map->buffer)));
			bup->diffuse_map = reinterpret_cast<texture_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->diffuse_map)));
			if (bup->diffuse_map)
				bup->diffuse_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->diffuse_map->buffer)));
			bup->specular_map = reinterpret_cast<texture_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->specular_map)));
			if (bup->specular_map)
				bup->specular_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->specular_map->buffer)));
			bup->emission_map = reinterpret_cast<texture_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->emission_map)));
			if (bup->emission_map)
				bup->emission_map->buffer = reinterpret_cast<vec4_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(bup->emission_map->buffer)));
		}

		// program members
		program->fragment_shader = reinterpret_cast<fragment_shader_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(program->fragment_shader)));
		program->vertex_shader = reinterpret_cast<vertex_shader_t*>(conv_addr_to_host(reinterpret_cast<unsigned char*>(program->vertex_shader)));
		for (int i = 0; i < 3; ++i)
			program->shader_attribs[i] = conv_addr_to_host(reinterpret_cast<unsigned char*>(program->shader_attribs[i]));
		program->shader_varyings = conv_addr_to_host(reinterpret_cast<unsigned char*>(program->shader_varyings));
		program->shader_uniforms = conv_addr_to_host(reinterpret_cast<unsigned char*>(program->shader_uniforms));
		for (int i = 0; i < MAX_VARYINGS; ++i) {
			program->in_varyings[i] = conv_addr_to_host(reinterpret_cast<unsigned char*>(program->in_varyings[i]));
			program->out_varyings[i] = conv_addr_to_host(reinterpret_cast<unsigned char*>(program->out_varyings[i]));
		}
	}
};

void* plugin_malloc(size_t size)
{
	return fb_plugin->allocate(size);
}

void plugin_free(void* ptr)
{
	fb_plugin->deallocate(ptr);
}

vec4_t plugin_fragment_shader(program_t *program, int *discard, int backface)
{
	return fb_plugin->invoke_fragment_shader(program, discard, backface);
}

vec4_t plugin_vertex_shader(program_t *program, int i)
{
	return fb_plugin->invoke_vertex_shader(program, i);
}

void plugin_update_fbaddr(unsigned char* addr)
{
	fb_plugin->invoke_update_fbaddr(addr);
}

void plugin_draw(uint32_t pixel, ptrdiff_t offset)
{
	fb_plugin->invoke_draw(pixel, offset);
}

void* plugin_set_shader(const char* file_name, char sdr_type)
{
	return fb_plugin->load_shader(file_name, sdr_type);
}

void plugin_shutdown()
{
	fb_plugin->shutdown();
}

static mmio_plugin_registration_t<framebuffer_plugin> framebuffer_plugin_registration("framebuffer_plugin");
