#include "protracker.h"
#include "buffer.h"
#include "converter.h"
#include "log.h"

#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        log_msg(LOG_INFO, "No file specified.\n");
        return 1;
    }

    protracker_t* module = protracker_load(argv[1]);
    if (!module)
    {
        log_msg(LOG_INFO, "Could not load module, aborting...\n");
        return 1;
    }

    // pre-process patterns

    protracker_remove_unused_patterns(module);

    // pre-process samples

    protracker_trim_samples(module);
    protracker_remove_unused_samples(module);
    protracker_remove_identical_samples(module);
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
            log_msg(LOG_INFO, "Conversion failed.\n");
            break;
        }

        log_msg(LOG_INFO, "Writing result to '%s'...", filename);

        fp = fopen(filename, "wb");
        if (!fp)
        {
            log_msg(LOG_INFO, "failed to open '%s' for writing.\n", filename);
            break;
        }

        size_t size = buffer_count(&buffer);
        if (fwrite(buffer_get(&buffer, 0), 1, size, fp) != size)
        {
            log_msg(LOG_INFO, "failed to write %lu bytes.\n", size);
            break;
        }

        log_msg(LOG_INFO, "done.\n");
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