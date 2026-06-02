// Test_LXSM_D1WD10View.h : interface of the CTest_LXSM_D1WD10View class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_TEST_LXSM_D1WD10VIEW_H__625B9FD6_13EF_4388_A95E_CE72DC2FC881__INCLUDED_)
#define AFX_TEST_LXSM_D1WD10VIEW_H__625B9FD6_13EF_4388_A95E_CE72DC2FC881__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class Forwarder;            // fwd decl; full definition in Forwarder.h (included in the .cpp)
#include <cstdint>

class CTest_LXSM_D1WD10View : public CView
{
protected: // create from serialization only
	CTest_LXSM_D1WD10View();
	DECLARE_DYNCREATE(CTest_LXSM_D1WD10View)
//	long OnStreamData(WPARAM wParam,LPARAM lParam);// 메시지 처리하는 함수.
// Attributes
public:
	CTest_LXSM_D1WD10Doc* GetDocument();

// Operations
public:
	int DISPMAXCH, DISPDATANUM, DISPWIDTH;
	Forwarder* m_forwarder;   // UDP frame forwarder to the Python LSL bridge
	uint32_t   m_seq;          // LXEM frame counter
// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTest_LXSM_D1WD10View)
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	protected:
	//}}AFX_VIRTUAL

// Implementation
public:
	int SrcGrp_idx;
	int Pga_idx;
	virtual ~CTest_LXSM_D1WD10View();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CTest_LXSM_D1WD10View)
	afx_msg void OnMENUInitDevicePolyGA();
	afx_msg void OnMENUSetSample128();
	afx_msg void OnMENUSetSample256();
	afx_msg void OnMENUSetSample512();
	afx_msg void OnMENUSetSample1024();
	afx_msg void OnMENUSetSample2048();
	afx_msg void OnMENUSetADCMaxNumCh32();
	afx_msg void OnMENUSetADCMaxNumCh16();
	afx_msg void OnMENUSetADCMaxNumCh8();
	afx_msg void OnMENUSetADCMaxNumCh4();
	afx_msg void OnMENUStartStream();
	afx_msg void OnMENUStopStream();
	afx_msg void OnMENUCloseDevice();
	afx_msg void OnMENUSetSample4096();
	afx_msg void OnMENUSetADCMaxNumCh2();
	afx_msg void OnMENUSetSample8192();
	afx_msg void OnMENUSetPGASourceGroup();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
//	afx_msg LRESULT OnAcqunitdata(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnStreamData(WPARAM wParam, LPARAM lParam);
public:
	afx_msg void OnMenuInitdevicePolygI();
};

#ifndef _DEBUG  // debug version in Test_LXSM_D1WD10View.cpp
inline CTest_LXSM_D1WD10Doc* CTest_LXSM_D1WD10View::GetDocument()
   { return (CTest_LXSM_D1WD10Doc*)m_pDocument; }
#endif

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEST_LXSM_D1WD10VIEW_H__625B9FD6_13EF_4388_A95E_CE72DC2FC881__INCLUDED_)
