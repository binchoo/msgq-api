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

/*produce struct query my_query*/
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


int post() {
	int fd = open("/dev/"DEV_NAME, O_RDWR);
	int ret;

	struct msgbuf msg = {
		.type = 3,
		.text = "hello world!\n",
	};

	struct query my_query = {
		.msgqid = 3,
		.msg = &msg,
	};

	ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
	printf("check_q==true >> %d\n", ret);
	ret = ioctl(fd, IOCTL_ADDUSER, &my_query);
	printf("get_q==3 >> %d\n", ret);

	ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
	printf("check_q==false >> %d\n", ret);
	ret = ioctl(fd, IOCTL_ADDMSG, &my_query);
	printf("msgadd==0 >> %d\n", ret);

	ret = ioctl(fd, IOCTL_SUBMSG, &my_query);
	printf("msgsub==0 >> %d\n", ret);
	printf("msgsub_type==3 >> %ld\n", my_query.msg->type);
	printf("msgsub_text==hello world! >> %s\n", my_query.msg->text);

	for (int i = 0; i < 3; i++) {
		ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
		printf("check_q==false >> %d\n", ret);
		ret = ioctl(fd, IOCTL_ADDMSG, &my_query);
		printf("msgadd==0 >> %d\n", ret);
	}

	for (int i = 0; i < 3; i++) {
		ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
		printf("check_q==false >> %d\n", ret);
		ret = ioctl(fd, IOCTL_SUBMSG, &my_query);
		printf("msgsub==0 >> %d\n", ret);
		printf("msgsub_type==3 >> %ld\n", my_query.msg->type);
		printf("msgsub_text==hello world! >> %s\n", my_query.msg->text);
	}

	ret = ioctl(fd, IOCTL_SUBMSG, &my_query);
	printf("msgsub==-1 >> %d\n", ret);
	ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
	printf("check_q==false >> %d\n", ret);

	ret = ioctl(fd, IOCTL_SUBUSER, &my_query);
	printf("close==0 >> %d\n", ret);
	ret = ioctl(fd, IOCTL_CHKEXCL, &my_query);
	printf("check_q==true >> %d\n", ret);

	close(fd);
}

void snd_rcv() {
	int q = ku_msgget(8, KU_IPC_CREAT);
	int ret;

	struct msgbuf rbuf;
	struct msgbuf wbuf = {
		.type = 3,
		.text = "hello my friend!",
	};

	assert(q == 8);
	ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT); //send type(3) msg

	assert(ret == 0);
	wbuf.type = 1;
	wbuf.text[0] = 'X';
	ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT); //send diffrent type(1) msg

	assert(ret == 0);
	assert(__chk_flag(KU_IPC_NOWAIT, KU_IPC_NOWAIT));
	ret = ku_msgrcv(9, &rbuf, sizeof(rbuf), -3, KU_IPC_NOWAIT); //wrong queue number
	
	assert(ret == -1);
	printf("test1\n");
	ret = ku_msgrcv(q, &rbuf, sizeof(rbuf), -3, KU_IPC_NOWAIT); //good get type 3 msg
	
	assert(ret > 0);
	printf("rbuf[%d] = %s\n", ret, rbuf.text); 
	printf("test2\n");
	ret = ku_msgrcv(q, &rbuf, 5, 1, KU_IPC_NOWAIT); //fail get type 1 with small buffer, NO_KU_MSG_NOERROR

	assert(ret == -1);
	printf("test3\n");
	memset(&rbuf, 0, sizeof(rbuf));
	ret = ku_msgrcv(q, &rbuf, 8+5, 1, KU_IPC_NOWAIT|KU_MSG_NOERROR); //good get type 1 msg, even small size

	assert(ret > 0);
	printf("rbuf[%d] = %s\n", ret, rbuf.text); 
	printf("test4\n");
	ret = ku_msgget(q, KU_IPC_CREAT|KU_IPC_EXCL); //wrong queue alloc, it is EXCLUSIVE

	assert(ret == -1);
	ret = ku_msgclose(q); //good queue close

	assert(ret == 0);
	ret = ku_msgclose(q); //wrong close, queue not exists

	assert(ret == -1);
}

void block_rcv() {
	int q = ku_msgget(8, KU_IPC_CREAT);
	int ret;

	struct msgbuf rbuf;

	assert(q == 8);
	ret = ku_msgrcv(q, &rbuf, 10000, 0, KU_MSG_NOERROR); //blocking read. should pass assert

	assert(ret > 0);
	printf("rbuf[%d] = %s\n", ret, rbuf.text); 
	ku_msgclose(q);
}


void send() {
	int q = ku_msgget(8, KU_IPC_CREAT);
	struct msgbuf wbuf = {
		.type = 11,
		.text = "hi my world!",
	};

	printf("q=%d?\n", q);
	assert(q == 8);
	int ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT);
	assert(ret == 0);

	ret = ku_msgclose(q);
	assert(ret == 0);
}

int main(void) {
	snd_rcv();
	printf("busy io\n");
	//block_rcv();
}

int ku_msgget(int key, int msgflg) {
	int retval = -1;
	int fd = open("/dev/"DEV_NAME, O_RDONLY);

	__make_query(key, NULL, 0);
	
	if (__chk_flag(msgflg, KU_IPC_EXCL)) {
		if(ioctl(fd, IOCTL_CHKEXCL, &my_query))
			if(0 == ioctl(fd, IOCTL_ADDUSER, &my_query))
				retval = key;
	} else if (0 == ioctl(fd, IOCTL_ADDUSER, &my_query))
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

	assert(retval == -1 || retval == 0);
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
		if (!ioctl(fd, IOCTL_PEEKMSG, &my_query) //if ubuf size is smaller
			&& !__chk_flag(msgflg, KU_MSG_NOERROR)) //and KU_MSG_NOEEROR not set.
				retval = ERROR;
		else 
			retval = ioctl(fd, IOCTL_SUBMSG, &my_query); //retrieve msg

		ioctl(fd, IOCTL_RELLOCK, &my_query);
	} while (!__chk_flag(msgflg, KU_IPC_NOWAIT) && retval == -1); // keep looping, when KU_IPC_NOWAIT is not set.

	close(fd);

	if (retval == ERROR)
		retval = -1;
	return retval;
}
