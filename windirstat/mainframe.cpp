// mainframe.cpp - Implementation of CMySplitterWnd, CPacmanControl and CMainFrame
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//

#include "stdafx.h"
#include "windirstat.h"

#include "graphview.h"
#include "dirstatview.h"
#include "typeview.h"
#include "dirstatdoc.h"
#include "osspecific.h"
#include "globalhelpers.h"
#include "item.h"

#include "pagecleanups.h"
#include "pagetreelist.h"
#include "pagetreemap.h"
#include "pagegeneral.h"

#include <common/mdexceptions.h>
#include <common/commonhelpers.h>

#include "mainframe.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
    // This must be synchronized with the IDR_MAINFRAME menu
    enum TOPLEVELMENU
    {
        TLM_FILE,
        TLM_EDIT,
        TLM_CLEANUP,
        TLM_TREEMAP,
        TLM_REPORT,
        TLM_VIEW,
        TLM_HELP
    };

    enum
    {
        // This is the position of the first "User defined cleanup" menu item in the "Cleanup" menu.
        // !!! MUST BE SYNCHRONIZED WITH THE MENU RESOURCE !!!
        MAINMENU_USERDEFINEDCLEANUP_POSITION = 11
    };

    enum
    {
        IDC_SUSPEND = 4712, // ID of "Suspend"-Button
        IDC_DEADFOCUS       // ID of dead-focus window
    };

    // Clipboard-Opener
    class COpenClipboard
    {
    public:
        COpenClipboard(CWnd *owner, bool empty =true)
        {
            m_open = owner->OpenClipboard();
            if(!m_open)
            {
                MdThrowStringException(IDS_CANNOTOPENCLIPBOARD);
            }
            if(empty)
            {
                if(!EmptyClipboard())
                {
                    MdThrowStringException(IDS_CANNOTEMTPYCLIPBOARD);
                }
            }
        }
        ~COpenClipboard()
        {
            if(m_open)
            {
                CloseClipboard();
            }
        }
    private:
        BOOL m_open;
    };


}


/////////////////////////////////////////////////////////////////////////////

IMPLEMENT_DYNAMIC(COptionsPropertySheet, CPropertySheet)

COptionsPropertySheet::COptionsPropertySheet()
    : CPropertySheet(IDS_WINDIRSTAT_SETTINGS)
    , m_restartApplication(false)
    , m_languageChanged(false)
    , m_alreadyAsked(false)
{
}

void COptionsPropertySheet::SetLanguageChanged(bool changed)
{
    m_languageChanged = changed;
}

BOOL COptionsPropertySheet::OnInitDialog()
{
    BOOL bResult = CPropertySheet::OnInitDialog();

    CRect rc;
    GetWindowRect(rc);
    CPoint pt = rc.TopLeft();
    CPersistence::GetConfigPosition(pt);
    CRect rc2(pt, rc.Size());
    MoveWindow(rc2);

    SetActivePage(CPersistence::GetConfigPage(GetPageCount() - 1));
    return bResult;
}

