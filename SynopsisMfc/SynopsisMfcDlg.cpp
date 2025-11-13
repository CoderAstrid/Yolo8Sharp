
// SynopsisMfcDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "SynopsisMfc.h"
#include "SynopsisMfcDlg.h"
#include "afxdialogex.h"
#include <mutex>
#include "Logger.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

CxImage MatToCxImage(const cv::Mat& mat)
{
	// Ensure we have supported type
	if (mat.empty())
		return CxImage();

	int width = mat.cols;
	int height = mat.rows;
	int bpp = mat.channels() * 8;  // bits per pixel

	// Create CxImage with the same dimensions and bit depth
	CxImage cximg(width, height, bpp, CXIMAGE_FORMAT_BMP);
	
	// Validate that the image was created successfully
	if (!cximg.IsValid()) {
		return CxImage(); // Return invalid image
	}

	// Convert color space if needed (OpenCV uses BGR, CxImage expects RGB)
	cv::Mat rgb;
	if (mat.channels() == 3)
		cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
	else if (mat.channels() == 4)
		cv::cvtColor(mat, rgb, cv::COLOR_BGRA2RGBA);
	else
		rgb = mat.clone();  // grayscale

	// Copy pixel data (CxImage stores bottom-up)
	for (int y = 0; y < height; ++y)
	{
		BYTE* dst = cximg.GetBits(height - 1 - y); // CxImage is bottom-up
		if (dst == nullptr) {
			// Failed to get bits, return invalid image
			return CxImage();
		}
		const BYTE* src = rgb.ptr<BYTE>(y);
		if (src == nullptr) {
			return CxImage();
		}
		memcpy(dst, src, width * mat.channels());
	}

	return cximg;
}
// CSynopsisMfcDlg dialog

CString GetAppPath()
{
	TCHAR szFullPath[MAX_PATH];
	::GetModuleFileName(NULL, szFullPath, MAX_PATH);

	CString strAppPath(szFullPath);
	int nPos = strAppPath.ReverseFind(_T('\\'));

	if (nPos != -1)
		strAppPath = strAppPath.Left(nPos + 1);

	return strAppPath; // includes trailing '\'
}

CSynopsisMfcDlg::CSynopsisMfcDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SYNOPSISMFC_DIALOG, pParent)
	, m_bInit(FALSE)
	, m_iFileMode(1)
	, m_iMode(0)
	, m_YoloHandle(nullptr)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSynopsisMfcDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_PATH, m_txtPath);
	DDX_Control(pDX, IDC_BTN_PLAY, m_btnPlayPause);
	DDX_Control(pDX, IDC_BTN_STOP, m_btnStop);
	DDX_Control(pDX, IDC_BTN_NEXTFRAME, m_btnNextFrame);
	DDX_Control(pDX, IDC_BTN_PREVFRAME, m_btnPrevFrame);
	DDX_Radio(pDX, IDC_RD_IMAGE, m_iFileMode);
	DDX_Radio(pDX, IDC_RD_DETECT, m_iMode);
	DDX_Control(pDX, IDC_SLIDER_SEEK, m_seekVideo);
	DDX_Control(pDX, IDC_CB_PLAYMODE, m_cbPlayMode);
	DDX_Control(pDX, IDC_LBL_TIMESTAMP, m_lblTimeStamp);
	DDX_Control(pDX, IDC_LBL_INFO, m_lblInfo);
}

