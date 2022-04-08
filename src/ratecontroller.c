#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "globals.h"
#include "utils.h"

const struct timespec sleep_time = {.tv_nsec = 5 * 1e8};

void * ratecontroller_loop()
{
    struct timespec start_time = get_time();
    struct timespec last_change = get_time();
    int last_change_t = last_change.tv_sec - start_time.tv_sec + last_change.tv_nsec / 1e9;
    int lastdump_t = last_change_t - 310;
}

int update_cake_bandwidth(char * interface, double bandwidth)
{
    long rate_in_kbit = floor(bandwidth);
    int len = snprintf(NULL, 0, "%ld", rate_in_kbit) + 4;
    char * bandwidth_str = calloc(1, len + 1);
    snprintf(bandwidth_str, len + 1, "%ldkbit", rate_in_kbit);

    char * argv[] = {"tc", "qdisc", "change", "dev", interface, "root", "cake", "bandwidth", bandwidth_str, NULL};

    if ((strcmp(interface, settings.dl_if) == 0 && rate_in_kbit >= 5000) || (strcmp(interface, settings.ul_if) == 0  && rate_in_kbit >= 2000))
    {
        int pid = fork();

        if (pid == 0)
        {
            execvp(argv[0], argv);
        }

        int ret = -1;

        waitpid(pid, &ret, 0);

        return ret;
    }

    return 0;
}