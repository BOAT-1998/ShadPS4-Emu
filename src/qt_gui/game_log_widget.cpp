#// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#include "game_log_widget.h"

#include <algorithm>
#include <QColor>
#include <QFont>
#include <QLabel>
#include <QScrollBar>
#include <QFrame>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextOption>
#include <QTime>
#include <QVBoxLayout>

GameLogWidget::GameLogWidget(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 4);
    layout->setSpacing(4);

    auto* title = new QLabel(tr("Game Log"), this);
    title->setContentsMargins(2, 0, 0, 0);
    layout->addWidget(title);

    m_text_edit = new QPlainTextEdit(this);
    m_text_edit->setReadOnly(true);
    m_text_edit->setFrameShape(QFrame::NoFrame);
    m_text_edit->setWordWrapMode(QTextOption::NoWrap);
    QFont font = m_text_edit->font();
    font.setStyleHint(QFont::Monospace);
    m_text_edit->setFont(font);
    layout->addWidget(m_text_edit);
}

void GameLogWidget::appendLog(const Common::Log::Entry& entry) {
    const QString message = QString("[%1][%2] %3")
                                .arg(QString::fromUtf8(GetLogClassName(entry.log_class)))
                                .arg(QString::fromUtf8(GetLevelName(entry.log_level)))
                                .arg(QString::fromStdString(entry.message));
    appendLog(message, entry.log_level);
}

void GameLogWidget::appendLog(const QString& message, Common::Log::Level level) {
    if (!m_text_edit) {
        return;
    }

    auto* scroll_bar = m_text_edit->verticalScrollBar();
    const bool at_bottom = scroll_bar->value() == scroll_bar->maximum();

    const auto lines = message.split('\n');
    for (const auto& line : lines) {
        const QString decorated = withTimestamp(line);

        QTextCharFormat format;
        format.setForeground(colorForMessage(level, decorated));

        QTextCursor cursor = m_text_edit->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(decorated + QLatin1Char('\n'), format);
    }

    trimExcessLines();
    if (m_auto_scroll && at_bottom) {
        scroll_bar->setValue(scroll_bar->maximum());
    }
}

void GameLogWidget::setMaxLines(int max_lines) {
    m_max_lines = std::max(1, max_lines);
    trimExcessLines();
}

QColor GameLogWidget::colorForMessage(Common::Log::Level level, const QString& message) const {
    const QString lower = message.toLower();
    if (lower.contains(QStringLiteral("completed")) || lower.contains(QStringLiteral("done")) ||
        lower.contains(QStringLiteral("success")) || lower.contains(QStringLiteral("finished"))) {
        return QColor(0, 200, 0);
    }

    switch (level) {
    case Common::Log::Level::Critical:
    case Common::Log::Level::Error:
        return QColor(Qt::red);
    case Common::Log::Level::Warning:
        return QColor(255, 200, 0);
    case Common::Log::Level::Info:
        return QColor(Qt::white);
    case Common::Log::Level::Debug:
    case Common::Log::Level::Trace:
    default:
        return QColor(180, 180, 180);
    }
}

QString GameLogWidget::withTimestamp(const QString& message) const {
    const auto timestamp = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    return QStringLiteral("[%1] %2").arg(timestamp, message);
}

void GameLogWidget::trimExcessLines() {
    auto* document = m_text_edit->document();
    while (document->blockCount() > m_max_lines) {
        QTextCursor cursor(document->firstBlock());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar();
    }
}
