1.����Ĭ�ϵ�ģʽΪ�̶���Ƶģʽ��
  ��ģʽ������ֻ��Ҫ����3���Ĵ�����0x21��0x51��0x52�����ò������£�
  1��ȷ��0x21�Ĵ�����ֵΪ0x6��
  2��0x51��0x52Ĭ��ֵ�ֱ�Ϊ0x8��0x1b����ȡ�������£�
     ./test -r 0x51;
     ./test -r 0x52;
  3��0x51��0x52д�������»���Ϊ��д��ֵ��������������������������������޷����ȥ��
     ./test -w 0x51 __;
     ./test -w 0x52 __;
  4������ģʽ�������������3���Ĵ�������������Ĵ��������������û����¶ȣ��������þ����������ɱ�ʹ������ԭ��rtc_temp_lut_tbl.h���ɣ�
  5��0x51��0x52�����Ĵ������ڵ���RTCʱ�ӷ�Ƶϵ�������ҷ�Ƶϵ����Ƶ�����Ӱ������ԡ����Զ�����û��׼ȷ���յĿͻ����������Ƶϵ����

2.��ȡ�����RTC��Ӧ�ü�������ϣ���ο����ϰ��еġ�RTCУ׼����Ӧ��ָ��.pdf��



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











