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

	genlmsg_parse(nlh, 0, attrs, ATTR_MAX, ca_policy);

	if (attrs[ATTR_CA_SIZE]) {
		uint32_t value = nla_get_u32(attrs[ATTR_CA_SIZE]);
		ca_size = value;
		printf("ca_size %d\n", value);
	}
	if (attrs[ATTR_CA_NUM] && attrs[ATTR_CA_DESCR]) {
		unsigned short ca_num = nla_get_u16(attrs[ATTR_CA_NUM]);
		ca_descr_t *ca = (ca_descr_t*)nla_data(attrs[ATTR_CA_DESCR]);
		printf("ca_num %d, idx %d, parity %d, cw %02X...%02X\n", ca_num, ca->index, ca->parity, ca->cw[0], ca->cw[7]);
		if (ca_num==0x0000 && ca->index==0){ // only adapter0/ca0, idx 0
			if(!example_decoder->sc->SetCaDescr(ca,0)) {
				printf("CA_SET_DESCR failed (%s). Expect a black screen.",strerror(errno));
			}
		}
	}
	if (attrs[ATTR_CA_NUM] && attrs[ATTR_CA_PID]) {
		unsigned short ca_num = nla_get_u16(attrs[ATTR_CA_NUM]);
		ca_pid_t *ca_pid = (ca_pid_t*)nla_data(attrs[ATTR_CA_PID]);

		printf("DOSTALEM CA_PID ca_num %02X, pid %04X, index %d\n", ca_num, ca_pid->pid, ca_pid->index);
		
		if (ca_pid->index == -1)
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
	ca_policy[ATTR_CA_DESCR].type = NLA_UNSPEC;
	ca_policy[ATTR_CA_PID].type = NLA_UNSPEC;

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

