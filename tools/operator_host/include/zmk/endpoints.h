#pragma once

enum zmk_transport {
    ZMK_TRANSPORT_USB = 0,
    ZMK_TRANSPORT_BLE = 1,
};

struct zmk_endpoint_instance {
    enum zmk_transport transport;
};

struct zmk_endpoint_instance zmk_endpoint_get_selected(void);
