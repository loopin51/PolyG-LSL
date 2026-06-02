// Test_LXSM_D1WD10Doc.cpp : implementation of the CTest_LXSM_D1WD10Doc class
//

#include "stdafx.h"
#include "Test_LXSM_D1WD10.h"

#include "Test_LXSM_D1WD10Doc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10Doc

IMPLEMENT_DYNCREATE(CTest_LXSM_D1WD10Doc, CDocument)

BEGIN_MESSAGE_MAP(CTest_LXSM_D1WD10Doc, CDocument)
	//{{AFX_MSG_MAP(CTest_LXSM_D1WD10Doc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10Doc construction/destruction

CTest_LXSM_D1WD10Doc::CTest_LXSM_D1WD10Doc()
{
	// TODO: add one-time construction code here

}

CTest_LXSM_D1WD10Doc::~CTest_LXSM_D1WD10Doc()
{
}

BOOL CTest_LXSM_D1WD10Doc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10Doc serialization

void CTest_LXSM_D1WD10Doc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10Doc diagnostics

#ifdef _DEBUG
void CTest_LXSM_D1WD10Doc::AssertValid() const
{
	CDocument::AssertValid();
}

void CTest_LXSM_D1WD10Doc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CTest_LXSM_D1WD10Doc commands
