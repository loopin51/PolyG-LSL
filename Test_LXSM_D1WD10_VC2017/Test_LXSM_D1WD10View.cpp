// Test_LXSM_D1WD10View.cpp : implementation of the CTest_LXSM_D1WD10View class
//
#include <iostream>  // std::cerr 사용을 위해 추가
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "Ws2_32.lib")

#include "stdafx.h"
#include "Test_LXSM_D1WD10.h"
#include "Test_LXSM_D1WD10Doc.h"
#include "Test_LXSM_D1WD10View.h"


// 본 응용프로그램의 비트수에 일치하는 라이브러리 임포팅하기 위한 분지처리. 
#ifdef _WIN64

#pragma comment(lib,"LIB_64bit\\LXSM-D1WD10.lib") 
#pragma comment(lib,"LIB_64bit\\ACQPLOT.lib")
//#pragma message("64bit")

#elif (WIN32)

#pragma comment(lib,"LIB_32bit\\LXSM-D1WD10.lib")
#pragma comment(lib,"LIB_32bit\\ACQPLOT.lib")

#endif


#include "LXSM-D1WD10.h"	// LXSM-D1WD10을 이용하기 위하여 함수정의부 포함함.
#include "ACQPLOTDLL.h"		// 실시간 파형 디스플레이(데모버전) 모듈 .
#include "DlgSetPgaSrcGroup.h" // 게인 설정용 다이날로그 
#include "Forwarder.h"		// UDP frame forwarder to the Python LSL bridge
#include "BridgeConfig.h"	// host/port/gain mirror of config.toml

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10View

IMPLEMENT_DYNCREATE(CTest_LXSM_D1WD10View, CView)

BEGIN_MESSAGE_MAP(CTest_LXSM_D1WD10View, CView)
	//{{AFX_MSG_MAP(CTest_LXSM_D1WD10View)
	ON_COMMAND(ID_MENU_InitDevice_PolyG_A, OnMENUInitDevicePolyGA)
	ON_COMMAND(ID_MENU_SetSample_128, OnMENUSetSample128)
	ON_COMMAND(ID_MENU_SetSample_256, OnMENUSetSample256)
	ON_COMMAND(ID_MENU_SetSample_512, OnMENUSetSample512)
	ON_COMMAND(ID_MENU_SetSample_1024, OnMENUSetSample1024)
	ON_COMMAND(ID_MENU_SetSample_2048, OnMENUSetSample2048)
	ON_COMMAND(ID_MENU_SetADCMaxNumCh_32, OnMENUSetADCMaxNumCh32)
	ON_COMMAND(ID_MENU_SetADCMaxNumCh_16, OnMENUSetADCMaxNumCh16)
	ON_COMMAND(ID_MENU_SetADCMaxNumCh_8, OnMENUSetADCMaxNumCh8)
	ON_COMMAND(ID_MENU_SetADCMaxNumCh_4, OnMENUSetADCMaxNumCh4)
	ON_COMMAND(ID_MENU_Start_Stream, OnMENUStartStream)
	ON_COMMAND(ID_MENU_Stop_Stream, OnMENUStopStream)
	ON_COMMAND(ID_MENU_Close_Device, OnMENUCloseDevice)
	ON_COMMAND(ID_MENU_SetSample_4096, OnMENUSetSample4096)
	ON_COMMAND(ID_MENU_SetADCMaxNumCh_2, OnMENUSetADCMaxNumCh2)
	ON_COMMAND(ID_MENU_Set_PGA_SourceGroup, OnMENUSetPGASourceGroup)
	//}}AFX_MSG_MAP
ON_MESSAGE(WM_AcqUnitData, &CTest_LXSM_D1WD10View::OnStreamData)
ON_COMMAND(ID_MENU_InitDevice_PolyG_I, &CTest_LXSM_D1WD10View::OnMenuInitdevicePolygI)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10View construction/destruction

CTest_LXSM_D1WD10View::CTest_LXSM_D1WD10View()
{
	// TODO: add construction code here
	DISPMAXCH = 33; // 파형 디스플레이시 표현할 최대 채널 초기값 설정.
	DISPDATANUM = 16; // 파형 디스플레이시 전달할 데이터 단위수량 . 최대 채널 설정값에 따라 달라짐.
	DISPWIDTH = 64;

	SrcGrp_idx = 0; 
	Pga_idx = 4;  

	m_forwarder = new Forwarder();
	m_seq = 0;

}

CTest_LXSM_D1WD10View::~CTest_LXSM_D1WD10View()
{
	delete m_forwarder;
}

BOOL CTest_LXSM_D1WD10View::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10View drawing

