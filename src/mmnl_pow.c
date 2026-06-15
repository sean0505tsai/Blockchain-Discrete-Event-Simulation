/* 
 * Bitcoin Network Simulation - Plan B (PoW Competition)
 * Based on Discrete Event Simulation for Blockchain Systems
 * 
 * System: 1000 parallel miners competing for block generation
 * Events: ARRIVAL (transaction), DEPARTURE (miner completion)
 * Warm-up: First 10 blocks are warm-up (not counted)
 * Statistics: Blocks 11-2026 are counted (re-numbered 1-2016)
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "lcgrand.h"  /* Header file for random-number generator. */

/* System Parameters */
#define NUM_SERVERS 1000           /* Number of miners */
#define NUM_EVENTS 2               /* ARRIVAL=1, DEPARTURE=2 */
#define NUM_WARM_UP_BLOCKS 10      /* Warm-up blocks (not counted) */

/* Input parameters (read from file) */
int num_blocks_to_simulate;        /* Statistics blocks (read from input) */
int queue_capacity;                /* Mempool capacity (read from input) */
int max_transactions_per_block;    /* Max transactions per block (read from input) */
int NUM_BLOCKS_TO_SIMULATE;        /* Total blocks = num_blocks_to_simulate + 10 */
int NUM_STATS_BLOCKS;              /* Same as num_blocks_to_simulate */

/* Dynamic arrays for block statistics */
int *block_num_transactions;
float *block_intervals;
float *block_utilization;

/* Mempool tracking */
int max_mempool_size = 0;          /* Peak mempool size (excluding warm-up) */

/* Event types */
#define ARRIVAL 1
#define DEPARTURE 2

/* Miner states */
#define IDLE 0
#define BUSY 1

/* Global variables */
int next_event_type;
float mean_interarrival, mean_service;
float sim_time, time_last_event, time_next_event[NUM_EVENTS + 1];

/* Mempool and transaction tracking */
int num_in_mempool = 0;
long total_arrived_transactions = 0;    /* All arrivals (blocks #11-#154) */
long dropped_transactions_stats = 0;    /* Dropped in blocks #11-#154 */

/* Miner state tracking */
int miner_state[NUM_SERVERS];           /* BUSY or IDLE */
float miner_finish_time[NUM_SERVERS];   /* Completion time for each miner */
int num_busy_servers = 0;

/* Block statistics */
int blocks_completed = 0;               /* Count of completed blocks */
int transactions_in_block = 0;          /* Transactions in current block */
float block_start_time = 0.0;           /* Start time of current block period */
float last_block_time = 0.0;            /* Time of last block generation */



/* Utilization tracking */
float miner_busy_in_block[NUM_SERVERS]; /* Busy time per miner in current block */
float total_busy_in_block = 0.0;        /* Total busy time in current block */

/* File pointers */
FILE *infile, *outfile, *outfile_tx, *outfile_interval, *outfile_util;

/* Function declarations */
void initialize(void);
void timing(void);
void arrive(void);
void depart(void);
void report(void);
void update_time_avg_stats(void);
float expon(float mean);


int main(void) {
    /* Open input and output files */
    infile = fopen("mmnl_pow.in", "r");
    outfile = fopen("mmnl_pow.out", "w");
    outfile_tx = fopen("num_transactions_block.out", "w");
    outfile_interval = fopen("block_generation_interval.out", "w");
    outfile_util = fopen("utilization.out", "w");

    if (!infile || !outfile || !outfile_tx || !outfile_interval || !outfile_util) {
        fprintf(stderr, "Error opening files\n");
        return 1;
    }

    /* Read input parameters: mean_interarrival, mean_service, queue_capacity, max_transactions_per_block, num_blocks */
    fscanf(infile, "%f %f %d %d %d", &mean_interarrival, &mean_service, &queue_capacity, 
           &max_transactions_per_block, &num_blocks_to_simulate);
    
    /* Calculate total blocks (including warm-up) */
    NUM_BLOCKS_TO_SIMULATE = num_blocks_to_simulate + NUM_WARM_UP_BLOCKS;
    NUM_STATS_BLOCKS = num_blocks_to_simulate;
    
    /* Allocate dynamic arrays */
    block_num_transactions = (int *)malloc((NUM_BLOCKS_TO_SIMULATE + 1) * sizeof(int));
    block_intervals = (float *)malloc((NUM_BLOCKS_TO_SIMULATE + 1) * sizeof(float));
    block_utilization = (float *)malloc((NUM_BLOCKS_TO_SIMULATE + 1) * sizeof(float));
    
    if (!block_num_transactions || !block_intervals || !block_utilization) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    
    fprintf(outfile, "Bitcoin Network Simulation (MMnL Model)\n\n");
    fprintf(outfile, "Mean inter-arrival time: %.3f seconds\n", mean_interarrival);
    fprintf(outfile, "Mean service time: %.3f seconds\n", mean_service);
    fprintf(outfile, "Number of servers (miners): %d\n", NUM_SERVERS);
    fprintf(outfile, "Number of blocks to simulate: %d\n", NUM_STATS_BLOCKS);
    fprintf(outfile, "Mempool capacity: %d\n\n", queue_capacity);

    /* Initialize the simulation */
    initialize();

    /* Run the simulation */
    while (blocks_completed < NUM_BLOCKS_TO_SIMULATE) {
        /* Determine the next event */
        timing();

        /* Update time-average statistical accumulators */
        update_time_avg_stats();

        /* Invoke the appropriate event function */
        switch (next_event_type) {
            case ARRIVAL:
                arrive();
                break;
            case DEPARTURE:
                depart();
                break;
        }
    }

    /* Generate report */
    report();

    /* Close files */
    fclose(infile);
    fclose(outfile);
    fclose(outfile_tx);
    fclose(outfile_interval);
    fclose(outfile_util);
    
    /* Note: memory freed in report() */

    return 0;
}



