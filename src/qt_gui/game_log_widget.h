#// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPlainTextEdit>
#include <QWidget>

#include "common/logging/filter.h"
#include "common/logging/log_entry.h"
#include "common/logging/types.h"

class GameLogWidget : public QWidget {
    Q_OBJECT

public:
    explicit GameLogWidget(QWidget* parent = nullptr);

    void appendLog(const QString& message, Common::Log::Level level);
    void appendLog(const Common::Log::Entry& entry);

    void setMaxLines(int max_lines);
    int maxLines() const {
        return m_max_lines;
    }
    void setAutoScrollEnabled(bool enabled) {
        m_auto_scroll = enabled;
    }
    bool autoScrollEnabled() const {
        return m_auto_scroll;
    }

private:
    QColor colorForMessage(Common::Log::Level level, const QString& message) const;
    QString withTimestamp(const QString& message) const;
    void trimExcessLines();

    QPlainTextEdit* m_text_edit = nullptr;
    int m_max_lines = 5000;
    bool m_auto_scroll = true;
};
