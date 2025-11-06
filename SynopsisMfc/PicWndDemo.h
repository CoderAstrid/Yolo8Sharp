/******************************************************************************
\author	Jewel
\date	10/17/2019
******************************************************************************/

#pragma once
#include <vector>
//#include <afxwin.h>
#include <afxext.h>
//#define _MSC_STDINT_H_		//  [9/26/2019 Jewel]

#include "ximage/ximage.h"
#pragma comment(lib, "cximagecrtu.lib")
#include <vector>

const UINT WM_READY_RECORG_MSG = ::RegisterWindowMessage(_T("WM_READY_RECORG_MSG"));

class CPicEditWnd : public CWnd
{
	DECLARE_DYNAMIC(CPicEditWnd)

public:
	CPicEditWnd();
	virtual ~CPicEditWnd();

	BOOL CreateWnd(CWnd* pParent, const RECT& rc, UINT nID, int id);
	void SetImage(const CString& strFile, bool bUpdate = true);
	void SetImage(const CxImage& img, bool bUpdate = true);

	int GetRealWidth() const { return m_iWidth; }
	int GetRealHeight() const { return m_iHeight; }
		
	void ShowResult(bool success, const CString& msg);
private:
	void calculateScale(int cx, int cy);
	bool isInRect(const CPoint& pt) const;
	void ClearState();
private:
	CString	m_sFilename;
	CxImage m_image;

	int		m_iWidth;
	int		m_iHeight;
	float	m_sScale;
	int		m_dx;
	int		m_dy;	
	int		m_id;
	bool	m_processed;
	bool	m_success;
	
	CPoint				m_lastMousePos;

	bool			m_bResult;
	CString			m_sResult;
protected:
	void DrawContents(CDC* pDC, const CRect& rc);
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnPaint();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
};
//.EOF