import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
ADAPTER = ROOT / "platform/JIELI/tuyaos/tuyaos_adapter"


class JieliTalContractTest(unittest.TestCase):
    def test_platform_adapter_contains_only_tkl_sources(self):
        self.assertFalse(list((ADAPTER / "src").glob("tal_*.c")))
        self.assertFalse(list((ADAPTER / "include").glob("tal_*_port.h")))
        for relative in (
            "src/tkl_thread.c",
            "src/tkl_mutex.c",
            "src/tkl_semaphore.c",
            "src/tkl_queue.c",
            "src/tkl_sleep.c",
            "src/tkl_uart.c",
        ):
            self.assertTrue((ADAPTER / relative).is_file(), relative)

    def test_tkl_sources_do_not_reference_platform_tal_port(self):
        for source in (ADAPTER / "src").glob("tkl_*.c"):
            text = source.read_text(encoding="utf-8")
            self.assertNotIn("tal_system_port", text, source.name)
            self.assertNotIn("tal_uart_port", text, source.name)

    def test_uart_contract_keeps_jieli_device_api_private(self):
        source = (ADAPTER / "src/tkl_uart.c").read_text(encoding="utf-8")
        self.assertIn("dev_open", source)
        self.assertIn("dev_read", source)
        self.assertIn("dev_write", source)

    def test_network_layer_uses_lwip_socket_api(self):
        source = (ADAPTER / "src/tkl_network.c").read_text(encoding="utf-8")
        self.assertIn("<lwip/sockets.h>", source)
        for symbol in ("tkl_net_socket_create", "tkl_net_connect", "tkl_net_send", "tkl_net_recv",
                       "tkl_net_gethostbyname"):
            self.assertIn(symbol, source)

    def test_wifi_layer_uses_wl82_native_station_api(self):
        source = (ADAPTER / "src/tkl_wifi.c").read_text(encoding="utf-8")
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
        source = (ADAPTER / "src/tkl_bluetooth.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_ble_stack_init")
        end = source.index("OPERATE_RET tkl_ble_stack_deinit", start)
        init_source = source[start:end]
        address_setup = init_source.index("lib_make_ble_address")
        stack_start = init_source.index("btstack_init()")
        self.assertLess(address_setup, stack_start)
        for symbol in ("bt_get_mac_addr", "le_controller_set_mac", "lmp_set_sniff_disable"):
            self.assertIn(symbol, source)

    def test_wifi_startup_accepts_vendor_async_start(self):
        source = (ADAPTER / "src/tkl_wifi.c").read_text(encoding="utf-8")
        self.assertIn("wifi_is_on", source)
        self.assertIn("wifi_on", source)
        self.assertIn("COUNTRY_CODE_CN", source)

    def test_wifi_low_power_is_a_safe_noop_on_wl82(self):
        source = (ADAPTER / "src/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_set_lp_mode")
        end = source.index("OPERATE_RET tkl_wifi_station_fast_connect", start)
        self.assertIn("return OPRT_OK", source[start:end])

    def test_wifi_softap_mode_is_started_by_ap_config(self):
        source = (ADAPTER / "src/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_set_work_mode")
        end = source.index("OPERATE_RET tkl_wifi_get_work_mode", start)
        self.assertIn("case WWM_SOFTAP:", source[start:end])
        self.assertIn("return OPRT_OK", source[start:end])
        self.assertIn("wifi_enter_ap_mode", source)

    def test_wifi_ap_start_does_not_shutdown_shared_network_stack(self):
        source = (ADAPTER / "src/tkl_wifi.c").read_text(encoding="utf-8")
        start = source.index("OPERATE_RET tkl_wifi_start_ap")
        end = source.index("OPERATE_RET tkl_wifi_stop_ap", start)
        ap_source = source[start:end]
        self.assertIn("wifi_set_sta_connect_best_ssid(0)", ap_source)
        self.assertNotIn("wifi_off", ap_source)

    def test_bluetooth_advertising_waits_for_controller_ready(self):
        source = (ADAPTER / "src/tkl_bluetooth.c").read_text(encoding="utf-8")
        self.assertIn("BT_STATUS_INIT_OK", source)
        self.assertIn("s_ble_stack_ready", source)
        self.assertIn("jieli_ble_apply_advertising", source)
        self.assertIn("ll_hci_adv_set_params", source)
        self.assertIn("ll_hci_adv_set_data", source)
        self.assertIn("ll_hci_adv_enable", source)
        self.assertNotIn("gap_advertisements_enable(1)", source)
        self.assertIn("BLE advertising enabled", source)

    def test_bluetooth_layer_uses_wl82_native_le_api(self):
        source = (ADAPTER / "src/tkl_bluetooth.c").read_text(encoding="utf-8")
        for symbol in ("btstack_init", "ble_user_cmd_prepare", "ll_hci_adv_enable",
                       "gatt_client_write_value_of_characteristic", "user_client_report_search_result",
                       "user_client_report_data_callback", "user_client_search_descriptor_is_enable"):
            self.assertIn(symbol, source)
        self.assertIn("ADV_DIRECT_IND_LOW", source)


if __name__ == "__main__":
    unittest.main()
