/*
 * Admission decisions. Every costed call arrives as a notification and is
 * allowed (CONTINUE), held until budget is available (the reply is
 * deferred), or rejected with an errno.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common.h"
#include "costs.h"
#include "sgd.h"

#ifndef SECCOMP_USER_NOTIF_FLAG_CONTINUE
#define SECCOMP_USER_NOTIF_FLAG_CONTINUE (1UL << 0)
#endif

const int64_t sg_wait_bounds_us[WAIT_BUCKETS] = {
    1000, 5000, 10000, 50000, 100000, 500000, 1000000, 2000000,
};

#define NOTE(j, p, field) ((j)->st.field++, (p)->st.field++, S.total.field++)

static void note_wait_time(struct job *j, struct pcb *p, int64_t wait_us)
{
    j->st.wait_us_total += wait_us;
    p->st.wait_us_total += wait_us;
    S.total.wait_us_total += wait_us;
    if (wait_us > j->st.wait_us_max)
        j->st.wait_us_max = wait_us;
    if (wait_us > p->st.wait_us_max)
        p->st.wait_us_max = wait_us;
    if (wait_us > S.total.wait_us_max)
        S.total.wait_us_max = wait_us;
}

static int job_can(struct job *j, int64_t cost, int64_t now)
{
    return sg_bucket_can(&j->acct.bucket, cost) ||
           sg_borrow(S.accts, S.naccts, &j->acct, cost, now, &cfg.borrow, &S.ledger);
}

static void reply(int lfd, uint64_t id, int err)
{
    memset(S.resp, 0, S.sz.seccomp_notif_resp);
    S.resp->id = id;
    if (err)
        S.resp->error = -err;
    else
        S.resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
    /* ENOENT: the thread died while we were deciding. */
    if (ioctl(lfd, SECCOMP_IOCTL_NOTIF_SEND, S.resp) < 0 && errno != ENOENT)
        perror("sgd: NOTIF_SEND");
}

/*
 * Rejections are answered after reject_delay_ms so that a job retrying in a
 * loop (fork after EAGAIN) cannot keep sgd busy; the thread sleeps meanwhile.
 */
static void reject(struct job *j, uint64_t id, int err, int64_t now)
{
    if (cfg.reject_delay_ms == 0) {
        reply(j->lfd, id, err);
        return;
    }
    if (S.nrejects == S.rejects_cap) {
        size_t cap = S.rejects_cap ? S.rejects_cap * 2 : 64;
        struct held_reject *v = realloc(S.rejects, cap * sizeof *v);

        if (!v)
            die("realloc");
        S.rejects = v;
        S.rejects_cap = cap;
    }
    S.rejects[S.nrejects++] = (struct held_reject){ j, id, err, now + cfg.reject_delay_ms * 1000 };
}

static void flush_rejects(int64_t now, int force, const struct job *gone)
{
    size_t i = 0;

    while (i < S.nrejects) {
        struct held_reject *r = &S.rejects[i];

        if (r->job == gone || force || r->due_us <= now) {
            if (r->job != gone)
                reply(r->job->lfd, r->id, r->err);
            S.rejects[i] = S.rejects[--S.nrejects];
        } else {
            i++;
        }
    }
}

/* See waitq.h for why this key implements aging. */
static int64_t wait_key(const struct job *j, int64_t arrive_us)
{
    return (int64_t)j->acct.prio * cfg.aging_ms * 1000 - arrive_us;
}

static void admit(struct job *j, struct pcb *p, const struct sg_waiter *w, int64_t wait_us)
{
    int b = 0;

    sg_bucket_take(&j->acct.bucket, w->cost);
    sg_bucket_take(&S.global, w->cost);
    reply(j->lfd, w->id, 0);
    NOTE(j, p, allowed);
    j->st.charged += w->cost;
    p->st.charged += w->cost;
    S.total.charged += w->cost;
    note_wait_time(j, p, wait_us);
    while (b < WAIT_BUCKETS && wait_us > sg_wait_bounds_us[b])
        b++;
    S.wait_hist[b]++;
    S.wait_hist_sum_us += wait_us;
    log_call(j, w->pid, w->nr, w->cls, w->cost, "ALLOW", wait_us);
}

