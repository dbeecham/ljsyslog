// Try to not define _GNU_SOURCE or _DEFAULT_SOURCE, since those enable
// glibc-specific features. Being able to compile to e.g. musl or uclibc
// makes porting to embedded linux systems much easier (and generally
// pressures the programmer into stricter and better programming practices).
#define _POSIX_C_SOURCE 201805L

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>

#include "ljsyslog.h"
#include "ljsyslog_journald.h"
#include "ljsyslog_nats.h"

#define EPOLL_NUM_EVENTS 8


static const char * ljsyslog_facility_str (
    const uint8_t facility
)
{
    switch (facility) {

        case 0:
            return "kern";

        case 1:
            return "user";

        case 2:
            return "mail";

        case 3:
            return "daemon";

        case 4:
            return "auth";

        case 5:
            return "syslog";

        case 6:
            return "lpr";

        case 7:
            return "news";

        case 8:
            return "uucp";

        case 9:
            return "clock";

        case 10:
            return "authpriv";

        case 11:
            return "ftp";

        case 12:
            return "ntp";

        case 13:
            return "audit";

        case 14:
            return "alert";

        case 15:
            return "clock2";

        case 16:
            return "local0";

        case 17:
            return "local1";

        case 18:
            return "local2";

        case 19:
            return "local3";

        case 20:
            return "local4";

        case 21:
            return "local5";

        case 22:
            return "local6";

        case 23:
            return "local7";

        default:
            return "unknown";
    }
}


static const char * ljsyslog_severity_str (
    const uint8_t severity
)
{
    switch (severity) {
        case LOG_EMERG:
            return "emerg";

        case LOG_ALERT:
            return "alert";

        case LOG_ERR:
            return "err";

        case LOG_WARNING:
            return "warning";

        case LOG_NOTICE:
            return "notice";

        case LOG_INFO:
            return "info";

        case LOG_DEBUG:
            return "debug";

        default:
            return "unknown";
    }
}


int ljsyslog_journald_event_log (
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
    int ret = 0;
    struct ljsyslog_s * ljsyslog = user_data;

    char topic[512];
    int topic_len = 0;

    topic_len = snprintf(topic, sizeof(topic), "%.*s.%.*s.%s.out",
        ljsyslog->hostname_len, ljsyslog->hostname,
        tag_len, tag,
        ljsyslog_severity_str(severity)
    );
    if (-1 == topic_len) {
        syslog(LOG_ERR, "%s:%d:%s: snprintf returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    ret = pub(
        /* ljsyslog = */ ljsyslog, 
        /* topic = */ topic,
        /* topic_len = */ topic_len,
        /* rt = */ NULL,
        /* rt_len = */ 0,
        /* payload = */ msg,
        /* payload_len = */ msg_len
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: pub returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int ljsyslog_epoll_event_journaldfd (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
)
{
    int ret = 0;
    char buf[65535];
    int bytes_read;

    bytes_read = read(event->data.fd, buf, sizeof(buf));
    if (-1 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: read: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // parse syslog and send it to nats
    ret = ljsyslog_journald_parser_parse(&ljsyslog->journald_parser, buf, bytes_read);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_journald_parser_parse returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // re-arm journaldfd on epoll
    ret = epoll_ctl(
        ljsyslog->epollfd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = event->data
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}

int ljsyslog_journald_listen (
    struct ljsyslog_s * ljsyslog
)
{
    int ret = 0;

    unlink("/run/systemd/journal/syslog");

    ljsyslog->journaldfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (-1 == ljsyslog->journaldfd) {
        syslog(LOG_ERR, "%s:%d:%s: socket: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    ret = bind(
        /* fd = */ ljsyslog->journaldfd,
        /* sockaddr = */ (struct sockaddr *)&(struct sockaddr_un) {
            .sun_family = AF_UNIX,
            .sun_path = "/run/systemd/journal/syslog"
        },
        /* sockaddr_len = */ sizeof(struct sockaddr_un)
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: bind: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Add the fd to epoll
    ret = epoll_ctl(
        ljsyslog->epollfd,
        EPOLL_CTL_ADD,
        ljsyslog->journaldfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = ljsyslog->journaldfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }


    return 0;
}
