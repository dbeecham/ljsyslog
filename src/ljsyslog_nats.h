#pragma once

#include <sys/epoll.h>

#include "ljsyslog.h"

int pub (
    struct ljsyslog_s * ljsyslog,
    const char * topic,
    const uint32_t topic_len,
    const char * rt,
    const uint32_t rt_len,
    const char * payload,
    const uint32_t payload_len
);

int ljsyslog_nats_connect (
    struct ljsyslog_s * ljsyslog
);

int ljsyslog_epoll_event_natsfd (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
);

int ljsyslog_nats_event_ping (
    struct ljsyslog_nats_parser_s * parser,
    void * user_data
);
