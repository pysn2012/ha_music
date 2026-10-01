#pragma once
// WiFi 兼容性补丁 —— 针对部分运营商光猫/网关与 ESP32-C3 反复 "Auth Expired"、
// 多轮重试才偶尔连上的问题, 把 STA 降级成"传统客户端":
//   1. 强制 802.11b/g/n: ESP32-C3 默认会协商 WiFi6(HE), 不少光猫的 OFDMA/HE
//      实现有缺陷, 传统 11n 客户端兼容性最好;
//   2. 关闭 PMF(802.11w): 网关为 WPA2/WPA3 混合模式时强制走纯 WPA2-PSK,
//      避开 SAE 认证超时这一最常见的不兼容点。
//
// 由 YAML 在两个时机调用: esphome.on_boot(WiFi 组件初始化完成后) 与
// wifi.on_disconnect(每次断开/认证失败后, 保证下一轮重试一定带新配置)。
// 返回 0 = 成功, 非 0 = esp_err_t 错误码(会打进串口日志, 便于确认生效)。

#include "esp_wifi.h"

inline int apply_wifi_compat() {
  int rc = 0;

  // 只改 pmf 两个标志, ssid/psk 等其余字段由 get/set 原样带回, 不影响配网
  wifi_config_t conf;
  if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
    conf.sta.pmf_cfg.capable = false;
    conf.sta.pmf_cfg.required = false;
    rc |= (int) esp_wifi_set_config(WIFI_IF_STA, &conf);
  } else {
    rc |= -1;
  }

  rc |= (int) esp_wifi_set_protocol(WIFI_IF_STA,
                                    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                        WIFI_PROTOCOL_11N);
  return rc;
}