BOOL COptionsPropertySheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
    CPersistence::SetConfigPage(GetActiveIndex());

    CRect rc;
    GetWindowRect(rc);
    CPersistence::SetConfigPosition(rc.TopLeft());

    int cmd = LOWORD(wParam);
    if((IDOK == cmd) || (ID_APPLY_NOW == cmd))
    {
        if(m_languageChanged && ((IDOK == cmd) || (!m_alreadyAsked)))
        {
            int r = AfxMessageBox(IDS_LANGUAGERESTARTNOW, MB_YESNOCANCEL);
            if(IDCANCEL == r)
            {
                return true;    // "Message handled". Don't proceed.
            }
            else if(IDNO == r)
            {
                m_alreadyAsked = true; // Don't ask twice.
            }
            else
            {
                ASSERT(IDYES == r);
                m_restartApplication = true;

                if(ID_APPLY_NOW == cmd)
                {
                    // This _posts_ a message...
                    EndDialog(IDOK);
                    // ... so after returning from this function, the OnOK()-handlers
                    // of the pages will be called, before the sheet is closed.
                }
            }
        }
    }

    return CPropertySheet::OnCommand(wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////

CMySplitterWnd::CMySplitterWnd(LPCTSTR name)
    : m_persistenceName(name)
    , m_splitterPos(0.5)
{
    CPersistence::GetSplitterPos(m_persistenceName, m_wasTrackedByUser, m_userSplitterPos);
}

BEGIN_MESSAGE_MAP(CMySplitterWnd, CSplitterWnd)
    ON_WM_SIZE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

void CMySplitterWnd::StopTracking(BOOL bAccept)
{
    CSplitterWnd::StopTracking(bAccept);

    if(bAccept)
    {
        CRect rcClient;
        GetClientRect(rcClient);

        if(GetColumnCount() > 1)
        {
            int dummy;
            int cxLeft;
            GetColumnInfo(0, cxLeft, dummy);

            if(rcClient.Width() > 0)
            {
                m_splitterPos = (double)cxLeft / rcClient.Width();
            }
        }
        else
        {
            int dummy;
            int cyUpper;
            GetRowInfo(0, cyUpper, dummy);

            if(rcClient.Height() > 0)
            {
                m_splitterPos = (double)cyUpper / rcClient.Height();
            }
        }
        m_wasTrackedByUser = true;
        m_userSplitterPos = m_splitterPos;
    }
}

double CMySplitterWnd::GetSplitterPos()
{
    return m_splitterPos;
}

void CMySplitterWnd::SetSplitterPos(double pos)
{
    m_splitterPos = pos;

    CRect rcClient;
    GetClientRect(rcClient);

    if(GetColumnCount() > 1)
    {
        if(m_pColInfo != NULL)
        {
            int cxLeft = (int) (pos * rcClient.Width());
            if(cxLeft >= 0)
            {
                SetColumnInfo(0, cxLeft, 0);
                RecalcLayout();
            }
        }
    }
    else
    {
        if(m_pRowInfo != NULL)
        {
            int cyUpper = (int) (pos * rcClient.Height());
            if(cyUpper >= 0)
            {
                SetRowInfo(0, cyUpper, 0);
                RecalcLayout();
            }
        }
    }
}

void CMySplitterWnd::RestoreSplitterPos(double posIfVirgin)
{
    if(m_wasTrackedByUser)
    {
        SetSplitterPos(m_userSplitterPos);
    }
    else
    {
        SetSplitterPos(posIfVirgin);
    }
}

void CMySplitterWnd::OnSize(UINT nType, int cx, int cy)
{
    if(GetColumnCount() > 1)
    {
        int cxLeft = (int)(cx * m_splitterPos);
        if(cxLeft > 0)
        {
            SetColumnInfo(0, cxLeft, 0);
        }
    }
    else
    {
        int cyUpper = (int)(cy * m_splitterPos);
        if(cyUpper > 0)
        {
            SetRowInfo(0, cyUpper, 0);
        }
    }
    CSplitterWnd::OnSize(nType, cx, cy);
}

void CMySplitterWnd::OnDestroy()
{
    CPersistence::SetSplitterPos(m_persistenceName, m_wasTrackedByUser, m_userSplitterPos);
    CSplitterWnd::OnDestroy();
}


/////////////////////////////////////////////////////////////////////////////

CPacmanControl::CPacmanControl()
{
    m_pacman.SetBackgroundColor(::GetSysColor(COLOR_BTNFACE));
    m_pacman.SetSpeed(0.00005);
}

void CPacmanControl::Drive(ULONGLONG readJobs)
{
    if(::IsWindow(m_hWnd) && m_pacman.Drive(readJobs))
    {
        RedrawWindow();
    }
}

void CPacmanControl::Start(bool start)
{
    m_pacman.Start(start);
}

BEGIN_MESSAGE_MAP(CPacmanControl, CStatic)
    ON_WM_PAINT()
    ON_WM_CREATE()
END_MESSAGE_MAP()

int CPacmanControl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if(CStatic::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    m_pacman.Reset();
    m_pacman.Start(true);
    return 0;
}

void CPacmanControl::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(rc);
    m_pacman.Draw(&dc, rc);
}

/////////////////////////////////////////////////////////////////////////////

CDeadFocusWnd::CDeadFocusWnd()
{
}

void CDeadFocusWnd::Create(CWnd *parent)
{
    CRect rc(0,0,0,0);
    VERIFY(CWnd::Create(AfxRegisterWndClass(0, 0, 0, 0), _T("_deadfocus"), WS_CHILD, rc, parent, IDC_DEADFOCUS));
}

