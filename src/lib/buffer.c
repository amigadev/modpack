#include "buffer.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

void modpack_buffer_init_write(modpack_buffer_t* buffer)
{
    buffer->instance = instance;
	buffer->data = NULL;

	buffer->size = 0;
	buffer->capacity = 0;
}

void modpack_buffer_init_read(modpack_buffer_t* buffer, const void* data, size_t size)
{
    buffer->instance = NULL;
    buffer->data = (uint8_t*)buffer;
    buffer->size = size;
}

void modpack_buffer_set(modpack_buffer_t* buffer, const uint8_t* data, size_t size)
{
    buffer->instance = NULL;
    buffer->data = (uint8_t*)data;

    buffer->size = size;
    buffer->capacity = 0;

}

void modpack_buffer_release(modpack_buffer_t* buffer)
{
	if (buffer->data && buffer->instance)
		buffer->instance->free(buffer->data);

    modpack_buffer_init_read(buffer, 0, 0);
}

static void* buffer_grow(modpack_buffer_t* buffer, size_t grow_by)
{
    modpack_t* instance = buffer->instance;
    if (!instance)
    {
        return NULL;
    }

	size_t newSize = buffer->size + grow_by;
	if (newSize > buffer->capacity)
	{
		size_t newCapacity = buffer->capacity > 0 ? (buffer->capacity * 15)/10 : newSize * 2;
		newCapacity = newCapacity < newSize ? newSize : newCapacity;

		uint8_t* newData = instance->alloc(newCapacity);

		if (buffer->data)
        {
            memcpy(newData, buffer->data, buffer->size);
			instance->free(buffer->data);
        }

		buffer->capacity = newCapacity;
		buffer->data = newData;
	}

	uint8_t* data = buffer->data + buffer->size;
	buffer->size = newSize;

	return data;
}

void* modpack_buffer_add(modpack_buffer_t* buffer, const void* data, size_t size)
{
	void* temp = grow(buffer, size);
	memcpy(temp, data, size);
	return temp;
}

void* modpack_buffer_get(const modpack_buffer_t* buffer, size_t offset)
{
    assert(offset < buffer->size);
	return buffer->data + offset;
}

size_t modpack_buffer_size(const modpack_buffer_t* buffer)
{
	return buffer->size;
}
