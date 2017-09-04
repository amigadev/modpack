#include "protracker.h"
#include "buffer.h"
#include "converter.h"

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
    protracker_remove_unused_samples(module);
    protracker_compact_sample_indexes(module);

    buffer_t buffer;
    buffer_init(&buffer, 1);

    FILE* fp = NULL;
    const char* filename = "test.mod";
    int ret = 1;

    do
    {
        if (convert(&buffer, module, CONVERT_FORMAT_PROTRACKER))
        {
            fprintf(stderr, "Conversion failed.\n");
            break;
        }

        fprintf(stderr, "Writing result to '%s'...", filename);

        fp = fopen(filename, "wb");
        if (!fp)
        {
            fprintf(stderr, "failed to open '%s' for writing.\n", filename);
            break;
        }

        size_t size = buffer_count(&buffer);
        if (fwrite(buffer_get(&buffer, 0), 1, size, fp) != size)
        {
            fprintf(stderr, "failed to write %lu bytes.\n", size);
            break;
        }

        fprintf(stderr, "done.\n");
        ret = 0;
    }
    while (0);

    if (fp)
    {
        fclose(fp);
    }

    protracker_free(module);

    return ret;
}