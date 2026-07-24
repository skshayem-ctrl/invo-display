#pragma once

void invo_debug_init(void);
void invo_debug_log(const char *fmt, ...);

#define INVO_DBG(fmt, ...) invo_debug_log(fmt, ##__VA_ARGS__)
