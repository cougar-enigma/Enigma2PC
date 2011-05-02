#ifndef __CA_NETLINK_H
#define __CA_NETLINK_H

#include <linux/netlink.h>
#include <net/genetlink.h>

int reply_ca(struct sk_buff *skb_2, struct genl_info *info);
int netlink_send_cw(unsigned char ca_num, unsigned char* cw, unsigned char type);
int netlink_send_pid(unsigned char ca_num, unsigned short pid, int index);
int register_netlink(void);
void unregister_netlink(void);

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

extern struct nla_policy ca_policy[ATTR_MAX + 1];
extern struct genl_family ca_family;
extern struct genl_ops ask_ca_size_ops;
extern int devices_counter;

#endif
