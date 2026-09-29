/*
 * COMP 410 - Week 4 - YOUR tests.
 *
 * Two boundary cases the course tests do not cover. Write each so that it
 * PASSES on your finished code and FAILS on the broken synchronize_rcu() it
 * names. Keep the function names and the messages in main(): `make test`
 * reports them.
 *
 * A test that cannot fail proves nothing. Before you hand in, break your own
 * synchronize_rcu() each way named below, run `make test`, and see the right
 * test fail; then put it back.
 *
 * Use rcu.h and table.h as they are. The helpers below are yours to use or not.
 */
#define _POSIX_C_SOURCE 200809L

#include "rcu.h"
#include "table.h"

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

__attribute__((unused))
static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

__attribute__((unused))
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

typedef struct { rcu_reader *r; atomic_int inside, leave; } overlap_reader; // A structure to hold the reader pointer, an int flag to show whether it is in the space or not, and  the leave flag tells the reader when to leave

static void *overlap_reader_worker(void *p) // A reader thread, This is built off the struct I just made. Is going to enter in the critical section, set its flag correctly, then when it is told to leave, it will exit
{
    overlap_reader *x = p; // takes the void pointer and gives it to the overlap_reader
    rcu_read_lock(x->r); // enters the read-side section
    atomic_store(&x->inside, 1); // sets the inside flag to 1 since it is now inside
    while (!atomic_load(&x->leave)) // stays inside the critical section until leave == 1
        sched_yield();  // gives up the CPU to allow other threads to run
    rcu_read_unlock(x->r); // exits the read-side space
    rcu_unregister(x->r); // unregisters the reader
    return NULL;
}

static void *update_thread(void *p) // This is a wrapper which updates the table.
{
    rt_update(p); // publishes a new table version
    return NULL;
}

static int mine_two_readers_both_waited_for(void)
{
    /*
     * TODO 6 - two readers, both inside at once.
     *
     * Two reader threads enter sections that overlap and leave at different
     * times. An update made while both are inside must free nothing until the
     * second one has left, and must free exactly one version in the end.
     *
     * Must FAIL on: a synchronize_rcu() that returns without waiting.
     */

    rcu_table t; // creates struct rcu_table t
    rt_init(&t); // initializes the rcu_table
    long base = rt_freed(); // gets the number of freed versions before the update

    overlap_reader readers[2] = {{ .r = NULL, .inside = 0, .leave = 0 },{ .r = NULL, .inside = 0, .leave = 0 },};   // creates an array of the overlaping readers, the readers will go in here when they are created
    pthread_t th[2];    // creates an array of the threads that will be created for the readers
    for (int i = 0; i < 2; i++) {   // increments through the readers
        readers[i].r = rcu_register(); // registers each reader on the main thread
        pthread_create(&th[i], NULL, overlap_reader_worker, &readers[i]); //starts the worker thread
    }

    while (!atomic_load(&readers[0].inside) || !atomic_load(&readers[1].inside)) // waits for both readers to enter the read-side critical section
        sched_yield(); // until then it gives up the CPU to allow other threads to run

    pthread_t update_th; // declares the update thread
    unsigned long waits_before = rcu_sync_waits(); // pulls the number of waits before the update is made, this is used to check if the update waited for the readers or not
    int ok = 1; // creates a flag to check if the test passed or failed
    if (pthread_create(&update_th, NULL, update_thread, &t) != 0) { // checks if the update thread was created successfully
        ok = 0; // if it was not created successfully, the test fails
    } else {
        sleep_ms(20); // gives the updater time to reach synchronize_rcu(); a broken version that doesn't wait would free the old version during this window
        if (rt_freed() != base) // checks if the number of freed versions matches with base we set earlier (both readers are still inside so nothing should be freed yet)
            ok = 0; // if it does not match, the test fails

        atomic_store(&readers[0].leave, 1); // sets the leave flag for the first reader to 1, so it will exit the space
        sleep_ms(20); // gives reader[0] time to exit, a broken version that only waits for one reader would free the old version during this window
        if (rt_freed() != base) // checks if the number of freed versions matches with base we set earlier, if reader 1 is still inside nothing should be freed
            ok = 0; // if it does not match, the test fails

        atomic_store(&readers[1].leave, 1); // sets the leave flag for the second reader to 1, so it will exit the space
        sleep_ms(20); // sleeps for another 20ms to give the reader time to leave
        pthread_join(update_th, NULL);
        if (rt_freed() != base + 1 || rcu_sync_waits() == waits_before) // if the number of freed versions does not match with base + 1, or if the number of waits did not increase, the test fails
            ok = 0;
    }

    for (int i = 0; i < 2; i++) // increments through the readers and joins the threads
        pthread_join(th[i], NULL); // joins the threads for the readers

    rt_destroy(&t); // destroys the rcu_table
    return ok; // returns the flag to show if the test passed or failed
}

