// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QSoundEffect>

class UiSoundPlayer : public QObject {
    Q_OBJECT

public:
    explicit UiSoundPlayer(QObject* parent = nullptr);

    void playSelect();
    void playError();
    void stopAll();
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

private:
    bool m_enabled = true;
    QSoundEffect m_selectSound;
    QSoundEffect m_errorSound;
};
