# TAL Network Diagnostics

This example exercises the TAL network API directly, without Tuya IoT,
provisioning, HTTP client or TLS.

## Build

Select any compatible TuyaOpen platform and board using the normal project
configuration flow, then build the example:

```sh
cd examples/system/os_network
tos.py build
```

The example does not define its own board or platform configuration. Wi-Fi
commands are available when the selected configuration enables `ENABLE_WIFI`.

## Commands

```text
net_wifi <ssid> <password>
net_dns <host>
net_tcp <host> <port>
net_http <host> [port] [path]
net_multi_http <host1> <host2>
net_ap_server [tcp_port udp_port]
net_udp <host> <port> <text>
```

`net_dns` tests `tal_net_gethostbyname()`. `net_tcp` tests
`tal_net_socket_create()`, timeout setup, `tal_net_connect()` and close.
`net_http` adds send/receive through a raw HTTP GET. `net_multi_http` keeps two
TCP sockets connected at the same time and verifies both HTTP responses.
`net_ap_server` starts TCP and UDP listeners concurrently and waits for one TCP
accept plus one UDP datagram. `net_udp` exercises `tal_net_send_to()`.
