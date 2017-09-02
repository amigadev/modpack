#include "protracker.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return 1;
    }

    protracker_t* module = protracker_load(argv[1]);

    return 0;
}