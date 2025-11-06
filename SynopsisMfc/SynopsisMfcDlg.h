
// SynopsisMfcDlg.h : header file
//
#pragma once
#include "VideoPlayer.h"
#include "PicWndDemo.h"
#include <mutex>
#include <opencv2/opencv.hpp>

const UINT WM_FRAME_ARRIVED = ::RegisterWindowMessage(_T("WM_FRAME_ARRIVED"));

CxImage MatToCxImage(const cv::Mat& mat);

// CSynopsisMfcDlg dialog
class CSynopsisMfcDlg : public CDialogEx
{
// Construction
public:
	CSynopsisMfcDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_SYNOPSISMFC_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

private:
	VideoPlayer m_videoPlayer;
	CPicEditWnd m_imageWnd;
	BOOL		m_bInit;

	std::mutex mtx_;
	cv::Mat frame_bgr_; // last frame
private:
	void InitControls();
	void FrameRcvCallback(const cv::Mat& frame, int64_t frameIdx, int64_t total, double fps);
// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnBnClickedBtnBrowser();
	afx_msg void OnBnClickedRdImage();
	afx_msg void OnBnClickedRdVideo();
	afx_msg void OnBnClickedBtnPlay();
	afx_msg void OnBnClickedBtnStop();
	afx_msg void OnBnClickedBtnNextframe();
	afx_msg void OnBnClickedBtnPrevframe();
	afx_msg LRESULT OnArrivedFrame(WPARAM wParam, LPARAM lParam);
	CEdit m_txtPath;
	CButton m_btnPlayPause;
	CButton m_btnStop;
	CButton m_btnNextFrame;
	CButton m_btnPrevFrame;
	int m_iFileMode;
	int m_iMode;
	afx_msg void OnBnClickedRdSegment();
	afx_msg void OnBnClickedRdDetect();
	afx_msg void OnBnClickedRdClassify();
	afx_msg void OnBnClickedRdPose();
	afx_msg void OnBnClickedRdObb();
};
