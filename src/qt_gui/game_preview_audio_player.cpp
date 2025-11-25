// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "game_preview_audio_player.h"

#include <QDirIterator>
#include <QUrl>
#include <array>

#include "common/logging/log.h"
#include "common/path_util.h"

namespace {
QString ToQString(const std::filesystem::path& path) {
    QString out;
    Common::FS::PathToQString(out, path);
    return out;
}
} // namespace

GamePreviewAudioPlayer::GamePreviewAudioPlayer(QObject* parent) : QObject(parent) {
    m_player = new QMediaPlayer(this);
    m_audio_output = new QAudioOutput(this);
    m_audio_output->setVolume(0.65);
    m_player->setAudioOutput(m_audio_output);
}

bool GamePreviewAudioPlayer::resolvePreviewUrl(const std::filesystem::path& game_root,
                                               QUrl& out_url) {
    // Priority list: curated file names first, then first discoverable media.
    static constexpr std::array<const char*, 8> kPreferred = {
        "sce_sys/sound/bgm.at9", "sce_sys/sound/bgm.mp3", "sce_sys/sound/bgm.ogg",
        "sce_sys/sound/bgm.wav", "sce_sys/trailer.mp4",   "sce_sys/intro.mp4",
        "trailer.mp4",           "intro.mp4"};

    for (const auto* rel : kPreferred) {
        const auto candidate = game_root / rel;
        if (std::filesystem::exists(candidate)) {
            out_url = QUrl::fromLocalFile(ToQString(candidate));
            return true;
        }
    }

    // Fallback: first media file we can find under sce_sys or root (shallow).
    const QString root_q = ToQString(game_root);
    QDirIterator it(root_q,
                    {"*.mp4", "*.webm", "*.mp3", "*.ogg", "*.wav", "*.at9"},
                    QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) {
        out_url = QUrl::fromLocalFile(it.next());
        return true;
    }
    return false;
}

void GamePreviewAudioPlayer::playPreview(const std::filesystem::path& game_root) {
    QUrl url;
    if (!resolvePreviewUrl(game_root, url)) {
        stop();
        return;
    }

    if (m_player->playbackState() == QMediaPlayer::PlayingState && m_current_url == url) {
        // Already playing this preview.
        return;
    }

    m_current_url = url;
    m_player->stop();
    m_player->setSource(url);
    m_player->setLoops(1);
    m_player->play();
    LOG_DEBUG(Frontend, "Playing preview audio: {}", url.toString().toStdString());
}

void GamePreviewAudioPlayer::stop() {
    if (m_player->playbackState() != QMediaPlayer::StoppedState) {
        m_player->stop();
        LOG_DEBUG(Frontend, "Stopped preview audio.");
    }
    m_player->setSource(QUrl());
    m_current_url = QUrl();
}
