#pragma once
// WiFi 兼容性补丁 —— 针对光猫/网关反复 "Auth Expired" 的问题,
// 把 STA 强制降级为传统客户端: 802.11b/g/n(放弃 WiFi6/HE)。
// 与 YAML 里 output_power: 8.5dBm(压低 TX 电流尖峰)配套使用。
//
// 调用时机(重要!): 只在 wifi.on_disconnect(断开/认证失败后的空闲时刻)调用。
// 严禁在 on_boot 里调 —— 实测 2026.9 在启动扫描进行中调 esp_wifi_set_protocol
// 会把首次认证流程挂死(无任何事件, 直到 10 分钟兜底重启), 表现为重启循环。
//
// 协议位图存于驱动层, 不随每次 connect 重置; 断开后设置, 下一轮连接生效。
// 返回 0 = 成功, 非 0 = esp_err_t 错误码(打进串口日志, 便于确认生效)。

#include "esp_wifi.h"

inline int apply_wifi_compat() {
  return (int) esp_wifi_set_protocol(WIFI_IF_STA,
                                     WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                         WIFI_PROTOCOL_11N);
}
