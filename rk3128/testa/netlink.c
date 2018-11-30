#include<assert.h>
#include<errno.h>
#include<ifaddrs.h>
#include<netdb.h>
#include<stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <netlink/netlink.h>
#include <netlink/msg.h>
#include <netlink/cache.h>
#include <netlink/socket.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <stdlib.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/route/link.h>
#include <linux/nl80211.h>

static int expectedId;
static int ifIndex;
struct wpa_scan_res 
{
		unsigned char bssid[6]; 
		int freq;
};

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) 
{

		int *ret = arg;
		*ret = err->error;
		return NL_SKIP;
}

static int finish_handler(struct nl_msg *msg, void *arg) 
{

		int *ret = arg;
		*ret = 0;
		return NL_SKIP;
}
static int ack_handler(struct nl_msg *msg, void *arg)
{
		int *err = arg;
		*err = 0;
		return NL_STOP;
}

static int bss_info_handler(struct nl_msg *msg, void *arg)
{

		printf("\nFunction: %s, Line: %d\n",__FUNCTION__,__LINE__);
		struct nlattr *tb[NL80211_ATTR_MAX + 1];
		struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
		struct nlattr *bss[NL80211_BSS_MAX + 1];
		static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
				[NL80211_BSS_BSSID] = { .type = NLA_UNSPEC },
				[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
				[NL80211_BSS_TSF] = { .type = NLA_U64 },
				[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
				[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
				[NL80211_BSS_INFORMATION_ELEMENTS] = { .type = NLA_UNSPEC },
				[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
				[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
				//[NL80211_BSS_STATUS] = { .type = NLA_U32 },
				//[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
				//[NL80211_BSS_BEACON_IES] = { .type = NLA_UNSPEC },
		};
		struct wpa_scan_res *r = NULL;

		r = (struct wpa_scan_res*)malloc(sizeof(struct wpa_scan_res));

		nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
						genlmsg_attrlen(gnlh, 0), NULL);
		if (!tb[NL80211_ATTR_BSS])
				return NL_SKIP;
		if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS],
								bss_policy))
				return NL_SKIP;
		if (bss[NL80211_BSS_BSSID])

				memcpy(r->bssid, nla_data(bss[NL80211_BSS_BSSID]),6);
		if (bss[NL80211_BSS_FREQUENCY])
				r->freq = nla_get_u32(bss[NL80211_BSS_FREQUENCY]);


		printf("\nFrequency: %d ,BSSID: %2x:%2x:%2x:%2x:%2x:%2x",r->freq,r->bssid[0],r->bssid[1],r->bssid[2],r->bssid[3],r->bssid[4],r->bssid[5]);
		return NL_SKIP;
}


static struct nl_msg* nl80211_scan_common(uint8_t cmd, int expectedId)
{
		const char* ssid = "amitssid";

		int ret;

		struct nl_msg *msg;
		int err;
		size_t i;
		int flags = 0,ifIndex;

		msg = nlmsg_alloc();
		if (!msg)
				return NULL;

		// setup the message
		if(NULL==genlmsg_put(msg, 0, 0, expectedId, 0, flags, cmd, 0))
		{
				printf("\nError return genlMsg_put\n");
		}
		else
		{

				printf("\nSuccess genlMsg_put\n");
		}

		ifIndex = if_nametoindex("wlan0");
		printf("nl80211_scan_common ifIndex : %d \n",ifIndex);

		if(nla_put_u32(msg, NL80211_ATTR_IFINDEX, ifIndex) < 0)
		{

				goto fail;
		}

		struct nl_msg *ssids = nlmsg_alloc();
		if(nla_put(ssids, 1,strlen(ssid) ,ssid) <0)
		{

				nlmsg_free(ssids);
				goto fail;
		}

		err =   nla_put_nested(msg,  NL80211_ATTR_SCAN_SSIDS,ssids);
		nlmsg_free(ssids);
		if (err < 0)
				goto fail;

		struct nl_msg *freqs = nlmsg_alloc();

		if( nla_put_u32(freqs,1 ,2437) < 0) //amitssid
		{
				printf("\nnla_put_fail\n");
				goto fail;
		}
		else
		{
				printf("\nnla_put_u32 pass\n");
		}

		//add message attributes
		if(nla_put_nested(msg, NL80211_FREQUENCY_ATTR_FREQ,freqs) < 0)
		{
				printf("\nnla_put_nested failing:\n");
		}
		else
		{
				printf("\nnla_put_nested pass\n");
		}

		nlmsg_free(freqs);
		if (err < 0)
				goto fail;

		return msg;

nla_put_failure:
		printf("\nnla_put_failure\n");
		nlmsg_free(msg);
		return NULL;


fail:
		nlmsg_free(msg);
		return NULL;

}
void mac_addr_n2a(char *mac_addr, unsigned char *arg) {
		// From http://git.kernel.org/cgit/linux/kernel/git/jberg/iw.git/tree/util.c.
		int i, l;

		l = 0;
		for (i = 0; i < 6; i++) {
				if (i == 0) {
						sprintf(mac_addr+l, "%02x", arg[i]);
						l += 2;
				} else {
						sprintf(mac_addr+l, ":%02x", arg[i]);
						l += 3;
				}
		}
}
void print_ssid(unsigned char *ie, int ielen) {
		uint8_t len;
		uint8_t *data;
		int i;

		while (ielen >= 2 && ielen >= ie[1]) {
				if (ie[0] == 0 && ie[1] >= 0 && ie[1] <= 32) {
						len = ie[1];
						data = ie + 2;
						for (i = 0; i < len; i++) {
								if (isprint(data[i]) && data[i] != ' ' && data[i] != '\\') printf("%c", data[i]);
								else if (data[i] == ' ' && (i != 0 && i != len -1)) printf(" ");
								else printf("\\x%.2x", data[i]);
						}
						break;
				}
				ielen -= ie[1] + 2;
				ie += ie[1] + 2;
		}
}