BEGIN_MESSAGE_MAP(CSynopsisMfcDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CSynopsisMfcDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDCANCEL, &CSynopsisMfcDlg::OnBnClickedCancel)
	ON_BN_CLICKED(IDC_BTN_BROWSER, &CSynopsisMfcDlg::OnBnClickedBtnBrowser)
	ON_BN_CLICKED(IDC_RD_IMAGE, &CSynopsisMfcDlg::OnBnClickedRdImage)
	ON_BN_CLICKED(IDC_RD_VIDEO, &CSynopsisMfcDlg::OnBnClickedRdVideo)
	ON_BN_CLICKED(IDC_BTN_PLAY, &CSynopsisMfcDlg::OnBnClickedBtnPlay)
	ON_BN_CLICKED(IDC_BTN_STOP, &CSynopsisMfcDlg::OnBnClickedBtnStop)
	ON_BN_CLICKED(IDC_BTN_NEXTFRAME, &CSynopsisMfcDlg::OnBnClickedBtnNextframe)
	ON_BN_CLICKED(IDC_BTN_PREVFRAME, &CSynopsisMfcDlg::OnBnClickedBtnPrevframe)
	ON_REGISTERED_MESSAGE(WM_FRAME_ARRIVED, &CSynopsisMfcDlg::OnArrivedFrame)
	ON_REGISTERED_MESSAGE(WM_PROCESSED_FRAME, &CSynopsisMfcDlg::OnProcessedFrame)

	ON_BN_CLICKED(IDC_RD_SEGMENT, &CSynopsisMfcDlg::OnBnClickedRdSegment)
	ON_BN_CLICKED(IDC_RD_DETECT, &CSynopsisMfcDlg::OnBnClickedRdDetect)
	ON_BN_CLICKED(IDC_RD_CLASSIFY, &CSynopsisMfcDlg::OnBnClickedRdClassify)
	ON_BN_CLICKED(IDC_RD_POSE, &CSynopsisMfcDlg::OnBnClickedRdPose)
	ON_BN_CLICKED(IDC_RD_OBB, &CSynopsisMfcDlg::OnBnClickedRdObb)
	ON_WM_TIMER()
	ON_WM_HSCROLL()
	ON_CBN_SELCHANGE(IDC_CB_PLAYMODE, &CSynopsisMfcDlg::OnCbnSelchangeCbPlaymode)
	ON_WM_SIZE()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CSynopsisMfcDlg message handlers

LRESULT CSynopsisMfcDlg::OnArrivedFrame(WPARAM wParam, LPARAM lParam)
{
	return 0L;
}

LRESULT CSynopsisMfcDlg::OnProcessedFrame(WPARAM wParam, LPARAM lParam)
{	
	int count = (int)(wParam >> 3);
	int mode = (int)(wParam & 0x7);
	
	LOG_DEBUG_STREAM("[OnProcessedFrame] Received message: count=" << count << ", mode=" << mode);
	
	if (mode == 1) {
		// Extract DetectionResult struct
		struct DetectionResult {
			Detection* detections;
			int count;
			int64_t frameIndex;
			PlayMode playMode;
		};
		DetectionResult* result = reinterpret_cast<DetectionResult*>(lParam);
		
		if (result == nullptr) {
			return 0L;
		}
		
		// Handle count == 0 case (no detections, but frame was processed)
		if (result->count <= 0 || result->detections == nullptr) {
			// Frame was processed but no detections found
			if (result->playMode == PlayMode::Timed) {
				// In Timed mode, clear detections if this matches current frame
				int64_t currentFrame;
				{
					std::lock_guard<std::mutex> lock(mtx_);
					currentFrame = m_currentDisplayedFrame;
				}
				if (result->frameIndex == currentFrame) {
					m_imageWnd.DrawDetections(nullptr, 0);
				}
				// Also clear from cache
				{
					std::lock_guard<std::mutex> lock(m_detectionCacheMutex);
					m_detectionCache.erase(result->frameIndex);
				}
			} else {
				// In Continuous mode, clear detections immediately
				m_imageWnd.DrawDetections(nullptr, 0);
			}
			delete result;
			return 0L;
		}
		
		// Handle based on play mode
		// Timed = sync with video playback, Continuous = display as fast as possible
		if (result->playMode == PlayMode::Timed) {
			// Timed mode: Store in cache, only display if matches current frame
			{
				std::lock_guard<std::mutex> lock(m_detectionCacheMutex);
				m_detectionCache[result->frameIndex].clear();
				for (int i = 0; i < result->count; i++) {
					m_detectionCache[result->frameIndex].push_back(result->detections[i]);
				}
				// Clean up old cache entries (keep only last 100 frames)
				if (m_detectionCache.size() > 100) {
					auto it = m_detectionCache.begin();
					m_detectionCache.erase(it);
				}
			}
			
			// Check if this matches currently displayed frame
			int64_t currentFrame;
			{
				std::lock_guard<std::mutex> lock(mtx_);
				currentFrame = m_currentDisplayedFrame;
			}
			
			// Log for debugging
			if (result->count > 0) {
				LOG_DEBUG_STREAM("[OnProcessedFrame] Timed mode: result frame=" << result->frameIndex 
					<< ", current frame=" << currentFrame << ", detections=" << result->count);
			}
			
			if (result->frameIndex == currentFrame) {
				// Exact match - display immediately
				m_imageWnd.DrawDetections(result->detections, result->count);
				LOG_DEBUG_STREAM("[OnProcessedFrame] Displayed " << result->count << " detections for frame " << currentFrame);
			} else {
				// Frame mismatch - will be displayed when FrameRcvCallback checks cache
				LOG_DEBUG_STREAM("[OnProcessedFrame] Frame mismatch: cached detections for frame " 
					<< result->frameIndex << ", current frame is " << currentFrame);
			}
		}
		else {
			// Continuous mode: Display immediately regardless of frame index
			LOG_DEBUG_STREAM("[OnProcessedFrame] Continuous mode: displaying " << result->count << " detections immediately");
			m_imageWnd.DrawDetections(result->detections, result->count);
		}
		
		// Clean up
		delete[] result->detections;
		delete result;
	}

	return 0L;
}

