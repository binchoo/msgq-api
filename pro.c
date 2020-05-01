#include "ku_ipc.h"

extern void produceOne();
extern void produceMany();

void main(void) {
	produceOne();
	produceMany();
	produceMany();
}
