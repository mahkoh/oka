#pragma once

struct delegate {
    void (*run)(struct delegate *);
};

struct delegator;

struct delegator *delegator_new(void);
int delegator_fd(struct delegator *);
void delegator_run(struct delegator *);
void delegator_free(struct delegator *);
void delegator_delegate(struct delegator *, struct delegate *);
void delegator_delegate_sync(struct delegator *, struct delegate *);

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
