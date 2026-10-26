/* JSON-lines event log: every decision, loans, per-job and per-process stats. */
#define _GNU_SOURCE
#include "common.h"
#include "costs.h"
#include "sgd.h"

static const char *sys_name(int nr)
{
    const struct sg_syscall *s = sg_lookup(nr);

    return s ? s->name : "?";
}

void log_call(const struct job *j, int pid, int nr, int cls, int64_t cost, const char *dec, int64_t wait_us)
{
    if (!cfg.log)
        return;
    fprintf(cfg.log,
            "{\"t_us\":%lld,\"ev\":\"call\",\"job\":%u,\"pid\":%d,\"sys\":\"%s\",\"cls\":\"%s\","
            "\"cost\":%.3f,\"dec\":\"%s\",\"wait_us\":%lld}\n",
            (long long)sg_now_us(), j->acct.id, pid, sys_name(nr), sg_classes[cls].name, units(cost), dec,
            (long long)wait_us);
}

void log_stats(const char *ev, const struct job *j, int pid, const struct stats *st)
{
    if (!cfg.log)
        return;
    fprintf(cfg.log,
            "{\"t_us\":%lld,\"ev\":\"%s\",\"job\":%u,\"name\":\"%s\",\"pid\":%d,\"prio\":%d,"
            "\"calls\":%llu,\"allowed\":%llu,\"waited\":%llu,\"rejected\":%llu,\"dropped\":%llu,"
            "\"charged\":%.3f,\"wait_us_total\":%lld,\"wait_us_max\":%lld",
            (long long)sg_now_us(), ev, j->acct.id, j->name, pid, j->acct.prio, st->calls, st->allowed,
            st->waited, st->rejected, st->dropped, units(st->charged), st->wait_us_total, st->wait_us_max);
    if (pid == 0)
        fprintf(cfg.log,
                ",\"rate\":%lld,\"fixed\":%d,\"fork_policy\":\"%s\",\"capacity\":%.3f,\"entitled\":%.3f,"
                "\"loans_taken\":%u,\"borrowed\":%.3f,\"loans_given\":%u,\"lent\":%.3f",
                (long long)j->acct.bucket.rate, j->fixed_rate != 0, sg_fork_policy_name(j->fork_policy),
                units(j->acct.bucket.capacity),
                units(j->acct.entitled_total), j->acct.loans_taken, units(j->acct.borrowed_total),
                j->acct.loans_given, units(j->acct.lent_total));
    fputs("}\n", cfg.log);
}

void log_loan(void *ctx, enum sg_loan_event ev, const struct sg_loan *l)
{
    static const char *const names[] = { "loan_open", "loan_repaid", "loan_written_off" };
    int64_t now = sg_now_us();

    (void)ctx;
    if (!cfg.log)
        return;
    fprintf(cfg.log,
            "{\"t_us\":%lld,\"ev\":\"%s\",\"loan\":%u,\"from\":%u,\"to\":%u,\"amount\":%.3f,"
            "\"repaid\":%.3f,\"age_us\":%lld}\n",
            (long long)now, names[ev], l->id, l->lender->id, l->borrower->id, units(l->amount),
            units(l->repaid), (long long)(now - l->t_start_us));
}

void log_sgd(const char *ev)
{
    if (!cfg.log)
        return;
    fprintf(cfg.log,
            "{\"t_us\":%lld,\"ev\":\"%s\",\"global_rate\":%lld,\"burst_ms\":%lld,\"tmax_ms\":%lld,"
            "\"fork_wait_ms\":%lld,\"aging_ms\":%lld,\"borrowing\":%d,\"loan_chunk_pct\":%lld,"
            "\"reject_delay_ms\":%lld,\"spin_us\":%lld,\"sync_wake\":%d,\"costs\":\"%s\","
            "\"global_entitled\":%.3f,\"charged\":%.3f}\n",
            (long long)sg_now_us(), ev, (long long)cfg.global_rate, (long long)cfg.burst_ms,
            (long long)cfg.tmax_ms, (long long)cfg.fork_wait_ms, (long long)cfg.aging_ms, cfg.borrow.enabled,
            (long long)cfg.borrow.chunk_pct, (long long)cfg.reject_delay_ms, (long long)cfg.spin_us,
            cfg.sync_wake, cfg.cost_file ? "calibrated" : "seed", units(S.global_entitled),
            units(S.total.charged));
}

void log_flush(void)
{
    if (cfg.log)
        fflush(cfg.log);
}
