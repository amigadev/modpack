#pragma once

#include "modpack.h"

#include <stdlib.h>

typedef struct
{
    modpack_t* instance;
	uint8_t* data;

	size_t size;
	size_t capacity;
} modpack_buffer_t;

void modpack_buffer_init_write(modpack_t* instance, modpack_buffer_t* buffer);
void modpack_buffer_init_read(modpack_buffer_t* buffer, const void* data, size_t size);

void modpack_buffer_free(modpack_buffer_t* buffer);

size_t modpack_buffer_size(const modpack_buffer_t* buffer);

void modpack_buffer_add(modpack_buffer_t* buffer, const void* data, size_t size);
void* modpack_buffer_get(const modpack_buffer_t* buffer, size_t offset);
