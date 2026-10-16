/* SPDX-License-Identifier: MIT */
#pragma once
struct bt_conn;
/* Existing split observer's slot map; does not reserve or change bonds. */
int prospector_peripheral_index_for_conn(struct bt_conn *conn);
