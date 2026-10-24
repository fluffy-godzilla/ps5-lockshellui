#ifndef __CONFIG_H_
#define __CONFIG_H_

typedef struct AppNode {
  char app_id[32];
  int value;
  struct AppNode *next;
} AppNode;

typedef struct {
  AppNode *head;
  AppNode *tail;
} AppList;

typedef struct {
  int debug;
  int oneshot;
  int quiet;
} GeneralConfig;

typedef struct {
  GeneralConfig general;
  AppList visibility;
  AppList sort;
} AppConfig;

void insert_or_update_app(AppList *list, const char *app_id, int value);
void insert_or_update_app(AppList *list, const char *app_id, int value);
void sort_app_list(AppList *list);

int load_config(const char *filename, AppConfig *config);
int save_config(const char *filename, const AppConfig *config);

void free_config(AppConfig *config);

#endif /* __CONFIG_H_ */
