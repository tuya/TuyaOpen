## 如果编译报错
’‘’
error: esp_icd_gd9a01.h: No such file or directory include "esp_Icd_gc9a01.h"
‘’‘
1、需要按照图片所示修改cmakelists文件

![](./image/1.png)
2、idf_component.yml 加一下 这个屏幕依赖
![](./image/idf_component.png)