void CSynopsisMfcDlg::FrameRcvCallback(const cv::Mat& f, int64_t frameIdx, int64_t total, double fps)
{
	// Minimize lock time - only protect frame_bgr_ update
	{
		std::lock_guard<std::mutex> lk(mtx_);
		frame_bgr_ = f.clone();
		m_currentDisplayedFrame = frameIdx; // Update current displayed frame index
	}

	// Push frame to queue immediately (non-blocking, outside of mutex)
	double ts = static_cast<double>(frameIdx) / fps;
	FrameInfo frameInfo(f, frameIdx, ts);
	
	// In Continuous mode: drop oldest when full (process latest only)
	// In Timed mode: don't drop (let queue grow, InferenceManager will skip if needed)
	bool dropOldest = (m_playMode == PlayMode::Continuous);
	bool pushed = m_frameInQueue.push(frameInfo, dropOldest);
	if (!pushed) {
		// This should only happen if queue is shutdown
		LOG_DEBUG_STREAM("[FrameRcvCallback] Frame " << frameIdx << " rejected (queue shutdown?)");
	} else {
		// Debug: log every 30 frames to verify queue is working
		if (frameIdx % 30 == 0) {
			LOG_DEBUG_STREAM("[FrameRcvCallback] Pushed frame " << frameIdx 
				<< ", queue size: " << m_frameInQueue.size());
		}
	}

	// UI updates (these should be fast, but done outside main mutex)
	if (GetSafeHwnd() && IsWindow(GetSafeHwnd())) {
		auto img = MatToCxImage(frame_bgr_);
		m_imageWnd.SetImage(img);
		auto pos = m_videoPlayer.CurrentFrame();
		
		m_seekVideo.SetPos((int)pos);
		CString timeStr = m_videoPlayer.GetFrameTimeStr();
		CString totalStr = m_videoPlayer.GetTotalTimeStr();
		CString timeInfo;
		timeInfo.Format(_T("%s / %s"), timeStr.GetString(), totalStr.GetString());
		m_lblTimeStamp.SetWindowText(timeInfo);
	}
	
	// For Timed mode: Check cache and display matching detections
	if (m_playMode == PlayMode::Timed) {
		std::lock_guard<std::mutex> lock(m_detectionCacheMutex);
		auto it = m_detectionCache.find(frameIdx);
		if (it != m_detectionCache.end() && !it->second.empty()) {
			// Display cached detections for this frame
			LOG_DEBUG_STREAM("[FrameRcvCallback] Found cached detections for frame " << frameIdx 
				<< ", count: " << it->second.size());
			m_imageWnd.DrawDetections(it->second.data(), static_cast<int>(it->second.size()));
		} else {
			// Clear detections if no cached results for this frame
			if (frameIdx % 30 == 0) {  // Log occasionally to avoid spam
				LOG_DEBUG_STREAM("[FrameRcvCallback] No cached detections for frame " << frameIdx 
					<< ", cache size: " << m_detectionCache.size());
			}
			m_imageWnd.DrawDetections(nullptr, 0);
		}
	}
	
	// (Optionally store idx/total/fps for a status bar)	
	//::PostMessage(hwnd, WM_FRAME_ARRIVED, 0, 0);
}

