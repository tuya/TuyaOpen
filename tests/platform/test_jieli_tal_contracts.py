import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
ADAPTER = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter"


class JieliTalContractTest(unittest.TestCase):
    def test_platform_adapter_contains_only_tkl_sources(self):
        self.assertFalse(list((ADAPTER / "src").glob("tal_*.c")))
        self.assertFalse(list((ADAPTER / "include").glob("tal_*_port.h")))
        for relative in (
            "src/system/tkl_thread.c",
            "src/system/tkl_mutex.c",
            "src/system/tkl_semaphore.c",
            "src/system/tkl_queue.c",
            "src/system/tkl_sleep.c",
            "src/driver/tkl_uart.c",
        ):
            self.assertTrue((ADAPTER / relative).is_file(), relative)

    def test_tkl_sources_do_not_reference_platform_tal_port(self):
        for source in (ADAPTER / "src").rglob("tkl_*.c"):
            text = source.read_text(encoding="utf-8")
            self.assertNotIn("tal_system_port", text, source.name)
            self.assertNotIn("tal_uart_port", text, source.name)

    def test_uart_contract_keeps_jieli_device_api_private(self):
        source = (ADAPTER / "src/driver/tkl_uart.c").read_text(encoding="utf-8")
        self.assertIn("dev_open", source)
        self.assertIn("dev_read", source)
        self.assertIn("dev_write", source)

    def test_network_layer_uses_lwip_socket_api(self):
        source = (ADAPTER / "src/driver/tkl_network.c").read_text(encoding="utf-8")
        self.assertIn("<lwip/sockets.h>", source)
        for symbol in ("tkl_net_socket_create", "tkl_net_connect", "tkl_net_send", "tkl_net_recv",
                       "tkl_net_gethostbyname"):
            self.assertIn(symbol, source)

    def test_wifi_layer_uses_wl82_native_station_api(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        for symbol in ("wifi_set_event_callback", "wifi_enter_sta_mode", "wifi_get_sta_connect_state",
                       "wifi_scan_req", "lwip_get_netif_info"):
            self.assertIn(symbol, source)
        self.assertIn("result[i].ssid_len > WIFI_SSID_LEN", source)

    def test_switch_entry_registers_wl82_bluetooth_tasks(self):
        source = (ROOT / "platform/JIELI/tuyaos_switch_app_main.c").read_text(encoding="utf-8")
        self.assertIn('#ifdef CONFIG_BT_ENABLE', source)
        self.assertIn('{ "#C0btctrler", 19, 512, 384 }', source)
        self.assertIn('{ "#C0btstack", 18, 1024, 384 }', source)
        self.assertIn('{ "btctrler", 19, 512, 384 }', source)
        self.assertIn('{ "btstack", 18, 768, 384 }', source)

    def test_bluetooth_sets_derived_mac_before_stack_start(self):
        source = (ADAPTER / "src/driver/tkl_bluetooth.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_ble_stack_init")
        end = source.index("OPERATE_RET tkl_ble_stack_deinit", start)
        init_source = source[start:end]
        address_setup = init_source.index("lib_make_ble_address")
        stack_start = init_source.index("btstack_init()")
        self.assertLess(address_setup, stack_start)
        for symbol in ("bt_get_mac_addr", "le_controller_set_mac", "lmp_set_sniff_disable"):
            self.assertIn(symbol, source)

    def test_wifi_startup_accepts_vendor_async_start(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        self.assertIn("wifi_is_on", source)
        self.assertIn("wifi_on", source)
        self.assertIn("COUNTRY_CODE_CN", source)

    def test_wifi_init_disables_saved_sta_autoconnect_before_wifi_on(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_init")
        end = source.index("OPERATE_RET tkl_wifi_scan_ap", start)
        init_source = source[start:end]
        self.assertIn("wifi_set_sta_connect_best_ssid(0)", init_source)
        self.assertNotIn("result = wifi_on()", init_source)
        self.assertIn("WiFi start is deferred", init_source)

    def test_wifi_station_connect_starts_deferred_native_wifi(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_station_connect")
        end = source.index("OPERATE_RET tkl_wifi_station_disconnect", start)
        station_source = source[start:end]
        # The deferred module bring-up runs on a resident worker thread fed by
        # a queue: creating a task (or driving wifi_on()) from the BLE
        # workqueue context crashed the FreeRTOS list code (2026-09-18 logs,
        # identical with 5120 and 11264 byte stacks).
        station_start = start
        worker_start = source.index("static void jieli_sta_connect_work")
        worker_source = source[worker_start:station_start] if station_start > worker_start \
            else source[worker_start:source.index("OPERATE_RET tkl_wifi_station_connect", worker_start)]
        self.assertIn("tkl_queue_post", station_source)
        self.assertIn("wifi_is_on()", worker_source)
        self.assertIn("wifi_on()", worker_source)
        self.assertIn("wifi_enter_sta_mode", worker_source)
        # set_work_mode(WWM_STATION) must not power the module up inline.
        mode_start = source.index("OPERATE_RET tkl_wifi_set_work_mode")
        mode_end = source.index("OPERATE_RET tkl_wifi_get_work_mode", mode_start)
        mode_source = source[mode_start:mode_end]
        self.assertNotIn("wifi_on()", mode_source)

    def test_wifi_low_power_is_a_safe_noop_on_wl82(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_set_lp_mode")
        end = source.index("OPERATE_RET tkl_wifi_station_fast_connect", start)
        self.assertIn("return OPRT_OK", source[start:end])

    def test_wifi_softap_mode_is_started_by_ap_config(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_set_work_mode")
        end = source.index("OPERATE_RET tkl_wifi_get_work_mode", start)
        self.assertIn("case WWM_SOFTAP:", source[start:end])
        self.assertIn("return OPRT_OK", source[start:end])
        self.assertIn("wifi_enter_ap_mode", source)

    def test_wifi_ap_start_does_not_shutdown_shared_network_stack(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_start_ap")
        end = source.index("OPERATE_RET tkl_wifi_stop_ap", start)
        ap_source = source[start:end]
        self.assertIn("wifi_set_sta_connect_best_ssid(0)", ap_source)
        self.assertNotIn("wifi_off", ap_source)

    def test_wifi_ap_start_disables_sta_autoconnect_after_mode_switch(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_start_ap")
        end = source.index("OPERATE_RET tkl_wifi_stop_ap", start)
        ap_source = source[start:end]
        self.assertGreater(
            ap_source.rindex("wifi_set_sta_connect_best_ssid(0)"),
            ap_source.index("wifi_enter_ap_mode"),
        )

    def test_wifi_ap_start_event_disables_sta_autoconnect(self):
        source = (ADAPTER / "src/driver/tkl_wifi.c").read_text(encoding="utf-8")
        callback_start = source.index("static int jieli_wifi_event_cb")
        callback_end = source.index("static uint8_t jieli_auth_mode", callback_start)
        callback_source = source[callback_start:callback_end]
        self.assertIn("case JIELI_WIFI_AP_START:", callback_source)
        ap_event = callback_source.index("case JIELI_WIFI_AP_START:")
        disable = callback_source.index("wifi_set_sta_connect_best_ssid(0)", ap_event)
        self.assertGreater(disable, ap_event)

    def test_bluetooth_advertising_waits_for_controller_ready(self):
        source = (ADAPTER / "src/driver/tkl_bluetooth.c").read_text(encoding="utf-8")
        self.assertIn("BT_STATUS_INIT_OK", source)
        self.assertIn("s_ble_stack_ready", source)
        self.assertIn("jieli_ble_apply_advertising", source)
        self.assertIn("ll_hci_adv_set_params", source)
        self.assertIn("ll_hci_adv_set_data", source)
        self.assertIn("ll_hci_adv_enable", source)
        self.assertNotIn("gap_advertisements_enable(1)", source)
        self.assertIn("BLE advertising enabled", source)

    def test_bluetooth_layer_uses_wl82_native_le_api(self):
        source = (ADAPTER / "src/driver/tkl_bluetooth.c").read_text(encoding="utf-8")
        for symbol in ("btstack_init", "ble_user_cmd_prepare", "ll_hci_adv_enable",
                       "gatt_client_write_value_of_characteristic", "user_client_report_search_result",
                       "user_client_report_data_callback", "user_client_search_descriptor_is_enable"):
            self.assertIn(symbol, source)
        self.assertIn("ADV_DIRECT_IND_LOW", source)

    def test_bluetooth_att_transport_matches_jieli_net_cfg(self):
        source = (ADAPTER / "src/driver/tkl_bluetooth.c").read_text(encoding="utf-8")
        self.assertIn("#define JIELI_ATT_LOCAL_PAYLOAD_SIZE (200)", source)
        self.assertIn("#define JIELI_ATT_SEND_CBUF_SIZE     (512)", source)
        self.assertIn("(ATT_CTRL_BLOCK_SIZE + JIELI_ATT_LOCAL_PAYLOAD_SIZE + JIELI_ATT_SEND_CBUF_SIZE)", source)
        self.assertIn("ble_vendor_set_default_att_mtu(JIELI_ATT_LOCAL_PAYLOAD_SIZE)", source)
        self.assertIn("ATT_EVENT_MTU_EXCHANGE_COMPLETE", source)

    def test_kv_and_authorize_diagnostics_keep_both_key_results_visible(self):
        kv_source = (ROOT / "src/tal_kv/src/tal_kv.c").read_text(encoding="utf-8")
        auth_source = (ROOT / "src/tuya_cloud_service/authorize/tuya_authorize.c").read_text(encoding="utf-8")
        main_source = (ROOT / "apps/tuya_cloud/switch_demo/src/tuya_main.c").read_text(encoding="utf-8")
        self.assertIn("[KV] partition", kv_source)
        self.assertIn("[KV] mount", kv_source)
        self.assertIn("uuid_rt", auth_source)
        self.assertIn("authkey_rt", auth_source)
        self.assertIn('tuya_debug_log_license("compile_fallback"', main_source)

    def test_ble_crypto_diagnostics_track_session_key_state(self):
        crypto_source = (ROOT / "src/tuya_cloud_service/ble/ble_cryption.c").read_text(encoding="utf-8")
        crypto_header = (ROOT / "src/tuya_cloud_service/ble/ble_cryption.h").read_text(encoding="utf-8")
        manager_source = (ROOT / "src/tuya_cloud_service/ble/ble_mgr.c").read_text(encoding="utf-8")
        self.assertIn("key_out_key11_valid", crypto_source)
        self.assertIn("tuya_ble_crypto_reset", crypto_header)
        self.assertIn("tuya_ble_crypto_reset", manager_source)

    def test_ble_write_event_reports_workqueue_result(self):
        manager_source = (ROOT / "src/tuya_cloud_service/ble/ble_mgr.c").read_text(encoding="utf-8")
        self.assertIn("[BLE][QUEUE] write event", manager_source)
        self.assertIn("[BLE][QUEUE] write event process", manager_source)
        self.assertIn("ble event queue schedule failed", manager_source)

    def test_jieli_write_callback_stays_quiet_on_btstack_task(self):
        source = (ADAPTER / "src/driver/tkl_bluetooth.c").read_text(encoding="utf-8")
        start = source.index("jieli_ble_att_write_callback")
        end = source.index("void ble_profile_init", start)
        callback = source[start:end]
        # One UART line costs ~5ms at 115200; printing from the ATT write
        # callback stalled the host and triggered the controller NACK
        # livelock on the 197-byte provisioning write (2026-09-18 log).
        # The workqueue side in ble_mgr reports the queued payload instead.
        self.assertNotIn("printf(", callback)
        self.assertNotIn("[JIELI][BLE] write dispatch", source)
        # The wide TKL event must stay off the btstack task stack; ATT
        # callbacks are serialized so a shared static is safe.
        self.assertIn("static TKL_BLE_GATT_PARAMS_EVT_T event", callback)
        self.assertIn("s_gatt_callback(&event)", callback)

    def test_switch_demo_starts_ble_netcfg_without_concurrent_softap(self):
        source = (ROOT / "apps/tuya_cloud/switch_demo/src/tuya_main.c").read_text(encoding="utf-8")
        self.assertIn("netmgr_conn_set(NETCONN_WIFI, NETCONN_CMD_NETCFG, &(netcfg_args_t){.type = NETCFG_TUYA_BLE});",
                      source)
        self.assertNotIn("NETCFG_TUYA_BLE | NETCFG_TUYA_WIFI_AP", source)


if __name__ == "__main__":
    unittest.main()
