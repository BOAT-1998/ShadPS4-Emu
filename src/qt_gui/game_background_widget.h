#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <filesystem>

#include <QAudioOutput>
#include <QGraphicsOpacityEffect>
#include <QImage>
#include <QLabel>
#include <QMediaPlayer>
#include <QPropertyAnimation>
#include <QVideoWidget>
#include <QWidget>

#include "game_list_utils.h"
#include "main_window_themes.h"

class GameBackgroundWidget : public QWidget {
    Q_OBJECT
public:
    explicit GameBackgroundWidget(QWidget* parent = nullptr);

    void SetThemeBackground(const QPixmap& pixmap);
    void SetBlurEnabled(bool enabled);

public Q_SLOTS:
    void OnGameSelected(const GameInfo& info);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void ShowFallback();
    void ShowImage(const QImage& image);
    void ShowVideo(const QUrl& url);
    void StartFade();
    void UpdateGeometry();

    std::optional<QString> FindBackgroundVideo(const std::filesystem::path& root) const;
    std::optional<QImage> FindBackgroundImage(const std::filesystem::path& root) const;
    static QString ToQString(const std::filesystem::path& path);

    QLabel* m_imageLabel = nullptr;
    QVideoWidget* m_videoWidget = nullptr;
    QMediaPlayer* m_mediaPlayer = nullptr;
    QAudioOutput* m_audioOutput = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_fadeAnimation = nullptr;
    QPixmap m_themeBackground;
    bool m_blurEnabled = true;
};
