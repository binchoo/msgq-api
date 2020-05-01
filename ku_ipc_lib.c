#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "ku_ipc.h"

#define DEV_NAME 	"ku_ipc"
#define IOCTL_MAGIC_NUM 'z'
#define IOCTL_START_NUM	0x89
#define IOCTL_ADDUSER 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+1, int)
#define IOCTL_SUBUSER 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+2, int)
#define IOCTL_ADDMSG 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+3, int)
#define IOCTL_SUBMSG 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+4, int)
#define IOCTL_PEEKMSG 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+5, int)
#define IOCTL_CHKEXCL 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+6, int)
#define IOCTL_AQLOCK 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+7, int)
#define IOCTL_RELLOCK 	_IOWR(IOCTL_MAGIC_NUM, IOCTL_START_NUM+8, int)

#define __chk_flag(TARGET, ASSERT) ((TARGET & ASSERT) == ASSERT)

struct query {
	int msgqid;
	struct msgbuf* msg;
	unsigned int bytes;
};

/*declare struct query my_query*/
#define __make_query(__msgqid, __msgbufp, __usrbytes) \
	struct query my_query = {	\
		.msgqid = __msgqid,	\
		.msg = __msgbufp,	\
		.bytes = __usrbytes	\
	}				\

int ku_msgget(int key, int msgflg);
int ku_msgclose(int msgqid);
int ku_msgsnd(int msgqid, void* msgp, int msgsz, int msgflg);
int ku_msgrcv(int msgqid, void* msgp, int msgsz, long msgtyp, int msgflg);

/* Test Functions */
struct testbuf {
	long type;
	char text[5];
};

void getQueueSuccess(int q, int msgflg) {
	printf("getQueueSuccess\n");
	assert(ku_msgget(q, msgflg) == q);
}
void getQueueFail(int q, int msgflg) {
	printf("getQueueFail\n");
	assert(ku_msgget(q, msgflg) == -1);
}
void sendMessageSuccess(int q, long type, char* message) {
	printf("sendMessageSuccess\n");
	struct msgbuf buffer = {
		.type = type,
	};
	strncpy(buffer.text, message, strlen(message) + 1);
	assert(ku_msgsnd(q, &buffer, sizeof(buffer), KU_IPC_NOWAIT) == 0);
}
void sendMessageFail(int q, long type, char* message) {
	printf("sendMessageFail\n");
	struct msgbuf buffer = {
		.type = type,
	};
	strncpy(buffer.text, message, strlen(message) + 1);
	assert(ku_msgsnd(q, &buffer, sizeof(buffer), KU_IPC_NOWAIT) == -1);
}
void readMessageSuccess(int q, long type) {
	printf("readMessageSuccess\n");
	struct msgbuf buffer = {
		.type = type,
	};
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_IPC_NOWAIT | KU_MSG_NOERROR) > 0);
	printf("Ive got (%s)\n", buffer.text);
}
void readMessageLoopingSuccess(int q, long type) {
	printf("readMessageLoopingSuccess\n");
	struct msgbuf buffer = {
		.type = type,
	};
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_MSG_NOERROR) > 0);
	printf("Ive got (%s)\n", buffer.text);
}
void readMessageFail(int q, long type) {
	printf("readMessageFail\n");
	struct msgbuf buffer = {
		.type = type,
	};
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_IPC_NOWAIT | KU_MSG_NOERROR) == -1);
}
void readMessageIntoSmallerSuccess(int q, long type) {
	printf("readMessageIntoSmallerSuccess\n");
	struct testbuf buffer;
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_IPC_NOWAIT | KU_MSG_NOERROR) > 0);
	printf("Ive got (%s)\n", buffer.text);
}
void readMessageIntoSmallerLoopingSuccess(int q, long type) {
	printf("readMessageIntoSmallerLoopingSuccess\n");
	struct testbuf buffer = {
		.type = type,
	};
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_MSG_NOERROR) > 0);
	printf("Ive got (%s)\n", buffer.text);
}
void readMessageIntoSmallerFail(int q, long type) {
	printf("readMessageIntoSmallerFail\n");
	struct testbuf buffer;
	assert(ku_msgrcv(q, &buffer, sizeof(buffer), type, KU_IPC_NOWAIT) == -1);
	printf("Ive got (%s)\n", buffer.text);
}
void closeQueueSuccess(int q) {
	printf("closeQueueSuccess\n");
	assert(ku_msgclose(q) == 0);
}
void closeQueueFail(int q) {
	printf("closeQueueFail\n");
	assert(ku_msgclose(q) == -1);
}

