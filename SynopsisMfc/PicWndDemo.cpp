/******************************************************************************
\author	Jewel
\date	10/17/2019
******************************************************************************/

#include "pch.h"
#include "SynopsisMfc.h"
#include "PicWndDemo.h"
#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CPicEditWnd

IMPLEMENT_DYNAMIC(CPicEditWnd, CWnd)
CPicEditWnd::CPicEditWnd()
	: m_iWidth(0)
	, m_iHeight(0)
	, m_sScale(1.0f)
	, m_dx(0)
	, m_dy(0)
	, m_id(0)
	, m_processed(false)
	, m_success(false)
	, m_bResult(false)
{
}

CPicEditWnd::~CPicEditWnd()
{
	if (m_image.IsValid()) {
		m_image.Destroy();
	}
	ClearState();
}

BEGIN_MESSAGE_MAP(CPicEditWnd, CWnd)
	ON_WM_PAINT()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()	
	ON_WM_RBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_SETCURSOR()
	ON_WM_MOUSEMOVE()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CPicWnd message handlers
BOOL CPicEditWnd::CreateWnd(CWnd* pParent, const RECT& rc, UINT nID, int id)
{
	m_id = id;
	return CWnd::Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_BORDER, rc, pParent, nID);
}

void CPicEditWnd::SetImage(const CString& strFile, bool bUpdate)
{
	m_processed = false;
	if (m_sFilename.CompareNoCase(strFile) == 0)
		return;
	m_sFilename = strFile;

	if (m_image.IsValid()) {
		m_image.Destroy();
		m_iWidth = m_iHeight = 0;
	}
	if (m_image.Load(strFile)) {
		m_iWidth = m_image.GetWidth();
		m_iHeight = m_image.GetHeight();
		ClearState();
	}
	else {
		m_iWidth = m_iHeight = 0;
	}
	if (GetSafeHwnd() && IsWindow(GetSafeHwnd())) {
		CRect rc;
		GetClientRect(&rc);
		calculateScale(rc.Width(), rc.Height());
		if(bUpdate)
			Invalidate();
	}
}

void CPicEditWnd::SetImage(const CxImage& newImg, bool bUpdate)
{
	if (m_image.IsValid()) {
		m_image.Destroy();
		m_iWidth = m_iHeight = 0;
	}
	m_image = newImg;	
	m_iWidth = m_image.GetWidth();
	m_iHeight = m_image.GetHeight();
	ClearState();
	
	if (GetSafeHwnd() && IsWindow(GetSafeHwnd())) {
		CRect rc;
		GetClientRect(&rc);
		calculateScale(rc.Width(), rc.Height());
		if (bUpdate)
			Invalidate();
	}
}

void CPicEditWnd::ClearState()
{
	m_processed = false;
	m_success = false;
}

void CPicEditWnd::ShowResult(bool success, const CString& msg)
{
	m_bResult = success;
	m_sResult = msg;
	Invalidate();
}


void CPicEditWnd::OnSize(UINT nType, int cx, int cy)
{
	if(m_iWidth == 0 || m_iHeight == 0)
		return;
	float oldScale = m_sScale;
	float dx0 = (float)m_dx, dy0 = (float)m_dy;
	calculateScale(cx, cy);

	if (oldScale > 0 && oldScale != m_sScale) {
		// Rescale the tracker rectangle
	}

	if (GetSafeHwnd() && IsWindow(GetSafeHwnd()))
		Invalidate();
}

void CPicEditWnd::calculateScale(int cx, int cy)
{
	if (cx == 0 || cy == 0)
		return;
	if (m_iWidth == 0 || m_iHeight == 0)
		return;
	float scaleX = (float)cx / (float)m_iWidth;
	float scaleY = (float)cy / (float)m_iHeight;
	m_sScale = min(scaleX, scaleY);

	int w = (int)(m_iWidth * m_sScale);
	int h = (int)(m_iHeight * m_sScale);
	m_dx = (cx > w ? (cx - w) / 2 : 0);
	m_dy = (cy > h ? (cy - h) / 2 : 0);
}

