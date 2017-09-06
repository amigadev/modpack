Modpack - Optimizer, Compressor and Converter
==================================

Utility to optimize, compress and convert modules between different format.

Arguments
---------

Arguments are processed from left to right. This means you can write more than one output if needed.

-in:<format> <name>     Load module in specified format. Available formats:

                        mod                     ProTracker module

                        Specifying '-' as name will read the input from standard input.

-optimize <options>     Apply optimizers on the loaded song. Available optimizers:

                        unused_patterns         Remove unused pattern
                        trim                    Trim samples to remove unused bytes
                        unused_samples          Remove unused samples (Sample index is preserved)
                        identical_samples       Merge identical samples (Pattern samples are rewritten to match)
                        compact_samples         Remove empty space in sample table
                        clean_effects           Clean up effects, removing unnecessary commands and downgrading complex ones to simpler variants
                        all                     Apply all available optimizers

-opts:<options>         Set comma-separated options for exporter

                        Preceeding a boolean option with a minus ('-') will disable the selection.

                        mod
                        ---
                        No options available

                        p61a
                        ----
                        [-]sign                 Add signature ('P61A') (default: disabled)
                        [-]4bit OR 4bit=RANGE   4-bit compression (lossy) (default: disabled)
                        [-]delta]               Delta encoding (default: disabled)
                        [-]samples              Write sample data to output (default: enabled)
                        [-]song                 Write song data to output (default: enabled)

                        Range examples:
                        [1]                     Apply to sample 1 
                        [4-7]                   Apply to sample 4-7
                        [1-4:8-12]              Apply to sample 1-4 and 8-12 (5-7 is not affected)

-out:<format> <file>    Save module in specified format. Available formats:

                        mod                     ProTracker module
                        p61a                    The Player 6.1a module

                        Specifying '-' as the output filename will write the result to standard output.

Examples
--------

Make module as small as possible (without resorting to destructive sample compression) and export as P61A
```
modpack -in:mod test.mod -optimize all -out:p61a test.p61
```

Remove unused data and export P61 (song and sample data separately) - sample index is preserved

```
modpack -in:mod test.mod -optimize unused_patterns,trim,unused_samples,identical_samples -opts:-samples -out:p61a test.p61 -opts:-song -out:p61a test.smp
```
