# BiliCat
BiliBili 视频/直播取流工具

## 用法
``````
bilicat [-h --help][-c --cookie][-v --video][-l --live][-q --qn]
               [-d --coder][-a --audio][-o --output]
``````

### 参数
``--video``: 视频取流，传入 bvid 或 aid  
[必要]  

``--cookie``: 指定 Cookie  
[可选]  

``--live``: 直播取流，传入房间号  
[必要，与 ``--video`` 二选一]  

``--qn``: 视频流质量  
[可选，默认取首个返回的视频流]   

| 值   | 含义           | 备注                                                         |
| ---- | -------------- | ------------------------------------------------------------ |
| 6    | 240P 极速      | 仅 MP4 格式支持<br />仅`platform=html5`时有效                |
| 16   | 360P 流畅      |                                                              |
| 32   | 480P 清晰      |                                                              |
| 64   | 720P 高清      | WEB 端默认值<br />~~B站前端需要登录才能选择，但是直接发送请求可以不登录就拿到 720P 的取流地址~~<br />**无 720P 时则为 720P60** |
| 74   | 720P60 高帧率  | 登录认证                                                     |
| 80   | 1080P 高清     | TV 端与 APP 端默认值<br />登录认证                           |
| 100  | 智能修复       | 人工智能增强画质<br />大会员认证
| 112  | 1080P+ 高码率  | 大会员认证                                                   |
| 116  | 1080P60 高帧率 | 大会员认证                                                   |
| 120  | 4K 超清        | 需要`fnval&128=128`且`fourk=1`<br />大会员认证               |
| 125  | HDR 真彩色     | 仅支持 DASH 格式<br />需要`fnval&64=64`<br />大会员认证      |
| 126  | 杜比视界       | 仅支持 DASH 格式<br />需要`fnval&512=512`<br />大会员认证    |
| 127  | 8K 超高清      | 仅支持 DASH 格式<br />需要`fnval&1024=1024`<br />大会员认证  |
| 129 | HDR Vivid | 大会员认证 |

来源: [qn视频清晰度标识 (bilibili-API-collect)](https://github.com/pskdje/bilibili-API-collect/blob/main/docs/video/videostream_url.md#qn%E8%A7%86%E9%A2%91%E6%B8%85%E6%99%B0%E5%BA%A6%E6%A0%87%E8%AF%86)  

``--audio``: 音频流质量  
[可选，默认取首个返回的音频流]  

| 值    | 含义 |
| ----- | ---- |
| 30216 | 64K  |
| 30232 | 132K |
| 30280 | 192K |
| 30250 | 杜比全景声 |
| 30251 | Hi-Res无损 |


来源: [视频伴音音质代码 (bilibili-API-collect)](https://github.com/pskdje/bilibili-API-collect/blob/main/docs/video/videostream_url.md#%E8%A7%86%E9%A2%91%E4%BC%B4%E9%9F%B3%E9%9F%B3%E8%B4%A8%E4%BB%A3%E7%A0%81)  

``--coder``: 视频流编码  
[可选，默认取首个返回的编码]  

| 值 | 含义     | 备注           |
| ---- | ---------- | ---------------- |
| 7  | AVC 编码 | 8K 视频不支持该格式 |
| 12 | HEVC 编码 |                |
| 13 | AV1 编码 |                |

来源: [视频编码代码 (bilibili-API-collect)](https://github.com/pskdje/bilibili-API-collect/blob/main/docs/video/videostream_url.md#%E8%A7%86%E9%A2%91%E7%BC%96%E7%A0%81%E4%BB%A3%E7%A0%81)  

``--output``: 指定输出目录，必须存在，且传入的值包含目录分隔符 ('/' 或 '\\')
[可选，默认为当前目录]

## 风控
如遇到 ``code: -352`` 错误，是触发了 BiliBili 的风控，稍后重试几次即可

## 从源码安装
编译依赖 [yyjson](https://ibireme.github.io/yyjson/doc/doxygen/html/) [libcurl](https://curl.se/libcurl/)，运行时需要安装 [FFmpeg (cli)](https://ffmpeg.org/)

使用 [Meson](https://mesonbuild.com/) 编译
``````sh
meson setup build
cd build
meson compile
``````
产物为 ``bilicat``
