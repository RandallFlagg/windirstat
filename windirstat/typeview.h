// typeview.h	- Declaration of CExtensionListControl and CTypeView
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003 Bernhard Seifert
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
// Author: bseifert@users.sourceforge.net, bseifert@daccord.net

#pragma once

#include "dirstatdoc.h"	// SExtensionRecord

//
// CExtensionListControl.
//
class CExtensionListControl: public COwnerDrawnListControl
{
protected:
	// Columns
	enum
	{
		COL_EXTENSION,
		COL_COLOR,
		COL_DESCRIPTION,
		COL_BYTES,
		COL_FILES,
	};

	// CListItem. The items of the CExtensionListControl.
	class CListItem: public COwnerDrawnListItem
	{
	public:
		CListItem(CExtensionListControl *list, LPCTSTR extension, SExtensionRecord r);

		virtual bool DrawSubitem(int subitem, CDC *pdc, CRect rc, UINT state, int *width) const;
		virtual CString GetText(int subitem) const;

		CString GetExtension() const;
		int GetImage() const;
		int Compare(const CSortingListItem *other, int subitem) const;

	private:
		void DrawColor(CDC *pdc, CRect rc, UINT state, int *width) const;

		CString GetDescription() const;

		CExtensionListControl *m_list;
		CString m_extension;
		SExtensionRecord m_record;
		mutable CString m_description;
		mutable int m_image;
	};

public:
	CExtensionListControl();
	virtual bool GetAscendingDefault(int column);
	void Initialize();
	void SetExtensionData(const CExtensionData *ed);
	void SelectExtension(LPCTSTR ext);

protected:
	CListItem *GetListItem(int i);

	DECLARE_MESSAGE_MAP()
	afx_msg void OnDestroy();
	afx_msg void OnLvnDeleteitem(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void MeasureItem(LPMEASUREITEMSTRUCT mis);
};


//
// CTypeView. The upper right view, which shows the extensions and their
// cushion colors.
//
class CTypeView: public CView
{
protected:
	CTypeView();
	DECLARE_DYNCREATE(CTypeView)

public:
	virtual ~CTypeView();
	CDirstatDoc* GetDocument() const;

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	bool IsShowTypes();
	void ShowTypes(bool show);

protected:
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	virtual void OnDraw(CDC* pDC);
	void SetSelection();

	bool m_showTypes;		// Whether this view shall be shown (F8 option)
	CExtensionListControl m_extensionListControl;	// The list control

	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
public:
	#ifdef _DEBUG
		virtual void AssertValid() const;
		virtual void Dump(CDumpContext& dc) const;
	#endif
};

#ifndef _DEBUG  // Debugversion in typeview.cpp
inline CDirstatDoc* CTypeView::GetDocument() const
   { return reinterpret_cast<CDirstatDoc*>(m_pDocument); }
#endif
