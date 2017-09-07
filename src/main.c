#include "protracker.h"
#include "player61a.h"
#include "buffer.h"
#include "options.h"
#include "log.h"

#include "../out/readme.h"

#include <stdio.h>
#include <string.h>

bool show_help(int argc, char* argv[])
{
    bool help = argc < 2;
    for (size_t i = 1; i < argc; ++i)
    {
        if (!strcmp("-h", argv[i]) || !strcmp("--help", argv[i]))
        {
            help = true;
        }
    }
    if (!help)
    {
        return false;
    }

    LOG_INFO("%s", README_md);
    return true;
}

int main(int argc, char* argv[])
{
    if (show_help(argc, argv))
    {
        return 0;
    }

    protracker_t* module = NULL;
    const char* options = "";
    size_t i;

    for (i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        const char* opt = i < (argc-1) ? argv[i+1] : NULL;

        if (!strncmp("-in:", arg, 4))
        {
            if (module)
            {
                protracker_free(module);
                module = NULL;
            }

            if (!opt)
            {
                LOG_ERROR("No filename specified.\n");
                break;
            }

            const char* format = arg+4;
            const char* filename = opt;

            if (!strcmp("mod", format))
            {
                module = protracker_load(filename);
            }
            else
            {
                LOG_ERROR("Unknown input format '%s'.\n", format);
                break;
            }

            if (!module)
            {
                LOG_ERROR("Failed to load module '%s'.\n");
                break;
            }

            ++i;
        }
        else if (!strncmp("-out:", arg, 5))
        {
            if (!opt)
            {
                LOG_ERROR("No filename specified.\n");
                break;
            }

            const char* format = arg+5;
            const char* filename = opt;

            buffer_t buffer;
            buffer_init(&buffer, 1);

            FILE* fp = NULL;
            int success = 0;

            do
            {
                if (!strcmp("mod", format))
                {
                    if (!protracker_convert(&buffer, module, options))
                    {
                        LOG_ERROR("Conversion to ProTracker failed.\n");
                        break;
                    }
                }
                else if (!strcmp("p61a", format))
                {
                    if (!player61a_convert(&buffer, module, options))
                    {
                        LOG_ERROR("Conversion to The Player 6.1A failed.\n");
                        break;
                    }
                }
                else
                {
                    LOG_ERROR("Unknown output format '%s'.\n", format);
                    break;
                }

                LOG_INFO("Writing result to '%s'...", filename);

                fp = fopen(filename, "wb");
                if (!fp)
                {
                    LOG_INFO("failed to open '%s' for writing.\n", filename);
                    break;
                }

                size_t size = buffer_count(&buffer);
                if ((size > 0) && (fwrite(buffer_get(&buffer, 0), 1, size, fp) != size))
                {
                    LOG_INFO("failed to write %lu bytes.\n", size);
                    break;
                }

                LOG_INFO("done.\n");
                success = 1;
            }
            while (0);

            if (fp)
            {
                fclose(fp);
            }

            buffer_release(&buffer);

            ++i;
        }
        else if (!strncmp("-opts:", arg, 6))
        {
            options = arg + 6;
        }
        else if (!strcmp("-optimize", arg))
        {
            if (!opt)
            {
                LOG_ERROR("No options specified for optimization.\n");
                break;
            }

            bool all = has_option(opt, "all", false);

            if (has_option(opt, "unused_patterns", false) || all)
            {
                protracker_remove_unused_patterns(module);
            }

            if (has_option(opt, "trim", false) || all)
            {
                protracker_trim_samples(module);
            }

            if (has_option(opt, "unused_samples", false) || all)
            {
                protracker_remove_unused_samples(module);
            }

            if (has_option(opt, "identical_samples", false) || all)
            {
                protracker_remove_identical_samples(module);
            }

            if (has_option(opt, "compact_samples", false) || all)
            {
                protracker_compact_sample_indexes(module);
            }

            if (has_option(opt, "clean_effects", false) || all)
            {
                protracker_clean_effects(module);
            }

            ++i;
        }
        else if (!strcmp("-d", arg))
        {
            if (!opt)
            {
                LOG_ERROR("No argument specified for debug info.\n");
                break;
            }

            set_log_level(strtoul(opt, NULL, 10));
            ++i;
        }
        else if (!strcmp("-q", arg))
        {
            set_log_level(LOG_LEVEL_NONE);
        }
    }

    if (module)
    {
        protracker_free(module);
    }

    return (i == argc) ? 0 : 1;
}
