#pragma once

#define EPOLL_NUM_EVENTS 8
#define NATS_BUF_LEN 2048

#ifndef CONFIG_NATS_HOST
#define CONFIG_NATS_HOST "127.0.0.1"
#endif

#ifndef CONFIG_NATS_PORT
#define CONFIG_NATS_PORT "4222"
#endif

#ifndef CONFIG_MAX_MSG_LEN 
#define CONFIG_MAX_MSG_LEN 65536
#endif

#ifndef CONFIG_MAX_HOSTNAME_LEN
#define CONFIG_MAX_HOSTNAME_LEN 128
#endif

// changing this requires changes in the parser; will make this editable soon...
#ifndef CONFIG_MAX_TAG_LEN
#define CONFIG_MAX_TAG_LEN 128
#endif
