#include <keyboard.h>

#define KEY_FLAG_IS_MODIFIER	(1)
typedef struct MatrixKey {
	uint16_t xk_code;
	char *row;
	char *col;
	int flags;
	int mod1;
	int mod2;
} MatrixKey;

void MatrixKeyboard_New(const char *name, Keyboard * keyboard, MatrixKey * keys, int nr_keys);
