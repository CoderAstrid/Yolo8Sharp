
// SynopsisMfcDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "SynopsisMfc.h"
#include "SynopsisMfcDlg.h"
#include "afxdialogex.h"
#include <mutex>

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
		const BYTE* src = rgb.ptr<BYTE>(y);
		memcpy(dst, src, width * mat.channels());
	}

	return cximg;
}
// CSynopsisMfcDlg dialog



CSynopsisMfcDlg::CSynopsisMfcDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_SYNOPSISMFC_DIALOG, pParent)
	, m_bInit(FALSE)
	, m_iFileMode(1)
	, m_iMode(0)
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
	ON_BN_CLICKED(IDC_RD_SEGMENT, &CSynopsisMfcDlg::OnBnClickedRdSegment)
	ON_BN_CLICKED(IDC_RD_DETECT, &CSynopsisMfcDlg::OnBnClickedRdDetect)
	ON_BN_CLICKED(IDC_RD_CLASSIFY, &CSynopsisMfcDlg::OnBnClickedRdClassify)
	ON_BN_CLICKED(IDC_RD_POSE, &CSynopsisMfcDlg::OnBnClickedRdPose)
	ON_BN_CLICKED(IDC_RD_OBB, &CSynopsisMfcDlg::OnBnClickedRdObb)
END_MESSAGE_MAP()


// CSynopsisMfcDlg message handlers

LRESULT CSynopsisMfcDlg::OnArrivedFrame(WPARAM wParam, LPARAM lParam)
{
	return 0;
}

void CSynopsisMfcDlg::FrameRcvCallback(const cv::Mat& f, int64_t frameIdx, int64_t total, double fps)
{
	std::scoped_lock lk(mtx_);
	frame_bgr_ = f.clone();

	auto img = MatToCxImage(frame_bgr_);
	m_imageWnd.SetImage(img);
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

	HWND hwnd = m_imageWnd.GetSafeHwnd();
	m_videoPlayer.SetCallback([this](const cv::Mat& frame, int64_t idx, int64_t total, double fps){
		FrameRcvCallback(frame, idx, total, fps);
	});

	if (!m_videoPlayer.Open(filename))
		AfxMessageBox(_T("Cannot open video."));
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
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedBtnStop()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedBtnNextframe()
{
	// TODO: Add your control notification handler code here
}


void CSynopsisMfcDlg::OnBnClickedBtnPrevframe()
{
	// TODO: Add your control notification handler code here
}

void CSynopsisMfcDlg::InitControls()
{
	CRect rcWnd;
	GetDlgItem(IDC_PIC_FRAME)->GetWindowRect(&rcWnd);
	ScreenToClient(&rcWnd);
	m_imageWnd.CreateWnd(this, rcWnd, IDC_PIC_FRAME, 0);

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
