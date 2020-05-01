#include "ku_ipc.h"

extern void consumeOne();
extern void consumeMany();
extern void consumeManyForSmallerBuffer();

void main(void) {
	consumeOne();
	consumeMany();
	consumeManyForSmallerBuffer();
}
