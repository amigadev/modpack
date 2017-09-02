#pragma once

#include <stdlib.h>

typedef struct buffer_t
{
	size_t size;
	size_t capacity;
	size_t elemsize;

	uint8_t* data;
} buffer_t;

void buffer_init(buffer_t* buffer, size_t elemsize);
void buffer_set(buffer_t* buffer, const uint8_t* data, size_t length);
void buffer_release(buffer_t* buffer);
void buffer_reset(buffer_t* buffer);

void* buffer_alloc(buffer_t* buffer, size_t elements);
void* buffer_add(buffer_t* buffer, const void* data, size_t size);
void* buffer_get(const buffer_t* buffer, size_t index);
size_t buffer_offset(const buffer_t* buffer, const void* member);
size_t buffer_count(const buffer_t* buffer);
