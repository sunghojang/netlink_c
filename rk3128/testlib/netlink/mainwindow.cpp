#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <errno.h>
#include <netlink/netlink.h>
#include <netlink/errno.h>
#include <netlink/genl/genl.h>
#include <linux/nl80211.h>
#include <netlink/genl/ctrl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
//extern unsigned int if_nametoindex (const char *__ifname) __THROW;
int family_handler(struct nl_msg *msg, void *arg) ;
int ack_handler(struct nl_msg *msg, void *arg);
int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) ;
int callback_trigger(struct nl_msg *msg, void *arg);
int finish_handler(struct nl_msg *msg, void *arg);
int no_seq_check(struct nl_msg *msg, void *arg) ;
int callback_dump(struct nl_msg *msg, void *arg);
void mac_addr_n2a(char *mac_addr, unsigned char *arg) ;
void print_ssid(unsigned char *ie, int ielen);
 int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id);
 int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group);
int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err, void *arg) {
    // Callback for errors.
    printf("error_handler() called.\n");
    int *ret = (int*)arg;
    *ret = err->error;
    return NL_STOP;
}


int finish_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_FINISH.
    int *ret = (int*)arg;
    *ret = 0;
    return NL_SKIP;
}


int ack_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_ACK.
    int *ret = (int*)arg;
    *ret = 0;
    return NL_STOP;
}


int no_seq_check(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_SEQ_CHECK.
    return NL_OK;
}


int family_handler(struct nl_msg *msg, void *arg) {
    // Callback for NL_CB_VALID within nl_get_multicast_id(). From http://sourcecodebrowser.com/iw/0.9.14/genl_8c.html.
    struct handler_args *grp = (handler_args *)arg;
    struct nlattr *tb[CTRL_ATTR_MAX + 1];
    struct genlmsghdr *gnlh = (struct genlmsghdr *)nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *mcgrp;
    int rem_mcgrp;

    nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0), genlmsg_attrlen(gnlh, 0), NULL);

    if (!tb[CTRL_ATTR_MCAST_GROUPS]) return NL_SKIP;

    for (mcgrp = (struct nlattr *) nla_data(tb[CTRL_ATTR_MCAST_GROUPS]), rem_mcgrp = nla_len(tb[CTRL_ATTR_MCAST_GROUPS]);     nla_ok(mcgrp, rem_mcgrp);   mcgrp = nla_next(mcgrp, &(rem_mcgrp)))
    {  // This is a loop.
        struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];

        nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX, (struct nlattr *)nla_data(mcgrp), nla_len(mcgrp), NULL);

        if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] || !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]) continue;
        if (strncmp((const char *)nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]), grp->group, nla_len(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]))) {
            continue;
        }

        grp->id = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
        break;
    }

    return NL_SKIP;
}


int nl_get_multicast_id(struct nl_sock *sock, const char *family, const char *group) {
    // From http://sourcecodebrowser.com/iw/0.9.14/genl_8c.html.
    struct nl_msg *msg;
    struct nl_cb *cb;
    int ret, ctrlid;
    struct handler_args grp = { group,
                -ENOENT
                              };

    msg = nlmsg_alloc();
    if (!msg) return -ENOMEM;

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        ret = -ENOMEM;
        goto out_fail_cb;
    }

    ctrlid = genl_ctrl_resolve(sock, "nlctrl");

    genlmsg_put(msg, 0, 0, ctrlid, 0, 0, CTRL_CMD_GETFAMILY, 0);

    ret = -ENOBUFS;
    NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

    ret = nl_send_auto_complete(sock, msg);
    if (ret < 0) goto out;

    ret = 1;
    //nl_cb_err(struct nl_cb *, enum nl_cb_kind, nl_recvmsg_err_cb_t,void *);
    typedef int (*nl_recvmsg_err_cb_t)(struct sockaddr_nl *nla,struct nlmsgerr *nlerr, void *arg);

    nl_cb_err((struct nl_cb *)cb, NL_CB_CUSTOM, error_handler,(void *) &ret);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &ret);
    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, family_handler, &grp);

    while (ret > 0) nl_recvmsgs(sock, cb);

    if (ret == 0) ret = grp.id;

nla_put_failure:
out:
    nl_cb_put(cb);
out_fail_cb:
    nlmsg_free(msg);
    return ret;
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


