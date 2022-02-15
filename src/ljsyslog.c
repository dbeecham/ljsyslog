// _GNU_SOURCE for pipe2()
#define _GNU_SOURCE

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
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "ljsyslog.h"
#include "ljsyslog_journald.h"
#include "ljsyslog_nats.h"


#define EPOLL_NUM_EVENTS 8

int ljsyslog_init (
    struct ljsyslog_s * ljsyslog
)
{

    int ret = 0;

    // initialize nats parser
    ret = ljsyslog_nats_parser_init(&ljsyslog->nats_parser, ljsyslog_nats_event_ping, ljsyslog);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_nats_parser_init returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // initialize journald parser
    ret = ljsyslog_journald_parser_init(&ljsyslog->journald_parser, ljsyslog_journald_event_log, ljsyslog);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_nats_parser_init returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // get hostname
    ret = gethostname(ljsyslog->hostname, sizeof(ljsyslog->hostname));
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: gethostname: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    ljsyslog->hostname_len = strlen(ljsyslog->hostname);

    // create epoll
    ljsyslog->epollfd = epoll_create1(EPOLL_CLOEXEC);
    if (-1 == ljsyslog->epollfd) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_create1: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Main thread needs a filled sigset on a signalfd to react to signals
    sigset_t sigset = {0};
    ret = sigfillset(&sigset);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sigfillset: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Create the signalfd
    ljsyslog->signalfd = signalfd(
        /* fd = */ -1,
        /* &sigset = */ &sigset,
        /* flags = */ SFD_NONBLOCK | SFD_CLOEXEC
    );
    if (-1 == ljsyslog->signalfd) {
        syslog(LOG_ERR, "%s:%d:%s: signalfd: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Block the signals
    ret = sigprocmask(
            /* how = */ SIG_BLOCK,
            /* &sigset = */ &sigset,
            /* &oldset = */ NULL
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: sigprocmask: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    // Add the signalfd to epoll
    ret = epoll_ctl(
        ljsyslog->epollfd,
        EPOLL_CTL_ADD,
        ljsyslog->signalfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLONESHOT,
            .data = {
                .fd = ljsyslog->signalfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}


static int ljsyslog_epoll_event_signalfd_sighup (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event,
    struct signalfd_siginfo * siginfo
)
{
    int ret = 0;
    syslog(LOG_INFO, "%s:%d:%s: caught SIGHUP", __FILE__, __LINE__, __func__);

    // Do something useful here maybe.

    // Re-arm the fd in epoll
    // Re-arm EPOLLONESHOT file descriptor in epoll
    ret = epoll_ctl(
        ljsyslog->epollfd,
        EPOLL_CTL_MOD,
        event->data.fd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET | EPOLLONESHOT,
            .data = {
                .fd = event->data.fd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d: epoll_ctl: %s", __func__, __LINE__, strerror(errno));
        return -1;
    }

    // We're done.
    return 0;
    (void)siginfo;
}


static int ljsyslog_epoll_event_signalfd_sigint (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event,
    struct signalfd_siginfo * siginfo
)
{
    syslog(LOG_INFO, "%s:%d:%s: caught SIGINT - exiting!", __FILE__, __LINE__, __func__);
    exit(EXIT_SUCCESS);
    (void)ljsyslog;
    (void)event;
    (void)siginfo;
}


static int ljsyslog_epoll_event_signalfd (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
)
{

    int bytes_read;
    struct signalfd_siginfo siginfo;

    bytes_read = read(event->data.fd, &siginfo, sizeof(struct signalfd_siginfo));
    if (-1 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: read: %s", __FILE__, __LINE__, __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (0 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: signalfd fd was closed - which is unexpected!", __FILE__, __LINE__, __func__);
        exit(EXIT_FAILURE);
    }

    // Dispatch on signal number
    if (SIGHUP == siginfo.ssi_signo)
        return ljsyslog_epoll_event_signalfd_sighup(ljsyslog, event, &siginfo);

    if (SIGINT == siginfo.ssi_signo)
        return ljsyslog_epoll_event_signalfd_sigint(ljsyslog, event, &siginfo);

    // window resize events - not interesting
    if (SIGWINCH == siginfo.ssi_signo)
        return 0;

    syslog(LOG_ERR, "%s:%d:%s: caught unknown signal %d - exiting", __FILE__, __LINE__, __func__, siginfo.ssi_signo);
    return -1;
}


static int ljsyslog_epoll_event_dispatch (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
)
{
    if (event->data.fd == ljsyslog->signalfd)
        return ljsyslog_epoll_event_signalfd(ljsyslog, event);

    if (event->data.fd == ljsyslog->journaldfd)
        return ljsyslog_epoll_event_journaldfd(ljsyslog, event);

    if (event->data.fd == ljsyslog->natsfd)
        return ljsyslog_epoll_event_natsfd(ljsyslog, event);

    syslog(LOG_WARNING, "%s:%d:%s: No match on epoll event.", __FILE__, __LINE__, __func__);
    return -1;
}


static int ljsyslog_epoll_handle_events (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event epoll_events[EPOLL_NUM_EVENTS],
    int ep_events_len
)
{
    int ret = 0;
    for (int i = 0; i < ep_events_len; i++) {
        ret = ljsyslog_epoll_event_dispatch(ljsyslog, &epoll_events[i]);
        if (0 != ret) {
            return ret;
        }
    }
    return 0;
}


int main (
    const int argc,
    const char *argv[]
)
{

    int ret = 0;

    openlog("ljsyslog", LOG_CONS | LOG_PID, LOG_USER);

    struct ljsyslog_s ljsyslog = {
        .sentinel = 8090
    };
    ret = ljsyslog_init(&ljsyslog);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_init returned %d", __FILE__, __LINE__, __func__, ret);
        exit(EXIT_FAILURE);
    }


    // connect to journald socket
    ret = ljsyslog_journald_listen(&ljsyslog);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_tcp_server_start returned %d", __FILE__, __LINE__, __func__, ret);
        exit(EXIT_FAILURE);
    }

    ret = ljsyslog_nats_connect(&ljsyslog);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_nats_connect returned -1", __FILE__, __LINE__, __func__);
        return -1;
    }

    // Time for the epoll_wait loop
    int ep_events_len = 0;
    struct epoll_event ep_events[EPOLL_NUM_EVENTS];
    for (ep_events_len = epoll_wait(ljsyslog.epollfd, ep_events, EPOLL_NUM_EVENTS, -1);
         ep_events_len > 0 || (-1 == ep_events_len && EINTR == errno);
         ep_events_len = epoll_wait(ljsyslog.epollfd, ep_events, EPOLL_NUM_EVENTS, -1))
    {
        ret = ljsyslog_epoll_handle_events(&ljsyslog, ep_events, ep_events_len);
        if (-1 == ret) {
            break;
        }
    }
    if (-1 == ep_events_len) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_wait: %s", __FILE__, __LINE__, __func__, strerror(errno));
        exit(EXIT_FAILURE);
    }


    exit(EXIT_SUCCESS);	
    (void)argc;
    (void)argv;
}
