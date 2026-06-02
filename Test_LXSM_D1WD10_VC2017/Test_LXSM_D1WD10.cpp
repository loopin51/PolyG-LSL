// Test_LXSM_D1WD10.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Test_LXSM_D1WD10.h"

#include "MainFrm.h"
#include "Test_LXSM_D1WD10Doc.h"
#include "Test_LXSM_D1WD10View.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10App

BEGIN_MESSAGE_MAP(CTest_LXSM_D1WD10App, CWinApp)
	//{{AFX_MSG_MAP(CTest_LXSM_D1WD10App)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10App construction

CTest_LXSM_D1WD10App::CTest_LXSM_D1WD10App()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTest_LXSM_D1WD10App object

CTest_LXSM_D1WD10App theApp;

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10App initialization

BOOL CTest_LXSM_D1WD10App::InitInstance()
{
	// 윈속 초기화
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR) {
		AfxMessageBox(_T("Winsock 초기화 실패!"));
		return FALSE;
	}

	// 콘솔 창 생성
	if (!AllocConsole()) {
		AfxMessageBox(_T("콘솔 창을 생성할 수 없습니다."));
	}

	freopen("CONOUT$", "w", stdout);  // 표준 출력을 콘솔 창으로 리디렉션
	freopen("CONOUT$", "w", stderr);  // 표준 에러를 콘솔 창으로 리디렉션

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	LoadStdProfileSettings();  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views.

	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CTest_LXSM_D1WD10Doc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CTest_LXSM_D1WD10View));
	AddDocTemplate(pDocTemplate);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Dispatch commands specified on the command line
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it.
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();

	return TRUE;
}

int CTest_LXSM_D1WD10App::ExitInstance() {
	// 윈속 정리
	WSACleanup();

	// 콘솔 창 해제
	FreeConsole();

	return CWinApp::ExitInstance();  // 기본 종료 코드를 호출하여 정상 종료
}

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
//	afx_msg LRESULT OnAcqunitdata(WPARAM wParam, LPARAM lParam);
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
//	ON_MESSAGE(WM_AcqUnitData, &CAboutDlg::OnAcqunitdata)
END_MESSAGE_MAP()

// App command to run the dialog
void CTest_LXSM_D1WD10App::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10App message handlers



//afx_msg LRESULT CAboutDlg::OnAcqunitdata(WPARAM wParam, LPARAM lParam)
//{
//	return 0;
//}
