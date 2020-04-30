/*
 * 201511298
 * Jaebin, Ju
 */

#define KUIPC_MAXMSG 	88888888 /* Max node count for one queue */
#define KUIPC_MAXVOL	12800  /*Limit for bytes of msg in one queue */

#define KU_IPC_CREAT 	0b0001
#define KU_IPC_EXCL	0b0010
#define KU_IPC_NOWAIT	0b0100
#define KU_MSG_NOERROR	0b1000

struct msgbuf {
	long type;	/* Type of Message */
	char text[128];	/* Message Text */
};
