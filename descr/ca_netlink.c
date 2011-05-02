/*
 * ca_netlink.c: Generic netlink communication
 *
 * See the main source file 'descr.c' for copyright information.
 *
 */

#include "ca_netlink.h"
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include "csa_decode.h"
#include "thread.h"
#include "tools.h"
#include "ringbuffer.h"
#include <linux/dvb/ca.h>
#include <unistd.h>
#include "decoder.h"

pthread_t netlink_thread;
struct nla_policy ca_policy[ATTR_MAX + 1];
struct nl_sock *sock;
int ca_size = 0;

extern cDecoder* example_decoder;

int parse_cb(struct nl_msg *msg, void *arg) {
	struct nlmsghdr *nlh = nlmsg_hdr(msg);
	struct nlattr *attrs[ATTR_MAX+1];
	unsigned short ca_num;
	bool ca_num_ok = false;
	bool cw_ok = false;
	unsigned char* cw_tmp;

	genlmsg_parse(nlh, 0, attrs, ATTR_MAX, ca_policy);

	if (attrs[ATTR_CA_SIZE]) {
		uint32_t value = nla_get_u32(attrs[ATTR_CA_SIZE]);
		ca_size = value;
		printf("ca_size %d\n", value);
	}
	if (attrs[ATTR_CA_NUM]) {
		ca_num = nla_get_u16(attrs[ATTR_CA_NUM]);
		ca_num_ok = true;
	}
	if (attrs[ATTR_CW] && ca_num_ok) {
		cw_tmp = (unsigned char*)nla_data(attrs[ATTR_CW]);
		cw_ok = true;
	}
	if (attrs[ATTR_CW_TYPE] && ca_num_ok) {
		unsigned char value = nla_get_u8(attrs[ATTR_CW_TYPE]);

		if (cw_ok) {
			printf("ca_num %d, parity %d, cw %02X...%02X\n", ca_num, value, cw_tmp[0], cw_tmp[7]);
			ca_descr_t ca_descr;
			ca_descr.index=0;
			ca_descr.parity=value;
			memcpy(ca_descr.cw,&cw_tmp[0],8);
			if (ca_num==0x0000){ // only adapter0/ca0
				if(!example_decoder->sc->SetCaDescr(&ca_descr,0)) {
					printf("CA_SET_DESCR failed (%s). Expect a black screen.",strerror(errno));
				}
			}
		}
	}
	if (attrs[ATTR_CA_PID] && attrs[ATTR_CA_PID_INDEX]) {
		unsigned short pid = nla_get_u16(attrs[ATTR_CA_PID]);
		int pid_index = nla_get_u32(attrs[ATTR_CA_PID_INDEX]);
		printf("DOSTALEM CA_PID %04X index %d\n", pid, pid_index);
		
		if (pid_index == -1)
			example_decoder->Restart();
	}

	return 0;
}

static void *netlink_thread_function( void *arg )
{
	while (1) {
		ca_netlink_recv();
	}
}

int ca_netlink_init() {
	int ret, family;
	struct nl_msg *msg;
	
	sock = nl_socket_alloc();
	genl_connect(sock);
	
	family = genl_ctrl_resolve(sock, "CA_SEND");
	if (family<0) {
		printf("Cannot resolve family name of generic netlink socket\n");
		return -1;
	}
	
	ca_policy[ATTR_CA_SIZE].type = NLA_U32;
	ca_policy[ATTR_CA_NUM].type = NLA_U16;
	ca_policy[ATTR_CW].type = NLA_UNSPEC;
	ca_policy[ATTR_CW_TYPE].type = NLA_U8;
	ca_policy[ATTR_CA_PID].type = NLA_U16;
	ca_policy[ATTR_CA_PID_INDEX].type = NLA_U32;

	msg = nlmsg_alloc();
	genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family, 0, NLM_F_ECHO,
			CMD_ASK_CA_SIZE, 1);
	nl_send_auto_complete(sock, msg);
	nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, parse_cb, NULL);
	nl_recvmsgs_default(sock);
	nlmsg_free(msg);

	nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, parse_cb, NULL);
	nl_socket_disable_seq_check(sock);

	ret = pthread_create( &netlink_thread, NULL, netlink_thread_function, NULL);
	if (ret<0) {
		printf("Creating thread error\n");
		return ret;
	}

	return 0;
}

void ca_netlink_recv() {
	nl_recvmsgs_default(sock);
}

void ca_netlink_close() {
	pthread_join( netlink_thread, NULL);

	nl_close(sock);
	nl_socket_free(sock);
}