void initialize(void) {
    int i;

    /* Initialize simulation clock */
    sim_time = 0.0;
    time_last_event = 0.0;
    block_start_time = 0.0;
    last_block_time = 0.0;
    blocks_completed = 0;

    /* Initialize all miners to BUSY state (cold start with empty block) */
    num_busy_servers = NUM_SERVERS;
    for (i = 0; i < NUM_SERVERS; i++) {
        miner_state[i] = BUSY;
        /* Generate first service completion time for each miner (exponential) */
        miner_finish_time[i] = sim_time + expon(mean_service);
        miner_busy_in_block[i] = 0.0;
    }

    /* Initialize mempool */
    num_in_mempool = 0;
    transactions_in_block = 0;

    /* Initialize statistics */
    total_arrived_transactions = 0;
    dropped_transactions_stats = 0;
    total_busy_in_block = 0.0;
    max_mempool_size = 0;

    /* Initialize event list */
    /* Schedule first arrival */
    time_next_event[ARRIVAL] = sim_time + expon(mean_interarrival);

    /* Find the earliest completion time among all miners (DEPARTURE) */
    float min_finish_time = 1.0e+30;
    for (i = 0; i < NUM_SERVERS; i++) {
        if (miner_finish_time[i] < min_finish_time) {
            min_finish_time = miner_finish_time[i];
        }
    }
    time_next_event[DEPARTURE] = min_finish_time;
}


void timing(void) {
    int i;
    float min_time_next_event = 1.0e+30;

    next_event_type = 0;

    /* Find the event with the smallest time */
    for (i = 1; i <= NUM_EVENTS; i++) {
        if (time_next_event[i] < min_time_next_event) {
            min_time_next_event = time_next_event[i];
            next_event_type = i;
        }
    }

    /* Check if event list is empty */
    if (next_event_type == 0) {
        fprintf(stderr, "Event list empty at time %f\n", sim_time);
        exit(1);
    }

    /* Advance simulation clock */
    sim_time = min_time_next_event;
}


void arrive(void) {
    /* Schedule next arrival */
    time_next_event[ARRIVAL] = sim_time + expon(mean_interarrival);

    /* Count arrival for statistics (only count in blocks #11-#154) */
    if (blocks_completed >= NUM_WARM_UP_BLOCKS) {
        total_arrived_transactions++;
    }

    /* Check if mempool is full */
    if (num_in_mempool < queue_capacity) {
        /* Add transaction to mempool */
        num_in_mempool++;
        
        /* Track max mempool size (only during stats blocks) */
        if (blocks_completed >= NUM_WARM_UP_BLOCKS && num_in_mempool > max_mempool_size) {
            max_mempool_size = num_in_mempool;
        }
    } else {
        /* Mempool is full, drop transaction (only count if in stats blocks) */
        if (blocks_completed >= NUM_WARM_UP_BLOCKS) {
            dropped_transactions_stats++;
        }
    }
}


