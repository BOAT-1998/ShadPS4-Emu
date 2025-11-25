// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QGraphicsOpacityEffect>
#include <QMovie>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSoundEffect>
#include <QTimer>
#include <QWidget>

union SDL_Event;

class BootScreen : public QWidget {
    Q_OBJECT

public:
    explicit BootScreen(const QSize& target_size = QSize(), QWidget* parent = nullptr);
    ~BootScreen() override;

signals:
    void bootFinished();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onFadeFinished();
    void pollController();

private:
    void startFadeIn();
    void startFadeOut();
    void triggerFinish();
    void configureWindow(const QSize& target_size);
    void ensureControllerPolling();
    void stopControllerPolling();
    bool handleControllerEvent(const SDL_Event& event) const;
    void setupLoadingAnimation();
    void paintLoadingSpinner(QPainter& painter, const QPoint& center, int radius) const;
    void drawTextWithShadow(QPainter& painter, const QRect& rect, const QString& text,
                            const QFont& font, const QColor& color,
                            Qt::Alignment alignment) const;

    QPixmap m_background;
    QPixmap m_logo;
    QMovie* m_loading_movie{};
    QTimer m_spinner_timer;
    QGraphicsOpacityEffect* m_opacity_effect{};
    QPropertyAnimation* m_fade_animation{};
    QSoundEffect m_startup_sound;
    QTimer m_controller_poller;
    QSize m_target_size;
    int m_spinner_step{0};
    bool m_fading_out{false};
    bool m_controller_active{false};
};
