# chromium
- [简介](#简介)
- [目录](#目录)
- [使用说明](#使用说明)
- [相关仓](#相关仓)
## 简介
### 内容介绍
1. Chromium是由Google主导开发的网页浏览器，以BSD许可证等多重自由版权发行并开放源代码，是Google的Chrome浏览器背后的引擎，其目的是为了创建一个安全、稳定和快速的通用浏览器。
2. OpenHarmony nweb基于Chromium构建。
### 软件架构
软件架构说明
![](figures/Web-architecture_ZH.png "web软件架构图")
* webview组件：OpenHarmony的UI组件。
* nweb：基于CEF构建的OpenHarmony web组件的Native引擎。
* CEF：CEF全称Chromium Embedded Framework，是一个基于Google Chromium 的开源项目。
* Chromium： Chromium是一个由Google主导开发的网页浏览器，以BSD许可证等多重自由版权发行并开放源代码。
## 使用说明
1. 下载代码
   
    repo init -u https://gitee.com/openharmony-sig/manifest -b master -m chromium.xml --no-repo-verify
    
    repo sync -c

    repo forall -c 'git lfs pull'

2. 编译
   
    ./build.sh  -t w -A -without-nweb-ex rk3568

    ***如若找不到sdk压缩包，可以下载大文件***
    
    cd src
    
    git lfs pull
    
3. 签名
   
   执行./sign.sh

4. 运行
   
    编译完成后，在out目录下找到NWeb-rk3568.hap,
    将它推送到设备中。
  ```
  hdc shell "mount -o remount, rw /"
  hdc file send NWeb-rk3568.hap /system/app/com.ohos.nweb/NWeb.hap
  hdc shell "rm /data/* -rf"
  hdc shell reboot
  ```