CDeadFocusWnd::~CDeadFocusWnd()
{
    DestroyWindow();
}

BEGIN_MESSAGE_MAP(CDeadFocusWnd, CWnd)
    ON_WM_KEYDOWN()
END_MESSAGE_MAP()

void CDeadFocusWnd::OnKeyDown(UINT nChar, UINT /* nRepCnt */, UINT /* nFlags */)
{
    if(nChar == VK_TAB)
    {
        GetMainFrame()->MoveFocus(LF_DIRECTORYLIST);
    }
}

/////////////////////////////////////////////////////////////////////////////
#ifdef SUPPORT_W7_TASKBAR
UINT CMainFrame::s_taskBarMessage = ::RegisterWindowMessage(TEXT("TaskbarButtonCreated"));
#endif // SUPPORT_W7_TASKBAR

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_MESSAGE(WM_ENTERSIZEMOVE, OnEnterSizeMove)
    ON_MESSAGE(WM_EXITSIZEMOVE, OnExitSizeMove)
    ON_WM_CLOSE()
    ON_WM_INITMENUPOPUP()
    ON_UPDATE_COMMAND_UI(ID_INDICATOR_MEMORYUSAGE, OnUpdateMemoryUsage)
    ON_WM_SIZE()
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWTREEMAP, OnUpdateViewShowtreemap)
    ON_COMMAND(ID_VIEW_SHOWTREEMAP, OnViewShowtreemap)
    ON_UPDATE_COMMAND_UI(ID_VIEW_SHOWFILETYPES, OnUpdateViewShowfiletypes)
    ON_COMMAND(ID_VIEW_SHOWFILETYPES, OnViewShowfiletypes)
    ON_COMMAND(ID_CONFIGURE, OnConfigure)
    ON_WM_DESTROY()
    ON_COMMAND(ID_TREEMAP_HELPABOUTTREEMAPS, OnTreemapHelpabouttreemaps)
    ON_BN_CLICKED(IDC_SUSPEND, OnBnClickedSuspend)
    ON_WM_SYSCOLORCHANGE()
#ifdef SUPPORT_W7_TASKBAR
    ON_REGISTERED_MESSAGE(s_taskBarMessage, OnTaskButtonCreated)
#endif // SUPPORT_W7_TASKBAR
END_MESSAGE_MAP()

static UINT indicators[] =
{
    ID_SEPARATOR,
    ID_INDICATOR_MEMORYUSAGE,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};

static UINT indicatorsWithoutMemoryUsage[] =
{
    ID_SEPARATOR,
    ID_INDICATOR_CAPS,
    ID_INDICATOR_NUM,
    ID_INDICATOR_SCRL,
};


CMainFrame *CMainFrame::_theFrame;

CMainFrame *CMainFrame::GetTheFrame()
{
    return _theFrame;
}

CMainFrame::CMainFrame()
    : m_wndSplitter(_T("main"))
    , m_wndSubSplitter(_T("sub"))
    , m_progressVisible(false)
    , m_progressRange(100)
    , m_progressPos(0)
    , m_logicalFocus(LF_NONE)
#ifdef SUPPORT_W7_TASKBAR
    , m_TaskbarButtonState(TBPF_INDETERMINATE)
    , m_TaskbarButtonPreviousState(TBPF_INDETERMINATE)
#endif // SUPPORT_W7_TASKBAR
{
    _theFrame = this;
}

CMainFrame::~CMainFrame()
{
    _theFrame = NULL;
}

#ifdef SUPPORT_W7_TASKBAR
LRESULT CMainFrame::OnTaskButtonCreated(WPARAM, LPARAM)
{
    if(!m_TaskbarList)
    {
        HRESULT hr = ::CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, reinterpret_cast<LPVOID*>(&m_TaskbarList));
        if(FAILED(hr))
        {
            VTRACE(_T("CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL) failed %08X"), hr);
        }
    }
    return 0;
}
#endif // SUPPORT_W7_TASKBAR

void CMainFrame::ShowProgress(ULONGLONG range)
{
    // A range of 0 means that we have no range.
    // In this case we display pacman.
    HideProgress();

    if(GetOptions()->IsFollowMountPoints() || GetOptions()->IsFollowJunctionPoints())
    {
        range = 0;
    }
    m_progressRange = range;
    m_progressPos = 0;
    m_progressVisible = true;
    if(range > 0)
    {
        CreateStatusProgress();
    }
    else
    {
        CreatePacmanProgress();
    }
    UpdateProgress();
}

