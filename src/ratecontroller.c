#define _GNU_SOURCE

#include <math.h>
#include <unistd.h>

#include "globals.h"

int update_cake_bandwidth(char * interface, double bandwidth)
{
    long rate_in_kbit = floor(bandwidth);
    char * program = "tc";
    char * argv[] = {"qdisc", "change", "root", "dev", interface, "cake", "bandwidth", rate_in_kbit, "Kbit"};

    if ((interface == dl_if && rate_in_kbit > 5000) || (interface == ul_if && rate_in_kbit > 2000))
    {
        return execvp(program, argv);
    }

    return 0;
}