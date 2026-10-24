#ifndef __EVENTS_H_
#define __EVENTS_H_

#include <sys/event.h>

struct EventContext;

// returns 0 upon success, <0 as a command to stop, >0 as error
typedef int (*EventHandler)(int kq, struct EventContext *ctx, struct kevent *e, struct kevent *chlist, int *nchanges);

typedef struct EventContext {
  int id;
  short filter;
  EventHandler handle_event;
} EventContext;

#endif /* __EVENTS_H_ */
