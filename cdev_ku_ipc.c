#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

/*a notation attached to a function that returns 0 for success, negative for failure*/
#define __linux_return 

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

#include "ku_ipc.h"
#define DEV_NAME 	"ku_ipc"
MODULE_LICENSE("GPL");

struct msgq {
	unsigned int ref;
	unsigned int len;
	struct list_head head;
	spinlock_t lock;
};

typedef struct msgq_node {
	struct list_head list;
	struct msgbuf* msg;
} qnode_t;


struct query {
	int msgqid;
	struct msgbuf* msg;
	unsigned int bytes;
};

static struct msgq q[10];

void init_queue(struct msgq* queue);
void delete_queue(struct msgq* queue);
void init_queues(void);
void delete_queues(void);

#define __create_new_node(name) \
	qnode_t* name = (qnode_t*) vmalloc(sizeof(qnode_t)); \
	name->msg = (struct msgbuf*) vmalloc(sizeof(struct msgbuf)); \

#define __first_entry(listhead, structure, field) \
	list_entry(listhead.next, structure, field);

int __linux_return append_node(qnode_t* node, struct msgq* queue);
int __linux_return remove_node(qnode_t* node, struct msgq* queue);
int __linux_return find_node_by_type(qnode_t** found, struct msgq* queue, long type);

inline int chk_noref(struct msgq* queue);
inline int get_volume(struct msgq* queue);
inline int __linux_return add_user(struct msgq* queue);
inline int __linux_return sub_user(struct msgq* queue);

int __linux_return add_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes);
int sub_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes);
int peek_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes);

int __linux_return acquire_lock(struct msgq* queue);
int __linux_return release_lock(struct msgq* queue);

#define __min(a, b) ((a < b)? a : b);

static int ku_ipc_open(struct inode* inode, struct file* file) {
	int i;
	struct msgq* queue = q;
	for(i = 0; i < 10; i++, queue++)
		printk("queue[%d] is referred by %d processes.", i, queue->ref);

	return 0;
}

static long ku_ipc_ioctl(struct file* file, unsigned int cmd, unsigned long arg) {
	struct query* query = (struct query*) arg;
	struct msgq* queue = &q[query->msgqid];
	struct msgbuf* msg = query->msg;

	int retval = 0;

	printk("ioctl msgqid=%d text=%s\n", query->msgqid, msg->text);

	switch(cmd) {
		case IOCTL_ADDUSER: 
			printk("add_user\n");
			retval = add_user(queue);
			break;
		case IOCTL_SUBUSER:
			printk("sub_user\n");
			retval = sub_user(queue);
			break;
		case IOCTL_ADDMSG:
			printk("add_msg\n");
			retval = add_msg(queue, msg, query->bytes);
			break;
		case IOCTL_SUBMSG:
			printk("sub_msg\n");
			retval = sub_msg(queue, msg, query->bytes);
			break;
		case IOCTL_PEEKMSG:
			printk("peek_msg\n");
			retval = peek_msg(queue, msg, query->bytes);
			break;
		case IOCTL_CHKEXCL:
			printk("check_queue_exclusiveness\n");
			retval = chk_noref(queue);
			break;
		case IOCTL_AQLOCK:
			printk("aquire_lock\n");
			retval = acquire_lock(queue);
			break;
		case IOCTL_RELLOCK:
			printk("release_lock\n");
			retval = release_lock(queue);
			break;
		default:
			return -ENOTTY;
	}
	return retval;
}

static struct file_operations ku_ipc_fops = {
	.open = ku_ipc_open,
	.unlocked_ioctl = ku_ipc_ioctl,
};

static dev_t dev_num;
static struct cdev* cd_cdev;

static int __init ku_ipc_init(void) {
	cd_cdev = cdev_alloc();
	cdev_init(cd_cdev, &ku_ipc_fops);
	alloc_chrdev_region(&dev_num, 0, 1, DEV_NAME);
	cdev_add(cd_cdev, dev_num, 1);

	init_queues();
	return 0;
}
module_init(ku_ipc_init);

static void __exit ku_ipc_exit(void) {
	cdev_del(cd_cdev);
	unregister_chrdev_region(dev_num, 1);

	delete_queues();
}
module_exit(ku_ipc_exit);

/***Implementations***/
void init_queue(struct msgq* queue) {
	queue->ref = queue->len = 0;
	INIT_LIST_HEAD(&queue->head);
	spin_lock_init(&queue->lock);
}

