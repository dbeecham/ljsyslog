#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <assert.h>
#include <stdint.h>
#include <syslog.h>

#include "ljsyslog_journald_parser.h"

void test_ljsyslog_journald_parser_init() {
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};
    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ NULL,
        /* context = */ NULL
    );
    if (-1 == ret) {
        printf("%s:%d:%s: ljsyslog_journald_parser_init returned -1\n", __FILE__, __LINE__, __func__);
        exit(EXIT_FAILURE);
    }
    
    printf("%s: OK\n", __func__);
}


int test_ljsyslog_journald_parser_parses_basic_log_cb (
    const char * tag,
    const uint32_t tag_len,
    const uint32_t facility,
    const uint32_t severity,
    const uint32_t pid,
    const char * msg,
    const uint32_t msg_len,
    void * user_data
)
{
    int * cb_called = user_data;
    *cb_called += 1;

    assert(72778 == pid);
    assert(3 == facility);
    assert(6 == severity);

    return 0;
}

void test_ljsyslog_journald_parser_parses_basic_log() {
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};
    int cb_called = 0;

    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ test_ljsyslog_journald_parser_parses_basic_log_cb,
        /* user_data = */ &cb_called
    );
    assert(0 == ret);

    char buf[] = "<30>Feb 15 06:03:45 nebula[72778]: time=\"2022-02-15T06:03:45+01:00\" level=error msg=\"Failed to send handshake message\" error=\"sendto: network is unreachable\" handshake=\"map[stage:1 style:ix_psk0]\" initiatorIndex=1378462437 udpAddr=\"34.243.111.178:123\" vpnIp=172.24.0.3";
    int buf_len = strlen(buf);

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    assert(0 == ret);
    assert(1 == cb_called);

    printf("%s: OK\n", __func__);
}


void test_ljsyslog_journald_parser_parses_multiple_logs() {
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};
    int cb_called = 0;

    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ test_ljsyslog_journald_parser_parses_basic_log_cb,
        /* user_data = */ &cb_called
    );
    assert(0 == ret);

    char buf[] = "<30>Feb 15 06:03:45 nebula[72778]: time=\"2022-02-15T06:03:45+01:00\" level=error msg=\"Failed to send handshake message\" error=\"sendto: network is unreachable\" handshake=\"map[stage:1 style:ix_psk0]\" initiatorIndex=1378462437 udpAddr=\"34.243.111.178:123\" vpnIp=172.24.0.3";
    int buf_len = strlen(buf);

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    assert(0 == ret);
    assert(1 == cb_called);

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    assert(0 == ret);
    assert(2 == cb_called);

    printf("%s: OK\n", __func__);
}


int test_ljsyslog_journald_parser_parses_basic_log_1_cb (
    const char * tag,
    const uint32_t tag_len,
    const uint32_t facility,
    const uint32_t severity,
    const uint32_t pid,
    const char * msg,
    const uint32_t msg_len,
    void * user_data
)
{
    int * cb_called = user_data;
    *cb_called += 1;

    assert(0 == pid);
    assert(10 == facility);
    assert(5 == severity);

    return 0;
}

void test_ljsyslog_journald_parser_parses_basic_log_1() {
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};
    int cb_called = 0;

    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ test_ljsyslog_journald_parser_parses_basic_log_1_cb,
        /* user_data = */ &cb_called
    );
    assert(0 == ret);

    char buf[] = "<85>Feb 15 06:59:08 sudo:      dbe : TTY=pts/11 ; PWD=/root ; USER=root ; COMMAND=/run/current-system/sw/bin/bash\x00";
    int buf_len = strlen(buf) + 1;

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    if (-1 == ret) {
        printf("%s:%d:%s: ljsyslog_journald_parser_parse returned -1\n", __FILE__, __LINE__, __func__);
        exit(EXIT_FAILURE);
    }

    assert(1 == cb_called);

    printf("%s: OK\n", __func__);
}


struct test_ljsyslog_journald_parser_facility_severity_s {
    int cb_called;
    int facility;
    int severity;
};

