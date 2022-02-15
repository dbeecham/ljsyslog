#pragma once

#include <sys/epoll.h>

int ljsyslog_journald_listen(struct ljsyslog_s * ljsyslog);
int ljsyslog_epoll_event_journaldfd (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
);

int ljsyslog_journald_event_log (
    const char * tag,
    const uint32_t tag_len,
    const uint32_t facility,
    const uint32_t severity,
    const uint32_t pid,
    const char * msg,
    const uint32_t msg_len,
    void * user_data
);