void CMainFrame::HideProgress()
{
    DestroyProgress();
    if(m_progressVisible)
    {
        m_progressVisible = false;
        if(::IsWindow(*GetMainFrame()))
        {
            GetDocument()->SetTitlePrefix(wds::strEmpty);
            SetMessageText(AFX_IDS_IDLEMESSAGE);
        }
    }
}

void CMainFrame::SetProgressPos(ULONGLONG pos)
{
    if(m_progressRange > 0 && pos > m_progressRange)
    {
        pos = m_progressRange;
    }

    m_progressPos = pos;
    UpdateProgress();
}

void CMainFrame::SetProgressPos100() // called by CDirstatDoc
{
    if(m_progressRange > 0)
    {
        SetProgressPos(m_progressRange);
    }
#ifdef SUPPORT_W7_TASKBAR
    if(m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NOPROGRESS);
    }
#endif // SUPPORT_W7_TASKBAR
}

bool CMainFrame::IsProgressSuspended()
{
    if(!::IsWindow(m_suspendButton.m_hWnd))
    {
        return false;
    }
    return ((m_suspendButton.GetState() & 0x3) != 0);
}

void CMainFrame::DrivePacman()
{
    m_pacman.Drive(GetDocument()->GetWorkingItemReadJobs());
}

void CMainFrame::UpdateProgress()
{
    if(m_progressVisible)
    {
        CString titlePrefix;
        CString suspended;

        if(IsProgressSuspended())
        {
            VERIFY(suspended.LoadString(IDS_SUSPENDED_));
        }

        if(m_progressRange > 0)
        {
            int pos = (int)((double) m_progressPos * 100 / m_progressRange);
            m_progress.SetPos(pos);
            titlePrefix.Format(_T("%d%% %s"), pos, suspended.GetString());
#ifdef SUPPORT_W7_TASKBAR
            if(m_TaskbarList && (m_TaskbarButtonState != TBPF_PAUSED))
            {
                switch(pos)
                {
                // FIXME: hardcoded value here and elsewhere in this file
                case 100:
                    m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE); // often happens before we're finished
                    break;
                default:
                    m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_NORMAL); // often happens before we're finished
                    m_TaskbarList->SetProgressValue(*this, m_progressPos, m_progressRange);
                    break;
                }
            }
#endif // SUPPORT_W7_TASKBAR
        }
        else
        {
            titlePrefix = LoadString(IDS_SCANNING_) + suspended;
        }

        GetDocument()->SetTitlePrefix(titlePrefix);
    }
}

void CMainFrame::CreateStatusProgress()
{
    if(m_progress.m_hWnd == NULL)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        CreateSuspendButton(rc);
        m_progress.Create(WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, 4711);
        m_progress.ModifyStyle(WS_BORDER, 0); // Doesn't help with XP-style control.
    }
#ifdef SUPPORT_W7_TASKBAR
    if(m_TaskbarList)
    {
        m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_INDETERMINATE);
    }
#endif // SUPPORT_W7_TASKBAR
}

void CMainFrame::CreatePacmanProgress()
{
    if(m_pacman.m_hWnd == NULL)
    {
        CRect rc;
        m_wndStatusBar.GetItemRect(0, rc);
        CreateSuspendButton(rc);
        m_pacman.Create(wds::strEmpty, WS_CHILD | WS_VISIBLE, rc, &m_wndStatusBar, 4711); // FIXME: hard-coded value out
    }
}

// rc [in]: Rect of status pane
// rc [out]: Rest for progress/pacman-control
void CMainFrame::CreateSuspendButton(CRect& rc)
{
    CRect rcButton = rc;
    rcButton.right = rcButton.left + 80; // FIXME: hardcoded value

    VERIFY(m_suspendButton.Create(LoadString(IDS_SUSPEND), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, rcButton, &m_wndStatusBar, IDC_SUSPEND));
    m_suspendButton.SetFont(GetDirstatView()->GetSmallFont());

    rc.left = rcButton.right;
}

