#ifndef __APPDB_H_
#define __APPDB_H_

#include "common.h"
#include "config.h"

typedef char UserID[MAX_USER_PROFILE_LEN];

int db_config_mismatch(AppConfig *config, int num_uids, UserID *uids);
int apply_config_to_db(AppConfig *config, int num_uids, UserID *uids);
int populate_user_ids(int *num_uids, UserID *uids);

#endif /* __APPDB_H_ */