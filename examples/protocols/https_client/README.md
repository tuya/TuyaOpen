# HTTP Introduction
The `example_https_client` routine demonstrates how to use the native HTTPS interface.

## Execution Results
```c
[01-03 06:10:17 ty D][iotdns.c:288] iotdns query [{"host":"httpbin.org", "port":443, "need_ca":true}]
[01-03 06:10:17 ty D][iotdns.c:108] http request send!
[01-03 06:10:17 ty D][tcp_transporter.c:102] bind ip:00000000 port:0 ok
[01-03 06:10:17 ty D][tuya_tls.c:570] TUYA_TLS Begin Connect h6.iot-dns.com:443
[01-03 06:10:17 ty D][tuya_tls.c:338] mbedtls authmode: MBEDTLS_SSL_VERIFY_REQUIRED
[01-03 06:10:17 ty D][tuya_tls.c:351] load root ca cert.
[01-03 06:10:17 ty D][tuya_tls.c:641] socket fd is set. set to inner send/recv to handshake
[01-03 06:10:17 ty D][tuya_tls.c:320] mbedtls_cert_pkey_free.
[01-03 06:10:17 ty D][tuya_tls.c:682] handshake finish for h6.iot-dns.com. set send/recv to user set
[01-03 06:10:17 ty D][tuya_tls.c:688] TUYA_TLS Success Connect h6.iot-dns.com:443 Suit:TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256
[01-03 06:10:17 ty D][http_client_wrapper.c:114] tls connencted!
[01-03 06:10:17 ty D][http_client_wrapper.c:142] http request send!
[01-03 06:10:17 ty D][http_client_wrapper.c:40] HTTP header add key:value
key=User-Agent : value=TUYA_OPEN_SDK
[01-03 06:10:17 ty D][http_client_wrapper.c:40] HTTP header add key:value
key=Content-Type : value=application/x-www-form-urlencoded;charset=UTF-8
[01-03 06:10:17 ty D][http_client_wrapper.c:51] Sending HTTP POST request to h6.iot-dns.com/device/dns_query
[01-03 06:10:17 ty D][http_client_wrapper.c:68] Response Headers:
content-length: 1570
content-type: application/json
date: Wed, 12 Feb 2025 07:57:15 GMT
server: https
Response Status:
200
Response Body:
[{"host":"httpbin.org","ca":"MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNVBAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIwMAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJVUzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UEChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZpZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0NTm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRoOt+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0CzyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg
[01-03 06:10:17 ty D][tls_transporter.c:122] tls transporter close socket fd:4
[01-03 06:10:17 ty D][tcp_transporter.c:194] tcp transporter close socket fd:4
[01-03 06:10:17 ty D][tls_transporter.c:130] tls transporter close tls handler:0x7815bb1fbb70
[01-03 06:10:17 ty D][tuya_tls.c:799] TUYA_TLS Disconnect ENTER
[01-03 06:10:17 ty D][tuya_tls.c:824] TUYA_TLS Disconnect Success
[01-03 06:10:17 ty D][tuya_tls.c:496] tuya_tls_connect_destroy.
[01-03 06:10:17 ty D][example_https_client.c:90] http request send!
[01-03 06:10:17 ty D][tcp_transporter.c:102] bind ip:00000000 port:0 ok
[01-03 06:10:17 ty D][tuya_tls.c:570] TUYA_TLS Begin Connect httpbin.org:443
[01-03 06:10:17 ty D][tuya_tls.c:338] mbedtls authmode: MBEDTLS_SSL_VERIFY_REQUIRED
[01-03 06:10:17 ty D][tuya_tls.c:351] load root ca cert.
[01-03 06:10:17 ty D][tuya_tls.c:641] socket fd is set. set to inner send/recv to handshake
[01-03 06:10:18 ty D][tuya_tls.c:320] mbedtls_cert_pkey_free.
[01-03 06:10:18 ty D][tuya_tls.c:682] handshake finish for httpbin.org. set send/recv to user set
[01-03 06:10:18 ty D][tuya_tls.c:688] TUYA_TLS Success Connect httpbin.org:443 Suit:TLS-ECDHE-RSA-WITH-AES-128-GCM-SHA256
[01-03 06:10:18 ty D][http_client_wrapper.c:114] tls connencted!
[01-03 06:10:18 ty D][http_client_wrapper.c:142] http request send!
[01-03 06:10:18 ty D][http_client_wrapper.c:40] HTTP header add key:value
key=Content-Type : value=application/json
[01-03 06:10:18 ty D][http_client_wrapper.c:51] Sending HTTP GET request to httpbin.org/get
[01-03 06:10:19 ty D][http_client_wrapper.c:68] Response Headers:
Date: Wed, 12 Feb 2025 07:57:16 GMT
Content-Type: application/json
Content-Length: 276
Connection: keep-alive
Server: gunicorn/19.9.0
Access-Control-Allow-Origin: *
Access-Control-Allow-Credentials: true
Response Status:
200
Response Body:
{
  "args": {}, 
  "headers": {
    "Content-Type": "application/json", 
    "Host": "httpbin.org", 
    "User-Agent": "TUYA_IOT_SDK", 
    "X-Amzn-Trace-Id": "Root=1-67ac545c-6b40081161add1ce1d14361d"
  }, 
  "origin": "115.236.167.98", 
  "url": "https://httpbin.org/get"
}


[01-03 06:10:19 ty D][tls_transporter.c:122] tls transporter close socket fd:4
[01-03 06:10:19 ty D][tcp_transporter.c:194] tcp transporter close socket fd:4
[01-03 06:10:19 ty D][tls_transporter.c:130] tls transporter close tls handler:0x7815bb1fb6f0
[01-03 06:10:19 ty D][tuya_tls.c:799] TUYA_TLS Disconnect ENTER
[01-03 06:10:19 ty D][tuya_tls.c:824] TUYA_TLS Disconnect Success
[01-03 06:10:19 ty D][tuya_tls.c:496] tuya_tls_connect_destroy.
[01-03 06:10:19 ty D][example_https_client.c:112] http_get_example body: 
{
  "args": {}, 
  "headers": {
    "Content-Type": "application/json", 
    "Host": "httpbin.org", 
    "User-Agent": "TUYA_IOT_SDK", 
    "X-Amzn-Trace-Id": "Root=1-67ac545c-6b40081161add1ce1d14361d"
  }, 
  "origin": "115.236.167.98", 
  "url": "https://httpbin.org/get"
}
```
The response content is extensive, and the log can only display the first 1K of data.