void CTest_LXSM_D1WD10View::OnDraw(CDC* pDC)
{
	CTest_LXSM_D1WD10Doc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	// TODO: add draw code for native data here
	// 파형 디스플레이를 위한 코드부
	// Time Plot의 Y축 값 초기화. -start
	char buf[64];
	CString y_text[64];
	int i = 0; 
	for(i=0;i<(DISPMAXCH-1);i++)
	{
		sprintf(buf,"Ch%d",i+1);
		y_text[i] = buf;
	}		
		sprintf(buf,"Ch%d-Mark",i+1);
		y_text[i] = buf;

	CRect client_Rect;
	GetClientRect(client_Rect);
	ACQPLOT_DLL_Window_Size_Changed_Or_Data_Set_Changed(this->m_hWnd,client_Rect,DISPWIDTH,DISPMAXCH,DISPDATANUM,1.25f,FALSE,TRUE,TRUE,4,0.5,1);
	ACQPLOT_DLL_Array_Draw_Box_Axis(pDC,TRUE,TRUE,TRUE);
	ACQPLOT_DLL_Draw_Axis_Y_Text(pDC,y_text, "system");
	/// 파형디스플레이 끝.
}

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10View diagnostics

#ifdef _DEBUG
void CTest_LXSM_D1WD10View::AssertValid() const
{
	CView::AssertValid();
}

void CTest_LXSM_D1WD10View::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CTest_LXSM_D1WD10Doc* CTest_LXSM_D1WD10View::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CTest_LXSM_D1WD10Doc)));
	return (CTest_LXSM_D1WD10Doc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10View message handlers

void SendDataOverUDP(const char* data, int dataSize, const char* ip, int port) {
	SOCKET sendingSocket;
	sockaddr_in receiverAddr;

	sendingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sendingSocket == INVALID_SOCKET) {
		std::cerr << "Error at socket(): " << WSAGetLastError() << "\n";
		return;
	}

	receiverAddr.sin_family = AF_INET;
	receiverAddr.sin_port = htons(port);
	receiverAddr.sin_addr.s_addr = inet_addr(ip);

	int iResult = sendto(sendingSocket, data, dataSize, 0, (SOCKADDR*)&receiverAddr, sizeof(receiverAddr));
	if (iResult == SOCKET_ERROR) {
		std::cerr << "sendto() failed with error code: " << WSAGetLastError() << "\n";
	}

	closesocket(sendingSocket);
}

afx_msg LRESULT CTest_LXSM_D1WD10View::OnStreamData(WPARAM wParam, LPARAM lParam)
{
	// DLL에서 AD변환된 float형 데이터를 저장하고 있는 배열변수의 포인터가 메시지로 전달되며, 메시지 파라메타 중 lParam 이 그것이다. 
	// 따라서 데이터를 이용하기 위하여 본 포인터를 활용하여야 한다.
	// 아래의 사용예는 파형디스플레이를 위한 모듈형 함수에 (float *)(lParam) 이라는 형식으로 포인터 변수를 인자로 넘기고 있다.  
	ACQPLOT_DLL_Array_Datain_Strip((float *)(lParam), DISPMAXCH, DISPDATANUM);

	// forward the raw frame to the Python LSL bridge over localhost UDP
	m_forwarder->Send(m_seq++, (const float*)lParam, (uint16_t)DISPMAXCH, (uint16_t)DISPDATANUM);

	return 0;
}



