
// SynopsisMfcDlg.h : header file
//
#pragma once
#include <mutex>
#include <map>
#include <vector>
#include <opencv2/opencv.hpp>
#include "VideoPlayer.h"
#include "PicWndDemo.h"
#include "SynopsisEngine.h"
#include "InferenceManager.h"

const UINT WM_FRAME_ARRIVED = ::RegisterWindowMessage(_T("WM_FRAME_ARRIVED"));
const UINT WM_PROCESSED_FRAME = ::RegisterWindowMessage(_T("WM_PROCESSED_FRAME"));

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
	InferenceManager m_InfManager;
	FrameQueue m_frameInQueue;
	FrameQueue m_frameOutQueue;

	CPicEditWnd m_imageWnd;
	BOOL		m_bInit;
	vsHandle    m_YoloHandle;

	std::mutex mtx_;
	cv::Mat frame_bgr_; // last frame
	int64_t m_currentDisplayedFrame = -1; // Track currently displayed frame index
	std::map<int64_t, std::vector<Detection>> m_detectionCache; // Cache detections by frame index (for Timed mode)
	std::mutex m_detectionCacheMutex; // Protect detection cache
	PlayMode m_playMode = PlayMode::Timed; // Current play mode (Timed = sync with video, Continuous = as fast as possible)
	static constexpr size_t MAX_QUEUE_SIZE = 60; // Maximum frames in queue (increased to handle slower detection)
private:
	void InitControls();
	void FrameRcvCallback(const cv::Mat& frame, int64_t frameIdx, int64_t total, double fps);
	void SetPlayMode(PlayMode mode); // Change play mode and restart inference if needed
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
	afx_msg LRESULT OnProcessedFrame(WPARAM wParam, LPARAM lParam);

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
	CSliderCtrl m_seekVideo;

	bool isSeeking_ = false;  // prevent loop updates during drag

	UINT_PTR m_timerID = 0;

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	CComboBox m_cbPlayMode;
	afx_msg void OnCbnSelchangeCbPlaymode();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	CStatic m_lblTimeStamp;
	CStatic m_lblInfo;
	afx_msg void OnDestroy();
};
