/* sgctl commands over the Unix socket. */
#define _GNU_SOURCE
#include <stdlib.h>

#include "common.h"
#include "sgd.h"

static void write_status(FILE *out)
{
    size_t n, i;
    struct job **v = jobs_by_id(&n);

    fprintf(out, "global: %.1f / %.1f units, refill %lld units/s, borrowing %s, up %.1f s\n",
            units(S.global.tokens), units(S.global.capacity), (long long)cfg.global_rate,
            cfg.borrow.enabled ? "on" : "off", (double)(sg_now_us() - S.start_us) / 1e6);
    fprintf(out, "totals: calls=%llu allowed=%llu waited=%llu rejected=%llu dropped=%llu charged=%.0f\n",
            S.total.calls, S.total.allowed, S.total.waited, S.total.rejected, S.total.dropped,
            units(S.total.charged));
    fprintf(out, "loans:  opened=%llu repaid=%llu written_off=%llu\n\n", S.ledger.opened, S.ledger.repaid,
            S.ledger.written_off);
    fprintf(out, "%-4s %-14s %4s %5s %8s %10s %10s %4s %8s %8s %7s %8s %8s %8s %6s\n", "JOB", "NAME", "PRIO",
            "MODE", "RATE/S", "TOKENS", "CAPACITY", "WAIT", "CALLS", "ALLOWED", "WAITED", "REJECTED", "OWED",
            "LENT", "FORK");
    for (i = 0; i < n; i++) {
        const struct job *j = v[i];

        fprintf(out,
                "%-4u %-14.14s %4d %5s %8lld %10.1f %10.1f %4u %8llu %8llu %7llu %8llu %8.1f %8.1f %6s\n",
                j->acct.id, j->name, j->acct.prio, j->fixed_rate ? "fixed" : "share",
                (long long)j->acct.bucket.rate, units(j->acct.bucket.tokens), units(j->acct.bucket.capacity),
                j->acct.nwaiting, j->st.calls, j->st.allowed, j->st.waited, j->st.rejected,
                units(j->acct.owed), units(j->acct.lent_out), sg_fork_policy_name(j->fork_policy));
    }
    if (n == 0)
        fprintf(out, "(no jobs)\n");
    free(v);
}

static void write_loans(FILE *out)
{
    int64_t now = sg_now_us();
    size_t n, i, rows = 0;
    struct job **v = jobs_by_id(&n);
    const struct sg_loan *l;

    fprintf(out, "%-5s %5s %5s %10s %10s %8s\n", "LOAN", "FROM", "TO", "AMOUNT", "REPAID", "AGE_S");
    for (i = 0; i < n; i++)
        for (l = v[i]->acct.loans; l; l = l->next, rows++)
            fprintf(out, "%-5u %5u %5u %10.1f %10.1f %8.2f\n", l->id, l->lender->id, l->borrower->id,
                    units(l->amount), units(l->repaid), (double)(now - l->t_start_us) / 1e6);
    if (rows == 0)
        fprintf(out, "(no outstanding loans)\n");
    free(v);
}

void ctl_handle(int fd, const struct sg_ctl *ctl)
{
    char *buf = NULL;
    size_t len = 0;
    FILE *out = open_memstream(&buf, &len);
    struct job *j = NULL;

    if (!out)
        return;
    if (ctl->cmd == SG_CTL_SET_PRIO || ctl->cmd == SG_CTL_SET_RATE) {
        j = find_job(ctl->job);
        if (!j) {
            fprintf(out, "error: no job %u\n", ctl->job);
            goto send;
        }
    }

    switch (ctl->cmd) {
    case SG_CTL_STATUS:
        write_status(out);
        break;
    case SG_CTL_LOANS:
        write_loans(out);
        break;
    case SG_CTL_METRICS:
        write_metrics(out);
        break;
    case SG_CTL_SET_PRIO:
        if (ctl->value < SG_PRIO_MIN || ctl->value > SG_PRIO_MAX) {
            fprintf(out, "error: priority must be %d..%d\n", SG_PRIO_MIN, SG_PRIO_MAX);
            break;
        }
        j->acct.prio = (int)ctl->value;
        rekey_job(j);
        recompute_rates(sg_now_us());
        log_change("set_prio", j, ctl->value);
        fprintf(out, "ok: job %u (%s) priority %d, rate %lld units/s\n", j->acct.id, j->name, j->acct.prio,
                (long long)j->acct.bucket.rate);
        break;
    case SG_CTL_SET_RATE:
        if (ctl->value < 0) {
            fprintf(out, "error: rate must be >= 0 (0 = priority share)\n");
            break;
        }
        j->fixed_rate = ctl->value;
        recompute_rates(sg_now_us());
        log_change("set_rate", j, ctl->value);
        fprintf(out, "ok: job %u (%s) %s rate %lld units/s\n", j->acct.id, j->name,
                j->fixed_rate ? "fixed" : "share", (long long)j->acct.bucket.rate);
        break;
    default:
        fprintf(out, "error: unknown command %d\n", ctl->cmd);
    }
send:
    fclose(out);
    send_all(fd, buf, len);
    free(buf);
}
