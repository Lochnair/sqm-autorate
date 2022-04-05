#include "globals.h"

#include <stddef.h>
#include <stdio.h>

int test_if_file_exists(settings_t * out, char * path, int retries, int retry_time)
{
	FILE * test_file = fopen(path, "r");

	if (test_file == NULL)
	{
		/*
		 * Let's wait and retry a few times before failing hard. These files typically
         * take some time to be generated following a reboot.
		 */
		struct timespec wait_time = {.tv_sec = retry_time};

		for (int i = 0; i < retries; i++)
		{
			log_warn("Stats file %s not yet available. Will retry again in %ld seconds. (Attempt %d of %d)", path, wait_time.tv_sec, i, retries);
			nanosleep(&wait_time, NULL);
			test_file = fopen(path, "r");
			if (test_file)
			{
				break;
			}
		}

		if (test_file == NULL)
		{
			log_fatal("Could not open stats file: %s", path);
			exit(1);
		}
	}

	fclose(test_file);
	return 1;
}

void set_statistics_paths(settings_t * out)
{
    char * dl_if = out->dl_if;
    char * ul_if = out->ul_if;

	log_info("Download interface: %s", dl_if);
	log_info("Upload interface: %s", ul_if);

	// Verify these are correct using "cat /sys/class/..."
	if (strstr(dl_if, "ifb") == dl_if || strstr(dl_if, "veth") == dl_if)
	{
		// length of constants + interface length and space for null terminator
		out->rx_statistics_path = calloc(1, 35 + strlen(dl_if) + 1);
		strcat(out->rx_statistics_path, "/sys/class/net/");
		strcat(out->rx_statistics_path, dl_if);
		strcat(out->rx_statistics_path, "/statistics/tx_bytes");
	}
	else if(strncmp(dl_if, "br-lan", 6) == 0)
	{
		// length of constants + interface length and space for null terminator
		out->rx_statistics_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(out->rx_statistics_path, "/sys/class/net/");
		strcat(out->rx_statistics_path, ul_if);
		strcat(out->rx_statistics_path, "/statistics/rx_bytes");
	}
	else
	{
		// length of constants + interface length and space for null terminator
		out->rx_statistics_path = calloc(1, 35 + strlen(dl_if) + 1);
		strcat(out->rx_statistics_path, "/sys/class/net/");
		strcat(out->rx_statistics_path, dl_if);
		strcat(out->rx_statistics_path, "/statistics/rx_bytes");
	}

	if (strstr(ul_if, "ifb") == ul_if || strstr(ul_if, "veth") == ul_if)
	{
		// length of constants + interface length and space for null terminator
		out->tx_statistics_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(out->tx_statistics_path, "/sys/class/net/");
		strcat(out->tx_statistics_path, ul_if);
		strcat(out->tx_statistics_path, "/statistics/rx_bytes");
	}
	else
	{
		// length of constants + interface length and space for null terminator
		out->tx_statistics_path = calloc(1, 35 + strlen(ul_if) + 1);
		strcat(out->tx_statistics_path, "/sys/class/net/");
		strcat(out->tx_statistics_path, ul_if);
		strcat(out->tx_statistics_path, "/statistics/tx_bytes");
	}

	log_debug("Path to RX statistics: %s", out->rx_statistics_path);
	log_debug("Path to TX statistics: %s", out->tx_statistics_path);

	test_if_file_exists(out, out->rx_statistics_path, 12, 5);
	log_info("Download device stats file found! Continuing...");
	test_if_file_exists(out, out->tx_statistics_path, 12, 5);
	log_info("Upload device stats file found! Continuing...");
}

int load_settings(settings_t * out)
{
    if (out == NULL)
        return -1;
    
    // deal with constant values first
    out->min_change_interval = 0.5;
    out->tick_duration = 0.5;

    out->reflector_list_icmp = "./reflectors-icmp.csv";
    out->reflector_list_icmp = "./reflectors-udp.csv";
    
    out->dl_if = getenv("SQM_DL_IF");
	out->ul_if = getenv("SQM_UL_IF");

	if (out->dl_if == NULL || out->ul_if == NULL)
	{
		log_fatal("Interfaces not defined: dl_if -> %p | ul_if -> %p", out->dl_if, out->ul_if);
		return 1;
	}

    set_statistics_paths(out);

    return 0;
}