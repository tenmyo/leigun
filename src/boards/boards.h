#ifndef _BOARDS_H
#define _BOARDS_H
#include <stdint.h>
typedef struct Board {
	char *name;
	char *description;
	int (*createBoard) ();
	char *defaultconfig;
	void (*runBoard) ();
	struct Board *next;
} Board;

void Board_Register(Board * board);
Board *Board_Find(const char *name);
void Board_Create(Board * board);
void Boards_Init();

static inline void
Board_Run(Board * board)
{
	board->runBoard();
}

static inline char *
Board_DefaultConfig(Board * board)
{
	return board->defaultconfig;
}
#endif
