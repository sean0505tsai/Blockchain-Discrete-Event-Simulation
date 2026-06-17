/*
 * Bitcoin Network Simulation - Plan B (PoW Competition)
 * Discrete-event simulation using next-event time advance.
 *
 * Core model:
 * - 1000 miners compete in parallel.
 * - Transactions enter mempool with dynamic size (vB).
 * - Block packing is FCFS and limited by max_block_size (vB).
 * - Warm-up blocks are excluded from reported statistics.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "lcgrand.h"

#define NUM_SERVERS 1000
#define NUM_EVENTS 2
#define WARMUP_BLOCKS 10

#define ARRIVAL 1
#define DEPARTURE 2

#define MIN_TX_SIZE_VB 60
#define PI 3.14159265358979323846

/* Inputs */
int num_blocks_to_simulate;
int queue_capacity_vb;
int max_block_size_vb;
double mean_interarrival;
double mean_service_time;

/* Derived sizes */
int total_blocks_to_simulate;
int stats_blocks;

/* Simulation state */
int next_event_type;
double sim_time;
double time_last_event;
double last_block_time;
double time_next_event[NUM_EVENTS + 1];

double miner_finish_time[NUM_SERVERS];

/* Mempool as circular queue of transaction sizes */
int *mempool_queue = NULL;
int queue_buffer_size = 0;
int queue_head = 0;
int queue_tail = 0;
int queue_count = 0;
long long mempool_current_size_vb = 0;
long long max_mempool_size_vb = 0;

/* Global counters */
int blocks_completed = 0;
long total_arrived_transactions = 0;
long dropped_transactions = 0;
long total_completed_transactions = 0;

double sum_block_interval = 0.0;
double sum_block_size_vb = 0.0;
long sum_block_tx_count = 0;

/* Per-stat-block arrays (1..stats_blocks) */
int *block_num_transactions = NULL;
double *block_intervals = NULL;
double *block_size_vb = NULL;

/* Files */
FILE *infile = NULL;
FILE *outfile_report = NULL;
FILE *outfile_data = NULL;

/* Function declarations */
void initialize(void);
void timing(void);
void arrive(void);
void depart(void);
void report(void);

double expon(double mean);
double uniform01(int stream);
double box_muller_normal(double mu, double sigma);
int generate_dynamic_tx_size(void);

int mempool_push(int tx_size_vb);
int mempool_front(void);
void mempool_pop(void);

static void cleanup_and_exit(int code) {
    if (infile != NULL) {
        fclose(infile);
    }
    if (outfile_report != NULL) {
        fclose(outfile_report);
    }
    if (outfile_data != NULL) {
        fclose(outfile_data);
    }

    free(mempool_queue);
    free(block_num_transactions);
    free(block_intervals);
    free(block_size_vb);

    exit(code);
}

int main(void) {
    infile = fopen("mmnl_pow.in", "r");
    outfile_report = fopen("../outputs/mmnl_pow.out", "w");
    outfile_data = fopen("../outputs/output.txt", "w");

    if (infile == NULL || outfile_report == NULL || outfile_data == NULL) {
        fprintf(stderr, "Error opening required files.\n");
        cleanup_and_exit(1);
    }

    if (fscanf(
            infile,
            "%lf %lf %d %d %d",
            &mean_interarrival,
            &mean_service_time,
            &queue_capacity_vb,
            &max_block_size_vb,
            &num_blocks_to_simulate
        ) != 5) {
        fprintf(stderr, "Invalid mmnl_pow.in format.\n");
        cleanup_and_exit(1);
    }

    if (mean_interarrival <= 0.0 || mean_service_time <= 0.0 ||
        queue_capacity_vb <= 0 || max_block_size_vb <= 0 || num_blocks_to_simulate <= 0) {
        fprintf(stderr, "Input values must be positive.\n");
        cleanup_and_exit(1);
    }

    stats_blocks = num_blocks_to_simulate;
    total_blocks_to_simulate = stats_blocks + WARMUP_BLOCKS;

    /* Worst-case transaction count if all tx are minimum size. */
    queue_buffer_size = (queue_capacity_vb / MIN_TX_SIZE_VB) + 2;
    mempool_queue = (int *)malloc((size_t)queue_buffer_size * sizeof(int));

    block_num_transactions = (int *)calloc((size_t)stats_blocks + 1, sizeof(int));
    block_intervals = (double *)calloc((size_t)stats_blocks + 1, sizeof(double));
    block_size_vb = (double *)calloc((size_t)stats_blocks + 1, sizeof(double));

    if (mempool_queue == NULL || block_num_transactions == NULL ||
        block_intervals == NULL || block_size_vb == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        cleanup_and_exit(1);
    }

    fprintf(outfile_report, "Bitcoin Network Simulation (MMnL Model)\n\n");
    fprintf(outfile_report, "Mean inter-arrival time: %.6f seconds\n", mean_interarrival);
    fprintf(outfile_report, "Mean service time: %.3f seconds\n", mean_service_time);
    fprintf(outfile_report, "Number of servers (miners): %d\n", NUM_SERVERS);
    fprintf(outfile_report, "Number of blocks to simulate: %d\n", stats_blocks);
    fprintf(outfile_report, "Mempool capacity: %d vB\n", queue_capacity_vb);
    fprintf(outfile_report, "Max block size: %d vB\n\n", max_block_size_vb);

    initialize();

    while (blocks_completed < total_blocks_to_simulate) {
        timing();

        if (next_event_type == ARRIVAL) {
            arrive();
        } else {
            depart();
        }

        time_last_event = sim_time;
    }

    report();
    cleanup_and_exit(0);
    return 0;
}

