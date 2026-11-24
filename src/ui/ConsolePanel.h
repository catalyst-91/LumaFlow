#pragma once
#include "UiContext.h"
#include <QWidget>
class QTableWidget; class QComboBox; class QPlainTextEdit; class QLabel;
namespace lf {
class ConsolePanel : public QWidget {
    Q_OBJECT
public:
    ConsolePanel(UiContext* ctx, QWidget* parent = nullptr);
    void refreshMappings();
private:
    QWidget* buildLinks(); QWidget* buildMappings(); QWidget* buildPersonality(); QWidget* buildPolicies();
    UiContext* c; QTableWidget* m_links = nullptr; QTableWidget* m_maps = nullptr; QComboBox* m_mode = nullptr; QPlainTextEdit* m_diff = nullptr;
    QTableWidget* m_profile = nullptr; QLabel* m_learn = nullptr; QTableWidget* m_policies = nullptr; bool m_updating = false;
};
}
