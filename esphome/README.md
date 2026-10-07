# ESP32-C3 无线音乐音箱 — ESPHome 固件

给自制板 **ESP32C3-插件版本**（ESP32-C3 SuperMini + MAX98357A + 3525 3W 扬声器 + 2×WS2812 眼睛灯 + 振动传感器 + 震动马达）写的无线音乐播放固件。

固件由 Home Assistant 当"音源"：HA 把本地音乐/电台/TTS 转码成 MP3/WAV，以 HTTP 流推给板子播放（需 HA 2024.10+）。手机（安卓可用 HA Companion App）、电脑、语音助手都能点歌，不需要苹果设备，也不依赖 AirPlay。

## 文件

| 文件 | 说明 |
|---|---|
| `esp32c3_player.yaml` | 固件主配置（含完整引脚注释） |
| `wifi_compat.h` | WiFi 兼容性补丁（强制 11b/g/n、关闭 PMF，见"WiFi 一直连不上"一节） |
| `secrets.yaml` | WiFi 名称与密码（**编译前必填**，CI 由仓库 Secrets 生成） |
| `schematic/Schematic1.png` | 板子的原理图高清导出 |
| `schematic/netlist.txt` | 原理图网表（引脚连接的权威依据） |
| `schematic/MAX98357A_datasheet.pdf` | 功放数据手册（SD/GAIN 控制依据） |

## 烧录步骤