void CMainFrame::DestroyProgress()
{
    if(::IsWindow(m_progress.m_hWnd))
    {
        m_progress.DestroyWindow();
        m_progress.m_hWnd = NULL;
    }
    else if(::IsWindow(m_pacman.m_hWnd))
    {
        m_pacman.DestroyWindow();
        m_pacman.m_hWnd = NULL;
    }
    if(::IsWindow(m_suspendButton.m_hWnd))
    {
        m_suspendButton.DestroyWindow();
        m_suspendButton.m_hWnd = NULL;
    }
}

void CMainFrame::OnBnClickedSuspend()
{
    bool const isSuspended = IsProgressSuspended();
    m_pacman.Start(!isSuspended);
#ifdef SUPPORT_W7_TASKBAR
    if(m_TaskbarList)
    {
        switch(m_TaskbarButtonState)
        {
        case TBPF_PAUSED:
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = m_TaskbarButtonPreviousState);
            break;
        default:
            m_TaskbarButtonPreviousState = m_TaskbarButtonState;
            m_TaskbarList->SetProgressState(*this, m_TaskbarButtonState = TBPF_PAUSED);
            break;
        }
    }
#endif // SUPPORT_W7_TASKBAR
    UpdateProgress();
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if(CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    VERIFY(m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC));
    VERIFY(m_wndToolBar.LoadToolBar(IDR_MAINFRAME));

    UINT *indic = indicators;
    UINT size = _countof(indicators);

    // If psapi is not supported, don't show that pane.
    if(GetWDSApp()->GetCurrentProcessMemoryInfo() == wds::strEmpty)
    {
        indic = indicatorsWithoutMemoryUsage;
        size = _countof(indicatorsWithoutMemoryUsage);
    }

    VERIFY(m_wndStatusBar.Create(this));
    VERIFY(m_wndStatusBar.SetIndicators(indic, size));
    m_wndDeadFocus.Create(this);

    m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
    EnableDocking(CBRS_ALIGN_ANY);
    DockControlBar(&m_wndToolBar);

    LoadBarState(CPersistence::GetBarStateSection());
    ShowControlBar(&m_wndToolBar, CPersistence::GetShowToolbar(), false);
    ShowControlBar(&m_wndStatusBar, CPersistence::GetShowStatusbar(), false);

    return 0;
}

void CMainFrame::InitialShowWindow()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    GetWindowPlacement(&wp);
    CPersistence::GetMainWindowPlacement(wp);
    MakeSaneShowCmd(wp.showCmd);
    SetWindowPlacement(&wp);
}

void CMainFrame::MakeSaneShowCmd(UINT& u)
{
    switch (u)
    {
    default:
    case SW_HIDE:
    case SW_MINIMIZE:
    case SW_SHOWMINNOACTIVE:
    case SW_SHOWNA:
    case SW_SHOWMINIMIZED:
    case SW_SHOWNOACTIVATE:
    case SW_RESTORE:
    case SW_FORCEMINIMIZE:
    case SW_SHOWDEFAULT:
    case SW_SHOW:
    case SW_SHOWNORMAL:
        {
            u = SW_SHOWNORMAL;
        }
        break;
    case SW_SHOWMAXIMIZED:
        break;
    }
}

void CMainFrame::OnClose()
{
    CWaitCursor wc;

    // It's too late, to do this in OnDestroy(). Because the toolbar, if undocked,
    // is already destroyed in OnDestroy(). So we must save the toolbar state here
    // in OnClose().
    SaveBarState(CPersistence::GetBarStateSection());
    CPersistence::SetShowToolbar((m_wndToolBar.GetStyle() & WS_VISIBLE) != 0);
    CPersistence::SetShowStatusbar((m_wndStatusBar.GetStyle() & WS_VISIBLE) != 0);

#ifdef _DEBUG
    // avoid memory leaks an show hourglass while deleting the tree
    GetDocument()->OnNewDocument();
#endif

    GetDocument()->ForgetItemTree();
    CFrameWnd::OnClose();
}