typedef struct {rcu_table *t; atomic_int stop; atomic_int done;} busy_arg;

static void *busy_reader_worker(void *p)  // the reader thread, it is going to be the one to enter a 1ms critical section 2 times in a row until told to stop
{
    busy_arg *arg = p;  // takes the void pointer and gives it to the busy_arg
    rcu_reader *r = rcu_register(); // registers the reader
    while (!atomic_load_explicit(&arg->stop, memory_order_relaxed)) { // while the stop flag is not set to 1
        rcu_read_lock(r); // enters the read-side critical section
        sleep_ms(1); // sleeps for 1 ms to simulate a busy reader
        rcu_read_unlock(r); // exits the read-side critical section
    }
    rcu_unregister(r); // unregisters the reader
    return NULL;
}

static void *busy_update_worker(void *p) // the updater thread, will set 50 updates then sets done
{
    busy_arg *arg = p;  // Same thing as above takes the void pointer and gives it to the busy_arg
    for (int i = 0; i < 50; i++) //iterates through the 50 updates
        rt_update(arg->t);
    atomic_store_explicit(&arg->done, 1, memory_order_release); // sets done and releases
    return NULL;
}

static int mine_busy_reader_does_not_stall(void)
{
    /*
     * TODO 7 - a busy reader must not stall updates.
     *
     * One reader re-enters a 1 ms section over and over, with no pause between
     * sections. 50 updates made beside it must all finish within 1.5 s, and
     * free 50 old versions.
     *
     * Must FAIL on: a synchronize_rcu() that waits for a reader's counter to
     * become even. Run the updates on their own thread, and when 1.5 s have
     * passed stop the reader before you join the updater, so the test ends even
     * when an update is stuck.
     */
    rcu_table t; // declares the rcu_table
    rt_init(&t); // initializes it
    long base = rt_freed(); // pulls the number of freed versions and set it to base. Same as in TODO 6, we'll cross reference this later

    busy_arg arg = { .t = &t, .stop = 0, .done = 0 }; // shares the table and the stop/done flags with both worker threads
    pthread_t reader_th, update_th; // stores the handles for the reader and updater threads
    unsigned long waits_before = rcu_sync_waits(); // records the wait count before this test's updates

    pthread_create(&reader_th, NULL, busy_reader_worker, &arg); // starts the reader that repeatedly enters 1 ms sections
    pthread_create(&update_th, NULL, busy_update_worker, &arg); // starts the updater that performs 50 updates

    double start = now_s(); // records when the 1.5 second completion window begins
    while (!atomic_load_explicit(&arg.done, memory_order_acquire) && now_s() - start < 1.5) // waits for completion or until the time limit expires
        sleep_ms(1); // avoids busy-waiting while polling the updater's done flag

    int in_time = atomic_load_explicit(&arg.done, memory_order_acquire); // records whether the updater finished before the deadline
    atomic_store_explicit(&arg.stop, 1, memory_order_release); // asks the reader to finish its loop before the updater is joined
    pthread_join(update_th, NULL); // joins the updates. Since the reader is stopped, this joins even if there is a stuck update

    int ok = 1; // assumes success until one of the required conditions fails
    if (!in_time || rcu_sync_waits() == waits_before) // fails unless all 50 updates finished in the 1.5s and the rcu synch had to wait for the reader atleast once
        ok = 0; // fails if the updates timed out or never reached synchronize_rcu()'s waiting path
    else if (rt_freed() - base != 50) // checks that exactly the 50 old table versions were reclaimed
        ok = 0; // fails if the expected number of versions was not freed

    pthread_join(reader_th, NULL); // waits for the reader to leave its final section and unregister
    rt_destroy(&t); // destroys the table after both worker threads have stopped using it
    return ok; // reports whether the busy-reader test passed
}

int main(void)
{
    alarm(60);
    int passed = 0, failed = 0;
    struct { const char *name; int (*fn)(void); } tests[] = {
        { "two readers inside at once: the update waits for both", mine_two_readers_both_waited_for },
        { "a reader re-entering every 1 ms does not stall 50 updates", mine_busy_reader_does_not_stall },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int ok = tests[i].fn();
        printf("  %s %s\n", ok ? "[ok]  " : "[FAIL]", tests[i].name);
        if (ok)
            passed++;
        else
            failed++;
    }
    printf("  %d of %d of your tests pass\n", passed, passed + failed);
    return failed ? 1 : 0;
}
