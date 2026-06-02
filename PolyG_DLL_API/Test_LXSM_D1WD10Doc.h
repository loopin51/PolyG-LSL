// Test_LXSM_D1WD10Doc.h : interface of the CTest_LXSM_D1WD10Doc class
//
/////////////////////////////////////////////////////////////////////////////

#if !defined(AFX_TEST_LXSM_D1WD10DOC_H__6DAC3A8F_C313_475D_9283_66F034075EC0__INCLUDED_)
#define AFX_TEST_LXSM_D1WD10DOC_H__6DAC3A8F_C313_475D_9283_66F034075EC0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


class CTest_LXSM_D1WD10Doc : public CDocument
{
protected: // create from serialization only
	CTest_LXSM_D1WD10Doc();
	DECLARE_DYNCREATE(CTest_LXSM_D1WD10Doc)

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CTest_LXSM_D1WD10Doc)
	public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CTest_LXSM_D1WD10Doc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	//{{AFX_MSG(CTest_LXSM_D1WD10Doc)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TEST_LXSM_D1WD10DOC_H__6DAC3A8F_C313_475D_9283_66F034075EC0__INCLUDED_)