1. 编辑 [secrets.yaml](secrets.yaml)：填入 WiFi 名称和密码。
2. 编译烧录，二选一：
   - **本地命令行（调试期推荐，一条命令=编译+烧录+看日志）**：`pip install esphome==2026.9.0` 后执行 `esphome run esp32c3_player.yaml`（目录里已放好本地用的 `secrets.yaml`，已被 .gitignore 排除；首次编译要下载 ESP-IDF 工具链 10~20 分钟，之后增量编译 1~2 分钟）。
   - **GitHub Actions**：推送后手动触发 `build esphome`，下载 Artifact 用 [web.esphome.io](https://web.esphome.io) 或 esptool 从 USB 刷入（工具链已加缓存，重复构建 3~5 分钟）。
3. 板子用 USB-C 线连接电脑。首次烧录如果识别不到串口：**按住板上的 BOOT 键再插 USB**，松开后开始烧录。
4. 烧录完成后设备从电池/USB 供电即可。**刷机前建议先全片擦除**：`esptool --port COM口 --chip esp32c3 erase_flash`——板上先后跑过多套固件栈（ESPHome 2026.9 多版、MicroPython、2025.12）时，NVS/RF 校准分区残留会捣乱（ESPEasy#5638 的同款教训）；顺手用 `esptool flash_id` 看一眼 flash 品牌，克隆板常见 XMC，个别固件栈版本对它有兼容怪癖。本固件**不启用配网热点**（WiFi 凭据由 CI 烧死，重配=重刷），WiFi 连不上时设备会每 10 分钟自动重启重试。

## 接入 Home Assistant

1. HA 自动发现设备后（设置 → 设备与服务 → 发现的设备），实体如下：
   - `media_player.speaker_music_player` — 播放器本体
   - `light.eye_leds` — 眼睛灯（播放时自动进入律动效果）
   - `switch.amp_enable` — 功放使能（固件默认常开，正常情况不用管）
   - `switch.vibration_motor` — 震动马达（也可用于 HA 通知时震动）
   - `binary_sensor.vibration_sensor` — 振动传感器
   - `sensor.wifi_signal` — WiFi 信号 / `button.restart` — 重启
2. 播放音乐的方式（**不装 Music Assistant 也完全够用**）：
   - **本地媒体库（推荐）**：把 mp3/flac 放进 HA 的 `/media/music`（HA OS 用 Samba 插件访问），媒体面板 → Local Media → 选歌 → "在此播放"选这个音箱；配合播放器"循环"模式可单曲循环，敲击启动播放的就是上一次点的内容。
   - **NAS 免拷贝**：设置 → 系统 → 存储 → 添加网络存储（SMB/NFS），NAS 的音乐目录挂进 `/media` 后自动出现在媒体库。
   - **电台/直链**：自动化里用 `media_player.play_media` 直接推流媒体 URL（网络电台、MP3 直链）。
   - **Music Assistant（可选）**：只在需要曲库管理、歌单队列、多房间同步或 Spotify 等在线源时才值得装。
   - TTS/通知走 announcement 流水线（44.1kHz WAV 单声道，喇叭以混合模式输出）。
3. **敲击交互**（固件内置）：
   - 待机/暂停状态敲一下机身 → 马达短震 120ms 确认 + 开始播放
   - 播放状态自动忽略敲击（避免喇叭低频和马达自震误触发），**暂停/切歌用 HA 或 Music Assistant 操作**
   - 注意：新设备第一次使用时需要先在 HA 里点播过一次内容，之后敲击启动才有歌可放
   - 传感器实体始终在 HA 里可见，想加"设备被挪动提醒"等自动化直接用它
4. **眼睛灯**：播放时自动进入 `Music Pulse` 随机律动（内置 random 效果，每 120ms 随机亮度跳变，与 eetree_ai 的"语音律动"同款，无自定义代码），暂停/停止自动熄灭；也可以在 HA 里手动控制颜色和亮度。

## 定时播放（5 组闹钟，时间在 HA 设备页面设置）

固件通过 `datetime` 组件在 HA 设备页面暴露 **Alarm 1 ~ Alarm 5** 五个时间选择器（值存在设备上，断电保留），到点动作在固件里执行：马达短震 120ms 回执 → 音量 60% → 通过 `homeassistant.service` 指挥 HA 播放 `/media/music/morning.mp3`（对时用 SNTP，Asia/Shanghai + 阿里云 NTP）。

- **改时间/停用**：HA 设备页面点开即改，不用重编固件；时间设成 `00:00:00` = 该组停用（固件有守卫，不会零点误触发）；
- 新烧录后闹钟是"未知"状态，在设备页面设置时间后生效；
- 换铃声改固件里对应组的 `media_content_id`、增减组数才需要重编。

## 功放 SD/GAIN 控制原理（按数据手册 Table 5，模块 SD 带 1MΩ 上拉到 5V）

| GPIO5 状态 | SD 电压 | 功放输出 |
|---|---|---|
| 开关"开"（引脚释放，高阻） | ≈0.45V（1M÷(1M+100k) 分压） | **(左/2+右/2) 混合 ✓ 正常播放** |
| 开关"关"（拉低） | 0V | 关机 |
| 推挽拉高（禁用） | 3.3V | 仅左声道 ✗ |

GAIN（GPIO6）保持悬空 = 9dB 默认增益；配置里备有注释掉的"拉低=12dB"开关，**不要拉高**（模块 VDD=5V，3.3V 恰好落进 3dB 最小音量档）。

### 学习：SD 阈值是绝对电压，档位不随供电变化（5V vs 3.3V）

SD 的三个比较器阈值（0.16 / 0.77 / 1.4V）是数据手册给出的**绝对电压**，与模块供电无关；而模块 1MΩ 上拉与芯片内部 100kΩ 下拉的分压比也固定。所以 GPIO5 各状态在两种供电下结果一致：

| GPIO5 状态 | VCC=5V：SD 电压 → 模式 | VCC=3.3V：SD 电压 → 模式 |
|---|---|---|
| 悬空 / 纯输入（高阻） | ≈0.45V → (L/2+R/2) 混合 | ≈0.30V → (L/2+R/2) 混合 |
| 输入 + 开内部上拉（≈45k） | ≈2.36V → 仅左声道 ✗ | ≈2.31V → 仅左声道 ✗ |
| 输入 + 开内部下拉（≈45k） | ≈0.15V → 临界，不稳定 ✗ | ≈0.10V → 关机（不可靠）✗ |
| 推挽拉高 3.3V | 3.3V → 仅左声道 ✗ | 3.3V → 仅左声道 ✗ |
| 推挽拉低 / 开漏"关" | 0V → 关机 | 0V → 关机 |

对比：**GAIN 的阈值是相对 VDD 的比例**（如 6dB=0.9–1.0×VDD、3dB=0.65–0.85×VDD），档位会随供电变化——3.3V 供电的模块 GPIO 拉高 GAIN 恰好是 6dB 档，5V 供电时同样的 3.3V 却落进 3dB 档。同一颗芯片两种阈值体系，这是读 MAX98357A 手册最容易混淆的点。

## 调优（ESP32-C3 无 PSRAM，量力而行）

| 现象 | 处理 |
|---|---|
| 启动后重启/内存报错 | `buffer_size` 从 65536 降到 49152 |
| 音乐卡顿（WiFi 弱） | 升 buffer_size 到 98304；或 media_pipeline 改 `format: FLAC`（省 CPU 费 WiFi，反之亦然） |
| 没声音 | 检查 `switch.amp_enable` 是否开着（固件已默认 ALWAYS_ON）；音量是不是 0 |
| 待机时敲击误触发 | 调低振动模块上的灵敏度电位器 |
| 想要更大音量 | 音量条上限被 `volume_max` 钳在 80%（参考 S3-BOX-3 官方示例保护小喇叭）；可调高该值、启用配置里注释的 GAIN 开关（关=12dB），或换大扬声器 |

OPUS 解码在 C3 上跑不动，不要把 pipeline 格式设成 OPUS。认真追求音质/FLAC 高码率时，换 ESP32-S3（N8R8，带 PSRAM）的板子最省心。

## WiFi 一直连不上（反复 Auth Expired / 认证挂死）

本板（ESP32-C3 rev0.4 早期步进 + 弱供电：SuperMini 板载 LDO 仅 250mA、载板走线细）上实测出的两个事实：

- **满功率下 TX 电流尖峰（~330mA）会把 3.3V 轨拉垮**，认证帧发不完整，表现为反复 `Auth Expired`、多轮重试才偶尔连上；
- **ESPHome 2026.9 的新 WiFi 状态机与这块芯片组合存在认证挂死问题**：无论在哪个时机调用 `esp_wifi_set_protocol` / `esp_wifi_set_max_tx_power`（on_boot、on_disconnect 都试过），或使用 2026.9 改为编译期 PHY 参数的 `output_power`，认证流程都会卡死（`attempt 1/2` 后再无任何事件，只能靠兜底重启）。MPY 在完全空闲的主任务里做同样的设置则 10/10 稳定。

因此固件固定用 **ESPHome 2025.12.0** 编译（旧 WiFi 架构，`output_power` 是运行时 API），并保留：

- **不要配置 `output_power`**（重要）：实测在这块 rev0.4+XMC 板上，无论 2026.9 的编译期 PHY 方式还是 2025.12 的运行时方式，只要 ESPHome 自己设置 TX 功率，认证流程就会挂死（MicroPython 手动设同样的 8.5dBm 则 10/10 稳定，说明是 ESPHome 的应用方式与板子相克）。全功率下的 `Auth Expired` 风暴由硬件整改（LDO/电容）根治，整改完成后也不需要它；
- **10 分钟仍未连上自动整机重启**，重置重试节奏（连接正常时永不触发）；
- **不启用 fallback 热点与 captive portal**（参考 QBIT#29 的教训：C3 单射频上 AP 与 STA 共存/切换会干扰 STA 连接；且重试风暴期间热点因信道跳变基本搜不到，实用价值为零）。

若仍连不上，多半在网关侧，按序排查：

1. 重启光猫/网关；
2. 网关后台关闭"防蹭网 / WiFi防破解 / 接入控制(MAC 过滤)"，确认板子 MAC 未被拉黑；
3. 2.4G 加密改 **WPA2-PSK(AES)**（不要 WPA2/WPA3 混合），关闭 WiFi6/AX 或设兼容模式；
4. 隔离实验：手机开 2.4G 热点（WPA2），把仓库 Secrets 换成热点的 SSID/密码跑一次 Action 并刷入——能连热点即坐实网关侧问题；连热点也失败再查硬件。

## 已规避的硬件坑（详见 YAML 内注释）

- **日志串口**：C3 的 UART0 默认占用 GPIO20/GPIO21，正好是本板的马达和灯——已改用 `USB_SERIAL_JTAG`，勿删。
- **功放 SD（GPIO5）**：必须用开漏控制——"开"=释放引脚靠模块 1MΩ 上拉进入混合输出档，"关"=拉低关机；推挽拉高（即 GPIO 直接输出 3.3V 高电平，内阻仅几十欧，会强行把 SD 钉在 3.3V）会越过 1.4V 阈值切成仅左声道。
- **GAIN（GPIO6）**：增益阈值相对模块 VDD（5V）按比例分档，3.3V 逻辑拉高会落进 3dB 档，只能悬空(9dB)或拉低(12dB)。
- **网表里的 "DIN" 网络**是 WS2812 灯链（GPIO21），真正的 I2S 数据是 GPIO7。
- **双流水线必须用 mixer**：announcement 和 media 两条流水线不允许共用同一个真实扬声器（编译期强制），各自占一个 mixer 源、由混音器汇入硬件扬声器；且两个源的采样率必须一致（本配置统一 44.1kHz），否则无法混音。
- **声道选择**：喇叭是单只，固件用 `channel: mono` + 两条流水线 `num_channels: 1`——左右声道由 HA 转码时混成单声道，内容无损、流带宽减半；混合模式下 `stereo`/`right` 都会丢声道内容或音量（详见 SD 控制原理一节）。
- GPIO8/9 是 C3 的 strapping 引脚，接了 I2S 时钟/左右声道但不影响启动（功放输入脚为高阻）。
