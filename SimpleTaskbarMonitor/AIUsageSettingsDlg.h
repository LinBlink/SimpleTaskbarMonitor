#pragma once
#include "TabDlg.h"
#include "SpinEdit.h"

class CAIUsageSettingsDlg : public CTabDlg
{
    DECLARE_DYNAMIC(CAIUsageSettingsDlg)

public:
    explicit CAIUsageSettingsDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_AI_USAGE_SETTINGS_DIALOG };
#endif

    virtual void OnOK() override;
    virtual void OnTabEntered() override { UpdateStatus(); }

protected:
    CSpinEdit m_refresh_edit;
    CComboBox m_region_combo;
    std::wstring SettingsPath() const;
    std::wstring CachePath() const;
    void UpdateStatus();
    bool SaveSettings();
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual void OnSettingsApplied() override;

    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog() override;
    afx_msg void OnBnClickedRefresh();
    afx_msg void OnBnClickedMiniMaxKey();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
};
