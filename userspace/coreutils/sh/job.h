#ifndef JOB_H
#define JOB_H

#include "prog.h"
#include "list.h"

enum job_state {
    JOB_STOPPED,
    JOB_RUNNING,
};

struct job {
    pid_t pgrp;
    char *name;

    enum job_state state;

    list_head_t prog_list;

    list_node_t job_entry;
};

#define JOB_INIT(job) \
    { \
        .prog_list = LIST_HEAD_INIT((job).prog_list), \
        .job_entry = LIST_NODE_INIT((job).job_entry), \
    }

static inline void job_init(struct job *job)
{
    *job = (struct job)JOB_INIT(*job);
}

extern list_head_t job_list;

struct job *job_pid_find(pid_t pid, int *job_id, struct prog_desc **prog);
struct job *job_pgrp_find(pid_t pid);
void job_add_prog(struct job *job, struct prog_desc *desc);

void job_clear(struct job *);

int job_is_empty(struct job *);
int job_is_simple_builtin(struct job *);
int job_builtin_run(struct job *);

void job_first_start(struct job *);
void job_kill(struct job *);

void job_stop(struct job *);
void job_start(struct job *);

void job_add(struct job *);
void job_remove(struct job *);

int job_output_list(struct prog_desc *desc);
int job_fg(struct prog_desc *desc);
int job_bg(struct prog_desc *desc);

void job_update_background(void);
void job_make_forground(struct job *);

#endif
