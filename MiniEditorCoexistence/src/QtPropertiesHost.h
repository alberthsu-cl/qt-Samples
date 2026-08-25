#pragma once

#include "ProjectState.h"

#include <afxwin.h>

#include <functional>
#include <memory>

class QtPropertiesPanel;

// The second explicit MFC/Qt native-child boundary. This host keeps the Qt
// signal connection and its lifetime away from MainFrame.
class QtPropertiesHost final
{
public:
    using ClipSettingsEditedHandler = std::function<void(const ClipSettings &)>;

    QtPropertiesHost();
    ~QtPropertiesHost();

    QtPropertiesHost(const QtPropertiesHost &) = delete;
    QtPropertiesHost &operator=(const QtPropertiesHost &) = delete;

    bool create(void *mfcParentWindowHandle);
    void resize(const CRect &bounds);
    void setSelectedAsset(const wchar_t *name, const wchar_t *kind,
                          const ClipSettings &settings);
    void setEditingEnabled(bool enabled);
    void setClipSettingsEditedHandler(ClipSettingsEditedHandler handler);

private:
    std::unique_ptr<QtPropertiesPanel> panel_;
    ClipSettingsEditedHandler clipSettingsEditedHandler_;
};
