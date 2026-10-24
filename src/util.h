#ifndef __UTIL_H_
#define __UTIL_H_

void log_enable(int enable);
void log_printf(const char *fmt, ...);

void notify_enable(int enable);
void notify(const char *fmt, ...);

void rename_service_thread(const char *name);
int terminate_instances(const char *name);
void shellui_reload(void);

#endif /* __UTIL_H_ */
