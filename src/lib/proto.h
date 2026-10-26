#ifndef SG_PROTO_H
#define SG_PROTO_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* sg-launch -> sgd registration, sent together with the listener fd. */

#define SG_PROTO_MAGIC 0x53474432u   /* "SGD2" */
#define SG_SOCK_MAX 108              /* sizeof(sockaddr_un.sun_path) */
#define SG_PRIO_MIN 1
#define SG_PRIO_MAX 10

/* What process creation does when the job is short of budget. */
enum sg_fork_policy {
    SG_FORK_BRIEF,    /* wait up to sgd -F, then EAGAIN */
    SG_FORK_WAIT,     /* wait up to T_max */
    SG_FORK_REJECT,   /* EAGAIN at once */
    SG_FORK_COUNT
};

struct sg_register {
    uint32_t magic;
    int32_t prio;     /* 1 (lowest) .. 10 (highest) */
    int64_t rate;     /* fixed refill in units/s; 0 = proportional share */
    char name[64];
    int32_t fork_policy;   /* enum sg_fork_policy */
    int32_t reserved;
};

const char *sg_fork_policy_name(int policy);
int sg_fork_policy_parse(const char *s);   /* -1 if unknown */

struct sg_ack {
    int32_t status;   /* 0 or -errno */
    uint32_t job_id;
};

void sg_default_sock(char *buf, size_t len);
int sg_send_fd(int sock, const void *buf, size_t len, int fd);
/* Returns bytes received; *fd is -1 if no descriptor came with them. */
ssize_t sg_recv_fd(int sock, void *buf, size_t len, int *fd);

#endif