void selfSendReceiveTest() {

	int q = 8;
	int i;

	getQueueFail(q, 16);
	getQueueSuccess(q, KU_IPC_CREAT);
	getQueueFail(q, KU_IPC_EXCL);

	for(i = 0; i < 10; i++)
		sendMessageSuccess(q, 1, "hello world!");
	sendMessageFail(q, 1, "hello world! LAST");

	for(i = 0; i < 10; i++)
		readMessageSuccess(q, -1);
	readMessageFail(q, 1);
	readMessageFail(q, 0);

	sendMessageSuccess(q, 1, "hello world!");
	readMessageIntoSmallerFail(q, 1);
	readMessageIntoSmallerSuccess(q, 1);

	closeQueueSuccess(q);
	closeQueueFail(q);
}

void produceOne() {
	int q = 7;
	getQueueSuccess(q, KU_IPC_CREAT);
	sendMessageSuccess(q, 1, "producer produces");
	closeQueueSuccess(q);
}

void produceMany() {
	int q = 7;
	getQueueSuccess(q, KU_IPC_CREAT);
	sendMessageSuccess(q, 1, "producer produces");
	sleep(1);
	sendMessageSuccess(q, 1, "producer produces");
	sleep(1);
	sendMessageSuccess(q, 1, "producer produces");
	sleep(1);
	closeQueueSuccess(q);
}

void consumeOne() {
	int q = 7;
	getQueueSuccess(q, KU_IPC_CREAT);
	readMessageSuccess(q, 1);
	closeQueueSuccess(q);
}

void consumeMany() {
	int q = 7;
	getQueueSuccess(q, KU_IPC_CREAT);
	readMessageLoopingSuccess(q, 1);
	readMessageLoopingSuccess(q, 1);
	readMessageLoopingSuccess(q, 1);
	closeQueueSuccess(q);
}

void consumeManyForSmallerBuffer() {
	int q = 7;
	getQueueSuccess(q, KU_IPC_CREAT);
	readMessageIntoSmallerLoopingSuccess(q, 1);
	readMessageIntoSmallerLoopingSuccess(q, 1);
	readMessageIntoSmallerLoopingSuccess(q, 1);
	closeQueueSuccess(q);
}

/*
void main(void) {
	selfSendReceiveTest();
}
*/

/* Implementaions */
int ku_msgget(int key, int msgflg) {
	int retval = -1;
	int fd = open("/dev/"DEV_NAME, O_RDONLY);

	__make_query(key, NULL, 0);

	if (__chk_flag(msgflg, KU_IPC_EXCL)) {			
		if (ioctl(fd, IOCTL_CHKEXCL, &my_query))	//check exclusiveness
			if (0 == ioctl(fd, IOCTL_ADDUSER, &my_query))
				retval = key;
	}
	else if (__chk_flag(msgflg, KU_IPC_CREAT))
		if (0 == ioctl(fd, IOCTL_ADDUSER, &my_query))
			retval = key;

	close(fd);
	return retval;
}

int ku_msgclose(int msgqid) {
	int fd = open("/dev/"DEV_NAME, O_RDONLY);
	int retval;

	__make_query(msgqid, NULL, 0);

	retval = ioctl(fd, IOCTL_SUBUSER, &my_query);

	close(fd);
	return retval;
}

int ku_msgsnd(int msgqid, void* msgp, int msgsz, int msgflg) {
	int retval;
	int fd = open("/dev/"DEV_NAME, O_RDONLY);

	__make_query(msgqid, msgp, msgsz);

	if (!msgp)
		return -1;

	do {
		retval = ioctl(fd, IOCTL_ADDMSG, &my_query);
		if (__chk_flag(msgflg, KU_IPC_NOWAIT))
			break;
	} while (retval == -1);

	close(fd);
	return retval;
}

int ku_msgrcv(int msgqid, void* msgp, int msgsz, long msgtyp, int msgflg) {
	static int ERROR = -34;
	int retval = -1;
	int fd = open("/dev/"DEV_NAME, O_RDONLY);

	__make_query(msgqid, msgp, msgsz);

	if (!msgp)
		return -1;

	do {
		ioctl(fd, IOCTL_AQLOCK, &my_query);
		if (!ioctl(fd, IOCTL_PEEKMSG, &my_query) 		//if ubuf size is smaller
			&& !__chk_flag(msgflg, KU_MSG_NOERROR)) 	//and KU_MSG_NOEEROR not set.
			retval = ERROR;
		else
			retval = ioctl(fd, IOCTL_SUBMSG, &my_query); 	//retrieve msg

		ioctl(fd, IOCTL_RELLOCK, &my_query);
	} while (!__chk_flag(msgflg, KU_IPC_NOWAIT) && retval == -1); 	//keep looping, when KU_IPC_NOWAIT is not set.

	close(fd);
	if (retval == ERROR)
		retval = -1;

	return retval;
}