void delete_queue(struct msgq* queue) {
	struct list_head* head, *pos, *next;
	qnode_t* node;

       	head = &queue->head;
	list_for_each_safe(pos, next, head) {
		node = list_entry(pos, qnode_t, list);
		if (node) {
			if (node->msg)
				vfree(node->msg);
			vfree(node);
		}	
	}
}

int append_node(qnode_t* node, struct msgq* queue) {
	list_add_tail(&node->list, &queue->head);
	queue->len++;
	return 0;
}

int remove_node(qnode_t* node, struct msgq* queue) {
	if (node) {
		list_del(&node->list);
		queue->len--;

		if (node->msg)
			vfree(node->msg);
		vfree(node);
		return 0;
	}
	return -1;
}

int find_node_by_type(qnode_t** found, struct msgq* queue, long type) {
	qnode_t* node = NULL;

	if (type == 0) {
		node = __first_entry(queue->head, qnode_t, list);
	} else if (type > 0) {
		list_for_each_entry(node, &queue->head, list) {
			if(node->msg->type == type) {
				break;
			}
		}
	} else {
		type = -type;
		list_for_each_entry(node, &queue->head, list) {
			if(node->msg->type <= type) {
				break;
			}
		}
	}

	*found = node;
	return (node == NULL)? -1 : 0;
}

void init_queues(void) {
	struct msgq* queue = q;
	int i;

	for(i = 0; i < 10; i++, queue++)
		init_queue(queue);
}

void delete_queues(void) {
	struct msgq* queue = q;
	int i;

	for(i = 0; i < 10; i++, queue++)
		delete_queue(queue);
}

inline int chk_noref(struct msgq* queue) {
	return queue->ref == 0;
}

inline int get_volume(struct msgq* queue) {
	return queue->len * sizeof(struct msgbuf);
}

inline int add_user(struct msgq* queue) {
	acquire_lock(queue);
	queue->ref++;

	release_lock(queue);
	return 0;
}

inline int sub_user(struct msgq* queue) {
	int retval = -1;
	acquire_lock(queue);
	if (queue->ref > 0) {
		queue->ref--;
		retval = 0;
	}

	release_lock(queue);
	return retval;
}

int add_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes) {
	int retval = -1;
	int limit;
	printk("get_volume of this queue = %d\n", get_volume(queue));

	acquire_lock(queue);
	if (queue->ref > 0 && queue->len < KUIPC_MAXMSG
			&& get_volume(queue) < KUIPC_MAXVOL) {
		__create_new_node(node);

		limit = __min(sizeof(struct msgbuf), bytes);
		copy_from_user(node->msg, msg, limit); 
		printk("kern text %p, usr text %p\n", node->msg->text, msg->text);
		printk("kern text %s, user text %s\n", node->msg->text, msg->text);

		retval = append_node(node, queue);
	}

	release_lock(queue);
	return retval;
}

int sub_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes) {
	qnode_t* node = NULL;
	int limit = -1;
	long type = msg->type;	

	if (queue->ref > 0 && queue->len > 0) {
		if (0 == find_node_by_type(&node, queue, type)) {
			limit = __min(sizeof(struct msgbuf), bytes);
			copy_to_user(msg->text, node->msg->text, limit - sizeof(long));
			printk("kern text %p, usr text %p\n", node->msg->text, msg->text);
			printk("kern text %s, user text %s\n", node->msg->text, msg->text);
		}
	}

	remove_node(node, queue);
	return limit;
}

int peek_msg(struct msgq* queue, struct msgbuf* msg, unsigned int bytes) {
	qnode_t* node = NULL;
	int retval = -1;
	int limit;
	long type = msg->type;	
	
	if (queue->ref > 0 && queue->len > 0) {
		if (0 == find_node_by_type(&node, queue, type)) {
			limit = __min(sizeof(struct msgbuf), bytes);
			//copy_to_user(msg, node->msg, limit);
			retval = (sizeof(struct msgbuf) <= bytes);
		}
	}

	return retval;
}


int __linux_return acquire_lock(struct msgq* queue) {
	if(queue) {
		spin_lock(&queue->lock);
		return 0;
	}
	return -1;
}

int __linux_return release_lock(struct msgq* queue) {
	if(queue) {
		spin_unlock(&queue->lock);
		return 0;
	}
	return -1;
}
