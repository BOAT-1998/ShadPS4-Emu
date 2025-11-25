// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot_screen.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QGraphicsOpacityEffect>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QtMath>
#include <QEasingCurve>
#include <QSoundEffect>
#include <SDL3/SDL.h>
#include <cmath>
#include <algorithm>

BootScreen::BootScreen(const QSize& target_size, QWidget* parent)
    : QWidget(parent), m_target_size(target_size) {
    setObjectName("BootScreen");
    configureWindow(target_size);

    m_background = QPixmap(QStringLiteral(":/images/ps4_boot_bg.png"));
    m_logo = QPixmap(QStringLiteral(":/images/ps4_logo.png"));

    m_opacity_effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacity_effect);
    m_opacity_effect->setOpacity(0.0);

    m_fade_animation = new QPropertyAnimation(m_opacity_effect, "opacity", this);
    m_fade_animation->setDuration(650);
    m_fade_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_fade_animation, &QPropertyAnimation::finished, this, &BootScreen::onFadeFinished);

    setupLoadingAnimation();
    ensureControllerPolling();
    startFadeIn();
    m_startup_sound.setSource(QUrl("qrc:/sounds/startup.wav"));
    m_startup_sound.setLoopCount(1);
    m_startup_sound.setVolume(0.7);
    m_startup_sound.play();

    QTimer::singleShot(0, this, [this]() { setFocus(Qt::OtherFocusReason); });
}

BootScreen::~BootScreen() {
    if (m_loading_movie) {
        m_loading_movie->stop();
        m_loading_movie->deleteLater();
    }
    if (m_spinner_timer.isActive()) {
        m_spinner_timer.stop();
    }
    stopControllerPolling();
}

void BootScreen::configureWindow(const QSize& target_size) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::BlankCursor);
    setFocusPolicy(Qt::StrongFocus);

    const QSize desired_size = target_size.isValid() ? target_size : QSize(1260, 720);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        const int width = std::min(desired_size.width(), available.width());
        const int height = std::min(desired_size.height(), available.height());
        setFixedSize(width, height);
        move(available.center() - QPoint(width / 2, height / 2));
    } else {
        setFixedSize(desired_size);
    }
}

void BootScreen::ensureControllerPolling() {
    if (!m_controller_active) {
        SDL_InitSubSystem(SDL_INIT_EVENTS);
        if (SDL_InitSubSystem(SDL_INIT_GAMEPAD) == 0) {
            m_controller_active = true;
            connect(&m_controller_poller, &QTimer::timeout, this, &BootScreen::pollController,
                    Qt::UniqueConnection);
            m_controller_poller.start(16);
        }
    }
}

void BootScreen::stopControllerPolling() {
    if (m_controller_poller.isActive()) {
        m_controller_poller.stop();
    }
}

void BootScreen::startFadeIn() {
    m_fading_out = false;
    m_fade_animation->stop();
    m_fade_animation->setStartValue(0.0);
    m_fade_animation->setEndValue(1.0);
    m_fade_animation->start();
}

void BootScreen::startFadeOut() {
    if (m_fading_out) {
        return;
    }
    m_fading_out = true;
    m_fade_animation->stop();
    m_fade_animation->setStartValue(m_opacity_effect->opacity());
    m_fade_animation->setEndValue(0.0);
    m_fade_animation->start();
}

void BootScreen::onFadeFinished() {
    if (m_fading_out) {
        triggerFinish();
    }
}

void BootScreen::triggerFinish() {
    stopControllerPolling();
    hide();
    emit bootFinished();
}

void BootScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    QLinearGradient gradient(0, 0, width(), height());
    gradient.setColorAt(0.0, QColor(10, 61, 145));
    gradient.setColorAt(0.45, QColor(14, 70, 150));
    gradient.setColorAt(1.0, QColor(4, 30, 78));
    painter.fillRect(rect(), gradient);

    if (!m_background.isNull()) {
        const QPixmap scaled_background =
            m_background.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        painter.setOpacity(0.85);
        painter.drawPixmap(QPoint((width() - scaled_background.width()) / 2,
                                  (height() - scaled_background.height()) / 2),
                           scaled_background);
        painter.setOpacity(1.0);
        painter.fillRect(rect(), QColor(8, 20, 45, 110));
    }

    const int margin = 36;
    if (!m_logo.isNull()) {
        int target_height = std::max(static_cast<int>(height() * 0.08), 72);
        const QPixmap scaled_logo = m_logo.scaledToHeight(target_height, Qt::SmoothTransformation);
        painter.setOpacity(0.95);
        painter.drawPixmap(margin, margin, scaled_logo);
        painter.setOpacity(1.0);
    } else {
        QFont logo_font(QStringLiteral("Roboto Medium"), 28, QFont::Bold);
        painter.setFont(logo_font);
        painter.setPen(QColor(230, 238, 255));
        painter.drawText(QRect(margin, margin, 260, 60), Qt::AlignLeft | Qt::AlignVCenter, tr("PS4"));
    }

    QFont heading_font(QStringLiteral("Roboto Light"), 40);
    heading_font.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    const QRect welcome_rect(0, height() / 2 - 100, width(), 90);
    drawTextWithShadow(painter, welcome_rect, tr("Welcome Back to PlayStation"), heading_font,
                       QColor(232, 239, 250), Qt::AlignHCenter | Qt::AlignBottom);

    QFont prompt_font(QStringLiteral("Roboto Medium"), 20);
    const QRect prompt_rect(0, height() / 2 - 10, width(), 50);
    drawTextWithShadow(painter, prompt_rect, tr("Press Enter to continue"), prompt_font,
                       QColor(214, 226, 245), Qt::AlignHCenter | Qt::AlignVCenter);

    QFont controller_font(QStringLiteral("Roboto"), 15);
    const QRect controller_rect(0, height() / 2 + 40, width(), 50);
    drawTextWithShadow(painter, controller_rect, tr("Press the PS button to use the controller"),
                       controller_font, QColor(188, 207, 232), Qt::AlignHCenter | Qt::AlignTop);

    const QPoint spinner_center(width() / 2, height() / 2 + 110);
    const int spinner_radius = std::max(24, width() / 40);
    if (m_loading_movie && m_loading_movie->isValid()) {
        const QPixmap frame = m_loading_movie->currentPixmap();
        const QSize frame_size = frame.size() / frame.devicePixelRatio();
        const QPoint top_left(spinner_center.x() - frame_size.width() / 2,
                              spinner_center.y() - frame_size.height() / 2);
        painter.drawPixmap(top_left, frame);
    } else {
        paintLoadingSpinner(painter, spinner_center, spinner_radius);
    }
}

void BootScreen::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Select || event->key() == Qt::Key_Space) {
        startFadeOut();
        return;
    }
    QWidget::keyPressEvent(event);
}

void BootScreen::mousePressEvent(QMouseEvent* /*event*/) {
    startFadeOut();
}

bool BootScreen::handleControllerEvent(const SDL_Event& event) const {
    switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        return event.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE ||
               event.gbutton.button == SDL_GAMEPAD_BUTTON_START ||
               event.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH ||
               event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST ||
               event.gbutton.button == SDL_GAMEPAD_BUTTON_WEST ||
               event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return std::abs(event.gaxis.value) > 12000;
    case SDL_EVENT_KEY_DOWN:
        return event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER;
    default:
        return false;
    }
}

void BootScreen::pollController() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (handleControllerEvent(event)) {
            startFadeOut();
            return;
        }
    }
}

void BootScreen::setupLoadingAnimation() {
    if (QFile::exists(QStringLiteral(":/images/ps4_loading.gif"))) {
        m_loading_movie = new QMovie(QStringLiteral(":/images/ps4_loading.gif"), QByteArray(), this);
        m_loading_movie->setCacheMode(QMovie::CacheAll);
        m_loading_movie->setSpeed(115);
        if (m_loading_movie->isValid()) {
            connect(m_loading_movie, &QMovie::frameChanged, this, qOverload<>(&BootScreen::update));
            m_loading_movie->start();
        } else {
            m_loading_movie->deleteLater();
            m_loading_movie = nullptr;
        }
    }

    if (!m_loading_movie) {
        m_spinner_timer.setInterval(70);
        connect(&m_spinner_timer, &QTimer::timeout, this, [this]() {
            m_spinner_step = (m_spinner_step + 12) % 360;
            update();
        });
        m_spinner_timer.start();
    }
}

void BootScreen::paintLoadingSpinner(QPainter& painter, const QPoint& center, int radius) const {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const int dots = 4;
    const qreal inner_radius = radius * 0.45;
    const qreal dot_radius = std::max(4, radius / 5);

    for (int i = 0; i < dots; ++i) {
        const qreal angle_deg = static_cast<qreal>(m_spinner_step + i * 90);
        const qreal angle_rad = qDegreesToRadians(angle_deg);
        const qreal x = center.x() + std::cos(angle_rad) * inner_radius;
        const qreal y = center.y() + std::sin(angle_rad) * inner_radius;
        const int alpha = 220 - i * 35;
        painter.setBrush(QColor(232, 239, 250, alpha));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(x, y), dot_radius, dot_radius);
    }

    painter.restore();
}

void BootScreen::drawTextWithShadow(QPainter& painter, const QRect& rect, const QString& text,
                                    const QFont& font, const QColor& color,
                                    Qt::Alignment alignment) const {
    painter.save();
    painter.setFont(font);
    painter.setPen(QColor(0, 0, 0, 110));
    painter.drawText(rect.translated(1, 1), alignment, text);
    painter.setPen(color);
    painter.drawText(rect, alignment, text);
    painter.restore();
}
