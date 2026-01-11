# Rawr Telegram bot

## Obtain resources
Download /resources directory and place to the root of the repo

## Build
1. Put stuff into credentials.h
```c
#define TGBOT_API_TOKEN "<BOT_TOKEN>"
#define TGBOT_ADMIN_CHAT_ID "12345678"
// Optional (webhook setup):
//#define TGBOT_WEBHOOK_URL "https://myserver.com/webhooks"
//#define TGBOT_WEBHOOK_SECRET "abcdefgh123456"
//#define TGBOT_WEBHOOK_PORT "6969"
```
2. Build and run
```sh
# Only 1 time:
gcc build.c -o build
./build
./out/rrtgbot
```

## Additional build flags:
```sh
# Optimized:
./build -O3
# Leak checking:
./build -sanitize
```