void CMainFrame::OnDestroy()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(wp);
    GetWindowPlacement(&wp);
    CPersistence::SetMainWindowPlacement(wp);

    CPersistence::SetShowFileTypes(GetTypeView()->IsShowTypes());
    CPersistence::SetShowTreemap(GetGraphView()->IsShowTreemap());

    CFrameWnd::OnDestroy();
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT /*lpcs*/, CCreateContext* pContext)
{
    VERIFY(m_wndSplitter.CreateStatic(this, 2, 1));
    VERIFY(m_wndSplitter.CreateView(1, 0, RUNTIME_CLASS(CGraphView), CSize(100, 100), pContext));
    VERIFY(m_wndSubSplitter.CreateStatic(&m_wndSplitter, 1, 2, WS_CHILD | WS_VISIBLE | WS_BORDER, m_wndSplitter.IdFromRowCol(0, 0)));
    VERIFY(m_wndSubSplitter.CreateView(0, 0, RUNTIME_CLASS(CDirstatView), CSize(700, 500), pContext));
    VERIFY(m_wndSubSplitter.CreateView(0, 1, RUNTIME_CLASS(CTypeView), CSize(100, 500), pContext));

    MinimizeGraphView();
    MinimizeTypeView();

    GetTypeView()->ShowTypes(CPersistence::GetShowFileTypes());
    GetGraphView()->ShowTreemap(CPersistence::GetShowTreemap());

    return TRUE;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if( !CFrameWnd::PreCreateWindow(cs) )
    {
        return FALSE;
    }

    return TRUE;
}


// CMainFrame Diagnose

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
    CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
    CFrameWnd::Dump(dc);
}

#endif //_DEBUG

void CMainFrame::MinimizeTypeView()
{
    m_wndSubSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreTypeView()
{
    if(GetTypeView()->IsShowTypes())
    {
        m_wndSubSplitter.RestoreSplitterPos(0.72);
        GetTypeView()->RedrawWindow();
    }
}

void CMainFrame::MinimizeGraphView()
{
    m_wndSplitter.SetSplitterPos(1.0);
}

void CMainFrame::RestoreGraphView()
{
    if(GetGraphView()->IsShowTreemap())
    {
        m_wndSplitter.RestoreSplitterPos(0.4);
        GetGraphView()->DrawEmptyView();
        GetGraphView()->RedrawWindow();
    }
}

CDirstatView* CMainFrame::GetDirstatView()
{
    CWnd* pWnd = m_wndSubSplitter.GetPane(0, 0);
    CDirstatView* pView = DYNAMIC_DOWNCAST(CDirstatView, pWnd);
    return pView;
}

CGraphView *CMainFrame::GetGraphView()
{
    CWnd *pWnd = m_wndSplitter.GetPane(1, 0);
    CGraphView *pView = DYNAMIC_DOWNCAST(CGraphView, pWnd);
    return pView;
}

CTypeView *CMainFrame::GetTypeView()
{
    CWnd *pWnd = m_wndSubSplitter.GetPane(0, 1);
    CTypeView *pView = DYNAMIC_DOWNCAST(CTypeView, pWnd);
    return pView;
}

LRESULT CMainFrame::OnEnterSizeMove(WPARAM, LPARAM)
{
    GetGraphView()->SuspendRecalculation(true);
    return 0;
}

LRESULT CMainFrame::OnExitSizeMove(WPARAM, LPARAM)
{
    GetGraphView()->SuspendRecalculation(false);
    return 0;
}

void CMainFrame::CopyToClipboard(LPCTSTR psz)
{
    try
    {
        COpenClipboard clipboard(this);
        SIZE_T cchBufLen = _tcslen(psz) + 1;

        HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (cchBufLen) * sizeof(TCHAR));
        if(h == NULL)
        {
            MdThrowStringException(_T("GlobalAlloc failed."));
        }

        LPVOID lp = ::GlobalLock(h);
        ASSERT(lp != NULL);

        if (!lp)
        {
            MdThrowStringException(_T("GlobalLock failed."));
        }

        _tcscpy_s((LPTSTR)lp, cchBufLen, psz);

        ::GlobalUnlock(h);

        UINT uFormat = CF_TEXT;
#ifdef UNICODE
        uFormat = CF_UNICODETEXT;
#endif
        if(NULL == ::SetClipboardData(uFormat, h))
        {
            MdThrowStringException(IDS_CANNOTSETCLIPBAORDDATA);
        }
    }
    catch (CException *pe)
    {
        pe->ReportError();
        pe->Delete();
    }
}

void CMainFrame::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CFrameWnd::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if(!bSysMenu)
    {
        switch (nIndex)
        {
        case TLM_CLEANUP:
            {
                UpdateCleanupMenu(pPopupMenu);
            }
            break;
        }
    }
}

