// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "launcher_sound_player.h"

#include <QUrl>

#include "common/logging/log.h"

UiSoundPlayer::UiSoundPlayer(QObject* parent) : QObject(parent) {
    m_selectSound.setParent(this);
    m_selectSound.setSource(QUrl("qrc:/sounds/select.wav"));
    m_selectSound.setLoopCount(1);
    m_selectSound.setVolume(0.45);

    m_errorSound.setParent(this);
    m_errorSound.setSource(QUrl("qrc:/sounds/error.wav"));
    m_errorSound.setLoopCount(1);
    m_errorSound.setVolume(0.65);
}

void UiSoundPlayer::playSelect() {
    if (!m_enabled) {
        LOG_DEBUG(Frontend, "UI select sound is disabled; skipping playback.");
        return;
    }

    if (m_selectSound.source().isEmpty()) {
        LOG_DEBUG(Frontend, "UI select sound has no source; cannot play.");
        return;
    }

    if (m_selectSound.isPlaying()) {
        m_selectSound.stop();
    }

    m_selectSound.play();
    LOG_DEBUG(Frontend, "Played UI select sound.");
}

void UiSoundPlayer::playError() {
    if (!m_enabled) {
        LOG_DEBUG(Frontend, "UI error sound is disabled; skipping playback.");
        return;
    }

    if (m_errorSound.source().isEmpty()) {
        LOG_DEBUG(Frontend, "UI error sound has no source; cannot play.");
        return;
    }

    if (m_errorSound.isPlaying()) {
        m_errorSound.stop();
    }

    m_errorSound.play();
    LOG_DEBUG(Frontend, "Played UI error sound.");
}

void UiSoundPlayer::stopAll() {
    if (m_selectSound.isPlaying()) {
        m_selectSound.stop();
        LOG_DEBUG(Frontend, "Stopped UI select sound playback.");
    }
    if (m_errorSound.isPlaying()) {
        m_errorSound.stop();
        LOG_DEBUG(Frontend, "Stopped UI error sound playback.");
    }
}

void UiSoundPlayer::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (!m_enabled) {
        stopAll();
    }
}
