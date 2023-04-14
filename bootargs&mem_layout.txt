### 1、TVT板DDR内存有1GB大小，其中0x80000000为DDR内存物理起始地址：
```
    -----|-------|  0x80000000   # Memory managed by OS.              
    512M | OS    |                                                 
         |       |                                                 
    -----|-------|  0xA0000000   # Memory managed by MMZ block anonymous.          
    506M | MMZ   |                                                 
         |       |                                                 
    -----|-------|  0xBFA00000   # Memory managed by MMZ block jpeg.                       
    5M   |       |                                                 
    -----|-------|  0xBFF00000   # Not used.                         
    1M   |       |                                                 
    -----|-------|  0x90000000   # End of DDR.  
```
	
### 2、UBOOT的启动参数配置：
```
setenv bootargs 'console=ttyS0,115200 mem=512M root=/dev/sdq1 rw rootwait init=/sbin/init mac=00:00:23:34:45:66 mtdparts=hi_sfc:2M(boot);hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user);'
```

### 3、load3535 脚本中MMZ的参数修改
`insmod mmz.ko mmz=anonymous,0,0xA0000000,506M:jpeg,0,0xBFA00000,5M anony=1 || report_error`
