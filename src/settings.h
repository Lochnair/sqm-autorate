#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

typedef struct settings {
    char *  dl_if;                  // download interface
    char *  ul_if;                  // upload interface
    
    char *  rx_statistics_path;     // path to file containing received bytes for the interface
    char *  tx_statistics_path;     // path to file containing sent bytes for the interface

    int     base_dl_rate;           // expected stable download speed
    int     base_ul_rate;           // expected stable upload speed
    int     min_dl_rate;            // minimum acceptable download speed
    int     min_ul_rate;            // minimum acceptable upload speed

    char *  reflector_list_icmp;    // the location of the input icmp reflector list
    char *  reflector_list_udp;     // the location of the input udp reflector list
    char *  reflector_type;         // reflector type icmp or udp (udp is not well supported)

    char *  speedhist_file;         // the location of the output speed history
    int     speedhist_size;         // the number of good speed settings to remember
    char *  stats_file;             // the file location of the output statistics
    bool    output_statistics;      // controls whether to output statistics to the filesystem

    float   min_change_interval;    // the minimum interval between speed changes
    float   tick_duration;          // the interval between 'pings'

    int     dl_max_delta_owd;       // download delay threshold to trigger a download speed change
    int     ul_max_delta_owd;       // upload delay threshold to trigger an upload speed change

    float   high_load_level;        // the relative load (to current speed) that is considered 'high'
} settings_t;

int load_settings(settings_t * out);

#endif // SETTINGS_H