int callback_trigger(struct nl_msg *msg, void *arg) {
    // Called by the kernel when the scan is done or has been aborted.
    struct genlmsghdr *gnlh =(struct genlmsghdr *) nlmsg_data(nlmsg_hdr(msg));
    struct trigger_results *results = (trigger_results *)arg;

    //printf("Got something.\n");
    //printf("%d\n", arg);
    //nl_msg_dump(msg, stdout);

    if (gnlh->cmd == NL80211_CMD_SCAN_ABORTED) {
        printf("Got NL80211_CMD_SCAN_ABORTED.\n");
        results->done = 1;
        results->aborted = 1;
    } else if (gnlh->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
        printf("Got NL80211_CMD_NEW_SCAN_RESULTS.\n");
        results->done = 1;
        results->aborted = 0;
    }  // else probably an uninteresting multicast message.

    return NL_SKIP;
}


int callback_dump(struct nl_msg *msg, void *arg) {
    // Called by the kernel with a dump of the successful scan's data. Called for each SSID.
    struct genlmsghdr *gnlh =(struct genlmsghdr *) nlmsg_data(nlmsg_hdr(msg));
    char mac_addr[20];
    struct nlattr *tb[NL80211_ATTR_MAX + 1];
    struct nlattr *bss[NL80211_BSS_MAX + 1];
    static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] ;

    bss_policy[NL80211_BSS_TSF].type = NLA_U64;
    bss_policy[NL80211_BSS_FREQUENCY].type = NLA_U32;
    bss_policy[NL80211_BSS_BSSID];
    bss_policy[NL80211_BSS_BEACON_INTERVAL].type = NLA_U16;
    bss_policy[NL80211_BSS_CAPABILITY].type = NLA_U16;
    bss_policy[NL80211_BSS_INFORMATION_ELEMENTS];
    bss_policy[NL80211_BSS_SIGNAL_MBM].type = NLA_U32;
    bss_policy[NL80211_BSS_SIGNAL_UNSPEC].type = NLA_U8;
    //    bss_policy[NL80211_BSS_STATUS].type = NLA_U32;
    //    bss_policy[NL80211_BSS_SEEN_MS_AGO].type = NLA_U32;
    //    bss_policy[NL80211_BSS_BEACON_IES];


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
    mac_addr_n2a(mac_addr, (unsigned char *)nla_data(bss[NL80211_BSS_BSSID]));
    printf("%s, ", mac_addr);
    printf("%d MHz, ", nla_get_u32(bss[NL80211_BSS_FREQUENCY]));
    print_ssid((unsigned char *)nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]), nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]));
    printf("\n");

    return NL_SKIP;
}


int do_scan_trigger(struct nl_sock *socket, int if_index, int driver_id) {
    // Starts the scan and waits for it to finish. Does not return until the scan is done or has been aborted.
    struct trigger_results results;
    results.done= 0;
    results.aborted = 0;
    struct nl_msg *msg;
    struct nl_cb *cb;
    struct nl_msg *ssids_to_scan;
    int err;
    int ret;
    int mcid = nl_get_multicast_id(socket, "nl80211", "scan");
    qDebug()<<"mcid"<<mcid<<nl_socket_add_membership(socket, mcid);  // Without this, callback_trigger() won't be called.

    // Allocate the messages and callback handler.
    msg = nlmsg_alloc();
    if (!msg) {
        printf("ERROR: Failed to allocate netlink message for msg.\n");
        return -ENOMEM;
    }
    ssids_to_scan = nlmsg_alloc();
    if (!ssids_to_scan) {
        printf("ERROR: Failed to allocate netlink message for ssids_to_scan.\n");
        nlmsg_free(msg);
        return -ENOMEM;
    }
    cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb) {
        printf("ERROR: Failed to allocate netlink callbacks.\n");
        nlmsg_free(msg);
        nlmsg_free(ssids_to_scan);
        return -ENOMEM;
    }

    // Setup the messages and callback handler.
        genlmsg_put(msg, 0, 0, driver_id, 0, 0, NL80211_CMD_TRIGGER_SCAN, 0);  // Setup which command to run.
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
        nla_put(ssids_to_scan, 1, 0, "");  // Scan all SSIDs.
        nla_put_nested(msg, NL80211_ATTR_SCAN_SSIDS, ssids_to_scan);  // Add message attribute, which SSIDs to scan for.
        nlmsg_free(ssids_to_scan);  // Copied to `msg` above, no longer need this.
        nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, callback_trigger, &results);  // Add the callback.
        nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
        nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
        nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);
        nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);  // No sequence checking for multicast messages.
    // Send NL80211_CMD_TRIGGER_SCAN to start the scan. The kernel may reply with NL80211_CMD_NEW_SCAN_RESULTS on
    // success or NL80211_CMD_SCAN_ABORTED if another scan was started by another process.
    err = 1;
    ret = nl_send_auto(socket, msg);  // Send the message.
    printf("NL80211_CMD_TRIGGER_SCAN sent %d bytes to the kernel.\n", ret);
    printf("Waiting for scan to complete...\n");
    while (err > 0) {
        ret = nl_recvmsgs(socket, cb);
        qDebug()<<ret<<err;
    }// First wait for ack_handler(). This helps with basic errors.
    if (err < 0) {
        printf("WARNING: err has a value of %d.\n", err);
    }
    if (ret < 0) {
        printf("ERROR: nl_recvmsgs() returned %d (%s).\n", ret, nl_geterror(-ret));
        return ret;
    }

    while (!results.done){
        nl_recvmsgs(socket, cb);
        qDebug()<<results.done;
        }// Now wait until the scan is done or aborted.
    if (results.aborted) {
        printf("ERROR: Kernel aborted scan.\n");
        return 1;
    }
    printf("Scan is done.\n");

    // Cleanup.
    nlmsg_free(msg);
    nl_cb_put(cb);
    nl_socket_drop_membership(socket, mcid);  // No longer need this.
    return 0;
}

