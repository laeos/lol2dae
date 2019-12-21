
// lol2dae.h : main header file for the PROJECT_NAME application
//

#pragma once

#include "resource.h"		// main symbols


// Clol2daeApp:
// See lol2dae.cpp for the implementation of this class
//

class Clol2daeApp : public CWinApp
{
public:
	Clol2daeApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern Clol2daeApp theApp;
