# This is the 3d show demo based on MPU6050 or MPU9250

## Flow

* the esp32 actes as a websocket server
* the esp32 caculte the euler angle and push the data to the web page
* the web page receive these data and render euler angle in the three.js
## How to use
* the esp32 as a sta mode(you also can use ap mode)
* keep your web browser(chorme test ok) in the same net(or join the esp32 ap)
* open the 3d_show.html use your browser,entering the esp32 ip address and connect it.
![3d_show](https://img.whyengineer.com/3d_show.png) 


* To support the MPU9250 3 axis sensor, https://github.com/natanaeljr/esp32-MPU-driver  must be installed. These modules must be downloaded into the components directory. ![MPU driver API](https://natanaeljr.github.io/esp32-MPU-driver/html/index.html)
* To try out the example, load the '3d_show.html' in your browser. ip-address/3d_show/3d_show.html

# It's a youtube demo video
[![3d_show](https://img.youtube.com/vi/n1iKDgKCfHM/0.jpg)](https://www.youtube.com/watch?v=n1iKDgKCfHM)