void on_notify(struct job *j)
{
    const struct sg_syscall *s;
    struct sg_waiter w;
    enum sg_class cls;
    enum sg_policy policy;
    int64_t now, burst_limit, max_wait_ms;
    struct pcb *p;
    int err = 0;

    memset(S.req, 0, S.sz.seccomp_notif);
    if (ioctl(j->lfd, SECCOMP_IOCTL_NOTIF_RECV, S.req) < 0) {
        if (errno == EINTR || errno == ENOENT)
            return;
        die("sgd: NOTIF_RECV");
    }
    now = sg_now_us();
    j->acct.last_active_us = now;
    S.last_notify_us = now;
    s = sg_lookup(S.req->data.nr);
    if (!s) {
        reply(j->lfd, S.req->id, 0);
        return;
    }
    p = pcb_get(j, (pid_t)S.req->pid);
    if (j->pgid == 0) {
        pid_t g = getpgid((pid_t)S.req->pid);   /* == the sg-launch leader's pid */
        if (g > 0)
            j->pgid = g;
    }
    w = (struct sg_waiter){
        .key = wait_key(j, now),
        .id = S.req->id,
        .arrive_us = now,
        .cost = sg_cost(s, S.req->data.args, &cls) * SG_MILLI,
        .owner = j,
        .aux = p,
        .pid = (int)S.req->pid,
        .nr = S.req->data.nr,
    };
    w.cls = cls;
    NOTE(j, p, calls);
    refill(j, now);
    global_refill(now);

    /* Keep per-job FIFO order, and do not overtake a waiter held for the global bucket. */
    if (j->acct.nwaiting == 0 && !(S.g_blocked && w.key <= S.g_blocked_key) &&
        sg_bucket_can(&S.global, w.cost) && job_can(j, w.cost, now)) {
        admit(j, p, &w, 0);
        return;
    }

    policy = sg_classes[cls].policy;
    max_wait_ms = cfg.tmax_ms;
    if (cls == SG_C_PROC) {
        policy = j->fork_policy == SG_FORK_REJECT ? SG_REJECT : SG_WAIT;
        if (j->fork_policy == SG_FORK_BRIEF && cfg.fork_wait_ms < max_wait_ms)
            max_wait_ms = cfg.fork_wait_ms;
    }

    burst_limit = j->acct.bucket.capacity;
    if (cfg.borrow.enabled)
        burst_limit += j->acct.bucket.capacity * cfg.borrow.cap_pct / 100;
    if (policy == SG_REJECT)
        err = sg_classes[cls].reject_errno;
    else if (w.cost > burst_limit || w.cost > S.global.capacity)
        err = EAGAIN;   /* could never be admitted */
    else if (j->acct.nwaiting >= cfg.qmax)
        err = EAGAIN;
    if (err) {
        reject(j, w.id, err, now);
        NOTE(j, p, rejected);
        log_call(j, w.pid, w.nr, w.cls, w.cost, "REJECT", 0);
        return;
    }

    w.seq = S.seq++;
    w.deadline_us = now + max_wait_ms * 1000;
    if (sg_waitq_push(&S.wq, &w) < 0)
        die("sgd: waitq");
    j->acct.nwaiting++;
    NOTE(j, p, waited);
    log_call(j, w.pid, w.nr, w.cls, w.cost, "WAIT", 0);
}

/* Pop every waiter into S.scratch, in priority order. */
static size_t drain_waiters(void)
{
    struct sg_waiter w;
    size_t n = 0;

    if (S.wq.n > S.scratch_cap) {
        size_t cap = S.wq.n * 2;
        struct sg_waiter *v = realloc(S.scratch, cap * sizeof *v);

        if (!v)
            die("realloc");
        S.scratch = v;
        S.scratch_cap = cap;
    }
    while (sg_waitq_pop(&S.wq, &w))
        S.scratch[n++] = w;
    return n;
}

static void requeue(const struct sg_waiter *w)
{
    if (sg_waitq_push(&S.wq, w) < 0)
        die("sgd: waitq");
}

void rekey_job(struct job *j)
{
    size_t n = drain_waiters(), i;

    for (i = 0; i < n; i++) {
        if (S.scratch[i].owner == j)
            S.scratch[i].key = wait_key(j, S.scratch[i].arrive_us);
        requeue(&S.scratch[i]);
    }
}

void on_tick(void)
{
    uint64_t expirations;
    struct job *jb;
    int64_t now;
    size_t n, i;

    if (read(S.timer_fd, &expirations, sizeof expirations) < 0 && errno != EAGAIN)
        die("sgd: timerfd");
    now = sg_now_us();
    flush_rejects(now, 0, NULL);
    global_refill(now);
    for (jb = S.jobs; jb; jb = jb->next)
        refill(jb, now);
    S.gen++;
    S.g_blocked = 0;

    n = drain_waiters();
    for (i = 0; i < n; i++) {
        struct sg_waiter *e = &S.scratch[i];
        struct job *j = e->owner;
        struct pcb *p = e->aux;
        int64_t waited = now - e->arrive_us;

        /* The thread may have been killed while it waited. */
        if (ioctl(j->lfd, SECCOMP_IOCTL_NOTIF_ID_VALID, &e->id) < 0) {
            j->acct.nwaiting--;
            NOTE(j, p, dropped);
            log_call(j, e->pid, e->nr, e->cls, e->cost, "DROP", waited);
            continue;
        }
        if (now > e->deadline_us) {
            reply(j->lfd, e->id, EAGAIN);
            j->acct.nwaiting--;
            NOTE(j, p, rejected);
            note_wait_time(j, p, waited);
            log_call(j, e->pid, e->nr, e->cls, e->cost, "REJECT", waited);
            continue;
        }
        if (j->blocked_gen == S.gen || S.g_blocked) {
            requeue(e);
            continue;
        }
        /* Borrowing is pointless if the global bucket could not admit the call anyway. */
        if (!sg_bucket_can(&j->acct.bucket, e->cost) &&
            !(sg_bucket_can(&S.global, e->cost) && job_can(j, e->cost, now))) {
            j->blocked_gen = S.gen;
            requeue(e);
            continue;
        }
        if (!sg_bucket_can(&S.global, e->cost)) {
            /* Hold everything behind it, or small low-priority calls would drain the global bucket first. */
            S.g_blocked = 1;
            S.g_blocked_key = e->key;
            requeue(e);
            continue;
        }
        j->acct.nwaiting--;
        admit(j, p, e, waited);
    }
    log_flush();
}

/* Drop the held calls and pending rejections of a job whose listener is gone. */
void forget_job_calls(const struct job *j, int64_t now)
{
    size_t n = drain_waiters(), i;

    for (i = 0; i < n; i++)
        if (S.scratch[i].owner != j)
            requeue(&S.scratch[i]);
    flush_rejects(now, 0, j);
}

/* A filter cannot be removed, so on shutdown held calls run rather than fail. */
void release_all_calls(void)
{
    size_t n = drain_waiters(), i;

    for (i = 0; i < n; i++)
        reply(((struct job *)S.scratch[i].owner)->lfd, S.scratch[i].id, 0);
    flush_rejects(sg_now_us(), 1, NULL);
}
