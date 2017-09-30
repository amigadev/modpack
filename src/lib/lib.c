#include "../../include/internal/modpack.h"

#include <stdlib.h>

static void* default_alloc(size_t sz, const char*)
{
    return mlloc(sz);
}

static void default_free(void* p)
{
    free(p);
}

static void default_log(const char* fmt, va_list ap)
{
}

static modpack_init_t default_init = {
    default_alloc,
    default_free,
    default_log
};

modpack_t* modpack_init(modpack_init_t* init)
{
    if (!init)
    {
        init = &default_init;
    }

    modpack_t* instance = init->alloc(sizeof(modpack_t));

    instance->alloc = init->alloc;
    instance->free = init->free;
    instance->log = init->log;

    return instance;
}

void modpack_shutdown(modpack_t* instance)
{
    if (!instance)
    {
        return;
    }

    instance->init.free(instance);
}

modpack_module_t* modpack_load(modpack_t* instance, const char* format, const void* in, size_t length)
{
}

isize_t modpack_save(const modpack_module_t* module, const char* format, void* out, size_t length, const char* options)
{
    if (!module)
    {
        return -1;
    }

    modpack_t* instance = module->instance;

    if (!strcmp(format, MODPACK_FORMAT_MOD))
    {
        protracker_
    }
    else if (!strcmp(format, MODPACK_FORMAT_P61A))
    {
    }
    else
    {
        ERROR_LOG(instance, "Unknown format: %s\n", format);
        return -1;
    }
}

int modpack_optimize(modpack_module_t* module, const char* options)
{
}

void modpack_free(modpack_module_t* module)
{
    if (!module)
    {
        return;
    }

    modpack_t* instance = module->instance;

    protracker_free(instance, module->module);
    instance->free(module);
}