void CTest_LXSM_D1WD10View::OnMENUInitDevicePolyGA() 
{
	short retv;
	char	buf[100];
	// DLL의 Init_Device 함수 호출 
	// 1번 인자는 메시지를 전송받을 윈도우 핸들을 넘기는 것이다. 메시지란 Start_Stream 함수가 실행되면 주기적으로 전송된다.
	// 2번 인자는 장치의 고유 아이디를 입력한다. PolyG-A인 경우 아이디 = 십진수 14.
	// 기능 :	입력한 장치 아이디에 해당하는 USB 장치 핸들을 잡고, 
	//			동시에 DLL에서 채널당 16샘플링의 데이터가 확보된 시점에 메시지가 발생하게 되는데 메시지를 전송할 곳을 알려준다.
	retv = Init_Device(this->m_hWnd,14);			// PolyG-A의 경우 장치의 고유아이디 14.
	if (retv == 1)
	{
		AfxMessageBox("PolyG-A 장치 초기화 성공");
		Sleep(1000); // 다음 호출할 함수 Set_ADCMaxNumChannel 이전에 1초 대기.

		DISPMAXCH = 33;  // PolyG-A 인 경우 최대 채널 32. + 마킹 채널1.
		retv = Set_ADCMaxNumChannel(DISPMAXCH-1);		// 장치로 최대 채널수를 32개 이용함을 설정함. 본 함수 정상 수행된 경우 반환값에는 메시지 발생시 DLL에서 전달해 주는 채널당 데이터 수량이 전달된다.
		Sleep(100);   // manual: wait 0.1s after Set_ADCMaxNumChannel
		if (retv > 1)
		{
			DISPDATANUM = retv;
			sprintf(buf, "메시지 발생시 DLL에서 반환되는 각 채널당 데이터 수 = %d", DISPDATANUM);
			AfxMessageBox(buf);
			OnUpdate(NULL, 0, 0);						// 화면 갱신 

			Set_PGA(BRIDGE_PGA_GAIN_IDX);							// 장치의 전압증폭기의 전압이득을 1로 설정한 것이다. 
			Sleep(100);   // manual: wait 0.1s after Set_PGA
			m_forwarder->Init(BRIDGE_HOST, BRIDGE_PORT);
			m_seq = 0;

		}


	}
	else if(retv == -1) AfxMessageBox("PolyG-A 장치 초기화 실패");
	else if(retv == -2) AfxMessageBox("이미 장치 초기화 되어 있음.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	


}

void CTest_LXSM_D1WD10View::OnMenuInitdevicePolygI()
{
	short retv;
	char	buf[100];
	// DLL의 Init_Device 함수 호출 
	// 1번 인자는 메시지를 전송받을 윈도우 핸들을 넘기는 것이다. 메시지란 Start_Stream 함수가 실행되면 주기적으로 전송된다.
	// 2번 인자는 장치의 고유 아이디를 입력한다. PolyG-ㅑ인 경우 아이디 = 십진수 16.
	// 기능 :	입력한 장치 아이디에 해당하는 USB 장치 핸들을 잡고, 
	//			동시에 DLL에서 채널당 32샘플링의 데이터가 확보된 시점에 메시지가 발생하게 되는데 메시지를 전송할 곳을 알려준다.
	retv = Init_Device(this->m_hWnd, 16);			// PolyG-A의 경우 장치의 고유아이디 16.
	if (retv == 1)
	{
		DISPMAXCH = 17;  // PolyG-I 인 경우 최대 채널 16ch
		retv = Set_ADCMaxNumChannel(DISPMAXCH-1);		// 장치로 최대 채널수를 16개 이용함을 설정함. 본 함수 정상 수행된 경우 반환값에는 메시지 발생시 DLL에서 전달해 주는 채널당 데이터 수량이 전달된다.
		Sleep(100);   // manual: wait 0.1s after Set_ADCMaxNumChannel
		if (retv > 1)
		{
			DISPDATANUM = retv;
			sprintf(buf, "메시지 발생시 DLL에서 반환되는 각 채널당 데이터 수 = %d", DISPDATANUM);
			AfxMessageBox(buf);
			OnUpdate(NULL, 0, 0);						// 화면 갱신 

			Set_PGA(BRIDGE_PGA_GAIN_IDX);							// 장치의 전압증폭기의 전압이득을 1로 설정한 것이다. 
			Sleep(100);   // manual: wait 0.1s after Set_PGA
			m_forwarder->Init(BRIDGE_HOST, BRIDGE_PORT);
			m_seq = 0;

		}

	}
	else if (retv == -1) AfxMessageBox("PolyG-I 장치 초기화 실패");
	else if (retv == -2) AfxMessageBox("이미 장치 초기화 되어 있음.");
	else if (retv == -3) AfxMessageBox("장치 명령 전송실패.");


}


void CTest_LXSM_D1WD10View::OnMENUSetSample128() 
{
	short retv;
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(7);		// 128Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("128Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");		
}

void CTest_LXSM_D1WD10View::OnMENUSetSample256() 
{
	short retv;
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(8);	// 256Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("256Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");			
}

void CTest_LXSM_D1WD10View::OnMENUSetSample512() 
{
	short retv;	
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(9);// 512Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("512Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");			
}

void CTest_LXSM_D1WD10View::OnMENUSetSample1024() 
{
	short retv;
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(10);// 1024Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("1024Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");			
}

void CTest_LXSM_D1WD10View::OnMENUSetSample2048() 
{
	short retv;
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(11);// 2048Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("2048Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");			
}

void CTest_LXSM_D1WD10View::OnMENUSetSample4096() 
{
	short retv;
	// DLL의 Set_SampleFrequency 함수 호출.
	// 인자결정법 : " 2^n =  원하는 주파수 " 가 되도록 n값을 함수에 전달. 
	retv = Set_SampleFreq(12);// 4096Hz 샘플링 주파수 설정.
	if(retv == 1) AfxMessageBox("4096Hz 설정함.");
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -3) AfxMessageBox("장치 명령 전송실패.");	
	else if(retv == -4) AfxMessageBox("현재 최대 채널수에서는 지원되지 샘플링 주파수 입니다. 낮은 값을 설정하세요.");		
	else if(retv == -10) AfxMessageBox("샘플링 주파수 인덱스를 0에서 14사이의 값으로 지정하세요.");				
}



