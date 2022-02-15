// Try to not define _GNU_SOURCE or _DEFAULT_SOURCE, since those enable
// glibc-specific features. Being able to compile to e.g. musl or uclibc
// makes porting to embedded linux systems much easier (and generally
// pressures the programmer into stricter and better programming practices).
#define _POSIX_C_SOURCE 201805L

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
#include <netinet/in.h>
#include <netdb.h>

#include "ljsyslog.h"
#include "ljsyslog_nats.h"
#include "ljsyslog_nats_parser.h"


int pub (
    struct ljsyslog_s * ljsyslog,
    const char * topic,
    const uint32_t topic_len,
    const char * rt,
    const uint32_t rt_len,
    const char * payload,
    const uint32_t payload_len
)
{
    int buf_len = 0;
    int bytes_written = 0;
    char buf[8192];

    if (8192 < payload_len) {
        syslog(LOG_ERR, "%s:%d:%s: payload_len is too big", __FILE__, __LINE__, __func__);
        return -1;
    }

    if (0 == rt_len) {
        buf_len = snprintf(buf, 8192, "PUB %.*s %d\r\n", 
                topic_len, topic, payload_len);
    } else {
        buf_len = snprintf(buf, 8192, "PUB %.*s %.*s %d\r\n", 
                topic_len, topic, rt_len, rt, payload_len);
    }

    if (0!= payload_len) {
        memcpy(buf + buf_len, payload, payload_len);
        buf_len += payload_len;
    }
    memcpy(buf + buf_len, "\r\n", 2);
    buf_len += 2;

    bytes_written = write(ljsyslog->natsfd, buf, buf_len);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: connection closed", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (buf_len != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write!", __FILE__, __LINE__, __func__);
        return -1;
    }

    return 0;
}


int ljsyslog_nats_event_ping (
    struct ljsyslog_nats_parser_s * parser,
    void * user_data
)
{
    int bytes_written = 0;
    int ret = 0;

    struct ljsyslog_s * ljsyslog = user_data;
    if (8090 != ljsyslog->sentinel) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog sentinel is wrong!", __FILE__, __LINE__, __func__);
        return -1;
    }

    bytes_written = write(ljsyslog->natsfd, "PONG\r\n", 6);
    if (-1 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: write: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: wrote 0 bytes! connection dead?", __FILE__, __LINE__, __func__);
        return -1;
    }
    if (6 != bytes_written) {
        syslog(LOG_ERR, "%s:%d:%s: partial write of %d bytes!", __FILE__, __LINE__, __func__, bytes_written);
        return -1;
    }

    return 0;
    (void)parser;
}


int ljsyslog_epoll_event_natsfd (
    struct ljsyslog_s * ljsyslog,
    struct epoll_event * event
)
{

    int ret = 0;
    int bytes_read = 0;
    char buf[NATS_BUF_LEN];


    bytes_read = read(event->data.fd, buf, NATS_BUF_LEN);
    if (-1 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: read: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }
    if (0 == bytes_read) {
        syslog(LOG_ERR, "%s:%d:%s: nats closed connection!", __FILE__, __LINE__, __func__);
        return -1;
    }

    // Parse the NATS data; one of the callbacks (named *_cb) will be called on
    // a successful parse.
    ret = ljsyslog_nats_parser_parse(&ljsyslog->nats_parser, buf, bytes_read);
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: ljsyslog_nats_parser_parse returned %d", __FILE__, __LINE__, __func__, ret);
        return -1;
    }

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


int ljsyslog_nats_connect (
    struct ljsyslog_s * ljsyslog
)
{

    int ret = 0;
    struct addrinfo *servinfo, *p;

    ret = getaddrinfo(
        /* host = */ CONFIG_NATS_HOST,
        /* port = */ CONFIG_NATS_PORT, 
        /* hints = */ &(struct addrinfo) {
            .ai_family = AF_UNSPEC,
            .ai_socktype = SOCK_STREAM
        },
        /* servinfo = */ &servinfo
    );
    if (0 != ret) {
        syslog(LOG_ERR, "%s:%d:%s: getaddrinfo: %s", __FILE__, __LINE__, __func__, gai_strerror(ret));
        return -1;
    }

    // Loop over the results from getaddrinfo and try to bind them up.
    for (p = servinfo; p != NULL; p = p->ai_next) {

        // Create a socket
        ljsyslog->natsfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (-1 == ljsyslog->natsfd) {
            syslog(LOG_WARNING, "%s:%d:%s: socket: %s", __FILE__, __LINE__, __func__, strerror(errno));
            // let's try the next entry...
            continue;
        }

        // Bind the socket to the port
        ret = connect(ljsyslog->natsfd, p->ai_addr, p->ai_addrlen);
        if (-1 == ret) {
            // Ok, we couldn't connect to this address result - close this
            // socket and try the next hit from getaddrinfo.
            syslog(LOG_WARNING, "%s:%d:%s: connect: %s", __FILE__, __LINE__, __func__, strerror(errno));
            close(ljsyslog->natsfd);
            continue;
        }

        // If we get here, it means that we've successfully bound up a tcp
        // socket. We don't need to try any more results from getaddrinfo.
        // Break out of the loop.
        break;
    }

    // Remember to free up the servinfo data!
    freeaddrinfo(servinfo);

    // If p is NULL, it means that the above loop went through all of the
    // results from getaddrinfo and never broke out of the loop - so we have no
    // valid socket.
    if (NULL == p) {
        syslog(LOG_ERR, "%s:%d:%s: failed to connect to any address", __FILE__, __LINE__, __func__);
        return -1;
    }

    // Add the fd to epoll
    ret = epoll_ctl(
        ljsyslog->epollfd,
        EPOLL_CTL_ADD,
        ljsyslog->natsfd,
        &(struct epoll_event){
            .events = EPOLLIN | EPOLLONESHOT,
            .data = {
                .fd = ljsyslog->natsfd
            }
        }
    );
    if (-1 == ret) {
        syslog(LOG_ERR, "%s:%d:%s: epoll_ctl: %s", __FILE__, __LINE__, __func__, strerror(errno));
        return -1;
    }

    return 0;
}