static int callback_dump(struct nl_msg *msg, void *arg) {
		// Called by the kernel with a dump of the successful scan's data. Called for each SSID.
		struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
		char mac_addr[20];
		struct nlattr *tb[NL80211_ATTR_MAX + 1];
		struct nlattr *bss[NL80211_BSS_MAX + 1];
		static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
				[NL80211_BSS_TSF] = { .type = NLA_U64 },
				[NL80211_BSS_FREQUENCY] = { .type = NLA_U32 },
				[NL80211_BSS_BSSID] = { },
				[NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
				[NL80211_BSS_CAPABILITY] = { .type = NLA_U16 },
				[NL80211_BSS_INFORMATION_ELEMENTS] = { },
				[NL80211_BSS_SIGNAL_MBM] = { .type = NLA_U32 },
				[NL80211_BSS_SIGNAL_UNSPEC] = { .type = NLA_U8 },
				//[NL80211_BSS_STATUS] = { .type = NLA_U32 },
				//[NL80211_BSS_SEEN_MS_AGO] = { .type = NLA_U32 },
				//[NL80211_BSS_BEACON_IES] = { },
		};

		// Parse and error check.
		nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);
		if (!tb[NL80211_ATTR_BSS]) {
				printf("bss info missing!\n");
				return NL_SKIP;
		}
		if (nla_parse_nested(bss, NL80211_BSS_MAX, tb[NL80211_ATTR_BSS], bss_policy)) {
				printf("failed to parse nested attributes!\n");
				return NL_SKIP;
		}
		if (!bss[NL80211_BSS_BSSID]) return NL_SKIP;
		if (!bss[NL80211_BSS_INFORMATION_ELEMENTS]) return NL_SKIP;

		// Start printing.
		mac_addr_n2a(mac_addr, nla_data(bss[NL80211_BSS_BSSID]));
		printf("%s, ", mac_addr);
		printf("%d MHz, ", nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
		print_ssid(nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
		printf("\n");

		return NL_SKIP;
}

int main(int argc, char** argv)
{

		struct nl_msg *msg= NULL;
		int ret = -1;
		struct nl_cb *cb = NULL;
		int err = -ENOMEM;
		int returnvalue,getret;
		int ifIndex, callbackret=-1;
		struct nl_sock* sk = (void*)nl_socket_alloc();
		if(sk == NULL)
		{
				printf("\nmemory error\n");
				return;
		}

		cb = nl_cb_alloc(NL_CB_CUSTOM);
		if(cb == NULL)
		{
				printf("\nfailed to allocate netlink callback\n");
		}

		enum nl80211_commands cmd;
		if(genl_connect((void*)sk))
		{
				printf("\nConnected failed\n");
				return;
		}

		//find the nl80211 driverID
		expectedId = genl_ctrl_resolve((void*)sk, "nl80211");

		if(expectedId < 0)
		{
				printf("\nnegative error code returned\n");
				return;
		}
		else
		{

				printf("\ngenl_ctrl_resolve returned:%d\n",expectedId);
		}


		msg = nl80211_scan_common(NL80211_CMD_TRIGGER_SCAN, expectedId);
		if (!msg)
		{
				printf("\nmsgbal:\n");
				return -1;
		}

		err = nl_send_auto_complete((void*)sk, msg);
		if (err < 0)
				goto out;
		else
		{
				printf("\nSent successfully\n");

		}
		err = 1;
		nl_cb_err(cb,NL_CB_CUSTOM,error_handler,&err);
		nl_cb_set(cb,NL_CB_FINISH,NL_CB_CUSTOM,finish_handler,&err);
		nl_cb_set(cb,NL_CB_ACK,NL_CB_CUSTOM,ack_handler,&err);


		callbackret = nl_cb_set(cb,NL_CB_VALID,NL_CB_CUSTOM,bss_info_handler,&err);

		if(callbackret < 0)
		{
				printf("\n*************CallbackRet failed:***************** %d\n",callbackret);
		}
		else
		{
				printf("\n*************CallbackRet pass:***************** %d\n",callbackret);
		}


		returnvalue=nl_recvmsgs((void*)sk,cb);
		printf("\n returnval:%d\n",returnvalue);

		nlmsg_free(msg);
		msg = NULL;
		msg = nlmsg_alloc();
		if (!msg)
				return -1;

		if(NULL==genlmsg_put(msg, 0, 0, expectedId, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0))
		{
				printf("\nError return genlMsg_put\n");
		}
		else
		{

				printf("\nSuccess genlMsg_put\n");
		}

		ifIndex = if_nametoindex("wlan0");
		printf("\nGet Scaninterface returned :%d\n",ifIndex);

		nla_put_u32(msg,NL80211_ATTR_IFINDEX,ifIndex);  
		nl_socket_modify_cb(sk, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);  // Add the callback.
		err = nl_send_auto_complete((void*)sk,msg);
		if(err < 0) goto out;
		printf("NL80211_CMD_GET_SCAN sent %d bytes to the kernel.\n", err);

		err = 1;
		getret= nl_recvmsgs((void*)sk,cb);

		printf("\nGt Scan resultreturn:%d\n",getret);

out:
		nlmsg_free(msg);
		return err;

nla_put_failure:
		printf("\nnla_put_failure\n");
		nlmsg_free(msg);
		return err;
}
