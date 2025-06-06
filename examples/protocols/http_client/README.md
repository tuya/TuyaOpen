# HTTP Introduction
The `example_http` routine demonstrates how to use the native HTTP interface.

## Execution Results
```c
[01-01 14:58:54 ty D][lr:0xadbd3] Connect: httpbin.org Port: 80  -->>
[01-01 14:58:54 ty D][lr:0x7a689] unw_gethostbyname httpbin.org, prio 1
[01-01 14:58:54 ty D][lr:0x7a7b5] use dynamic dns ip:44.194.147.17 for domain:httpbin.org
[01-01 14:58:54 ty D][lr:0xbc05d] bind ip:c0a81cc2 port:0 ok
[01-01 14:58:54 ty D][lr:0xadbed] Connect: httpbin.org Port: 80  --<< ,r:0
[01-01 14:58:54 ty I][example_http.c:126] rsp:{
  "args": {}, 
  "headers": {
    "Host": "httpbin.org", 
    "User-Agent": "TUYA_IOT_SDK", 
    "X-Amzn-Trace-Id": "Root=1-65308a26-2d1b68cd71ac3ac235a5020a"
  }, 
  "origin": "124.90.34.126", 
  "url": "h[01-01 14:58:54 ty D][lr:0xbc0fd] tcp transporter close socket fd:4
```
The response content is extensive, and the log can only display the first 1K of data.

## Technical Support

You can get support from Tuya through the following methods:

- TuyaOS Forum: https://www.tuyaos.com

- Developer Center: https://developer.tuya.com

- Help Center: https://support.tuya.com/help

- Technical Support Ticket Center: https://service.console.tuya.com