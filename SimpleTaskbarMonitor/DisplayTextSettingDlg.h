#pragma once
#include "BaseDialog.h"
#include "CommonData.h"
#include "ListCtrlEx.h"

// CDisplayTextSettingDlg 对话框

class CDisplayTextSettingDlg : public CBaseDialog
{
	DECLARE_DYNAMIC(CDisplayTextSettingDlg)

public:
	CDisplayTextSettingDlg(DispStrings& display_texts, CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CDisplayTextSettingDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DISPLAY_TEXT_SETTINGS_DIALOG };
#endif

private:
    DispStrings& m_display_texts;

    CListCtrlEx m_list_ctrl;
    CMenu m_menu;
    int m_item_selected{ -1 };

protected:
    virtual CString GetDialogName() const override;
    CommonDisplayItem GetDisplayItem(int row);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

public:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    afx_msg void OnBnClickedRestoreDefaultButton();
    afx_msg void OnRestoreDefault();
    afx_msg void OnNMRClickList1(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnInitMenu(CMenu* pMenu);
    afx_msg void OnNMClickList1(NMHDR* pNMHDR, LRESULT* pResult);
};