void depart(void) {
    int i;
    float min_finish_time;
    float block_duration;

    /* ===== Block Generation Event ===== */

    /* 1. Record transactions in this block (limited to max_transactions_per_block) */
    transactions_in_block = (num_in_mempool > max_transactions_per_block) ? max_transactions_per_block : num_in_mempool;
    
    /* Update mempool after removing the transactions included in this block */
    num_in_mempool -= transactions_in_block;

    /* 2. Calculate block interval and utilization */
    if (blocks_completed == 0) {
        /* First block: interval from time 0 to current time */
        block_duration = sim_time - 0.0;
    } else {
        /* Subsequent blocks: interval from last block to current time */
        block_duration = sim_time - last_block_time;
    }

    /* Calculate utilization for this block */
    float block_utilization_percent = 100.0; /* All miners are BUSY in this design */
    if (block_duration > 0) {
        block_utilization_percent = (total_busy_in_block / (NUM_SERVERS * block_duration)) * 100.0;
    }



    /* 4. Increment block counter */
    blocks_completed++;

    /* 5. Record block statistics (for all 154 blocks) */
    block_num_transactions[blocks_completed] = transactions_in_block;
    block_intervals[blocks_completed] = block_duration;
    block_utilization[blocks_completed] = block_utilization_percent;

    /* Print block info (all 154 blocks) */
    printf("#%d, Transactions: %d, Interval: %.1f s, Utilization: %.2f%%\n",
           blocks_completed, transactions_in_block, block_duration, block_utilization_percent);

    /* Update block timing */
    last_block_time = sim_time;
    block_start_time = sim_time;
    total_busy_in_block = 0.0;

    /* Reset miner busy times for next block */
    for (i = 0; i < NUM_SERVERS; i++) {
        miner_busy_in_block[i] = 0.0;
    }

    /* ===== Reschedule all miners to BUSY state ===== */

    num_busy_servers = NUM_SERVERS;
    min_finish_time = 1.0e+30;

    for (i = 0; i < NUM_SERVERS; i++) {
        /* All miners transition to BUSY and get new service time */
        miner_state[i] = BUSY;
        miner_finish_time[i] = sim_time + expon(mean_service);

        if (miner_finish_time[i] < min_finish_time) {
            min_finish_time = miner_finish_time[i];
        }
    }

    /* Update next departure event */
    time_next_event[DEPARTURE] = min_finish_time;
}


void report(void) {
    int i, stats_blocks_count;
    double avg_transactions = 0.0;
    double mean_interval = 0.0;
    double mean_utilization = 0.0;
    double drop_rate = 0.0;
    long total_completed_transactions = 0;

    stats_blocks_count = blocks_completed - NUM_WARM_UP_BLOCKS;

    /* Write warm-up blocks are NOT printed to output, start from block #11 */
    for (i = NUM_WARM_UP_BLOCKS + 1; i <= blocks_completed; i++) {
        fprintf(outfile, "#%d, Transactions: %d, Interval: %.1f s, Utilization: %.1f%%\n",
                i, block_num_transactions[i], block_intervals[i], block_utilization[i]);
    }

    /* Calculate statistics only for stats blocks */
    if (stats_blocks_count > 0) {
        for (i = NUM_WARM_UP_BLOCKS + 1; i <= blocks_completed; i++) {
            avg_transactions += block_num_transactions[i];
            mean_interval += block_intervals[i];
            mean_utilization += block_utilization[i];
            total_completed_transactions += block_num_transactions[i];
        }
        avg_transactions /= stats_blocks_count;
        mean_interval /= stats_blocks_count;
        mean_utilization /= stats_blocks_count;

        if (total_arrived_transactions > 0) {
            drop_rate = (double)dropped_transactions_stats / total_arrived_transactions * 100.0;
        }
    }

    /* Write summary statistics */
    fprintf(outfile, "\n\n===== SIMULATION SUMMARY =====\n\n");
    fprintf(outfile, "Total blocks generated: %d\n", stats_blocks_count);
    fprintf(outfile, "Total transactions completed: %ld\n", total_completed_transactions);
    fprintf(outfile, "Avg. Number of Transactions/Block: %.1f\n", avg_transactions);
    fprintf(outfile, "Mean Block Generation Interval: %.1f seconds\n", mean_interval);
    fprintf(outfile, "Mean Utilization of Servers: %.1f%%\n", mean_utilization);
    fprintf(outfile, "Total simulation time: %.1f seconds\n", sim_time);
    fprintf(outfile, "Max Mempool Size: %d\n\n", max_mempool_size);

    fprintf(outfile, "Transaction Statistics:\n");
    fprintf(outfile, "Total arrived transactions: %ld\n", total_arrived_transactions);
    fprintf(outfile, "Dropped transactions: %ld\n", dropped_transactions_stats);
    fprintf(outfile, "Drop rate: %.6f%%\n", drop_rate);

    /* Write statistics to .out files (only stats blocks) */
    for (i = 1; i <= stats_blocks_count; i++) {
        fprintf(outfile_tx, "%d, %d\n", i, block_num_transactions[NUM_WARM_UP_BLOCKS + i]);
        fprintf(outfile_interval, "%d, %.1f\n", i, block_intervals[NUM_WARM_UP_BLOCKS + i]);
        fprintf(outfile_util, "%d, %.2f\n", i, block_utilization[NUM_WARM_UP_BLOCKS + i]);
    }
    
    /* Free allocated memory */
    free(block_num_transactions);
    free(block_intervals);
    free(block_utilization);
}


void update_time_avg_stats(void) {
    int i;
    float time_since_last_event = sim_time - time_last_event;

    /* Update busy time accumulation for each miner in current block */
    for (i = 0; i < NUM_SERVERS; i++) {
        if (miner_state[i] == BUSY) {
            /* Miner is busy: add the time interval to its busy area in this block */
            miner_busy_in_block[i] += time_since_last_event;
            total_busy_in_block += time_since_last_event;
        }
    }

    time_last_event = sim_time;
}


float expon(float mean)  /* Exponential variate generation function. */
{
    /* Return an exponential random variate with mean "mean". */

    return -mean * log(lcgrand(1));
}

