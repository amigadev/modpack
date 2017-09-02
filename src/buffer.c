#include "buffer.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

void buffer_init(buffer_t* buffer, size_t elemsize)
{
	buffer->size = 0;
	buffer->capacity = 0;
	buffer->elemsize = elemsize;

	buffer->data = NULL;
}

void buffer_set(buffer_t* buffer, const uint8_t* data, size_t length)
{
    buffer->size = length;
    buffer->capacity = 0;

    buffer->data = (uint8_t*)data;
}

void buffer_release(buffer_t* buffer)
{
	if (buffer->data && buffer->capacity > 0)
		free(buffer->data);
	buffer_init(buffer, 0);
}

void buffer_reset(buffer_t* buffer)
{
	buffer->size = 0;
}

void* buffer_alloc(buffer_t* buffer, size_t elements)
{
	size_t newSize = buffer->size + (elements * buffer->elemsize);
	if (newSize > buffer->capacity)
	{
		size_t newCapacity = buffer->capacity > 0 ? (buffer->capacity * 15)/10 : newSize * 2;
		newCapacity = newCapacity < newSize ? newSize : newCapacity;

		uint8_t* newData = malloc(newCapacity);
		memcpy(newData, buffer->data, buffer->size);

		if (buffer->data)
			free(buffer->data);

		buffer->capacity = newCapacity;
		buffer->data = newData;
	}

	uint8_t* data = buffer->data + buffer->size;
	buffer->size = newSize;

	return data;
}

void* buffer_add(buffer_t* buffer, const void* data, size_t size)
{
    assert(buffer->elemsize == 1); // TODO: should we support non byte-arrays?

	void* temp = buffer_alloc(buffer, size);
	memcpy(temp, data, size);
	return temp;
}

void* buffer_get(const buffer_t* buffer, size_t index)
{
    assert(index < (buffer->size / buffer->elemsize));
	return buffer->data + index * buffer->elemsize;
}

size_t buffer_offset(const buffer_t* buffer, const void* member)
{
	return (((const uint8_t*)member) - buffer->data) / buffer->elemsize;
}

size_t buffer_count(const buffer_t* buffer)
{
	return buffer->size / buffer->elemsize;
}