void CPicEditWnd::OnPaint()
{
	CPaintDC dc(this); // device context for painting
					   // TODO: Add your message handler code here
					   // Do not call CWnd::OnPaint() for painting messages
	CRect rc;
	GetClientRect(&rc);
	CMemDC memDC(dc, this);
	DrawContents(&memDC.GetDC(), rc);
}

void CPicEditWnd::DrawContents(CDC* pDC, const CRect& rc)
{
	pDC->FillSolidRect(&rc, RGB(49, 49, 49));
	if (!m_image.IsValid())
		return;
	
	int w = (int)(m_iWidth * m_sScale);
	int h = (int)(m_iHeight * m_sScale);
	CRect rcImg(m_dx, m_dy, m_dx + w, m_dy + h);

	m_image.Draw(pDC->GetSafeHdc(), rcImg);
}

BOOL CPicEditWnd::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		int iKey = -1;
		if (pMsg->wParam == VK_LEFT) {
			iKey = 0;
		}
		else if (pMsg->wParam == VK_RIGHT) {
			iKey = 1;
		}
		else if (pMsg->wParam == VK_HOME) {
			iKey = 2;
		}
		else if (pMsg->wParam == VK_END) {
			iKey = 3;
		}
		if (iKey >= 0) {

		}
	}

	return CWnd::PreTranslateMessage(pMsg);
}

bool CPicEditWnd::isInRect(const CPoint& pt) const
{
	if(m_iWidth == 0 || m_iHeight == 0)
		return false;
	CRect rc;
	GetClientRect(&rc);	
	CRect rcImg(m_dx, m_dy, rc.right - m_dx, rc.bottom - m_dy);
	if(rcImg.IsRectEmpty())
		return false;
	if (rcImg.PtInRect(pt)) {
		return true;
	}
	return false;
}

void CPicEditWnd::OnLButtonDown(UINT nFlags, CPoint point)
{	
	SetFocus();
	if (!isInRect(point)) {
		CWnd::OnLButtonDown(nFlags, point);
		return;
	}	

	BOOL bUpdate = FALSE;
	m_lastMousePos = point; // Save initial point for tracking

	if(bUpdate)
		Invalidate();

	CWnd::OnLButtonDown(nFlags, point);
}

void CPicEditWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	if (!isInRect(point) || GetFocus() != this) {
		CWnd::OnMouseMove(nFlags, point);
		return;
	}
	BOOL bUpdate = FALSE;	
	
	if (bUpdate)
		Invalidate();
	CWnd::OnMouseMove(nFlags, point);
}


void CPicEditWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	if (!isInRect(point) || GetFocus() != this) {
		CWnd::OnLButtonUp(nFlags, point);
		return;
	}
	BOOL bUpdate = FALSE;
	if (bUpdate) {
		Invalidate();
	}

	CWnd::OnLButtonUp(nFlags, point);
}

void CPicEditWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	/*CString cmd_str(_T("explorer /select, ") + m_sFilename);
	CT2A pss(cmd_str);
	WinExec(pss.m_psz, SW_SHOW);*/

	CWnd::OnLButtonDblClk(nFlags, point);
}

void CPicEditWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
	SetFocus();
	if (!isInRect(point)) {
		CWnd::OnRButtonDown(nFlags, point);
		return;
	}

	BOOL bUpdate = FALSE;
	if (bUpdate)
		Invalidate();

	CWnd::OnRButtonDown(nFlags, point);
}

void CPicEditWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
	if (!isInRect(point) || GetFocus() != this) {
		CWnd::OnRButtonUp(nFlags, point);
		return;
	}
	BOOL bUpdate = FALSE;
	if (bUpdate) {
		Invalidate();
	}

	CWnd::OnRButtonUp(nFlags, point);
}

BOOL CPicEditWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{	
	return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

//.EOF