unsigned int if_nametoindex(const char *ifname)
{
    int index;
    int ctl_sock;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    index = 0;
    if ((ctl_sock = socket(AF_INET, SOCK_DGRAM, 0)) >= 0) {
        if (ioctl(ctl_sock, SIOCGIFINDEX, &ifr) >= 0) {
            index = ifr.ifr_ifindex;
        }
        close(ctl_sock);
    }
    return index;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    int if_index = if_nametoindex("wlan0"); // Use this wireless interface for scanning.
    qDebug()<<"CHEDDDDKDDKDKDKDKDKKDKDK        111111111"<<if_index;
    // Open socket to kernel.
    struct nl_sock *socket = nl_socket_alloc();  // Allocate new netlink socket in memory.
    qDebug()<<genl_connect(socket);  // Create file descriptor and bind socket.

    int driver_id = genl_ctrl_resolve(socket, "nl80211");  // Find the nl80211 driver ID.
    qDebug()<<"CHEDDDDKDDKDKDKDKDKKDKDK        2222222222"<<driver_id;
    // Issue NL80211_CMD_TRIGGER_SCAN to the kernel and wait for it to finish.
    int err = do_scan_trigger(socket, if_index, driver_id);
    qDebug()<<"CHEDDDDKDDKDKDKDKDKKDKDK        3333333333"<<err;
        if (err != 0) {
            printf("do_scan_trigger() failed with %d.\n", err);
        }

        // Now get info for all SSIDs detected.
        struct nl_msg *msg = nlmsg_alloc();  // Allocate a message.
        genlmsg_put(msg, 0, 0, driver_id, 0, NLM_F_DUMP, NL80211_CMD_GET_SCAN, 0);  // Setup which command to run.
        nla_put_u32(msg, NL80211_ATTR_IFINDEX, if_index);  // Add message attribute, which interface to use.
        nl_socket_modify_cb(socket, NL_CB_VALID, NL_CB_CUSTOM, callback_dump, NULL);  // Add the callback.
        qDebug()<<"CHEDDDDKDDKDKDKDKDKKDKDK        444444444";
        int ret = nl_send_auto(socket, msg);  // Send the message.
        printf("NL80211_CMD_GET_SCAN sent %d bytes to the kernel.\n", ret);
        ret = nl_recvmsgs_default(socket);  // Retrieve the kernel's answer. callback_dump() prints SSIDs to stdout.
        nlmsg_free(msg);
        qDebug()<<"CHEDDDDKDDKDKDKDKDKKDKDK        55555555"<<ret;
        if (ret < 0) {
            printf("ERROR: nl_recvmsgs_default() returned %d (%s).\n", ret, nl_geterror(-ret));
        }

}

MainWindow::~MainWindow()
{
    delete ui;
}


