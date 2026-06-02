// DlgSetPgaSrcGroup.cpp : implementation file
//

#include "stdafx.h"
#include "Test_LXSM_D1WD10.h"
#include "DlgSetPgaSrcGroup.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgSetPgaSrcGroup dialog


CDlgSetPgaSrcGroup::CDlgSetPgaSrcGroup(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSetPgaSrcGroup::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgSetPgaSrcGroup)
	m_SrcGrp_idx = 0;
	m_Pga_idx = 0;
	//}}AFX_DATA_INIT
}


void CDlgSetPgaSrcGroup::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgSetPgaSrcGroup)
	DDX_Text(pDX, IDC_EDIT1, m_SrcGrp_idx);
	DDV_MinMaxInt(pDX, m_SrcGrp_idx, 0, 255);
	DDX_Text(pDX, IDC_EDIT2, m_Pga_idx);
	DDV_MinMaxInt(pDX, m_Pga_idx, 0, 15);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgSetPgaSrcGroup, CDialog)
	//{{AFX_MSG_MAP(CDlgSetPgaSrcGroup)
		// NOTE: the ClassWizard will add message map macros here
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgSetPgaSrcGroup message handlers