BOOL CSynopsisMfcDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSynopsisMfcDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSynopsisMfcDlg::OnPaint()
{
	if (!m_bInit) {
		m_bInit = TRUE;
		InitControls();
	}
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSynopsisMfcDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CSynopsisMfcDlg::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}


void CSynopsisMfcDlg::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	CDialogEx::OnCancel();
}


void CSynopsisMfcDlg::OnBnClickedBtnBrowser()
{
	CFileDialog dlg(TRUE, nullptr, nullptr,
		OFN_FILEMUSTEXIST,
		_T("Videos|*.mp4;*.avi;*.mkv;*.mov|All Files|*.*||"), this);
	if (dlg.DoModal() != IDOK) 
		return;
	CString filename = dlg.GetPathName();
	m_txtPath.SetWindowText(filename);
	m_videoPlayer.Stop();

	// Clear detection cache when opening new video
	{
		std::lock_guard<std::mutex> lock(m_detectionCacheMutex);
		m_detectionCache.clear();
	}
	m_currentDisplayedFrame = -1;

	HWND hwnd = m_imageWnd.GetSafeHwnd();
	m_videoPlayer.SetCallback([this](const cv::Mat& frame, int64_t idx, int64_t total, double fps){
		FrameRcvCallback(frame, idx, total, fps);
	});

	if (!m_videoPlayer.Open(filename))
		AfxMessageBox(_T("Cannot open video."));

	auto totalCnt = m_videoPlayer.FrameCount();
	m_seekVideo.SetRange(0, (totalCnt > 0) ? (int)(totalCnt - 1) : 0);
	m_seekVideo.SetPos(0);
	auto fps = m_videoPlayer.FPS();
	//m_timerID = SetTimer(1, 1000 / fps, nullptr);
	m_lblInfo.SetWindowText(m_videoPlayer.GetVideoSummary());
}


void CSynopsisMfcDlg::OnBnClickedRdImage()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedRdVideo()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedBtnPlay()
{	
	if(m_videoPlayer.GetState() == VideoPlayer::State::Playing) {
		m_videoPlayer.Pause();
		m_btnPlayPause.SetWindowText(_T("▶"));		
	}
	else {
		m_videoPlayer.Play();
		m_btnPlayPause.SetWindowText(_T("⏸"));
	}
}


void CSynopsisMfcDlg::OnBnClickedBtnStop()
{
	// Stop works from any state (Playing, Paused, etc.)
	m_videoPlayer.Stop();
	// Update UI button state
	m_btnPlayPause.SetWindowText(_T("▶"));
}


void CSynopsisMfcDlg::OnBnClickedBtnNextframe()
{
	m_videoPlayer.NextFrame();
}


void CSynopsisMfcDlg::OnBnClickedBtnPrevframe()
{
	m_videoPlayer.PrevFrame();
}

