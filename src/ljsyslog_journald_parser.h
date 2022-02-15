#pragma once

#include <stdint.h>

#include "ljsyslog_config.h"

struct ljsyslog_journald_parser_s {
    int cs;
    int fd;
    int prival;
    int severity;
    int facility;

    int (*log_cb)(
        const char * tag,
        const uint32_t tag_len,
        const uint32_t facility,
        const uint32_t severity,
        const uint32_t pid,
        const char * msg,
        const uint32_t msg_len,
        void * user_data
    );
    void * user_data;

    char msg[CONFIG_MAX_MSG_LEN];
    uint32_t msg_len;

    char tag[CONFIG_MAX_TAG_LEN];
    uint32_t tag_len;

    int pid;
};

int ljsyslog_journald_parser_init (
    struct ljsyslog_journald_parser_s * log,
    int (*log_cb)(
        const char * tag,
        const uint32_t tag_len,
        const uint32_t facility,
        const uint32_t severity,
        const uint32_t pid,
        const char * msg,
        const uint32_t msg_len,
        void * user_data
    ),
    void * user_data
);

int ljsyslog_journald_parser_parse (
    struct ljsyslog_journald_parser_s * log,
    const uint8_t * const buf,
    const uint32_t buf_len
);
