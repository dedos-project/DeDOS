/**
 * timeseries.c
 *
 * Contains code relevant to storing and processing a round-robin database of timeseries
 */

#include "timeseries.h"
#include "stats.h"
#include "dfg.h"
#include "logging.h"

double average_n(struct timed_rrdb *timeseries, int n_samples) {
    double sum = 0;
    int gathered = 0;
    int i;
    int start_index = timeseries->write_index - 1;
    for (i = 0; i < n_samples; i++) {
        int index = start_index - i;
        if ( index < 0 )
            index = RRDB_ENTRIES + index;
        if ( timeseries->time[index].tv_sec < 0 ){
            continue;
        }
        gathered++;
        sum += timeseries->data[index];
    }

    if (gathered == 0)
        return -1;
    return sum / (double)gathered;
}

/** Appends a number of timed statistics to a timeseries.
 * @param input The timed statistics to append to the timeseries
 * @param input_size The length of *input
 * @param timeseries The timeseries to which the data is to be appended
 * @return 0 on success
 * */
int append_to_timeseries(struct timed_stat *input, int input_size,
                         struct timed_rrdb *timeseries) {
    int write_index = timeseries->write_index;
    for (int i=0; i<input_size; i++) {
        write_index %= RRDB_ENTRIES;
        if (input[i].time.tv_sec == 0 && input[i].time.tv_nsec == 0) {
            // FIXME: I don't know why this happens sometimes
            continue;
        }
        timeseries->data[write_index] = input[i].value;
        timeseries->time[write_index] = input[i].time;
        write_index++;
    }
    timeseries->write_index = write_index % RRDB_ENTRIES;
    return 0;
}

/** The length of the begnning and end of the timeseries that's printed
 * when print_timeseries() is called */
#define PRINT_LEN 6

/** Prints the beginning and end of a timeseries.
 * @param timeseries The timeseries to print
 */
void print_timeseries(struct timed_rrdb *timeseries){

    char str[RRDB_ENTRIES * 10 * 2];
    char *buff = str;

    buff += sprintf(buff, "TIME: ");

    for (int i=0; i<PRINT_LEN; i++) {
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        double time = timeseries->time[index].tv_sec +
                      ((double)timeseries->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "| ... ");

    for (int i=RRDB_ENTRIES-PRINT_LEN; i<RRDB_ENTRIES; i++) {
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        double time = timeseries->time[index].tv_sec +
                      ((double)timeseries->time[index].tv_nsec * 1e-9);
        buff += sprintf(buff, "| %9.4f ", time);
    }

    buff += sprintf(buff, "\nDATA: ");
    for (int i=0; i<PRINT_LEN; i++){
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        buff += sprintf(buff, "| %9d ", (int)timeseries->data[index]);
    }
    buff += sprintf(buff, "| ... ");

    for (int i=RRDB_ENTRIES-PRINT_LEN; i<RRDB_ENTRIES; i++){
        int index = (timeseries->write_index + i) % RRDB_ENTRIES;
        buff += sprintf(buff, "| %9d ", (int)timeseries->data[index]);
    }
    buff += sprintf(buff, "\n");
    printf("%s", str);
}
