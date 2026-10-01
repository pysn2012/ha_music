#pragma once
// WiFi 兼容性补丁 —— 复刻 MPY 在本板实测 10/10 稳定的那组设置(全部运行时调用):
//   1. esp_wifi_set_max_tx_power(34): 34 = 8.5dBm(单位 0.25dBm)。
//      压低 TX 电流尖峰(满功率 ~330mA -> ~130mA), 避免拉垮弱供电的 3.3V 轨;
//   2. esp_wifi_set_protocol(11b/g/n): 放弃 WiFi6/HE, 缩小与老网关的协商差异。
//
// 调用时机(重要!): 只在 wifi.on_disconnect(断开/认证失败后的空闲时刻)调用。
//   - 不要在 on_boot 调: 启动扫描中途动驱动, 实测挂死首次认证(2026.9);
//   - 不要用 YAML 的 output_power: 那是编译期 PHY 参数
//     (CONFIG_ESP_PHY_MAX_TX_POWER), 实测同样挂死认证, 必须用运行时 API。
// 首次连接会以满功率/默认模式先试一次(快速失败, 原有行为), 本函数随后把它
// 切到 MPY 验证过的组合, 第二轮起稳定连接。
// 返回 0 = 成功, 非 0 = esp_err_t 错误码(打进串口日志, 便于确认生效)。

#include "esp_wifi.h"

inline int apply_wifi_compat() {
  int rc = (int) esp_wifi_set_max_tx_power(34);   // 8.5dBm
  rc |= (int) esp_wifi_set_protocol(WIFI_IF_STA,
                                    WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                        WIFI_PROTOCOL_11N);
  return rc;
}
