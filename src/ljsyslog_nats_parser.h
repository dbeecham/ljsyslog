#pragma once

struct ljsyslog_nats_parser_s {
    int cs;
    int (*ping_cb)(struct ljsyslog_nats_parser_s * parser, void * user_data);
    void * user_data;
};

int ljsyslog_nats_parser_init (
    struct ljsyslog_nats_parser_s * parser,
    int (*ping_cb)(struct ljsyslog_nats_parser_s * parser, void * user_data),
    void * user_data
);

int ljsyslog_nats_parser_parse (
    struct ljsyslog_nats_parser_s * parser,
    const char * const buf,
    const int buf_len
);
