#if !defined(AFX_DLGSETPGASRCGROUP_H__FE05F5F9_6F7B_40E9_8E50_7D083A3F9E2E__INCLUDED_)
#define AFX_DLGSETPGASRCGROUP_H__FE05F5F9_6F7B_40E9_8E50_7D083A3F9E2E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DlgSetPgaSrcGroup.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CDlgSetPgaSrcGroup dialog

class CDlgSetPgaSrcGroup : public CDialog
{
// Construction
public:
	CDlgSetPgaSrcGroup(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDlgSetPgaSrcGroup)
	enum { IDD = IDD_DIALOG1 };
	int		m_SrcGrp_idx;
	int		m_Pga_idx;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgSetPgaSrcGroup)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgSetPgaSrcGroup)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGSETPGASRCGROUP_H__FE05F5F9_6F7B_40E9_8E50_7D083A3F9E2E__INCLUDED_)
