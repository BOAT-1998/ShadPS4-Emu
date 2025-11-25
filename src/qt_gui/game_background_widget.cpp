#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#include "game_background_widget.h"

#include <QDirIterator>
#include <QGraphicsBlurEffect>
#include <QPainter>
#include <QUrl>
#include <QResizeEvent>

#include "common/path_util.h"

namespace {
QPixmap ScalePixmap(const QPixmap& pixmap, const QSize& size) {
    if (pixmap.isNull() || !size.isValid()) {
        return pixmap;
    }
    return pixmap.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
}
} // namespace

GameBackgroundWidget::GameBackgroundWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAutoFillBackground(false);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_videoWidget = new QVideoWidget(this);
    m_videoWidget->hide();

    m_mediaPlayer = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(0.0);
    m_mediaPlayer->setVideoOutput(m_videoWidget);
    m_mediaPlayer->setAudioOutput(m_audioOutput);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(0.0);
    m_fadeAnimation->setEndValue(1.0);

    auto* blurEffect = new QGraphicsBlurEffect(this);
    blurEffect->setBlurRadius(12.0);
    m_imageLabel->setGraphicsEffect(blurEffect);
}

void GameBackgroundWidget::SetThemeBackground(const QPixmap& pixmap) {
    m_themeBackground = pixmap;
    const bool hasImage = !m_imageLabel->pixmap(Qt::ReturnByValue).isNull();
    if (m_videoWidget->isHidden() && !hasImage) {
        ShowFallback();
    }
}

void GameBackgroundWidget::SetBlurEnabled(bool enabled) {
    m_blurEnabled = enabled;
    if (auto* effect = qobject_cast<QGraphicsBlurEffect*>(m_imageLabel->graphicsEffect())) {
        effect->setEnabled(enabled);
    }
}

void GameBackgroundWidget::OnGameSelected(const GameInfo& info) {
    // Do not auto-play game video previews; prefer static backgrounds.
    m_mediaPlayer->stop();
    m_videoWidget->hide();

    const auto image = FindBackgroundImage(info.path);
    if (image.has_value()) {
        ShowImage(image.value());
        return;
    }

    ShowFallback();
}

void GameBackgroundWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    UpdateGeometry();

    // Rescale currently displayed pixmap to new size
    if (!m_imageLabel->pixmap(Qt::ReturnByValue).isNull()) {
        m_imageLabel->setPixmap(
            ScalePixmap(m_imageLabel->pixmap(Qt::ReturnByValue), event->size()));
    } else if (!m_themeBackground.isNull() && m_videoWidget->isHidden()) {
        m_imageLabel->setPixmap(ScalePixmap(m_themeBackground, size()));
    }
}

void GameBackgroundWidget::ShowFallback() {
    m_mediaPlayer->stop();
    m_videoWidget->hide();
    if (!m_themeBackground.isNull()) {
        ShowImage(m_themeBackground.toImage());
    } else {
        QImage placeholder(size(), QImage::Format_ARGB32_Premultiplied);
        placeholder.fill(QColor(10, 10, 10, 200));
        ShowImage(placeholder);
    }
}

void GameBackgroundWidget::ShowImage(const QImage& image) {
    m_mediaPlayer->stop();
    m_videoWidget->hide();
    m_imageLabel->show();

    QPixmap pixmap = QPixmap::fromImage(image);
    m_imageLabel->setPixmap(ScalePixmap(pixmap, size()));
    StartFade();
}

void GameBackgroundWidget::ShowVideo(const QUrl& url) {
    m_videoWidget->show();
    m_imageLabel->hide();

    m_mediaPlayer->stop();
    m_mediaPlayer->setSource(url);
    m_mediaPlayer->setLoops(QMediaPlayer::Infinite);
    m_mediaPlayer->play();
    StartFade();
}

void GameBackgroundWidget::StartFade() {
    if (!m_opacityEffect || !m_fadeAnimation) {
        return;
    }
    m_fadeAnimation->stop();
    m_opacityEffect->setOpacity(0.0);
    m_fadeAnimation->start();
}

void GameBackgroundWidget::UpdateGeometry() {
    m_imageLabel->setGeometry(rect());
    m_videoWidget->setGeometry(rect());
}

std::optional<QString> GameBackgroundWidget::FindBackgroundVideo(
    const std::filesystem::path& root) const {
    const QString rootPath = ToQString(root);
    QDirIterator it(rootPath, {"*.mp4", "*.webm"}, QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        return it.next();
    }
    return std::nullopt;
}

std::optional<QImage> GameBackgroundWidget::FindBackgroundImage(
    const std::filesystem::path& root) const {
    const std::filesystem::path pic0 = root / "sce_sys" / "pic0.png";
    if (std::filesystem::exists(pic0)) {
        return QImage(ToQString(pic0));
    }

    const std::filesystem::path pic1 = root / "sce_sys" / "pic1.png";
    if (std::filesystem::exists(pic1)) {
        return QImage(ToQString(pic1));
    }

    QDirIterator it(ToQString(root), {"*.jpg", "*.jpeg", "*.png"}, QDir::Files,
                    QDirIterator::Subdirectories);
    if (it.hasNext()) {
        return QImage(it.next());
    }

    return std::nullopt;
}

QString GameBackgroundWidget::ToQString(const std::filesystem::path& path) {
    QString qPath;
    Common::FS::PathToQString(qPath, path);
    return qPath;
}
