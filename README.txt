Modpack - Optimize, compress and convert ProTracker/P61A modules
================================================================

Arguments are processed from left to right. This means you can write more
than one output if needed.

Importing / exporting modules:

  -in:FORMAT NAME           Load module in specified format.
  -out:FORMAT NAME          Save module in specified format.

  Available formats:

    mod                     Protracker
    p61a                    The Player 6.1A

  If NAME is -, standard input/output will be utilized.
  
  -opts:OPTIONS             Set import/export options

  P61A export options:
  
    sign                    Add signature when exporting ('P61A') (disabled)
    4bit[=RANGE]            Compress specified samples to 4-bit (disabled)
    delta                   Delta-encode samples (disabled)
    [-]compress_patterns    Compress pattern data (enabled)
    [-]song                 Write song data to output (enabled)
    [-]samples              Write sample data to output (enabled)
  
  Preceeding a boolean option with a minus ('-') will disable the option.
  
  Range examples:
  
  [1]                       Apply to sample 1
  [4-7]                     Apply to sample 4-7
  [1-4:8-12]                Apply to sample 1-4 and 8-12 (5-7 is not affected)
  
  
Optimization options:

  -optimize OPTIONS
  
  Available options:
  
    unused_patterns         Remove unused patterns
    unused_samples          Remove unused samples (sample index is preserved)
    trim                    Trim tailing null data in samples (not looped samples)
    trim_loops              Also trim looped samples (implies 'trim')
    identical_samples       Merge identical samples (pattern data is rewritten
                            to match)
    compact_samples         Remove empty space in the sample table
    clean                   Clean effects in pattern data
    clean:e8                Remove E8x from pattern data (implies 'clean', not
                            enabled by 'all')
    all                     Apply all available optimizes (where applicable)

  Preceeding a boolean option with a minus ('-') will disable the option.

Miscellaneous:

  -d N                      Set log level (0 = info, 1 = debug, 2 = trace)
  -q                        Quiet mode

Remove unused patterns and samples, and re-save as MOD:
  
  modpack -in:mod in.mod -optimize unused_patterns,unused_samples
    -out:mod out.mod

Fully optimize module and export P61A (song and samples separately):

  modpack -in:mod test.mod -optimize all -opts:-samples -out:p61a test.p61
    -opts:-song -out:p61a test.smp
