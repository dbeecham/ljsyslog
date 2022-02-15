#define _POSIX_C_SOURCE 201805L

#include <syslog.h>
#include <stdio.h>

#include "ljsyslog_nats_parser.h"

%%{

    machine nats;

    access parser->;

    loop := (
        'PING\r\n' @{ parser->ping_cb(parser, parser->user_data); fgoto loop; } |
        '+OK\r\n' @{ fgoto loop; }
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse nats at %c\n", __FILE__, __LINE__, __func__, *p); fgoto loop; };

    info = (
        'INFO {' 
        (any - '\n' - '\r' - '}')+ 
        '}' 
        ' '? 
        '\r\n' @{fgoto loop;}
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse info at %c\n", __FILE__, __LINE__, __func__, *p); fgoto main; };

    main := info;

    write data;

}%%

int ljsyslog_nats_parser_init (
    struct ljsyslog_nats_parser_s * parser,
    int (*ping_cb)(struct ljsyslog_nats_parser_s * parser, void * user_data),
    void * user_data
)
{
    %% write init;
    parser->user_data = user_data;
    parser->ping_cb = ping_cb;
    return 0;
}

int ljsyslog_nats_parser_parse (
    struct ljsyslog_nats_parser_s * parser,
    const char * const buf,
    const int buf_len
)
{

    const char * p = buf;
    const char * pe = buf + buf_len;
    const char * eof = 0;

    %% write exec;

    return 0;
}
