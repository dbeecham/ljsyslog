#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/stat.h>
#include <pthread.h>

#include "ljsyslog_config.h"
#include "ljsyslog_nats_parser.h"
#include "ljsyslog_journald_parser.h"

struct ljsyslog_s {
    int sentinel;
    int epollfd;
    int journaldfd;
    int natsfd;
    struct ljsyslog_nats_parser_s nats_parser;
    struct ljsyslog_journald_parser_s journald_parser;
    int signalfd;
    char hostname[64];
    uint32_t hostname_len;
};
