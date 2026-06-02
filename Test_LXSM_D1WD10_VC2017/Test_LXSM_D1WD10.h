// Test_LXSM_D1WD10.h : main header file for the TEST_LXSM_D1WD10 application
//

#if !defined(AFX_TEST_LXSM_D1WD10_H__5E81EC60_8BAD_42E9_A1C1_4051B23B470F__INCLUDED_)
#define AFX_TEST_LXSM_D1WD10_H__5E81EC60_8BAD_42E9_A1C1_4051B23B470F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10App:
// See Test_LXSM_D1WD10.cpp for the implementation of this class
//

class CTest_LXSM_D1WD10App : public CWinApp
{
public:
	CTest_LXSM_D1WD10App();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTest_LXSM_D1WD10App)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();  // ExitInstance 선언 추가
	//}}AFX_VIRTUAL

// Implementation
	//{{AFX_MSG(CTest_LXSM_D1WD10App)
	afx_msg void OnAppAbout();
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEST_LXSM_D1WD10_H__5E81EC60_8BAD_42E9_A1C1_4051B23B470F__INCLUDED_)
