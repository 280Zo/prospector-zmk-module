#pragma once

#include <stdint.h>

#define BT_CONN_TYPE_LE 1
#define BT_CONN_ROLE_CENTRAL 1

struct bt_conn {
    int unused;
};

struct bt_conn_info {
    uint8_t type;
    uint8_t role;
};

struct bt_conn_cb {
    void (*connected)(struct bt_conn *conn, uint8_t err);
    void (*disconnected)(struct bt_conn *conn, uint8_t reason);
};

#define BT_CONN_CB_DEFINE(name) static struct bt_conn_cb name

static inline int bt_conn_get_info(struct bt_conn *conn, struct bt_conn_info *info) {
    (void)conn;
    info->type = BT_CONN_TYPE_LE;
    info->role = BT_CONN_ROLE_CENTRAL;
    return 0;
}

static inline struct bt_conn *bt_conn_ref(struct bt_conn *conn) {
    return conn;
}

static inline void bt_conn_unref(struct bt_conn *conn) {
    (void)conn;
}

static inline const void *bt_conn_get_dst(struct bt_conn *conn) {
    return conn;
}

static inline int bt_hci_get_conn_handle(struct bt_conn *conn, uint16_t *handle) {
    (void)conn;
    *handle = 0;
    return 0;
}