void CTest_LXSM_D1WD10View::OnMENUSetADCMaxNumCh32() 
{
	short retv;
	// DLL의 Set_ADCMaxNumChannel 함수호출.
	// 장치에 따라서 최대 채널수가 차이가 있으며, PolyG-A의 경우 최대 32채널 시스템이므로 32를 설정한다.
	retv = Set_ADCMaxNumChannel(32);
	if(retv > 0) 
	{
		AfxMessageBox("최대 채널 32 설정함.");
		// 파형 디스플레이화면의 최대 채널을 32+1로 하는 것이다. 여기서 1은 마킹 채널이다.
		// 본 설정은 연결된 장비에 의존된다. 
		DISPMAXCH = 33;
		DISPDATANUM = retv;
		if(DISPDATANUM <= 64) DISPWIDTH = 64;
		if(DISPDATANUM > 64) DISPWIDTH = retv;
		OnUpdate(NULL,0,0);
	}
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -2) AfxMessageBox("데이터 수집중 호출불가.");
	else if(retv == -3) AfxMessageBox("장치로 명령 전송실패.");	
	else if(retv == -10) AfxMessageBox("최대 채널수는 2,4,8,16,32만 가능합니다.");			

	
}

void CTest_LXSM_D1WD10View::OnMENUSetADCMaxNumCh16() 
{
	short retv;
	// DLL의 Set_ADCMaxNumChannel 함수호출.
	retv = Set_ADCMaxNumChannel(16);
	if(retv > 0)
	{
		AfxMessageBox("최대 채널 16 설정함.");
		// 파형 디스플레이화면의 최대 채널을 16+1로 하는 것이다. 여기서 1은 마킹 채널이다.
		// 본 설정은 연결된 장비에 의존된다. 
		DISPMAXCH = 17;
		DISPDATANUM = retv;
		if(DISPDATANUM <= 64) DISPWIDTH = 64;
		if(DISPDATANUM > 64) DISPWIDTH = retv;
		OnUpdate(NULL,0,0);
	}
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -2) AfxMessageBox("데이터 수집중 호출불가.");
	else if(retv == -3) AfxMessageBox("장치로 명령 전송실패.");	
	else if(retv == -10) AfxMessageBox("최대 채널수는 2,4,8,16,32만 가능합니다.");

	
}

void CTest_LXSM_D1WD10View::OnMENUSetADCMaxNumCh8() 
{
	short retv;
	// DLL의 Set_ADCMaxNumChannel 함수호출.
	retv = Set_ADCMaxNumChannel(8);
	if(retv > 0) 
	{
		AfxMessageBox("최대 채널 8 설정함.");
		// 파형 디스플레이화면의 최대 채널을 4+1로 하는 것이다. 여기서 1은 마킹 채널이다.
		// 본 설정은 연결된 장비에 의존된다. 
		DISPMAXCH = 9;
		DISPDATANUM = retv;
		if(DISPDATANUM <= 64) DISPWIDTH = 64;
		if(DISPDATANUM > 64) DISPWIDTH = retv;
		OnUpdate(NULL,0,0);
	}
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -2) AfxMessageBox("데이터 수집중 호출불가.");
	else if(retv == -3) AfxMessageBox("장치로 명령 전송실패.");	
	else if(retv == -10) AfxMessageBox("최대 채널수는 2,4,8,16,32만 가능합니다.");

	
}

void CTest_LXSM_D1WD10View::OnMENUSetADCMaxNumCh4() 
{
	short retv;
	// DLL의 Set_ADCMaxNumChannel 함수호출.
	retv = Set_ADCMaxNumChannel(4);
	if(retv > 0) 
	{
		AfxMessageBox("최대 채널 4 설정함.");// 성공메시지창 보인다.
		// 파형 디스플레이화면의 최대 채널을 4+1로 하는 것이다. 여기서 1은 마킹 채널이다.
		// 본 설정은 연결된 장비에 의존된다. 
		DISPMAXCH = 5;
		DISPDATANUM = retv;
		if(DISPDATANUM <= 64) DISPWIDTH = 64;
		if(DISPDATANUM > 64) DISPWIDTH = retv;
		OnUpdate(NULL,0,0);
	}
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -2) AfxMessageBox("데이터 수집중 호출불가.");
	else if(retv == -3) AfxMessageBox("장치로 명령 전송실패.");	
	else if(retv == -10) AfxMessageBox("최대 채널수는 2,4,8,16,32만 가능합니다.");	


}