void CMainFrame::UpdateCleanupMenu(CMenu *menu)
{
    CString s = LoadString(IDS_EMPTYRECYCLEBIN);
    VERIFY(menu->ModifyMenu(ID_CLEANUP_EMPTYRECYCLEBIN, MF_BYCOMMAND | MF_STRING, ID_CLEANUP_EMPTYRECYCLEBIN, s));
    // TODO: can be cleaned, so that we don't disable and then enable the menu item
    menu->EnableMenuItem(ID_CLEANUP_EMPTYRECYCLEBIN, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);

    ULONGLONG items;
    ULONGLONG bytes;

    queryRecycleBin(items, bytes);

    CString info;
    if(items == 1)
    {
        info.FormatMessage(IDS__ONEITEMss, FormatBytes(bytes).GetString(), GetOptions()->IsHumanFormat() && bytes != 0 ? wds::strEmpty : (wds::strBlankSpace + GetSpec_Bytes()).GetString() );
    }
    else
    {
        info.FormatMessage(IDS__sITEMSss, FormatCount(items).GetString(), FormatBytes(bytes).GetString(), GetOptions()->IsHumanFormat() && bytes != 0 ? wds::strEmpty : (wds::strBlankSpace + GetSpec_Bytes()).GetString());
    }

    s += info;
    VERIFY(menu->ModifyMenu(ID_CLEANUP_EMPTYRECYCLEBIN, MF_BYCOMMAND | MF_STRING, ID_CLEANUP_EMPTYRECYCLEBIN, s));

    // ModifyMenu() re-enables the item. So we disable (or enable) it again.

    UINT flags = (items > 0 ? MF_ENABLED : MF_DISABLED | MF_GRAYED);
    flags |= MF_BYCOMMAND;

    menu->EnableMenuItem(ID_CLEANUP_EMPTYRECYCLEBIN, flags);

    UINT toRemove = menu->GetMenuItemCount() - MAINMENU_USERDEFINEDCLEANUP_POSITION;
    for(UINT i = 0; i < toRemove; i++)
    {
        menu->RemoveMenu(MAINMENU_USERDEFINEDCLEANUP_POSITION, MF_BYPOSITION);
    }

    AppendUserDefinedCleanups(menu);
}

