#ifndef __CA_NETLINK_H
#define __CA_NETLINK_H

int parse_cb(struct nl_msg *msg, void *arg);
int ca_netlink_init();
void ca_netlink_recv();
void ca_netlink_close();

// attributes
enum {
	ATTR_UNSPEC,
	ATTR_CA_SIZE,
	ATTR_CA_NUM,
	ATTR_CW,
	ATTR_CW_TYPE,
	ATTR_CA_PID,
	ATTR_CA_PID_INDEX,
        __ATTR_MAX,
};
#define ATTR_MAX (__ATTR_MAX - 1)

// commands
enum {
	CMD_UNSPEC,
	CMD_ASK_CA_SIZE,
	CMD_SET_CW,
	CMD_SET_PID,
	CMD_MAX,
};

extern int ca_size;

#endif
