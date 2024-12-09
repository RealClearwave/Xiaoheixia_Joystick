# Xiaoheixia_Joystick
A Wifi X360 Joystick midified from Xiaoheixia remotes

## WIFI连接
通过adb shell进入命令行，输入以下命令连接WiFi，其中WIFI6_TEST和1234567890分别替换为自己的ssid和密码

```
wpa_cli scan
sleep 1
wpa_cli scan_results
sleep 1
wpa_cli add_network
wpa_cli set_network 0 ssid "\"WIFI6_TEST\""
wpa_cli set_network 0 psk "\"1234567890\""
wpa_cli enable_network 0
wpa_cli save_config
wpa_cli status
wpa_cli quit
```