void CMainFrame::queryRecycleBin(ULONGLONG& items, ULONGLONG& bytes)
{
    // On W2k, the first parameter to SHQueryRecycleBin must not be NULL.
    // So we must sum the item counts and sizes of the recycle bins of all local drives.

    items = 0;
    bytes = 0;

    DWORD drives = ::GetLogicalDrives();
    int i;
    DWORD mask = 0x00000001;
    for(i = 0; i < wds::iNumDriveLetters; i++, mask <<= 1)
    {
        if((drives & mask) == 0)
        {
            continue;
        }

        CString s;
        s.Format(_T("%c:\\"), i + wds::chrCapA);

        UINT type = ::GetDriveType(s);
        if(type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
        {
            continue;
        }

        if(type == DRIVE_REMOTE)
        {
            continue;
        }

        SHQUERYRBINFO qbi;
        ZeroMemory(&qbi, sizeof(qbi));
        qbi.cbSize = sizeof(qbi);

        HRESULT hr = ::SHQueryRecycleBin(s, &qbi);

        if(FAILED(hr))
        {
            continue;
        }

        items += qbi.i64NumItems;
        bytes += qbi.i64Size;
    }
}

void CMainFrame::AppendUserDefinedCleanups(CMenu *menu)
{
    CArray<int, int> indices;
    GetOptions()->GetEnabledUserDefinedCleanups(indices);
    if(indices.GetSize() > 0)
    {
        for(int i = 0; i < indices.GetSize(); i++)
        {
            CString string;
            string.FormatMessage(IDS_UDCsCTRLd, GetOptions()->GetUserDefinedCleanup(indices[i])->title.GetString(), indices[i]);

            UINT flags = MF_GRAYED | MF_DISABLED;
            if(
            GetLogicalFocus() == LF_DIRECTORYLIST
            // FIXME: Multi-select
            && GetDocument()->UserDefinedCleanupWorksForItem(GetOptions()->GetUserDefinedCleanup(indices[i]), GetDocument()->GetSelection(0))
            )
            {
                flags = MF_ENABLED;
            }
            menu->AppendMenu(flags|MF_STRING, ID_USERDEFINEDCLEANUP0 + indices[i], string);
        }
    }
    else
    {
        // This is just to show new users, that they can configure user defined cleanups.
        menu->AppendMenu(MF_GRAYED, 0, LoadString(IDS_USERDEFINEDCLEANUP0));
    }
}

void CMainFrame::SetLogicalFocus(LOGICAL_FOCUS lf)
{
    if(lf != m_logicalFocus)
    {
        m_logicalFocus = lf;
        SetSelectionMessageText();

        GetDocument()->UpdateAllViews(NULL, HINT_SELECTIONSTYLECHANGED);
    }
}

LOGICAL_FOCUS CMainFrame::GetLogicalFocus()
{
    return m_logicalFocus;
}

void CMainFrame::MoveFocus(LOGICAL_FOCUS lf)
{
    switch (lf)
    {
    case LF_NONE:
        {
            SetLogicalFocus(LF_NONE);
            m_wndDeadFocus.SetFocus();
        }
        break;
    case LF_DIRECTORYLIST:
        {
            GetDirstatView()->SetFocus();
        }
        break;
    case LF_EXTENSIONLIST:
        {
            GetTypeView()->SetFocus();
        }
        break;
    }
}

void CMainFrame::SetSelectionMessageText()
{
    switch (GetLogicalFocus())
    {
    case LF_NONE:
        {
            SetMessageText(AFX_IDS_IDLEMESSAGE);
        }
        break;
    case LF_DIRECTORYLIST:
        // FIXME: Multi-select
        if(GetDocument()->GetSelection(0) != NULL)
        {
            // FIXME: Multi-select
            SetMessageText(GetDocument()->GetSelection(0)->GetPath());
        }
        else
        {
            SetMessageText(AFX_IDS_IDLEMESSAGE);
        }
        break;
    case LF_EXTENSIONLIST:
        {
            SetMessageText(wds::strStar + GetDocument()->GetHighlightExtension());
        }
        break;
    }
}

void CMainFrame::OnUpdateMemoryUsage(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(true);
    pCmdUI->SetText(GetWDSApp()->GetCurrentProcessMemoryInfo());
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);

    if(!::IsWindow(m_wndStatusBar.m_hWnd))
    {
        return;
    }

    CRect rc;
    m_wndStatusBar.GetItemRect(0, rc);

    if(m_suspendButton.m_hWnd != NULL)
    {
        CRect suspend;
        m_suspendButton.GetClientRect(suspend);
        rc.left = suspend.right;
    }

    if(m_progress.m_hWnd != NULL)
    {
        m_progress.MoveWindow(rc);
    }
    else if(m_pacman.m_hWnd != NULL)
    {
        m_pacman.MoveWindow(rc);
    }
}

void CMainFrame::OnUpdateViewShowtreemap(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(GetGraphView()->IsShowTreemap());
}

void CMainFrame::OnViewShowtreemap()
{
    GetGraphView()->ShowTreemap(!GetGraphView()->IsShowTreemap());
    if(GetGraphView()->IsShowTreemap())
    {
        RestoreGraphView();
    }
    else
    {
        MinimizeGraphView();
    }
}

void CMainFrame::OnUpdateViewShowfiletypes(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(GetTypeView()->IsShowTypes());
}

void CMainFrame::OnViewShowfiletypes()
{
    GetTypeView()->ShowTypes(!GetTypeView()->IsShowTypes());
    if(GetTypeView()->IsShowTypes())
    {
        RestoreTypeView();
    }
    else
    {
        MinimizeTypeView();
    }
}

void CMainFrame::OnConfigure()
{
    COptionsPropertySheet sheet;

    CPageGeneral general;
    CPageTreelist treelist;
    CPageTreemap treemap;
    CPageCleanups cleanups;

    sheet.AddPage(&general);
    sheet.AddPage(&treelist);
    sheet.AddPage(&treemap);
    sheet.AddPage(&cleanups);

    sheet.DoModal();

    GetOptions()->SaveToRegistry();

    if(sheet.m_restartApplication)
    {
        GetWDSApp()->RestartApplication();
    }
}

void CMainFrame::OnTreemapHelpabouttreemaps()
{
    GetWDSApp()->DoContextHelp(IDH_Treemap);
}

void CMainFrame::OnSysColorChange()
{
    CFrameWnd::OnSysColorChange();
    GetDirstatView()->SysColorChanged();
    GetTypeView()->SysColorChanged();
}
