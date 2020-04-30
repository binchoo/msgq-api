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

#define CHK_FLAG(TARGET, ASSERT) ((TARGET & ASSERT) == ASSERT)

struct query {
	int msgqid;
	struct msgbuf* msg;
};

#define MAKE_QUERY(__msgqid, __msgbufp) \
	struct query my_query = {	\
		.msgqid = __msgqid,	\
		.msg = __msgbufp,	\
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
	int q = ku_msgget(1, KU_IPC_CREAT);
	int ret;

	struct msgbuf rbuf;
	struct msgbuf wbuf = {
		.type = 3,
		.text = "hello my friend!",
	};

	assert(q == 1);
	ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT); //send type(3) msg

	assert(ret == 0);
	wbuf.type = 1;
	wbuf.text[0] = 'X';
	ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT); //send diffrent type(1) msg

	assert(ret == 0);
	assert(CHK_FLAG(KU_IPC_NOWAIT, KU_IPC_NOWAIT));
	ret = ku_msgrcv(9, &rbuf, sizeof(rbuf), 3, KU_IPC_NOWAIT); //wrong queue number
	
	assert(ret == -1);
	ret = ku_msgrcv(q, &rbuf, sizeof(rbuf), 1, KU_IPC_NOWAIT); //good get type 1 msg
	
	assert(ret > 0);
	printf("rbuf[%d] = %s\n", ret, rbuf.text); 
	ret = ku_msgrcv(q, &rbuf, 5, 3, KU_IPC_NOWAIT); //fail get type 3 small msg, NO_KU_MSG_NOERROR

	assert(ret == -1);
	ret = ku_msgrcv(q, &rbuf, 5, 3, KU_IPC_NOWAIT|KU_MSG_NOERROR); //good get type 3 msg, even small size

	assert(ret > 0);
	printf("rbuf[%d] = %s\n", ret, rbuf.text); 

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
	ret = ku_msgrcv(q, &rbuf, sizeof(rbuf), 0, KU_MSG_NOERROR); //blocking read. should pass assert

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

	int ret = ku_msgsnd(q, &wbuf, sizeof(wbuf), KU_IPC_NOWAIT);
	assert(ret == 0);

	ret = ku_msgclose(q);
	assert(ret == 0);
	close(q);
}

void rcv() {
	int q = ku_msgget(8, KU_IPC_CREAT);
	struct msgbuf rbuf ;

	int ret = ku_msgrcv(q, &rbuf, sizeof(rbuf), 0, KU_IPC_NOWAIT|KU_MSG_NOERROR);
	assert(ret > 0);

	ret = ku_msgclose(q);
	assert(ret ==  0);
	close(q);
	printf("send and rcv %s\n", rbuf.text);

}

int main(void) {
	send();
//	rcv();
}

int ku_msgget(int key, int msgflg) {
	int fd = open("/dev/"DEV_NAME, O_RDONLY);
	int rtn;

	assert(fd >= 0);
	MAKE_QUERY(key, NULL);
	
	if (CHK_FLAG(msgflg, KU_IPC_EXCL) 
			&& !ioctl(fd, IOCTL_CHKEXCL, &my_query))
		rtn = -1;
	else
		rtn = ioctl(fd, IOCTL_ADDUSER, &my_query);	
	
	close(fd);
	assert(-1 <= rtn && rtn < 10);
	return rtn;
}

int ku_msgclose(int msgqid) {
	int fd = open("/dev/"DEV_NAME, O_RDONLY);
	int rtn;

	MAKE_QUERY(msgqid, NULL);
	
	if (!ioctl(fd, IOCTL_CHKEXCL, &my_query))
		rtn = ioctl(fd, IOCTL_SUBUSER, &my_query);
	else 
		rtn = -1;
	
	close(fd);
	assert(rtn == -1 || rtn == 0);
	return rtn;
}

int ku_msgsnd(int msgqid, void* msgp, int msgsz, int msgflg) {
	int fd = open("/dev/"DEV_NAME, O_RDONLY);
	int rtn;

	MAKE_QUERY(msgqid, msgp);

	if (!msgp) return -1;

	do {
		rtn = ioctl(fd, IOCTL_ADDMSG, &my_query);
		if (CHK_FLAG(msgflg, KU_IPC_NOWAIT))
				break;
	} while (rtn == -1);

	assert(rtn == -1 || rtn == 0);
	return rtn;
}

int ku_msgrcv(int msgqid, void* msgp, int msgsz, long msgtyp, int msgflg) {
	int fd = open("/dev/"DEV_NAME, O_RDONLY);
	int rtn;
	
	struct msgbuf* userbuf = (struct msgbuf*) msgp;
	struct msgbuf rcvbuf = {
		.type = msgtyp,
	};

	MAKE_QUERY(msgqid, &rcvbuf);

	if (!msgp) return -1;

	do {
		rtn = ioctl(fd, IOCTL_PEEKMSG, &my_query);	
		if (CHK_FLAG(msgflg, KU_IPC_NOWAIT))
				break;
	} while (rtn == -1);

	if (rtn == 0) {
		int rcv_size = sizeof(rcvbuf) - sizeof(long);
		printf("receive size %d, msgsz %d\n", rcv_size, msgsz);

		if(rcv_size > msgsz && !CHK_FLAG(msgflg, KU_MSG_NOERROR)) { // E2BIG && !NOERROR
			rtn = -1;
		} else {
			ioctl(fd, IOCTL_SUBMSG, &my_query);

			msgsz = sizeof(userbuf->text);
			strncpy(userbuf->text, rcvbuf.text, msgsz);
			userbuf->type = rcvbuf.type;
			userbuf->text[msgsz - 1] = '\0';
			rtn = strlen(userbuf->text) + 1;
		}
	}

	return rtn;
}
