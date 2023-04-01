1.驱动默认的模式为固定分频模式。
  本模式下用于只需要配置3个寄存器：0x21、0x51、0x52。配置步骤如下：
  1）确保0x21寄存器的值为0x6；
  2）0x51、0x52默认值分别为0x8、0x1b；读取命令如下：
     ./test -r 0x51;
     ./test -r 0x52;
  3）0x51、0x52写方法：下划线为待写入值，以下两个命令必须连续操作，否则无法配进去。
     ./test -w 0x51 __;
     ./test -w 0x52 __;
  4）这种模式下无需调节以上3个寄存器以外的其他寄存器，即无需配置环境温度，无需配置晶体曲线生成表，使用驱动原有rtc_temp_lut_tbl.h即可；
  5）0x51、0x52两个寄存器调节的是RTC时钟分频系数，并且分频系数对频率输出影响很明显。所以对数据没有准确把握的客户不建议调分频系数。

2.获取更多的RTC的应用及相关资料，请参考资料包中的《RTC校准方案应用指导.pdf》



1.The default mode of the driver for RTC is Fixed Frequency-Division Mode.
  In this mode,only 3 registers need to be configed:0x21/0x51/0x52.The config procedures are as follows:
  1)Make sure the register whose offset register is 0x21 is set to 0x6;
  2)The default velue of 0x51 & 0x52 registers is 0x8 & 0x1b;They can be read through these commands:
     ./test -r 0x51;
     ./test -r 0x52;
  3)Write commands of 0x51 & 0x52 registers:replace the "__"with the value you want to write,and the two commands should be executed continuously,otherwise the registers can not be set successfully.
     ./test -w 0x51 __;
     ./test -w 0x52 __;
  4)In this mode,only 3 registers need to be configed:0x21/0x51/0x52.And there is no need to config any temperature value and RTC Crystal Correction Creation Table.For the driver,please use the original "rtc_temp_lut_tbl.h" that contains in the driver packet. 
  5)the 0x51 & 0x52 regisgers determine the Frequency divider,and their values have large effect to the frequency of the RTC.As a result,if the frequency data is not accurate enough,please use the default value.
  
2.For more details,please refer the "RTC Correction Scheme Appliation Notes.pdf"