void CTest_LXSM_D1WD10View::OnMENUSetADCMaxNumCh2() 
{
	short retv;
	// DLL의 Set_ADCMaxNumChannel 함수호출.
	retv = Set_ADCMaxNumChannel(2);
	if(retv > 0) 
	{
		AfxMessageBox("최대 채널 2 설정함.");// 성공메시지창 보인다.
		// 파형 디스플레이화면의 최대 채널을 4+1로 하는 것이다. 여기서 1은 마킹 채널이다.
		// 본 설정은 연결된 장비에 의존된다. 
		DISPMAXCH = 3;
		DISPDATANUM = retv;
		if(DISPDATANUM <= 64) DISPWIDTH = 64;
		if(DISPDATANUM > 64) DISPWIDTH = retv;
		OnUpdate(NULL,0,0);
	}
	else if(retv == -1) AfxMessageBox("장치 초기화 부터 하세요.");
	else if(retv == -2) AfxMessageBox("데이터 수집중 호출불가.");
	else if(retv == -3) AfxMessageBox("장치로 명령 전송실패.");	
	else if(retv == -10) AfxMessageBox("최대 채널수는 2,4,8,16,32만 가능합니다.");	
	
}

void CTest_LXSM_D1WD10View::OnMENUStartStream() 
{
	// TODO: Add your command handler code here
	// DLL의 Start_Stream 함수 호출
	// 기능 : 본 함수가 호출되면 장치에 내장된 AD변환기로부터 AD변한된 데이터를 DLL에서 연속적으로 확보하며
	// 채널당 지정된 샘플링 데이터 수만큼 모아지면 메시지를 전송하게 된다. 이동작은 Stop_Stream 이 호출될때까지 무한반복한다.
	// 채널당 지정된 샘플링 데이터 수는 최대 채널수의 설정상태에 따라 다르며, 다음과 같다.
	// 
	// 최대 채널수 4 인 경우 채널당 128개의 데이터가 모아지면 DLL에서 메시지 전송됨. 
	// 최대 채널수 8 인 경우 채널당 64개의 데이터가 모아지면 DLL에서 메시지 전송됨. 
	// 최대 채널수 16인 경우 채널당 32개의 데이터가 모아지면 DLL에서 메시지 전송됨. 
	// 최대 채널수 32인 경우 채널당 16개의 데이터가 모아지면 DLL에서 메시지 전송됨. 
	Start_Stream();
	// Start_Stream이 호출되고 나면 이후 처리는 모두 메시지 처리 루틴에서 모든 처리가 이뤄져야 한다.
}

void CTest_LXSM_D1WD10View::OnMENUStopStream() 
{
	// DLL의 Stop_Stream 함수 호출
	// 기능 : 실시간 데이터 수집 중단.
	Stop_Stream();
	
}

void CTest_LXSM_D1WD10View::OnMENUCloseDevice() 
{
	short retv;
	// DLL의 Close_Device 함수 호출.
	retv = Close_Device();
	if(retv == 1) AfxMessageBox("장치 해제함.");
}

void CTest_LXSM_D1WD10View::OnMENUSetPGASourceGroup() 
{
	// 측정대상별 게인설정을 위하여 다이알로그를 이용한다.
	CDlgSetPgaSrcGroup dlg;
	dlg.m_Pga_idx = Pga_idx;
	dlg.m_SrcGrp_idx = SrcGrp_idx;

	if(dlg.DoModal() == IDOK)
	{
		Pga_idx = dlg.m_Pga_idx ;
		SrcGrp_idx = dlg.m_SrcGrp_idx ;

		// DLL의 Set_PGA함수 호출한다.
		// 단 , 값의 범위가 유효해야 한다. 전압 이득은 0에서 15까지 유효하고, 소스그룹은 PolyG-A의 경우 0~7, PolyG-I 0~5, 254,255
		if((Pga_idx > -1 && Pga_idx < 16 ) && ((SrcGrp_idx > -1 && SrcGrp_idx < 8) || SrcGrp_idx > 253))
			Set_PGA_SourceGroup(SrcGrp_idx, Pga_idx);		
		else
		AfxMessageBox("값의 범위가 유효하지 않아 장치로 명령 전송을 하지 않았습니다.");
	}		
	
}



