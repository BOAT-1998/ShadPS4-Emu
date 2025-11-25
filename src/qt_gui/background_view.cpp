// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "background_view.h"

#include <QFile>
#include <QPainter>
#include <QPaintEvent>
#include <QUrl>

BackgroundView::BackgroundView(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    m_default_background = QPixmap(":/images/ps4_boot_bg.png");
    setDefaultBackground();
}

void BackgroundView::setDefaultBackground() {
    m_background = m_default_background;
    update();
}

void BackgroundView::setBackground(const QString& path) {
    if (!path.isEmpty() && QFile::exists(path)) {
        QPixmap pix(path);
        if (!pix.isNull()) {
            m_background = pix;
            update();
            return;
        }
    }
    setDefaultBackground();
}

void BackgroundView::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!m_background.isNull()) {
        QPixmap scaled =
            m_background.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const int x = (width() - scaled.width()) / 2;
        const int y = (height() - scaled.height()) / 2;
        painter.drawPixmap(x, y, scaled);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
}
