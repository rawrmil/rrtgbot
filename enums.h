#ifndef ENUMS_H
#define ENUMS_H

#define X_TGB_CHAT_MODE \
	X(TGB_CM_DEFAULT) \
	X(TGB_CM_ECHO) \
	X(TGB_CM_FOO) \
	X(TGB_CM_LENGTH)

#define X(name_) name_,
typedef enum TGB_ChatMode {
	X_TGB_CHAT_MODE
} TGB_ChatMode;
#undef X

extern char* TGB_CHAT_MODE_NAMES[];

#endif /* ENUMS_H */

#ifdef ENUMS_IMPLEMENTATION

#define X(name_) #name_,
char* TGB_CHAT_MODE_NAMES[] = { X_TGB_CHAT_MODE };
#undef X

#endif /* ENUMS_IMPLEMENTATION */
