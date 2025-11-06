
// SynopsisMfc.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include <gdiplus.h>

// CSynopsisMfcApp:
// See SynopsisMfc.cpp for the implementation of this class
//

class CSynopsisMfcApp : public CWinApp
{
public:
	CSynopsisMfcApp();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance(); // to shut down GDI+
private:
	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CSynopsisMfcApp theApp;
