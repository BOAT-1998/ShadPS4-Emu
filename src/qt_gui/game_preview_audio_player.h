// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QUrl>

class GamePreviewAudioPlayer : public QObject {
    Q_OBJECT

public:
    explicit GamePreviewAudioPlayer(QObject* parent = nullptr);
    void playPreview(const std::filesystem::path& game_root);
    void stop();

private:
    bool resolvePreviewUrl(const std::filesystem::path& game_root, QUrl& out_url);

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio_output = nullptr;
    QUrl m_current_url;
};
