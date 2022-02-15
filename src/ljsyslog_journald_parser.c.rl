#define _POSIX_C_SOURCE 201805L

#include "ljsyslog.h"
#include "ljsyslog_journald_parser.h"

%%{

    machine client;

    access parser->;

    alphtype unsigned int;

    rfc3164_eol := (
        [\r\n\0] @{ 
            if (NULL != parser->log_cb) {
                ret = parser->log_cb(
                    /* tag = */ parser->tag,
                    /* tag_len = */ parser->tag_len,
                    /* facility = */ parser->facility,
                    /* severity = */ parser->severity,
                    /* pid = */ parser->pid,
                    /* msg = */ parser->msg,
                    /* msg_len = */ parser->msg_len,
                    /* user_data = */ parser->user_data
                ); 
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: parser->callbacks.default_cb returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
            }
            fgoto main; 
        }
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse eof marker at %c", __FILE__, __LINE__, __func__, *p); return -1; };


    action message_init {
        parser->msg_len = 0;
    }
    action message_copy {
        if (CONFIG_MAX_MSG_LEN <= parser->msg_len) {
            fhold; fgoto rfc3164_eol;
        } else {
            parser->msg[parser->msg_len++] = *p;
        }
    }
    message = 
        (any - '\r' - '\n') $message_init $message_copy 
        (any - '\r' - '\n')* $message_copy $eof{
            if (NULL != parser->log_cb) {
                ret = parser->log_cb(
                    /* tag = */ parser->tag,
                    /* tag_len = */ parser->tag_len,
                    /* facility = */ parser->facility,
                    /* severity = */ parser->severity,
                    /* pid = */ parser->pid,
                    /* msg = */ parser->msg,
                    /* msg_len = */ parser->msg_len,
                    /* user_data = */ parser->user_data
                ); 
                if (-1 == ret) {
                    syslog(LOG_ERR, "%s:%d:%s: parser->callbacks.default_cb returned -1", __FILE__, __LINE__, __func__);
                    return -1;
                }
            }
            fgoto main; 
        };

    # rfc3164 syslog message
    rfc3164_message := (
        space*
        message
        [\r\n\0] @{ fhold; fgoto rfc3164_eol; }
    ) 
    $eof{
        if (NULL != parser->log_cb) {
            ret = parser->log_cb(
                /* tag = */ parser->tag,
                /* tag_len = */ parser->tag_len,
                /* facility = */ parser->facility,
                /* severity = */ parser->severity,
                /* pid = */ parser->pid,
                /* msg = */ parser->msg,
                /* msg_len = */ parser->msg_len,
                /* user_data = */ parser->user_data
            ); 
            if (-1 == ret) {
                syslog(LOG_ERR, "%s:%d:%s: parser->callbacks.default_cb returned -1", __FILE__, __LINE__, __func__);
                return -1;
            }
        }
        fgoto main; 
    }
    $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse message at %c", __FILE__, __LINE__, __func__, *p); return -1; };

    # rfc3164 pid field
    rfc3164_pid := (
        ':' @{parser->pid = 0; fgoto rfc3164_message;} |
        ( 
            '[' 
            digit{1,8} >to{parser->pid = 0;} ${parser->pid *= 10; parser->pid += (*p - '0');} 
            ']:' @{ fgoto rfc3164_message;}
        )
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse process id at %c", __FILE__, __LINE__, __func__, *p); return -1; };


    # rfc3164 tag field (also contains the version in gwyos)
    action rfc3164_tag_unknown {
        memcpy(parser->tag, "unknown", strlen("unknown"));
        parser->tag_len = strlen("unknown");
    }
    action rfc3164_tag_copy {
        parser->tag[parser->tag_len++] = *p;
    }
    action rfc3164_tag_init {
        parser->tag_len = 0;
    }
    rfc3164_tag := (
        ( 
            '-' @rfc3164_tag_unknown 
            | ([A-Za-z0-9.+\-]{1,127} >to(rfc3164_tag_init) $rfc3164_tag_copy)
        )
        ( 
            '[' @{ fhold; fgoto rfc3164_pid;}
            | ':' @{fgoto rfc3164_message;}
        )
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse tag at %c, buf=%.*s", __FILE__, __LINE__, __func__, *p, buf_len, buf); return -1; };


    # rfc3164 date field ("Feb 15 06:03:44")
    rfc3164_date := (
        ('Jan' | 'Feb' | 'Mar' | 'Apr' | 'May' | 'Jun' | 'Jul' | 'Aug' | 'Sep' | 'Oct' | 'Nov' | 'Dec')
        ' '
        digit{1,2}
        ' '
        digit{2} ':' digit{2} ':' digit{2}
        ' ' @{fgoto rfc3164_tag;}
    ) $err{ syslog(LOG_WARNING, "%s:%d:%s: failed to parse date at %c", __FILE__, __LINE__, __func__, *p); return -1; };


    # rfc3164 priority field
    rfc3164_priority = (

        # clean out any remaining EOL markers
        ('\r' | '\n' | 0 | ' ')*

        '<' (
            # No number can follow a starting 0
            '0>' @{ parser->severity = 0; parser->facility = 0; parser->prival = 0; fgoto rfc3164_date; } |

            # These can be '1', '9', '10', '100', '191', but not '192', '200', '900'
            '1' @{ parser->prival = 1; } (

                # 1 is ok
                '>' @{parser->severity = 1; parser->facility = 0; fgoto rfc3164_date; } |

                # 10-18, 10X, 11X, ..., 18X
                [0-8] @{parser->prival = 10 + (*p - '0');} (
                        # 10-18 are OK
                        '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; } |

                        # 100-109, 110-119, ..., 180-189 are ok
                        [0-9] ${parser->prival *= 10; parser->prival += (*p - '0');} (
                            '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; }
                        ) 
                      ) |

                # 19, 19X
                '9' ${ parser->prival *= 10; parser->prival += (*p - '0'); } (

                    # 19 is valid
                    '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; } |

                    # 190, 191 are valid numbers
                    [0-1] ${ parser->prival *= 10; parser->prival += (*p - '0'); } (
                        '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; }
                    )

                    # 192-199 are not valid
                )
            ) |

            # 2-9, 20-29. 200 is too large.
            [2-9] @{parser->prival = (*p - '0'); } (
                '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; } |
                [0-9] ${parser->prival *= 10; parser->prival += (*p - '0'); } (
                    '>' @{d = div(parser->prival, 8); parser->severity = d.rem; parser->facility = d.quot; fgoto rfc3164_date; }
                )
            )
        )
    ) 
    >eof{
        return 0;
    }
    $err{ 
        syslog(LOG_WARNING, "%s:%d:%s: failed to parse priority: *p=0x%02x index=%ld state=%d buf=\"%s\"", 
            __FILE__, __LINE__, __func__, fc, p - buf, fcurs, buf); 
        return -1; 
    };
        

    # rfc3164 message main entry; always starts with a priority
    main := rfc3164_priority;


    write data;

}%%

int ljsyslog_journald_parser_init (
    struct ljsyslog_journald_parser_s * parser,
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
)
{
    %% write init;
    parser->user_data = user_data;
    parser->log_cb = log_cb;

    return 0;
}

int ljsyslog_journald_parser_parse (
    struct ljsyslog_journald_parser_s * parser,
    const uint8_t * const buf,
    const uint32_t buf_len
)
{
    int ret = 0;
    div_t d = {0};

    const uint8_t * p = buf;
    const uint8_t * pe = buf + buf_len;
    const uint8_t * eof = buf + buf_len;

    %% write exec;

    return 0;

}
