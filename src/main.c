#include "protracker.h"
#include "buffer.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "No file specified.\n");
        return 1;
    }

    protracker_t* module = protracker_load(argv[1]);
    if (!module)
    {
        fprintf(stderr, "Could not load module, aborting...\n");
        return 1;
    }

    protracker_remove_unused_patterns(module);

    protracker_save(module, "test.mod");

    buffer_t buffer;
    buffer_init(&buffer, 1);

    protracker_free(module);

    return 0;
}