void initialize(void) {
    int i;
    double min_finish_time;

    sim_time = 0.0;
    time_last_event = 0.0;
    last_block_time = 0.0;

    blocks_completed = 0;
    total_arrived_transactions = 0;
    dropped_transactions = 0;
    total_completed_transactions = 0;
    sum_block_interval = 0.0;
    sum_block_size_vb = 0.0;
    sum_block_tx_count = 0;

    queue_head = 0;
    queue_tail = 0;
    queue_count = 0;
    mempool_current_size_vb = 0;
    max_mempool_size_vb = 0;

    time_next_event[ARRIVAL] = sim_time + expon(mean_interarrival);

    min_finish_time = 1.0e30;
    for (i = 0; i < NUM_SERVERS; i++) {
        miner_finish_time[i] = sim_time + expon(mean_service_time);
        if (miner_finish_time[i] < min_finish_time) {
            min_finish_time = miner_finish_time[i];
        }
    }
    time_next_event[DEPARTURE] = min_finish_time;
}

void timing(void) {
    if (time_next_event[ARRIVAL] <= time_next_event[DEPARTURE]) {
        next_event_type = ARRIVAL;
        sim_time = time_next_event[ARRIVAL];
    } else {
        next_event_type = DEPARTURE;
        sim_time = time_next_event[DEPARTURE];
    }
}

void arrive(void) {
    int tx_size;

    time_next_event[ARRIVAL] = sim_time + expon(mean_interarrival);
    tx_size = generate_dynamic_tx_size();

    if (blocks_completed >= WARMUP_BLOCKS) {
        total_arrived_transactions++;
    }

    if (mempool_current_size_vb + tx_size <= queue_capacity_vb) {
        if (!mempool_push(tx_size)) {
            fprintf(stderr, "Mempool queue overflow (capacity bookkeeping mismatch).\n");
            cleanup_and_exit(1);
        }

        mempool_current_size_vb += tx_size;
        if (mempool_current_size_vb > max_mempool_size_vb) {
            max_mempool_size_vb = mempool_current_size_vb;
        }
    } else if (blocks_completed >= WARMUP_BLOCKS) {
        dropped_transactions++;
    }
}

void depart(void) {
    int i;
    int tx_count;
    long long packed_block_size_vb;
    double block_interval;
    double min_finish_time;

    tx_count = 0;
    packed_block_size_vb = 0;

    block_interval = sim_time - last_block_time;
    last_block_time = sim_time;

    while (queue_count > 0) {
        int next_tx_size = mempool_front();
        if (packed_block_size_vb + next_tx_size > max_block_size_vb) {
            break;
        }

        mempool_pop();
        packed_block_size_vb += next_tx_size;
        mempool_current_size_vb -= next_tx_size;
        tx_count++;
    }

    blocks_completed++;

    if (blocks_completed > WARMUP_BLOCKS) {
        int stat_block_no = blocks_completed - WARMUP_BLOCKS;

        block_num_transactions[stat_block_no] = tx_count;
        block_intervals[stat_block_no] = block_interval;
        block_size_vb[stat_block_no] = (double)packed_block_size_vb;

        total_completed_transactions += tx_count;
        sum_block_interval += block_interval;
        sum_block_tx_count += tx_count;
        sum_block_size_vb += (double)packed_block_size_vb;

        printf(
            "#%d, Transactions: %d, Interval: %.1f s, Block Size: %.2f MiB\n",
            stat_block_no,
            tx_count,
            block_interval,
            ((double)packed_block_size_vb) / 1048576.0
        );
    }

    min_finish_time = 1.0e30;
    for (i = 0; i < NUM_SERVERS; i++) {
        miner_finish_time[i] = sim_time + expon(mean_service_time);
        if (miner_finish_time[i] < min_finish_time) {
            min_finish_time = miner_finish_time[i];
        }
    }
    time_next_event[DEPARTURE] = min_finish_time;
}