int test_ljsyslog_journald_parser_facility_severity_cb (
    const char * tag,
    const uint32_t tag_len,
    const uint32_t facility,
    const uint32_t severity,
    const uint32_t pid,
    const char * msg,
    const uint32_t msg_len,
    void * user_data
)
{
    struct test_ljsyslog_journald_parser_facility_severity_s * cb_info = user_data;

    assert(72778 == pid);

    cb_info->cb_called += 1;
    cb_info->severity = severity;
    cb_info->facility = facility;

    return 0;
}

void test_ljsyslog_journald_parser_facility_severity (
    int facility,
    int severity
)
{
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};
    struct test_ljsyslog_journald_parser_facility_severity_s cb_info = {0};

    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ test_ljsyslog_journald_parser_facility_severity_cb,
        /* context = */ &cb_info
    );
    assert(0 == ret);

    char buf[512];
    snprintf(buf, 512, 
        "<%d>Feb 15 06:03:45 nebula[72778]: time=\"2022-02-15T06:03:45+01:00\" level=error msg=\"Failed to send handshake message\" error=\"sendto: network is unreachable\" handshake=\"map[stage:1 style:ix_psk0]\" initiatorIndex=1378462437 udpAddr=\"34.243.111.178:123\" vpnIp=172.24.0.3\n",
        facility * 8 + severity
    );

    int buf_len = strlen(buf);

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    assert(0 == ret);
    assert(1 == cb_info.cb_called);
    assert(facility == cb_info.facility);
    assert(severity == cb_info.severity);
}


void test_ljsyslog_journald_parser_parses_facilities_and_severities() {
    for (int severity = 0; severity <= 7; severity += 1) {
        for (int facility = 0; facility <= 23; facility += 1) {
            test_ljsyslog_journald_parser_facility_severity(facility, severity);
        }
    }

    printf("%s: OK\n", __func__);
}



int test_ljsyslog_journald_parser_does_not_parse_invalid_prival_cb (
    const char * tag,
    const uint32_t tag_len,
    const uint32_t facility,
    const uint32_t severity,
    const uint32_t pid,
    const char * msg,
    const uint32_t msg_len,
    void * user_data
)
{
    assert(0);
}

int test_ljsyslog_journald_parser_does_not_parse_invalid_prival (
    int prival
) 
{
    int ret = 0;
    struct ljsyslog_journald_parser_s parser = {0};

    ret = ljsyslog_journald_parser_init(
        /* parser = */ &parser,
        /* log_cb = */ test_ljsyslog_journald_parser_does_not_parse_invalid_prival_cb,
        /* context = */ NULL
    );
    assert(0 == ret);

    char buf[512];
    snprintf(buf, 512, 
        "<%d>Feb 15 06:03:45 nebula[72778]: time=\"2022-02-15T06:03:45+01:00\" level=error msg=\"Failed to send handshake message\" error=\"sendto: network is unreachable\" handshake=\"map[stage:1 style:ix_psk0]\" initiatorIndex=1378462437 udpAddr=\"34.243.111.178:123\" vpnIp=172.24.0.3",
        prival
    );
    int buf_len = strlen(buf);

    ret = ljsyslog_journald_parser_parse(
        /* parser = */ &parser,
        /* buf = */ buf,
        /* buf_len = */ buf_len
    );
    assert(-1 == ret);

    return 0;
}


void test_ljsyslog_journald_parser_does_not_parse_invalid_privals() {
    test_ljsyslog_journald_parser_does_not_parse_invalid_prival(-1);
    test_ljsyslog_journald_parser_does_not_parse_invalid_prival(-2);
    for (int i = 192; i < 1024; i++) {
        test_ljsyslog_journald_parser_does_not_parse_invalid_prival(i);
    }

    printf("%s: OK\n", __func__);
}


int main (
    int argc,
    char const* argv[]
)
{
    openlog("test_driver", LOG_CONS, LOG_USER);
    
    test_ljsyslog_journald_parser_init();
    test_ljsyslog_journald_parser_parses_basic_log();
    test_ljsyslog_journald_parser_parses_basic_log_1();
    test_ljsyslog_journald_parser_parses_multiple_logs();
    test_ljsyslog_journald_parser_parses_facilities_and_severities();
    test_ljsyslog_journald_parser_does_not_parse_invalid_privals();

    return 0;
}
