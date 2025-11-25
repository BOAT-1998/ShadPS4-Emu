// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QPixmap>
#include <QWidget>

class BackgroundView : public QWidget {
    Q_OBJECT

public:
    explicit BackgroundView(QWidget* parent = nullptr);

    void setBackground(const QString& path);
    void setDefaultBackground();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_background;
    QPixmap m_default_background;
};
