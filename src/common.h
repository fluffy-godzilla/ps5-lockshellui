#ifndef __COMMON_H_
#define __COMMON_H_

#define MAX_NUM_USER_PROFILES 16
#define MAX_USER_PROFILE_LEN 32
#define MAX_KQUEUE_EVENTS 16
#define TIMER_COOLDOWN_MS 1000
#define TIMER_POLL_MS 500

#if defined(PS5_PAYLOAD_SDK)
#define DB_PATH "/system_data/priv/mms/app.db"
#define CONFIG_DIR "/data/lockshellui"
#define CONFIG_PATH CONFIG_DIR"/config.ini"
#define ELF_NAME "lockshellui.elf"
#else
#define DB_PATH "/tmp/app.db"
#define CONFIG_DIR "/tmp"
#define CONFIG_PATH CONFIG_DIR"/lockshellui.ini"
#define ELF_NAME "lockshellui"
#endif

#endif /* __COMMON_H_ */