void CSynopsisMfcDlg::SetPlayMode(PlayMode mode)
{
	if (m_playMode == mode)
		return;
	
	m_playMode = mode;
	
	// Update combobox selection to match (SetCurSel doesn't trigger CBN_SELCHANGE)
	if (m_cbPlayMode.GetSafeHwnd()) {
		int sel = (mode == PlayMode::Timed) ? 0 : 1;
		m_cbPlayMode.SetCurSel(sel);
	}
	
	// Also update video player's play mode
	m_videoPlayer.SetPlayMode(mode);
	
	// Restart inference manager with new mode
	m_InfManager.stop();
	
	// Reset queues (clear shutdown flag and clear contents)
	m_frameInQueue.reset();
	m_frameOutQueue.reset();
	
	// Clear cache when switching modes
	{
		std::lock_guard<std::mutex> lock(m_detectionCacheMutex);
		m_detectionCache.clear();
	}
	
	// Restart with new mode
	m_InfManager.start(&m_frameInQueue, &m_frameOutQueue, 
		m_playMode, 
		m_YoloHandle, this, WM_PROCESSED_FRAME);
}

void CSynopsisMfcDlg::InitControls()
{
	CRect rcWnd;
	GetDlgItem(IDC_PIC_FRAME)->GetWindowRect(&rcWnd);
	ScreenToClient(&rcWnd);
	m_imageWnd.CreateWnd(this, rcWnd, IDC_PIC_FRAME, 0);
	vsInitYoloModel(&m_YoloHandle, GetAppPath());

	// Initialize play mode combobox
	m_cbPlayMode.ResetContent();
	m_cbPlayMode.AddString(_T("Timed"));           // Index 0 = Timed (sync with video)
	m_cbPlayMode.AddString(_T("Continuous"));     // Index 1 = Continuous (as fast as possible)
	m_cbPlayMode.SetCurSel(m_playMode == PlayMode::Timed ? 0 : 1);

	// Set queue max size to prevent memory issues
	m_frameInQueue.setMaxSize(MAX_QUEUE_SIZE);
	m_frameOutQueue.setMaxSize(MAX_QUEUE_SIZE);
	
	// Ensure queues are in active state (not shutdown)
	m_frameInQueue.reset();
	m_frameOutQueue.reset();

	// Start inference manager with current play mode
	m_InfManager.start(&m_frameInQueue, &m_frameOutQueue, 
		m_playMode, 
		m_YoloHandle, this, WM_PROCESSED_FRAME);
}

void CSynopsisMfcDlg::OnCbnSelchangeCbPlaymode()
{
	int sel = m_cbPlayMode.GetCurSel();
	if (sel == CB_ERR)
		return;
	
	PlayMode newMode = (sel == 0) ? PlayMode::Timed : PlayMode::Continuous;
	SetPlayMode(newMode);
}

void CSynopsisMfcDlg::OnBnClickedRdSegment()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedRdDetect()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedRdClassify()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedRdPose()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedRdObb()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnTimer(UINT_PTR nIDEvent)
{
	CDialogEx::OnTimer(nIDEvent);
}


void CSynopsisMfcDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar->GetSafeHwnd() == m_seekVideo.GetSafeHwnd())
	{
		isSeeking_ = true;

		/*if (nSBCode == TB_ENDTRACK || nSBCode == SB_THUMBPOSITION)
		{
			int frameIndex = m_seekVideo.GetPos();
			m_videoPlayer.seekFrame(frameIndex);

			cv::Mat frame;
			if (m_videoPlayer.nextFrame(frame)) {
				ShowFrame(frame);
			}

			isSeeking_ = false;
		}*/
	}

	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}


void CSynopsisMfcDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	if (m_bInit) {
		CRect rc;
		GetDlgItem(IDC_PIC_FRAME)->GetWindowRect(&rc);
		ScreenToClient(&rc);
		m_imageWnd.MoveWindow(rc);
	}
}


void CSynopsisMfcDlg::OnDestroy()
{
	CDialogEx::OnDestroy();

	if (m_YoloHandle) {
		vsReleaseYoloModel(m_YoloHandle);
		m_YoloHandle = nullptr;
	}
	m_InfManager.stop();
}
