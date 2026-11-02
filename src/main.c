#include "protracker.h"
#include "player61a.h"
#include "buffer.h"
#include "options.h"
#include "log.h"

static const unsigned char help_text[3000] = {
"Modpack - Optimize, compress and convert ProTracker/P61A modules\n"
"================================================================\n"
"Arguments are processed from left to right. This means you can write more\n"
"than one output if needed.\n\n"

"Importing / exporting modules:\n"
"  -in:FORMAT NAME      Load module in specified format.\n"
"  -out:FORMAT NAME     Save module in specified format.\n\n"
"  Available formats:\n"
"    mod                Protracker\n"
"    p61a               The Player 6.1A\n\n"

"  If NAME is -, standard input/output will be utilized.\n\n"
"  -opts:OPTIONS                Set import/export options\n\n"
"  P61A export options:\n"
"    sign                  Add signature when exporting (\'P61A\') (disabled)\n"
"    4bit[=RANGE]          Compress specified samples to 4-bit (disabled)\n"
"    delta                 Delta-encode samples (disabled)\n"
"    [-]compress_patterns  Compress pattern data (enabled)\n"
"    [-]song               Write song data to output (enabled)\n"
"    [-]samples            Write sample data to output (enabled)\n\n"
"  Preceeding a boolean option with a minus ('-') will disable the option.\n\n"
"  Range examples:\n"
"    [1]                Apply to sample 1\n"
"    [4-7]              Apply to sample 4-7\n"
"    [1-4:8-12]         Apply to sample 1-4 and 8-12 (5-7 is not affected)\n\n"
"Optimization options:\n"
"  -optimize OPTIONS\n\n"

"  Available options:\n"
"    unused_patterns    Remove unused patterns\n"
"    unused_samples     Remove unused samples\n"
"                       (sample index is preserved)\n"
"    trim               Trim tailing null data in samples\n"
"                       (not looped samples)\n"
"    trim_loops         Also trim looped samples\n"
"                       (implies \'trim\')\n"
"    identical_samples  Merge identical samples\n"
"                       (pattern data is rewritten to match)\n"
"    compact_samples    Remove empty space in the sample table\n"
"    clean              Clean effects in pattern data\n"
"    clean:e8           Remove E8x from pattern data\n"
"                       (implies 'clean', not enabled by 'all')\n"
"    all                Apply all available optimizes\n"
"                       (where applicable)\n\n"
"  Preceeding a boolean option with a minus ('-') will disable the option.\n\n"
"Miscellaneous:\n"
"  -d N			Set log level (0 = info, 1 = debug, 2 = trace)\n"
"  -q			Quiet mode\n\n"
"Remove unused patterns and samples, and re-save as MOD:\n"
"  modpack -in:mod in.mod -optimize unused_patterns,unused_samples\n"
"    -out:mod out.mod\n\n"

"Fully optimize module and export P61A (song and samples separately):\n"
"  modpack -in:mod test.mod -optimize all -opts:-samples -out:p61a test.p61\n"
"    -opts:-song -out:p61a test.smp\n"
};

#include <stdio.h>
#include <string.h>

static bool show_help(int argc, char* argv[]);
static protracker_t* module_load(const char* filename, const char* format);

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

            LOG_INFO("Loading '%s'...\n", filename);

            module = module_load(filename, format);
            if (!module)
            {
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

                if (!strcmp(filename, "-"))
                {
                    fp = stdout;
                }
                else
                {
                    fp = fopen(filename, "wb");
                    if (!fp)
                    {
                        LOG_INFO("failed to open '%s' for writing.\n", filename);
                        break;
                    }
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

            if (fp && (fp != stdout))
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

            if (has_option(opt, "clean", false) || has_option(opt, "clean:e8", false) || all)
            {
                protracker_clean_effects(module, opt);
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

static bool show_help(int argc, char* argv[])
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

    LOG_INFO("%s", help_text);
    return true;
}

static protracker_t* module_load(const char* filename, const char* format)
{
    protracker_t* module = NULL;
    FILE* fp = NULL;

    buffer_t buffer;
    buffer_init(&buffer, 1);

    do
    {
        if (strcmp("-", filename))
        {
            fp = fopen(filename, "rb");
            if (!fp)
            {
                LOG_ERROR("Failed to open file '%s'.\n", filename);
                break;
            }
        }
        else
        {
            fp = stdin;
        }

        char buf[1024];
        do {
            size_t sz = fread(buf, 1, sizeof(buf), fp);
            buffer_add(&buffer, buf, sz);
            if (sz < sizeof(buf))
            {
                break;
            }
        } while (true);

        if (!strcmp("mod", format))
        {
            module = protracker_load(&buffer);
        }
        else if (!strcmp("p61a", format))
        {
            module = player61a_load(&buffer);
        }
        else
        {
            LOG_ERROR("Unknown input format '%s'.\n", format);
            break;
        }

        if (!module)
        {
            LOG_ERROR("Failed to load module '%s'.\n", filename);
            break;
        }
    }
    while (false);

    if (fp && (fp != stdin))
    {
        fclose(fp);
    }

    buffer_release(&buffer);

    return module;
}
