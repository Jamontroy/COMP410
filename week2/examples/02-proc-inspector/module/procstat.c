// COMP 410 - Week 2 - STARTER: a loadable kernel module with a /proc entry.
//
// Builds against Linux kernel headers; runs in the QEMU guest. This is the
// artifact A2 asks for: a module that reports something real, guards the
// user-copy boundary, and unloads without leaking.
//
// Read alongside src/copyguard.c, which models the SAME boundary rules in
// user space so they can be tested on any machine.

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define PROCNAME "comp410_procstat"

// Shared state. Two readers can be in the file at once, so every touch of
// these is under the mutex -- this is the concurrency guard A2 asks for.
static DEFINE_MUTEX(stat_lock);
static unsigned long read_count;

// Count tasks by state. Walking the task list requires the RCU read lock:
// tasks can exit underneath us, and without it the `next` pointer we follow
// may already be freed.
static void count_tasks(unsigned long *running, unsigned long *sleeping,
                        unsigned long *zombie, unsigned long *total)
{
        struct task_struct *t;

        *running = *sleeping = *zombie = *total = 0;

        // TODO 4. Walking the task list without the RCU read lock is a race:
        // a task can exit under you and the pointer you follow is then freed.
        // Take it here and release it after the loop.
	rcu_read_lock();
        for_each_process(t) {
                (*total)++;
                if (t->exit_state & EXIT_ZOMBIE)
                        (*zombie)++;
                else if (task_is_running(t))
                        (*running)++;
                else
                        (*sleeping)++;
        }
	rcu_read_unlock();
}

// seq_file does the user-copy for us. That is the point of using it: we never
// call copy_to_user() by hand, so we cannot get the length wrong. Compare
// src/copyguard.c, which shows what the hand-written version must check.
static int procstat_show(struct seq_file *m, void *v)
{
        unsigned long running, sleeping, zombie, total, reads;

        count_tasks(&running, &sleeping, &zombie, &total);

        // TODO 5. Two readers can be inside this function at once, so this
        // increment is a data race. Guard it with stat_lock.
	mutex_lock(&stat_lock);
        reads = ++read_count;
	mutex_unlock(&stat_lock); //the shared state will only be touched under lock now

        seq_printf(m, "tasks_total    %lu\n", total);
        seq_printf(m, "tasks_running  %lu\n", running);
        seq_printf(m, "tasks_sleeping %lu\n", sleeping);
        seq_printf(m, "tasks_zombie   %lu\n", zombie);
        seq_printf(m, "reads          %lu\n", reads);
        return 0;

}

static int procstat_open(struct inode *inode, struct file *file)
{
        return single_open(file, procstat_show, NULL);
}

static const struct proc_ops procstat_ops = {
        .proc_open    = procstat_open,
        .proc_read    = seq_read,
        .proc_lseek   = seq_lseek,
        .proc_release = single_release,
};

static struct proc_dir_entry *entry;

static int __init procstat_init(void)
{

        entry = proc_create(PROCNAME, 0444, NULL, &procstat_ops);
        // TODO 6. proc_create can fail. Check it, log it, and return an error
        // -- a module whose init cannot fail will oops on the first read.

	if(!entry) {
		pr_err("failed to make /proc/%s\n", PROCNAME); //Denotes when the proc fails to create
		return -ENOMEM;
	}

        pr_info("comp410: /proc/%s created\n", PROCNAME);
        return 0;
}

static void __exit procstat_exit(void)
{
        proc_remove(entry);       // every create in init has its remove here
        pr_info("comp410: /proc/%s removed\n", PROCNAME);
}

module_init(procstat_init);
module_exit(procstat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("COMP 410");
MODULE_DESCRIPTION("Task-state counters over /proc");
MODULE_VERSION("1.0");