void report(void) {
    int i;
    double avg_tx_per_block;
    double mean_block_interval;
    double mean_block_size_mib;
    double drop_rate_percent;

    for (i = 1; i <= stats_blocks; i++) {
        fprintf(
            outfile_report,
            "#%d, Transactions: %d, Interval: %.1f s, Block Size: %.2f MiB\n",
            i,
            block_num_transactions[i],
            block_intervals[i],
            block_size_vb[i] / 1048576.0
        );
    }

    avg_tx_per_block = (stats_blocks > 0) ? ((double)sum_block_tx_count / stats_blocks) : 0.0;
    mean_block_interval = (stats_blocks > 0) ? (sum_block_interval / stats_blocks) : 0.0;
    mean_block_size_mib = (stats_blocks > 0) ? ((sum_block_size_vb / stats_blocks) / 1048576.0) : 0.0;

    if (total_arrived_transactions > 0) {
        drop_rate_percent = ((double)dropped_transactions / total_arrived_transactions) * 100.0;
    } else {
        drop_rate_percent = 0.0;
    }

    fprintf(outfile_report, "\n\n===== SIMULATION SUMMARY =====\n\n");
    fprintf(outfile_report, "Total blocks generated: %d\n", stats_blocks);
    fprintf(outfile_report, "Total transactions completed: %ld\n", total_completed_transactions);
    fprintf(outfile_report, "Avg. Number of Transactions/Block: %.2f\n", avg_tx_per_block);
    fprintf(outfile_report, "Mean Block Generation Interval: %.2f seconds\n", mean_block_interval);
    fprintf(outfile_report, "Mean Block Size: %.4f MiB\n", mean_block_size_mib);
    fprintf(outfile_report, "Total simulation time: %.2f seconds\n", sim_time);
    fprintf(outfile_report, "Max Mempool Size: %lld vB\n\n", max_mempool_size_vb);

    fprintf(outfile_report, "Transaction Statistics:\n");
    fprintf(outfile_report, "Total arrived transactions: %ld\n", total_arrived_transactions);
    fprintf(outfile_report, "Dropped transactions: %ld\n", dropped_transactions);
    fprintf(outfile_report, "Drop rate: %.6f%%\n", drop_rate_percent);

    for (i = 1; i <= stats_blocks; i++) {
        fprintf(
            outfile_data,
            "%d, %.0f, %d, %.2f\n",
            i,
            block_intervals[i],
            block_num_transactions[i],
            block_size_vb[i] / 1048576.0
        );
    }
}

int mempool_push(int tx_size_vb) {
    if (queue_count + 1 >= queue_buffer_size) {
        return 0;
    }

    mempool_queue[queue_tail] = tx_size_vb;
    queue_tail = (queue_tail + 1) % queue_buffer_size;
    queue_count++;
    return 1;
}

int mempool_front(void) {
    return mempool_queue[queue_head];
}

void mempool_pop(void) {
    queue_head = (queue_head + 1) % queue_buffer_size;
    queue_count--;
}

double uniform01(int stream) {
    double u = lcgrand(stream);
    if (u <= 1.0e-12) {
        u = 1.0e-12;
    }
    if (u >= 1.0 - 1.0e-12) {
        u = 1.0 - 1.0e-12;
    }
    return u;
}

double box_muller_normal(double mu, double sigma) {
    double u1 = uniform01(2);
    double u2 = uniform01(3);
    double z = sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    return mu + sigma * z;
}

int generate_dynamic_tx_size(void) {
    double p = uniform01(4);
    double size;

    if (p < 0.45) {
        size = box_muller_normal(220.0, 15.0);
    } else if (p < 0.85) {
        size = box_muller_normal(350.0, 30.0);
    } else {
        size = box_muller_normal(650.0, 100.0);
    }

    if (size < MIN_TX_SIZE_VB) {
        return MIN_TX_SIZE_VB;
    }
    return (int)llround(size);
}

double expon(double mean) {
    return -mean * log(uniform01(1));
}
