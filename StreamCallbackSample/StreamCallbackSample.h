// StreamCallbackSample.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CStreamCallbackSampleApp:
// See StreamCallbackSample.cpp for the implementation of this class
//

class CStreamCallbackSampleApp : public CWinApp
{
private: 
	INT_PTR m_nReturnCode;

public:
	CStreamCallbackSampleApp();

// Overrides
	public:
	virtual BOOL InitInstance();
		virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CStreamCallbackSampleApp theApp;