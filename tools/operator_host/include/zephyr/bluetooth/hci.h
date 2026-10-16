#pragma once

#include <stddef.h>
#include <stdint.h>

#define BT_HCI_OP_READ_RSSI 0x1405

struct bt_hci_cp_read_rssi {
    uint16_t handle;
};

struct bt_hci_rp_read_rssi {
    uint8_t status;
    int8_t rssi;
};

struct net_buf {
    uint8_t data[sizeof(struct bt_hci_rp_read_rssi)];
};

static inline struct net_buf *bt_hci_cmd_create(uint16_t opcode, size_t len) {
    (void)opcode;
    (void)len;
    static struct net_buf buf;
    return &buf;
}

static inline void *net_buf_add(struct net_buf *buf, size_t len) {
    (void)len;
    return buf->data;
}

static inline int bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf, struct net_buf **rsp) {
    (void)opcode;
    (void)buf;
    static struct net_buf response;
    struct bt_hci_rp_read_rssi *rp = (struct bt_hci_rp_read_rssi *)response.data;
    rp->status = 0;
    rp->rssi = -60;
    *rsp = &response;
    return 0;
}

static inline void net_buf_unref(struct net_buf *buf) {
    (void)buf;
}
