/* Job lifecycle, per-process stats and priority-share rates. */
#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common.h"
#include "sgd.h"

#ifndef SECCOMP_IOCTL_NOTIF_SET_FLAGS
#define SECCOMP_IOCTL_NOTIF_SET_FLAGS SECCOMP_IOW(4, __u64)
#endif
#ifndef SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP
#define SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP (1UL << 0)
#endif

struct job *find_job(uint32_t id)
{
    struct job *j;

    for (j = S.jobs; j; j = j->next)
        if (j->acct.id == id)
            return j;
    return NULL;
}

struct job **jobs_by_id(size_t *count)
{
    struct job **v, *j;
    size_t n = 0;

    for (j = S.jobs; j; j = j->next)
        n++;
    v = calloc(n ? n : 1, sizeof *v);
    if (!v)
        die("calloc");
    *count = n;
    for (j = S.jobs; j; j = j->next)   /* S.jobs is newest first */
        v[--n] = j;
    return v;
}

struct pcb *pcb_get(struct job *j, pid_t pid)
{
    struct pcb **head = &S.pcb[(unsigned)pid % PCB_BUCKETS], *p;

    for (p = *head; p; p = p->next)
        if (p->pid == pid && p->job == j)
            return p;
    p = calloc(1, sizeof *p);
    if (!p)
        die("calloc");
    p->pid = pid;
    p->job = j;
    p->next = *head;
    *head = p;
    return p;
}

static void pcb_drop_job(struct job *j)
{
    size_t i;

    for (i = 0; i < PCB_BUCKETS; i++) {
        struct pcb **pp = &S.pcb[i];

        while (*pp) {
            if ((*pp)->job == j) {
                struct pcb *dead = *pp;

                log_stats("pcb", j, dead->pid, &dead->st);
                *pp = dead->next;
                free(dead);
            } else {
                pp = &(*pp)->next;
            }
        }
    }
}

void refill(struct job *j, int64_t now)
{
    sg_acct_refill(&j->acct, now, &cfg.borrow, &S.ledger);
}

void global_refill(int64_t now)
{
    S.global_entitled += sg_bucket_refill(&S.global, now);
}

static void rebuild_accts(void)
{
    size_t n = 0;
    struct job *j;

    for (j = S.jobs; j; j = j->next)
        n++;
    if (n > S.accts_cap) {
        struct sg_acct **v = realloc(S.accts, n * 2 * sizeof *v);

        if (!v)
            die("realloc");
        S.accts = v;
        S.accts_cap = n * 2;
    }
    S.naccts = 0;
    for (j = S.jobs; j; j = j->next)
        S.accts[S.naccts++] = &j->acct;
}

/* Share jobs get R_g * prio / sum(prio); capacity is rate * burst. */
void recompute_rates(int64_t now)
{
    int64_t sumw = 0, rate;
    struct job *j;

    for (j = S.jobs; j; j = j->next)
        if (!j->fixed_rate)
            sumw += j->acct.prio;
    for (j = S.jobs; j; j = j->next) {
        rate = j->fixed_rate ? j->fixed_rate : cfg.global_rate * j->acct.prio / sumw;
        if (rate < 1)
            rate = 1;
        refill(j, now);
        /* units/s * ms = milli-units */
        sg_bucket_set_rate(&j->acct.bucket, rate, rate * cfg.burst_ms);
    }
}

struct job *job_new(const struct sg_register *reg, int lfd)
{
    struct job *j = calloc(1, sizeof *j);
    int64_t now = sg_now_us();
    char *c;

    if (!j)
        die("calloc");
    j->kind = EP_JOB;
    j->lfd = lfd;
    j->fixed_rate = reg->rate;
    j->fork_policy = reg->fork_policy;
    j->acct.id = ++S.next_id;
    j->acct.prio = reg->prio;
    j->acct.last_active_us = now;
    memcpy(j->name, reg->name, sizeof j->name);
    j->name[sizeof j->name - 1] = '\0';
    for (c = j->name; *c; c++)   /* keep the JSON log and metric labels valid */
        if (*c == '"' || *c == '\\' || (unsigned char)*c < 0x20)
            *c = '_';
    j->start_us = now;
    sg_bucket_init(&j->acct.bucket, 1, 0, now);
    j->next = S.jobs;
    S.jobs = j;
    rebuild_accts();
    recompute_rates(now);
    j->acct.bucket.tokens = j->acct.bucket.capacity;
    j->acct.entitled_total = j->acct.bucket.capacity;
    ep_add(lfd, j);
    if (cfg.sync_wake &&
        ioctl(lfd, SECCOMP_IOCTL_NOTIF_SET_FLAGS, SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP) < 0) {
        fprintf(stderr, "[sgd] note: synchronous wake-up unavailable (%s; needs Linux 6.6), continuing without\n",
                strerror(errno));
        cfg.sync_wake = 0;
    }

    fprintf(stderr, "[sgd] job %u (%s) started: prio %d, %s %lld units/s, fork policy %s\n", j->acct.id,
            j->name, j->acct.prio, j->fixed_rate ? "fixed" : "share", (long long)j->acct.bucket.rate,
            sg_fork_policy_name(j->fork_policy));
    log_stats("job_start", j, 0, &j->st);
    return j;
}

void job_free(struct job *j)
{
    struct job **pp;

    sg_acct_close(S.accts, S.naccts, &j->acct, &S.ledger);
    for (pp = &S.jobs; *pp; pp = &(*pp)->next)
        if (*pp == j) {
            *pp = j->next;
            break;
        }
    rebuild_accts();
    pcb_drop_job(j);
    epoll_ctl(S.ep, EPOLL_CTL_DEL, j->lfd, NULL);
    close(j->lfd);
    free(j);
}

/* The listener reports POLLHUP only once every task of the job has exited and been reaped. */
void end_job(struct job *j)
{
    int64_t now = sg_now_us();

    forget_job_calls(j, now);
    refill(j, now);
    fprintf(stderr,
            "[sgd] job %u (%s) ended after %.3fs: calls=%llu allowed=%llu waited=%llu rejected=%llu "
            "dropped=%llu charged=%lld max_wait_ms=%.1f loans=%u borrowed=%lld lent=%lld\n",
            j->acct.id, j->name, (double)(now - j->start_us) / 1e6, j->st.calls, j->st.allowed,
            j->st.waited, j->st.rejected, j->st.dropped, j->st.charged / SG_MILLI,
            (double)j->st.wait_us_max / 1000, j->acct.loans_taken,
            (long long)(j->acct.borrowed_total / SG_MILLI), (long long)(j->acct.lent_total / SG_MILLI));
    log_stats("job_end", j, 0, &j->st);
    job_free(j);
    recompute_rates(now);
    log_flush();
}
