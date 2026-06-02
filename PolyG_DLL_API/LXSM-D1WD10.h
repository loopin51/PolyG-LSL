// This header file must be included in main program to use "LXSM-D1WD10.DLL"
// DLL version 1.0 Released 2005-05-10
#define WM_AcqUnitData	WM_USER+1

extern "C" __declspec(dllimport) short Init_Device(HWND msgtarget_window,int pid);						// H/W장치 초기화, stream message받을 윈도우 handle 및 장치의 고유ID전달.
extern "C" __declspec(dllimport) short Close_Device();	
extern "C" __declspec(dllimport) short Start_Stream();													// Stream 시작. 선행 Init_Device, 단 한번.
extern "C" __declspec(dllimport) short Stop_Stream();													// Stream 종료. 선행 Init_Device, Start_Stream.
extern "C" __declspec(dllimport) short Set_SampleFreq(unsigned char samplefreq_idx);					// 장치의 Sampling Frequency 설정.
extern "C" __declspec(dllimport) short Set_PGA(unsigned char gain_idx);									// 전채널 동일한 게인설정.
extern "C" __declspec(dllimport) short Set_PGA_SourceGroup(unsigned char sourcegroup_idx, unsigned char gain_idx);	// 장치 대상에 따른 PGA 인덱스 값 설정.
extern "C" __declspec(dllimport) short Set_PGA_EachChannel(unsigned char channel_idx, unsigned char gain_idx);		// 채널1개별로 PGA 인덱스 값 설정.
extern "C" __declspec(dllimport) short Set_ADCMaxNumChannel(unsigned char maxnum_channel);				// 장치의 ADC앞단의 다채널 지원 MUX의 최대 채널을 설정한다. 2^n 로 채널수를 설정한다. 
extern "C" __declspec(dllimport) short Set_ConfigChannel(unsigned char *Is_Select_Channel);				// 채널선택.