#// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#include "SDL3/SDL_events.h"

#include <algorithm>
#include <QBoxLayout>
#include <QFrame>
#include <QKeyEvent>
#include <QMetaObject>
#include <QPointer>
#include <QProgressDialog>
#include <QVBoxLayout>

#include "about_dialog.h"
#include "cheats_patches.h"
#ifdef ENABLE_UPDATER
#include "check_update.h"
#endif
#include "common/io_file.h"
#include "common/logging/backend.h"
#include "common/path_util.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "control_settings.h"
#include "core/file_format/pkg.h"
#include "core/firmware/firmware_installer.h"
#include "core/loader.h"
#include "game_install_dialog.h"
#include "hotkeys.h"
#include "input/input_handler.h"
#include "install_dir_select.h"
#include "kbm_gui.h"
#include "main_window.h"
#include "settings_dialog.h"

#ifdef ENABLE_DISCORD_RPC
#include "common/discord_rpc_handler.h"
#endif

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);
    installEventFilter(this);
    setAttribute(Qt::WA_DeleteOnClose);
    m_gui_settings = std::make_shared<gui_settings>();
    m_preview_audio_player.reset(new GamePreviewAudioPlayer(this));
    m_ui_sound_player.reset(new UiSoundPlayer(this));
    m_ui_sound_player->setEnabled(Config::getUiSoundEffectsEnabled());
    m_current_theme = static_cast<Theme>(m_gui_settings->GetValue(gui::gen_theme).toInt());
    ui->toggleLabelsAct->setChecked(
        m_gui_settings->GetValue(gui::mw_showLabelsUnderIcons).toBool());
}

MainWindow::~MainWindow() {
    SaveWindowState();
}

bool MainWindow::Init() {
    auto start = std::chrono::steady_clock::now();
    // setup ui
    LoadTranslation();
    AddUiWidgets();
    CreateActions();
    CreateRecentGameActions();
    ConfigureGuiFromSettings();
    CreateDockWindows();
    SetupLogBackend();
    CreateConnects();
    SetLastUsedTheme();
    SetupBackgroundLayer();
    SetLastIconSizeBullet();
    // show ui
    setMinimumSize(720, 405);
    std::string window_title = "";
    std::string remote_url(Common::g_scm_remote_url);
    std::string remote_host = Common::GetRemoteNameFromLink();
    if (Common::g_is_release) {
        if (remote_host == "shadps4-emu" || remote_url.length() == 0) {
            window_title = fmt::format("shadPS4 v{}", Common::g_version);
        } else {
            window_title = fmt::format("shadPS4 {}/v{}", remote_host, Common::g_version);
        }
    } else {
        if (remote_host == "shadps4-emu" || remote_url.length() == 0) {
            window_title = fmt::format("shadPS4 v{} {} {}", Common::g_version, Common::g_scm_branch,
                                       Common::g_scm_desc);
        } else {
            window_title = fmt::format("shadPS4 v{} {}/{} {}", Common::g_version, remote_host,
                                       Common::g_scm_branch, Common::g_scm_desc);
        }
    }
    setWindowTitle(QString::fromStdString(window_title));
    this->show();
    // load game list
    LoadGameLists();
#ifdef ENABLE_UPDATER
    // Check for update
    CheckUpdateMain(true);
#endif

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    statusBar.reset(new QStatusBar);
    this->setStatusBar(statusBar.data());
    // Update status bar
    int numGames = m_game_info->m_games.size();
    QString statusMessage = tr("Games: ") + QString::number(numGames) + " (" +
                            QString::number(duration.count()) + "ms)";
    statusBar->showMessage(statusMessage);

#ifdef ENABLE_DISCORD_RPC
    if (Config::getEnableDiscordRPC()) {
        auto* rpc = Common::Singleton<DiscordRPCHandler::RPC>::Instance();
        rpc->init();
        rpc->setStatusIdling();
    }
#endif

    return true;
}

void MainWindow::CreateActions() {
    // create action group for icon size
    m_icon_size_act_group = new QActionGroup(this);
    m_icon_size_act_group->addAction(ui->setIconSizeTinyAct);
    m_icon_size_act_group->addAction(ui->setIconSizeSmallAct);
    m_icon_size_act_group->addAction(ui->setIconSizeMediumAct);
    m_icon_size_act_group->addAction(ui->setIconSizeLargeAct);

    // create action group for list mode
    m_list_mode_act_group = new QActionGroup(this);
    m_list_mode_act_group->addAction(ui->setlistModeListAct);
    m_list_mode_act_group->addAction(ui->setlistModeGridAct);
    m_list_mode_act_group->addAction(ui->setlistElfAct);

    // create action group for themes
    m_theme_act_group = new QActionGroup(this);
    m_theme_act_group->addAction(ui->setThemeDark);
    m_theme_act_group->addAction(ui->setThemeLight);
    m_theme_act_group->addAction(ui->setThemeGreen);
    m_theme_act_group->addAction(ui->setThemeBlue);
    m_theme_act_group->addAction(ui->setThemeViolet);
    m_theme_act_group->addAction(ui->setThemeGruvbox);
    m_theme_act_group->addAction(ui->setThemeTokyoNight);
    m_theme_act_group->addAction(ui->setThemeOled);
}

void MainWindow::PauseGame() {
    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_TOGGLE_PAUSE;
    is_paused = !is_paused;
    UpdateToolbarButtons();
    SDL_PushEvent(&event);
}

void MainWindow::toggleLabelsUnderIcons() {
    bool showLabels = ui->toggleLabelsAct->isChecked();
    m_gui_settings->SetValue(gui::mw_showLabelsUnderIcons, showLabels);
    UpdateToolbarLabels();
    if (isGameRunning) {
        UpdateToolbarButtons();
    }
}

void MainWindow::toggleFullscreen() {
    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_TOGGLE_FULLSCREEN;
    SDL_PushEvent(&event);
}

QWidget* MainWindow::createButtonWithLabel(QPushButton* button, const QString& labelText,
                                           bool showLabel) {
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(button);

    QLabel* label = nullptr;
    if (showLabel && ui->toggleLabelsAct->isChecked()) {
        label = new QLabel(labelText, this);
        label->setAlignment(Qt::AlignCenter | Qt::AlignBottom);
        layout->addWidget(label);
        button->setToolTip("");
    } else {
        button->setToolTip(labelText);
    }

    container->setLayout(layout);
    container->setProperty("buttonLabel", QVariant::fromValue(label));
    return container;
}

QWidget* createSpacer(QWidget* parent) {
    QWidget* spacer = new QWidget(parent);
    spacer->setFixedWidth(15);
    spacer->setFixedHeight(15);
    return spacer;
}

void MainWindow::AddUiWidgets() {
    // add toolbar widgets
    QApplication::setStyle("Fusion");

    bool showLabels = ui->toggleLabelsAct->isChecked();
    ui->toolBar->clear();

    ui->toolBar->addWidget(createSpacer(this));
    ui->toolBar->addWidget(createButtonWithLabel(ui->playButton, tr("Play"), showLabels));
    ui->toolBar->addWidget(createButtonWithLabel(ui->pauseButton, tr("Pause"), showLabels));
    ui->toolBar->addWidget(createButtonWithLabel(ui->stopButton, tr("Stop"), showLabels));
    ui->toolBar->addWidget(createButtonWithLabel(ui->restartButton, tr("Restart"), showLabels));
    ui->toolBar->addWidget(createSpacer(this));
    ui->toolBar->addWidget(createButtonWithLabel(ui->settingsButton, tr("Settings"), showLabels));
    ui->toolBar->addWidget(
        createButtonWithLabel(ui->fullscreenButton, tr("Full Screen"), showLabels));
    ui->toolBar->addWidget(createSpacer(this));
    ui->toolBar->addWidget(
        createButtonWithLabel(ui->controllerButton, tr("Controllers"), showLabels));
    ui->toolBar->addWidget(createButtonWithLabel(ui->keyboardButton, tr("Keyboard"), showLabels));
    ui->toolBar->addWidget(createSpacer(this));
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Sunken);
    line->setMinimumWidth(2);
    ui->toolBar->addWidget(line);
    ui->toolBar->addWidget(createSpacer(this));
    if (showLabels) {
        QLabel* pauseButtonLabel = ui->pauseButton->parentWidget()->findChild<QLabel*>();
        if (pauseButtonLabel) {
            pauseButtonLabel->setVisible(false);
        }
    }
    ui->toolBar->addWidget(
        createButtonWithLabel(ui->refreshButton, tr("Refresh List"), showLabels));
    ui->toolBar->addWidget(createSpacer(this));

    QBoxLayout* toolbarLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    toolbarLayout->setSpacing(2);
    toolbarLayout->setContentsMargins(2, 2, 2, 2);
    ui->sizeSliderContainer->setFixedWidth(150);

    QWidget* searchSliderContainer = new QWidget(this);
    QBoxLayout* searchSliderLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    searchSliderLayout->setContentsMargins(0, 0, 6, 6);
    searchSliderLayout->setSpacing(2);
    ui->mw_searchbar->setFixedWidth(150);

    searchSliderLayout->addWidget(ui->sizeSliderContainer);
    searchSliderLayout->addWidget(ui->mw_searchbar);

    searchSliderContainer->setLayout(searchSliderLayout);

    ui->toolBar->addWidget(searchSliderContainer);

    if (!showLabels) {
        toolbarLayout->addWidget(searchSliderContainer);
    }

    ui->playButton->setVisible(true);
    ui->pauseButton->setVisible(false);
}

void MainWindow::UpdateToolbarButtons() {
    // add toolbar widgets when game is running
    bool showLabels = ui->toggleLabelsAct->isChecked();

    ui->playButton->setVisible(false);
    ui->pauseButton->setVisible(true);

    if (showLabels) {
        QLabel* playButtonLabel = ui->playButton->parentWidget()->findChild<QLabel*>();
        if (playButtonLabel)
            playButtonLabel->setVisible(false);
    }

    if (is_paused) {
        ui->pauseButton->setIcon(ui->playButton->icon());
        ui->pauseButton->setToolTip(tr("Resume"));
    } else {
        if (isIconBlack) {
            ui->pauseButton->setIcon(QIcon(":images/pause_icon.png"));
        } else {
            ui->pauseButton->setIcon(RecolorIcon(QIcon(":images/pause_icon.png"), isWhite));
        }
        ui->pauseButton->setToolTip(tr("Pause"));
    }

    if (showLabels) {
        QLabel* pauseButtonLabel = ui->pauseButton->parentWidget()->findChild<QLabel*>();
        if (pauseButtonLabel) {
            pauseButtonLabel->setText(is_paused ? tr("Resume") : tr("Pause"));
            pauseButtonLabel->setVisible(true);
        }
    }
}

void MainWindow::UpdateToolbarLabels() {
    AddUiWidgets();
}

void MainWindow::CreateDockWindows() {
    m_main_splitter.reset(new QSplitter(Qt::Vertical, this));
    m_main_splitter->setChildrenCollapsible(false);

    QWidget* top_container = new QWidget(this);
    auto* top_layout = new QVBoxLayout(top_container);
    top_layout->setContentsMargins(0, 0, 0, 0);
    top_layout->setSpacing(0);

    m_background_view.reset(new BackgroundView(top_container));
    m_background_view->setGeometry(top_container->rect());
    m_background_view->lower();

    m_game_list_frame.reset(new GameListFrame(m_gui_settings, m_game_info, m_compat_info, this));
    m_game_list_frame->setObjectName("gamelist");
    m_game_list_frame->setStyleSheet(
        "QTableWidget { background-color: rgba(0,0,0,180); }"
        "QTableWidget::item { background-color: transparent; }");
    m_game_grid_frame.reset(new GameGridFrame(m_gui_settings, m_game_info, m_compat_info, this));
    m_game_grid_frame->setObjectName("gamegridlist");
    m_game_grid_frame->setStyleSheet("QTableWidget { background-color: rgba(0,0,0,170); }");
    m_elf_viewer.reset(new ElfViewer(m_gui_settings, this));
    m_elf_viewer->setObjectName("elflist");

    m_game_view_stack.reset(new QStackedWidget(top_container));
    m_game_view_stack->addWidget(m_game_list_frame.data());
    m_game_view_stack->addWidget(m_game_grid_frame.data());
    m_game_view_stack->addWidget(m_elf_viewer.data());
    top_layout->addWidget(m_game_view_stack.data());
    top_container->setLayout(top_layout);

    m_main_splitter->addWidget(top_container);

    m_game_log_widget.reset(new GameLogWidget(this));
    m_game_log_widget->setObjectName("gamelogwidget");
    m_game_log_widget->setMaxLines(
        std::max(1, m_gui_settings->GetValue(gui::mw_game_log_max_lines).toInt()));
    m_game_log_widget->setAutoScrollEnabled(
        m_gui_settings->GetValue(gui::mw_log_auto_scroll).toBool());
    m_main_splitter->addWidget(m_game_log_widget.data());

    int table_mode = m_gui_settings->GetValue(gui::gl_mode).toInt();
    int slider_pos = 0;
    if (table_mode == 0) { // List
        m_game_grid_frame->hide();
        m_elf_viewer->hide();
        m_game_list_frame->show();
        m_game_view_stack->setCurrentWidget(m_game_list_frame.data());
        slider_pos = m_gui_settings->GetValue(gui::gl_slider_pos).toInt();
        ui->sizeSlider->setSliderPosition(slider_pos); // set slider pos at start;
        isTableList = true;
    } else if (table_mode == 1) { // Grid
        m_game_list_frame->hide();
        m_elf_viewer->hide();
        m_game_grid_frame->show();
        m_game_view_stack->setCurrentWidget(m_game_grid_frame.data());
        slider_pos = m_gui_settings->GetValue(gui::gg_slider_pos).toInt();
        ui->sizeSlider->setSliderPosition(slider_pos); // set slider pos at start;
        isTableList = false;
    } else {
        m_game_list_frame->hide();
        m_game_grid_frame->hide();
        m_elf_viewer->show();
        m_game_view_stack->setCurrentWidget(m_elf_viewer.data());
        isTableList = false;
    }

    const auto splitter_state =
        m_gui_settings->GetValue(gui::mw_game_log_splitter_state).toByteArray();
    if (!splitter_state.isEmpty()) {
        m_main_splitter->restoreState(splitter_state);
    } else {
        m_main_splitter->setStretchFactor(0, 3);
        m_main_splitter->setStretchFactor(1, 1);
    }

    setCentralWidget(m_main_splitter.data());
    ToggleGameLog(m_gui_settings->GetValue(gui::mw_show_game_log_panel).toBool());
}

void MainWindow::AppendToGameLog(const QString& text, Common::Log::Level level) {
    if (m_game_log_widget) {
        m_game_log_widget->appendLog(text, level);
    }
}

void MainWindow::ToggleGameLog(bool visible) {
    if (!m_game_log_widget || !m_main_splitter) {
        return;
    }

    m_game_log_widget->setVisible(visible);
    if (visible) {
        const auto sizes = m_main_splitter->sizes();
        if (sizes.size() >= 2 && sizes[1] == 0) {
            const int top = std::max(0, this->height() - 200);
            m_main_splitter->setSizes({top, 200});
        }
    } else {
        auto sizes = m_main_splitter->sizes();
        if (sizes.size() >= 2) {
            const int total = sizes[0] + sizes[1];
            sizes[0] = total;
            sizes[1] = 0;
            m_main_splitter->setSizes(sizes);
        }
    }
    if (ui->showGameLogAct) {
        ui->showGameLogAct->setChecked(visible);
    }
    m_gui_settings->SetValue(gui::mw_show_game_log_panel, visible, false);
}

void MainWindow::SetupLogBackend() {
    if (!m_game_log_widget) {
        return;
    }

    QPointer<GameLogWidget> target = m_game_log_widget.data();
    Common::Log::RegisterCallbackBackend([target](const Common::Log::Entry& entry) {
        if (!target) {
            return;
        }
        QMetaObject::invokeMethod(
            target.data(),
            [target, entry]() {
                if (target) {
                    target->appendLog(entry);
                }
            },
            Qt::QueuedConnection);
    });
    Common::Log::SetColorConsoleBackendEnabled(false);
    AppendToGameLog(tr("Game log initialized"), Common::Log::Level::Info);
}

void MainWindow::StartLoggedProcess(const QString& program, const QStringList& arguments,
                                    const QString& friendly_name) {
    auto* process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::MergedChannels);

    const QString label = friendly_name.isEmpty() ? program : friendly_name;
    AppendToGameLog(tr("Starting %1 ...").arg(label), Common::Log::Level::Info);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        const QByteArray data = process->readAllStandardOutput();
        const auto lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            AppendToGameLog(line, Common::Log::Level::Debug);
        }
    });

    connect(process, &QProcess::finished, this,
            [this, process, label](int exitCode, QProcess::ExitStatus status) {
                if (exitCode == 0 && status == QProcess::NormalExit) {
                    AppendToGameLog(tr("%1 finished").arg(label), Common::Log::Level::Info);
                } else {
                    AppendToGameLog(tr("%1 failed (exit %2)")
                                        .arg(label)
                                        .arg(exitCode),
                                    Common::Log::Level::Error);
                }
                process->deleteLater();
            });

    connect(process, &QProcess::errorOccurred, this, [this, process, label](QProcess::ProcessError) {
        AppendToGameLog(tr("%1 error: %2").arg(label, process->errorString()),
                        Common::Log::Level::Error);
    });

    process->start();
}

void MainWindow::SetupBackgroundLayer() {
    if (!m_background_widget) {
        m_background_widget.reset(new GameBackgroundWidget(this));
        m_background_widget->setObjectName("gameBackgroundWidget");
        m_background_widget->lower();
    }
    m_background_widget->setGeometry(this->rect());
    m_background_widget->SetThemeBackground(
        m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
}

void MainWindow::LoadGameLists() {
    AppendToGameLog(tr("Scanning game library..."), Common::Log::Level::Info);
    // Load compatibility database
    if (Config::getCompatibilityEnabled()) {
        AppendToGameLog(tr("Loading compatibility database"), Common::Log::Level::Info);
        m_compat_info->LoadCompatibilityFile();
    }

    // Update compatibility database
    if (Config::getCheckCompatibilityOnStartup()) {
        AppendToGameLog(tr("Updating compatibility database"), Common::Log::Level::Info);
        m_compat_info->UpdateCompatibilityDatabase(this);
    }

    // Get game info from game folders.
    AppendToGameLog(tr("Reading game metadata"), Common::Log::Level::Info);
    m_game_info->GetGameInfo(this);
    if (isTableList) {
        m_game_list_frame->PopulateGameList();
    } else {
        m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
    }

    if (m_background_widget) {
        if (isTableList && m_game_list_frame->currentItem()) {
            const int row = m_game_list_frame->currentItem()->row();
            if (row >= 0 && row < m_game_info->m_games.size()) {
                m_background_widget->OnGameSelected(m_game_info->m_games[row]);
                UpdateBackgroundView(m_game_info->m_games[row]);
            }
        } else if (!m_game_info->m_games.isEmpty()) {
            m_background_widget->OnGameSelected(m_game_info->m_games.front());
            UpdateBackgroundView(m_game_info->m_games.front());
        } else {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
            if (m_background_view) {
                m_background_view->setDefaultBackground();
            }
        }
    }
    AppendToGameLog(
        tr("Game scan finished (%1 entries)").arg(m_game_info->m_games.size()),
        Common::Log::Level::Info);
}

#ifdef ENABLE_UPDATER
void MainWindow::CheckUpdateMain(bool checkSave) {
    if (checkSave) {
        if (!m_gui_settings->GetValue(gui::gen_checkForUpdates).toBool()) {
            return;
        }
    }
    auto checkUpdate = new CheckUpdate(m_gui_settings, false);
    checkUpdate->exec();
}
#endif

void MainWindow::CreateConnects() {
    connect(this, &MainWindow::WindowResized, this, &MainWindow::HandleResize);
    connect(ui->mw_searchbar, &QLineEdit::textChanged, this, &MainWindow::SearchGameTable);
    connect(ui->exitAct, &QAction::triggered, this, &QWidget::close);
    connect(ui->refreshGameListAct, &QAction::triggered, this, &MainWindow::RefreshGameTable);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::RefreshGameTable);
    connect(ui->showGameListAct, &QAction::triggered, this, &MainWindow::ShowGameList);
    connect(ui->showGameLogAct, &QAction::toggled, this, &MainWindow::ToggleGameLog);
    connect(this, &MainWindow::ExtractionFinished, this, &MainWindow::RefreshGameTable);
    connect(ui->toggleLabelsAct, &QAction::toggled, this, &MainWindow::toggleLabelsUnderIcons);
    connect(ui->fullscreenButton, &QPushButton::clicked, this, &MainWindow::toggleFullscreen);

    connect(ui->sizeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (isTableList) {
            m_game_list_frame->icon_size =
                48 + value; // 48 is the minimum icon size to use due to text disappearing.
            m_game_list_frame->ResizeIcons(48 + value);
            m_gui_settings->SetValue(gui::gl_icon_size, 48 + value);
            m_gui_settings->SetValue(gui::gl_slider_pos, value);
        } else {
            m_game_grid_frame->icon_size = 69 + value;
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
            m_gui_settings->SetValue(gui::gg_icon_size, 69 + value);
            m_gui_settings->SetValue(gui::gg_slider_pos, value);
        }
    });

    connect(ui->shadFolderAct, &QAction::triggered, this, [this]() {
        QString userPath;
        Common::FS::PathToQString(userPath, Common::FS::GetUserPath(Common::FS::PathType::UserDir));
        QDesktopServices::openUrl(QUrl::fromLocalFile(userPath));
    });

    connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::StartGame);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::PauseGame);
    connect(m_game_grid_frame.get(), &QTableWidget::cellDoubleClicked, this,
            &MainWindow::StartGame);
    connect(m_game_list_frame.get(), &QTableWidget::cellDoubleClicked, this,
            &MainWindow::StartGame);
    connect(m_game_list_frame.get(), &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int /*currentColumn*/, int previousRow, int previousColumn) {
                if (previousRow < 0 && previousColumn < 0) {
                    return;
                }
                PlayUiSelectSound();
                if (currentRow >= 0 && currentRow < m_game_info->m_games.size()) {
                    const auto& info = m_game_info->m_games[currentRow];
                    PlayPreviewAudio(info);
                    UpdateBackgroundView(info);
                }
            });
    connect(m_game_grid_frame.get(), &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int currentColumn, int previousRow, int previousColumn) {
                if (previousRow < 0 && previousColumn < 0) {
                    return;
                }
                PlayUiSelectSound();
                if (currentRow >= 0 && currentColumn >= 0) {
                    const int itemID =
                        (m_game_grid_frame->columnCount() * currentRow) + currentColumn;
                    if (itemID >= 0 && itemID < m_game_info->m_games.size()) {
                        const auto& info = m_game_info->m_games[itemID];
                        PlayPreviewAudio(info);
                        UpdateBackgroundView(info);
                    }
                }
            });
    connect(m_game_list_frame.get(), &GameListFrame::GameSelected, this,
            [this](const GameInfo& info) {
                if (m_background_widget) {
                    m_background_widget->OnGameSelected(info);
                }
                UpdateBackgroundView(info);
            });

    connect(ui->configureAct, &QAction::triggered, this, [this]() {
        auto settingsDialog =
            new SettingsDialog(m_gui_settings, m_compat_info, this, isGameRunning);

        connect(settingsDialog, &SettingsDialog::LanguageChanged, this,
                &MainWindow::OnLanguageChanged);

        connect(settingsDialog, &SettingsDialog::CompatibilityChanged, this,
                &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::UiSoundEffectsToggled, this,
                [this](bool enabled) {
                    if (m_ui_sound_player) {
                        m_ui_sound_player->setEnabled(enabled);
                    }
                });
        connect(settingsDialog, &SettingsDialog::LauncherBgmToggled, this,
                [](bool enabled) {
                    if (!enabled) {
                        BackgroundMusicPlayer::getInstance().stopMusic();
                    }
                });
        connect(settingsDialog, &SettingsDialog::AutoScrollLogToggled, this,
                [this](bool enabled) {
                    if (m_game_log_widget) {
                        m_game_log_widget->setAutoScrollEnabled(enabled);
                    }
                });

        connect(settingsDialog, &SettingsDialog::accepted, this, &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::rejected, this, &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::close, this, &MainWindow::RefreshGameTable);

        connect(settingsDialog, &SettingsDialog::BackgroundOpacityChanged, this,
                [this](int opacity) {
                    m_gui_settings->SetValue(gui::gl_backgroundImageOpacity,
                                             std::clamp(opacity, 0, 100));
                    if (m_game_list_frame) {
                        QTableWidgetItem* current = m_game_list_frame->GetCurrentItem();
                        if (current) {
                            m_game_list_frame->SetListBackgroundImage(current);
                        }
                    }
                    if (m_game_grid_frame) {
                        if (m_game_grid_frame->IsValidCellSelected()) {
                            m_game_grid_frame->SetGridBackgroundImage(m_game_grid_frame->crtRow,
                                                                      m_game_grid_frame->crtColumn);
                        }
                    }
                });

        settingsDialog->exec();
    });

    connect(ui->settingsButton, &QPushButton::clicked, this, [this]() {
        auto settingsDialog =
            new SettingsDialog(m_gui_settings, m_compat_info, this, isGameRunning);

        connect(settingsDialog, &SettingsDialog::LanguageChanged, this,
                &MainWindow::OnLanguageChanged);

        connect(settingsDialog, &SettingsDialog::CompatibilityChanged, this,
                &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::UiSoundEffectsToggled, this,
                [this](bool enabled) {
                    if (m_ui_sound_player) {
                        m_ui_sound_player->setEnabled(enabled);
                    }
                });
        connect(settingsDialog, &SettingsDialog::LauncherBgmToggled, this,
                [](bool enabled) {
                    if (!enabled) {
                        BackgroundMusicPlayer::getInstance().stopMusic();
                    }
                });

        connect(settingsDialog, &SettingsDialog::accepted, this, &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::rejected, this, &MainWindow::RefreshGameTable);
        connect(settingsDialog, &SettingsDialog::close, this, &MainWindow::RefreshGameTable);

        connect(settingsDialog, &SettingsDialog::BackgroundOpacityChanged, this,
                [this](int opacity) {
                    m_gui_settings->SetValue(gui::gl_backgroundImageOpacity,
                                             std::clamp(opacity, 0, 100));
                    if (m_game_list_frame) {
                        QTableWidgetItem* current = m_game_list_frame->GetCurrentItem();
                        if (current) {
                            m_game_list_frame->SetListBackgroundImage(current);
                        }
                    }
                    if (m_game_grid_frame) {
                        if (m_game_grid_frame->IsValidCellSelected()) {
                            m_game_grid_frame->SetGridBackgroundImage(m_game_grid_frame->crtRow,
                                                                      m_game_grid_frame->crtColumn);
                        }
                    }
                });

        settingsDialog->exec();
    });

    connect(ui->controllerButton, &QPushButton::clicked, this, [this]() {
        ControlSettings* remapWindow =
            new ControlSettings(m_game_info, isGameRunning, runningGameSerial, this);
        remapWindow->exec();
    });

    connect(ui->keyboardButton, &QPushButton::clicked, this, [this]() {
        auto kbmWindow = new KBMSettings(m_game_info, isGameRunning, runningGameSerial, this);
        kbmWindow->exec();
    });

#ifdef ENABLE_UPDATER
    connect(ui->updaterAct, &QAction::triggered, this, [this]() {
        auto checkUpdate = new CheckUpdate(m_gui_settings, true);
        checkUpdate->exec();
    });
#endif

    connect(ui->aboutAct, &QAction::triggered, this, [this]() {
        auto aboutDialog = new AboutDialog(m_gui_settings, this);
        aboutDialog->exec();
    });

    connect(ui->configureHotkeys, &QAction::triggered, this, [this]() {
        auto hotkeyDialog = new Hotkeys(isGameRunning, this);
        hotkeyDialog->exec();
    });

    connect(ui->setIconSizeTinyAct, &QAction::triggered, this, [this]() {
        if (isTableList) {
            m_game_list_frame->icon_size =
                36; // 36 is the minimum icon size to use due to text disappearing.
            ui->sizeSlider->setValue(0); // icone_size - 36
            m_gui_settings->SetValue(gui::gl_icon_size, 36);
            m_gui_settings->SetValue(gui::gl_slider_pos, 0);
        } else {
            m_game_grid_frame->icon_size = 69;
            ui->sizeSlider->setValue(0); // icone_size - 36
            m_gui_settings->SetValue(gui::gg_icon_size, 69);
            m_gui_settings->SetValue(gui::gg_slider_pos, 9);
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        }
    });

    connect(ui->setIconSizeSmallAct, &QAction::triggered, this, [this]() {
        if (isTableList) {
            m_game_list_frame->icon_size = 64;
            ui->sizeSlider->setValue(28);
            m_gui_settings->SetValue(gui::gl_icon_size, 64);
            m_gui_settings->SetValue(gui::gl_slider_pos, 28);
        } else {
            m_game_grid_frame->icon_size = 97;
            ui->sizeSlider->setValue(28);
            m_gui_settings->SetValue(gui::gg_icon_size, 97);
            m_gui_settings->SetValue(gui::gg_slider_pos, 28);
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        }
    });

    connect(ui->setIconSizeMediumAct, &QAction::triggered, this, [this]() {
        if (isTableList) {
            m_game_list_frame->icon_size = 128;
            ui->sizeSlider->setValue(92);
            m_gui_settings->SetValue(gui::gl_icon_size, 128);
            m_gui_settings->SetValue(gui::gl_slider_pos, 92);
        } else {
            m_game_grid_frame->icon_size = 161;
            ui->sizeSlider->setValue(92);
            m_gui_settings->SetValue(gui::gg_icon_size, 161);
            m_gui_settings->SetValue(gui::gg_slider_pos, 92);
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        }
    });

    connect(ui->setIconSizeLargeAct, &QAction::triggered, this, [this]() {
        if (isTableList) {
            m_game_list_frame->icon_size = 256;
            ui->sizeSlider->setValue(220);
            m_gui_settings->SetValue(gui::gl_icon_size, 256);
            m_gui_settings->SetValue(gui::gl_slider_pos, 220);
        } else {
            m_game_grid_frame->icon_size = 256;
            ui->sizeSlider->setValue(220);
            m_gui_settings->SetValue(gui::gg_icon_size, 256);
            m_gui_settings->SetValue(gui::gg_slider_pos, 220);
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        }
    });
    // List
    connect(ui->setlistModeListAct, &QAction::triggered, this, [this]() {
        BackgroundMusicPlayer::getInstance().stopMusic();
        m_game_view_stack->setCurrentWidget(m_game_list_frame.data());
        m_game_grid_frame->hide();
        m_elf_viewer->hide();
        m_game_list_frame->show();
        m_game_list_frame->clearContents();
        m_game_list_frame->PopulateGameList();
        isTableList = true;
        m_gui_settings->SetValue(gui::gl_mode, 0);
        int slider_pos = m_gui_settings->GetValue(gui::gl_slider_pos).toInt();
        ui->sizeSlider->setEnabled(true);
        ui->sizeSlider->setSliderPosition(slider_pos);
        ui->mw_searchbar->setText("");
        SetLastIconSizeBullet();
    });
    // Grid
    connect(ui->setlistModeGridAct, &QAction::triggered, this, [this]() {
        BackgroundMusicPlayer::getInstance().stopMusic();
        m_game_view_stack->setCurrentWidget(m_game_grid_frame.data());
        m_game_grid_frame->show();
        m_game_list_frame->hide();
        m_elf_viewer->hide();
        if (m_game_grid_frame->item(0, 0) == nullptr) {
            m_game_grid_frame->clearContents();
            m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        }
        isTableList = false;
        m_gui_settings->SetValue(gui::gl_mode, 1);
        int slider_pos_grid = m_gui_settings->GetValue(gui::gg_slider_pos).toInt();
        ui->sizeSlider->setEnabled(true);
        ui->sizeSlider->setSliderPosition(slider_pos_grid);
        ui->mw_searchbar->setText("");
        SetLastIconSizeBullet();
    });
    // Elf Viewer
    connect(ui->setlistElfAct, &QAction::triggered, this, [this]() {
        BackgroundMusicPlayer::getInstance().stopMusic();
        m_game_view_stack->setCurrentWidget(m_elf_viewer.data());
        m_game_grid_frame->hide();
        m_game_list_frame->hide();
        m_elf_viewer->show();
        isTableList = false;
        ui->sizeSlider->setDisabled(true);
        m_gui_settings->SetValue(gui::gl_mode, 2);
        SetLastIconSizeBullet();
    });

    // Cheats/Patches Download.
    connect(ui->downloadCheatsPatchesAct, &QAction::triggered, this, [this]() {
        QDialog* panelDialog = new QDialog(this);
        QVBoxLayout* layout = new QVBoxLayout(panelDialog);
        QPushButton* downloadAllCheatsButton =
            new QPushButton(tr("Download Cheats For All Installed Games"), panelDialog);
        QPushButton* downloadAllPatchesButton =
            new QPushButton(tr("Download Patches For All Games"), panelDialog);

        layout->addWidget(downloadAllCheatsButton);
        layout->addWidget(downloadAllPatchesButton);

        panelDialog->setLayout(layout);

        connect(downloadAllCheatsButton, &QPushButton::clicked, this, [this, panelDialog]() {
            QEventLoop eventLoop;
            int pendingDownloads = 0;

            auto onDownloadFinished = [&]() {
                if (--pendingDownloads <= 0) {
                    eventLoop.quit();
                }
            };

            for (const GameInfo& game : m_game_info->m_games) {
                QString empty = "";
                QString gameSerial = QString::fromStdString(game.serial);
                QString gameVersion = QString::fromStdString(game.version);

                CheatsPatches* cheatsPatches =
                    new CheatsPatches(empty, empty, empty, empty, empty, nullptr);
                connect(cheatsPatches, &CheatsPatches::downloadFinished, onDownloadFinished);

                pendingDownloads += 3;

                cheatsPatches->downloadCheats("wolf2022", gameSerial, gameVersion, false);
                cheatsPatches->downloadCheats("GoldHEN", gameSerial, gameVersion, false);
                cheatsPatches->downloadCheats("shadPS4", gameSerial, gameVersion, false);
            }
            eventLoop.exec();

            QMessageBox::information(
                nullptr, tr("Download Complete"),
                tr("You have downloaded cheats for all the games you have installed."));

            panelDialog->accept();
        });
        connect(downloadAllPatchesButton, &QPushButton::clicked, [panelDialog]() {
            QEventLoop eventLoop;
            int pendingDownloads = 0;

            auto onDownloadFinished = [&]() {
                if (--pendingDownloads <= 0) {
                    eventLoop.quit();
                }
            };

            QString empty = "";
            CheatsPatches* cheatsPatches =
                new CheatsPatches(empty, empty, empty, empty, empty, nullptr);
            connect(cheatsPatches, &CheatsPatches::downloadFinished, onDownloadFinished);

            pendingDownloads += 2;

            cheatsPatches->downloadPatches("GoldHEN", false);
            cheatsPatches->downloadPatches("shadPS4", false);

            eventLoop.exec();
            QMessageBox::information(
                nullptr, tr("Download Complete"),
                QString(tr("Patches Downloaded Successfully!") + "\n" +
                        tr("All Patches available for all games have been downloaded.")));
            cheatsPatches->createFilesJson("GoldHEN");
            cheatsPatches->createFilesJson("shadPS4");
            panelDialog->accept();
        });
        panelDialog->exec();
    });

    // Dump game list.
    connect(ui->dumpGameListAct, &QAction::triggered, this, [&] {
        QString filePath = qApp->applicationDirPath().append("/GameList.txt");
        QFile file(filePath);
        QTextStream out(&file);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qDebug() << "Failed to open file for writing:" << file.errorString();
            return;
        }
        out << QString("%1 %2 %3 %4 %5\n")
                   .arg("          NAME", -50)
                   .arg("    ID", -10)
                   .arg("FW", -4)
                   .arg(" APP VERSION", -11)
                   .arg("                Path");
        for (const GameInfo& game : m_game_info->m_games) {
            QString game_path;
            Common::FS::PathToQString(game_path, game.path);
            out << QString("%1 %2 %3     %4 %5\n")
                       .arg(QString::fromStdString(game.name), -50)
                       .arg(QString::fromStdString(game.serial), -10)
                       .arg(QString::fromStdString(game.fw), -4)
                       .arg(QString::fromStdString(game.version), -11)
                       .arg(game_path);
        }
    });

    // Package install.
    connect(ui->bootInstallPkgAct, &QAction::triggered, this, &MainWindow::InstallPkg);
    connect(ui->bootGameAct, &QAction::triggered, this, &MainWindow::BootGame);
    connect(ui->installFirmwareAct, &QAction::triggered, this, &MainWindow::InstallFirmware);
    connect(ui->gameInstallPathAct, &QAction::triggered, this, &MainWindow::InstallDirectory);

    // elf viewer
    connect(ui->addElfFolderAct, &QAction::triggered, m_elf_viewer.data(),
            &ElfViewer::OpenElfFolder);

    // Package Viewer.
    connect(ui->pkgViewerAct, &QAction::triggered, this, [this]() {
        PKGViewer* pkgViewer = new PKGViewer(
            m_game_info, this, [this](std::filesystem::path file, int pkgNum, int nPkg) {
                this->InstallDragDropPkg(file, pkgNum, nPkg);
            });
        pkgViewer->show();
    });

    // Trophy Viewer
    connect(ui->trophyViewerAct, &QAction::triggered, this, [this]() {
        if (m_game_info->m_games.empty()) {
            QMessageBox::information(
                this, tr("Trophy Viewer"),
                tr("No games found. Please add your games to your library first."));
            return;
        }

        const auto& firstGame = m_game_info->m_games[0];
        QString trophyPath, gameTrpPath;
        Common::FS::PathToQString(trophyPath, firstGame.serial);
        Common::FS::PathToQString(gameTrpPath, firstGame.path);

        auto game_update_path = Common::FS::PathFromQString(gameTrpPath);
        game_update_path += "-UPDATE";
        if (std::filesystem::exists(game_update_path)) {
            Common::FS::PathToQString(gameTrpPath, game_update_path);
        } else {
            game_update_path = Common::FS::PathFromQString(gameTrpPath);
            game_update_path += "-patch";
            if (std::filesystem::exists(game_update_path)) {
                Common::FS::PathToQString(gameTrpPath, game_update_path);
            }
        }

        QVector<TrophyGameInfo> allTrophyGames;
        for (const auto& game : m_game_info->m_games) {
            TrophyGameInfo gameInfo;
            gameInfo.name = QString::fromStdString(game.name);
            Common::FS::PathToQString(gameInfo.trophyPath, game.serial);
            Common::FS::PathToQString(gameInfo.gameTrpPath, game.path);

            auto update_path = Common::FS::PathFromQString(gameInfo.gameTrpPath);
            update_path += "-UPDATE";
            if (std::filesystem::exists(update_path)) {
                Common::FS::PathToQString(gameInfo.gameTrpPath, update_path);
            } else {
                update_path = Common::FS::PathFromQString(gameInfo.gameTrpPath);
                update_path += "-patch";
                if (std::filesystem::exists(update_path)) {
                    Common::FS::PathToQString(gameInfo.gameTrpPath, update_path);
                }
            }

            allTrophyGames.append(gameInfo);
        }

        QString gameName = QString::fromStdString(firstGame.name);
        TrophyViewer* trophyViewer =
            new TrophyViewer(m_gui_settings, trophyPath, gameTrpPath, gameName, allTrophyGames);
        trophyViewer->show();
    });

    // Themes
    connect(ui->setThemeDark, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Dark, ui->mw_searchbar);
        m_current_theme = Theme::Dark;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Dark));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeLight, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Light, ui->mw_searchbar);
        m_current_theme = Theme::Light;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Light));
        if (!isIconBlack) {
            SetUiIcons(true);
            isIconBlack = true;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeGreen, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Green, ui->mw_searchbar);
        m_current_theme = Theme::Green;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Green));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeBlue, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Blue, ui->mw_searchbar);
        m_current_theme = Theme::Blue;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Blue));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeViolet, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Violet, ui->mw_searchbar);
        m_current_theme = Theme::Violet;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Violet));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeGruvbox, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Gruvbox, ui->mw_searchbar);
        m_current_theme = Theme::Gruvbox;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Gruvbox));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeTokyoNight, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::TokyoNight, ui->mw_searchbar);
        m_current_theme = Theme::TokyoNight;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::TokyoNight));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
    connect(ui->setThemeOled, &QAction::triggered, &m_window_themes, [this]() {
        m_window_themes.SetWindowTheme(Theme::Oled, ui->mw_searchbar);
        m_current_theme = Theme::Oled;
        m_gui_settings->SetValue(gui::gen_theme, static_cast<int>(Theme::Oled));
        if (isIconBlack) {
            SetUiIcons(false);
            isIconBlack = false;
        }
        if (m_background_widget) {
            m_background_widget->SetThemeBackground(
                m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
        }
    });
}

void MainWindow::PlayUiSelectSound() {
    if (!m_ui_sound_player || isGameRunning) {
        return;
    }

    if (!Config::getUiSoundEffectsEnabled()) {
        return;
    }

    m_ui_sound_player->playSelect();
}

void MainWindow::PlayUiErrorSound() {
    if (!m_ui_sound_player || isGameRunning) {
        return;
    }

    if (!Config::getUiSoundEffectsEnabled()) {
        return;
    }

    m_ui_sound_player->playError();
}

void MainWindow::ShowCritical(QWidget* parent, const QString& title, const QString& text) {
    PlayUiErrorSound();
    QMessageBox::critical(parent, title, text);
}

void MainWindow::PlayPreviewAudio(const GameInfo& info) {
    if (!m_preview_audio_player || isGameRunning) {
        return;
    }
    m_preview_audio_player->playPreview(info.path);
}

void MainWindow::UpdateBackgroundView(const GameInfo& info) {
    if (!m_background_view) {
        return;
    }
    QString bgPath;
    if (std::filesystem::exists(info.pic_path)) {
        Common::FS::PathToQString(bgPath, info.pic_path);
    } else if (std::filesystem::exists(info.icon_path)) {
        Common::FS::PathToQString(bgPath, info.icon_path);
    }
    if (bgPath.isEmpty()) {
        m_background_view->setDefaultBackground();
    } else {
        m_background_view->setBackground(bgPath);
    }
}

void MainWindow::StartGame() {
    BackgroundMusicPlayer::getInstance().stopMusic();
    if (m_ui_sound_player) {
        m_ui_sound_player->stopAll();
    }
    QString gamePath = "";
    int table_mode = m_gui_settings->GetValue(gui::gl_mode).toInt();
    if (table_mode == 0) {
        if (m_game_list_frame->currentItem()) {
            int itemID = m_game_list_frame->currentItem()->row();
            Common::FS::PathToQString(gamePath, m_game_info->m_games[itemID].path / "eboot.bin");
            runningGameSerial = m_game_info->m_games[itemID].serial;
        }
    } else if (table_mode == 1) {
        if (m_game_grid_frame->cellClicked) {
            int itemID = (m_game_grid_frame->crtRow * m_game_grid_frame->columnCnt) +
                         m_game_grid_frame->crtColumn;
            Common::FS::PathToQString(gamePath, m_game_info->m_games[itemID].path / "eboot.bin");
            runningGameSerial = m_game_info->m_games[itemID].serial;
        }
    } else {
        if (m_elf_viewer->currentItem()) {
            int itemID = m_elf_viewer->currentItem()->row();
            gamePath = m_elf_viewer->m_elf_list[itemID];
        }
    }
    if (gamePath != "") {
        AddRecentFiles(gamePath);
        const auto path = Common::FS::PathFromQString(gamePath);
        if (!std::filesystem::exists(path)) {
            ShowCritical(nullptr, tr("Run Game"), QString(tr("Eboot.bin file not found")));
            return;
        }
        StartEmulator(path);

        UpdateToolbarButtons();
    }
}

bool isTable;
void MainWindow::SearchGameTable(const QString& text) {
    if (isTableList) {
        if (isTable != true) {
            m_game_info->m_games = m_game_info->m_games_backup;
            m_game_list_frame->PopulateGameList();
            isTable = true;
        }
        for (int row = 0; row < m_game_list_frame->rowCount(); row++) {
            QString game_name = QString::fromStdString(m_game_info->m_games[row].name);
            bool match = (game_name.contains(text, Qt::CaseInsensitive)); // Check only in column 1
            m_game_list_frame->setRowHidden(row, !match);
        }
    } else {
        isTable = false;
        m_game_info->m_games = m_game_info->m_games_backup;
        m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);

        QVector<GameInfo> filteredGames;
        for (const auto& gameInfo : m_game_info->m_games) {
            QString game_name = QString::fromStdString(gameInfo.name);
            if (game_name.contains(text, Qt::CaseInsensitive)) {
                filteredGames.push_back(gameInfo);
            }
        }
        std::sort(filteredGames.begin(), filteredGames.end(), m_game_info->CompareStrings);
        m_game_info->m_games = filteredGames;
        m_game_grid_frame->PopulateGameGrid(filteredGames, true);
    }
}

void MainWindow::ShowGameList() {
    if (ui->showGameListAct->isChecked()) {
        RefreshGameTable();
    } else {
        m_game_grid_frame->clearContents();
        m_game_list_frame->clearContents();
    }
};

void MainWindow::RefreshGameTable() {
    // m_game_info->m_games.clear();
    m_game_info->GetGameInfo(this);
    m_game_list_frame->clearContents();
    m_game_list_frame->PopulateGameList();
    m_game_grid_frame->clearContents();
    m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
    statusBar->clearMessage();
    int numGames = m_game_info->m_games.size();
    QString statusMessage = tr("Games: ") + QString::number(numGames);
    statusBar->showMessage(statusMessage);
}

void MainWindow::ConfigureGuiFromSettings() {
    if (!restoreGeometry(m_gui_settings->GetValue(gui::mw_geometry).toByteArray())) {
        // By default, set the window to 70% of the screen
        resize(QGuiApplication::primaryScreen()->availableSize() * 0.7);
    }
    ui->showGameListAct->setChecked(true);
    ui->showGameLogAct->setChecked(
        m_gui_settings->GetValue(gui::mw_show_game_log_panel).toBool());
    int table_mode = m_gui_settings->GetValue(gui::gl_mode).toInt();
    if (table_mode == 0) {
        ui->setlistModeListAct->setChecked(true);
    } else if (table_mode == 1) {
        ui->setlistModeGridAct->setChecked(true);
    } else if (table_mode == 2) {
        ui->setlistElfAct->setChecked(true);
    }
    BackgroundMusicPlayer::getInstance().setVolume(
        m_gui_settings->GetValue(gui::gl_backgroundMusicVolume).toInt());
}

void MainWindow::SaveWindowState() {
    m_gui_settings->SetValue(gui::mw_geometry, saveGeometry(), false);
    if (m_main_splitter) {
        m_gui_settings->SetValue(gui::mw_game_log_splitter_state, m_main_splitter->saveState(),
                                 false);
    }
    if (m_game_log_widget) {
        m_gui_settings->SetValue(gui::mw_game_log_max_lines, m_game_log_widget->maxLines(), false);
    }
    m_gui_settings->sync();
}

void MainWindow::InstallPkg() {
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter(tr("PKG File (*.PKG *.pkg)"));
    if (dialog.exec()) {
        QStringList fileNames = dialog.selectedFiles();
        int nPkg = fileNames.size();
        int pkgNum = 0;
        for (const QString& file : fileNames) {
            ++pkgNum;
            std::filesystem::path path = Common::FS::PathFromQString(file);
            MainWindow::InstallDragDropPkg(path, pkgNum, nPkg);
        }
    }
}

void MainWindow::InstallFirmware() {
    QFileDialog dialog(this, tr("Select PS4 Firmware (PUP or unpacked folder)"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setNameFilters({tr("PS4 Update (PS4UPDATE.PUP)"), tr("All files (*)")});

    QString selectedPath;
    if (dialog.exec()) {
        const auto files = dialog.selectedFiles();
        if (!files.isEmpty()) {
            selectedPath = files.front();
        }
    }

    if (selectedPath.isEmpty()) {
        QString defaultDir;
        Common::FS::PathToQString(defaultDir,
                                  Common::FS::GetUserPath(Common::FS::PathType::FirmwareDir));
        selectedPath = QFileDialog::getExistingDirectory(
            this, tr("Select unpacked firmware folder"), defaultDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    }

    if (selectedPath.isEmpty()) {
        return;
    }

    AppendToGameLog(tr("Installing firmware from %1").arg(selectedPath),
                    Common::Log::Level::Info);

    const auto firmwarePath = Common::FS::PathFromQString(selectedPath);
    const bool installed = Core::Firmware::FirmwareInstaller::Install(
        Common::FS::PathToUTF8String(firmwarePath));

    if (installed) {
        QMessageBox::information(this, tr("Firmware Install"),
                                 tr("Firmware installed successfully."));
    } else {
        ShowCritical(this, tr("Firmware Install"),
                     tr("Firmware installation failed. Check the log for details."));
    }
}

void MainWindow::BootGame() {
    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter(tr("ELF files (*.bin *.elf *.oelf)"));
    if (dialog.exec()) {
        QStringList fileNames = dialog.selectedFiles();
        int nFiles = fileNames.size();

        if (nFiles > 1) {
            ShowCritical(nullptr, tr("Game Boot"), QString(tr("Only one file can be selected!")));
        } else {
            std::filesystem::path path = Common::FS::PathFromQString(fileNames[0]);
            if (!std::filesystem::exists(path)) {
                ShowCritical(nullptr, tr("Run Game"), QString(tr("Eboot.bin file not found")));
                return;
            }
            StartEmulator(path);
        }
    }
}

void MainWindow::InstallDragDropPkg(std::filesystem::path file, int pkgNum, int nPkg) {
    if (Loader::DetectFileType(file) == Loader::FileTypes::Pkg) {
        std::string failreason;
        pkg = PKG();
        if (!pkg.Open(file, failreason)) {
            ShowCritical(this, tr("PKG ERROR"), QString::fromStdString(failreason));
            return;
        }
        if (!psf.Open(pkg.sfo)) {
            ShowCritical(this, tr("PKG ERROR"), "Could not read SFO. Check log for details");
            return;
        }
        auto category = psf.GetString("CATEGORY");

        if (!use_for_all_queued || pkgNum == 1) {
            InstallDirSelect ids;
            const auto selected = ids.exec();
            if (selected == QDialog::Rejected) {
                return;
            }

            last_install_dir = ids.getSelectedDirectory();
            delete_file_on_install = ids.deleteFileOnInstall();
            use_for_all_queued = ids.useForAllQueued();
        }
        std::filesystem::path game_install_dir = last_install_dir;

        QString pkgType = QString::fromStdString(pkg.GetPkgFlags());
        bool use_game_update = pkgType.contains("PATCH") && Config::getSeparateUpdateEnabled();

        // Default paths
        auto game_folder_path = game_install_dir / pkg.GetTitleID();
        auto game_update_path = use_game_update ? game_folder_path.parent_path() /
                                                      (std::string{pkg.GetTitleID()} + "-patch")
                                                : game_folder_path;
        const int max_depth = 5;

        if (pkgType.contains("PATCH")) {
            // For patches, try to find the game recursively
            auto found_game = Common::FS::FindGameByID(game_install_dir,
                                                       std::string{pkg.GetTitleID()}, max_depth);
            if (found_game.has_value()) {
                game_folder_path = found_game.value().parent_path();
                game_update_path = use_game_update ? game_folder_path.parent_path() /
                                                         (std::string{pkg.GetTitleID()} + "-patch")
                                                   : game_folder_path;
            }
        } else {
            // For base games, we check if the game is already installed
            auto found_game = Common::FS::FindGameByID(game_install_dir,
                                                       std::string{pkg.GetTitleID()}, max_depth);
            if (found_game.has_value()) {
                game_folder_path = found_game.value().parent_path();
            }
            // If the game is not found, we install it in the game install directory
            else {
                game_folder_path = game_install_dir / pkg.GetTitleID();
            }
            game_update_path = use_game_update ? game_folder_path.parent_path() /
                                                     (std::string{pkg.GetTitleID()} + "-patch")
                                               : game_folder_path;
        }

        QString gameDirPath;
        Common::FS::PathToQString(gameDirPath, game_folder_path);
        QDir game_dir(gameDirPath);
        if (game_dir.exists()) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("PKG Extraction"));

            std::string content_id;
            if (auto value = psf.GetString("CONTENT_ID"); value.has_value()) {
                content_id = std::string{*value};
            } else {
                ShowCritical(this, tr("PKG ERROR"), "PSF file there is no CONTENT_ID");
                return;
            }
            std::string entitlement_label = Common::SplitString(content_id, '-')[2];

            auto addon_extract_path =
                Config::getAddonInstallDir() / pkg.GetTitleID() / entitlement_label;
            QString addonDirPath;
            Common::FS::PathToQString(addonDirPath, addon_extract_path);
            QDir addon_dir(addonDirPath);

            if (pkgType.contains("PATCH")) {
                QString pkg_app_version;
                if (auto app_ver = psf.GetString("APP_VER"); app_ver.has_value()) {
                    pkg_app_version = QString::fromStdString(std::string{*app_ver});
                } else {
                    ShowCritical(this, tr("PKG ERROR"), "PSF file there is no APP_VER");
                    return;
                }
                std::filesystem::path sce_folder_path =
                    std::filesystem::exists(game_update_path / "sce_sys" / "param.sfo")
                        ? game_update_path / "sce_sys" / "param.sfo"
                        : game_folder_path / "sce_sys" / "param.sfo";
                psf.Open(sce_folder_path);
                QString game_app_version;
                if (auto app_ver = psf.GetString("APP_VER"); app_ver.has_value()) {
                    game_app_version = QString::fromStdString(std::string{*app_ver});
                } else {
                    ShowCritical(this, tr("PKG ERROR"), "PSF file there is no APP_VER");
                    return;
                }
                double appD = game_app_version.toDouble();
                double pkgD = pkg_app_version.toDouble();
                if (pkgD == appD) {
                    msgBox.setText(QString(tr("Patch detected!") + "\n" +
                                           tr("PKG and Game versions match: ") + pkg_app_version +
                                           "\n" + tr("Would you like to overwrite?")));
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::No);
                } else if (pkgD < appD) {
                    msgBox.setText(QString(tr("Patch detected!") + "\n" +
                                           tr("PKG Version %1 is older than installed version: ")
                                               .arg(pkg_app_version) +
                                           game_app_version + "\n" +
                                           tr("Would you like to overwrite?")));
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::No);
                } else {
                    msgBox.setText(QString(tr("Patch detected!") + "\n" +
                                           tr("Game is installed: ") + game_app_version + "\n" +
                                           tr("Would you like to install Patch: ") +
                                           pkg_app_version + " ?"));
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::No);
                }
                int result = msgBox.exec();
                if (result == QMessageBox::Yes) {
                    // Do nothing.
                } else {
                    return;
                }
            } else if (category == "ac") {
                if (!addon_dir.exists()) {
                    QMessageBox addonMsgBox;
                    addonMsgBox.setWindowTitle(tr("DLC Installation"));
                    addonMsgBox.setText(QString(tr("Would you like to install DLC: %1?"))
                                            .arg(QString::fromStdString(entitlement_label)));

                    addonMsgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    addonMsgBox.setDefaultButton(QMessageBox::No);
                    int result = addonMsgBox.exec();
                    if (result == QMessageBox::Yes) {
                        game_update_path = addon_extract_path;
                    } else {
                        return;
                    }
                } else {
                    msgBox.setText(QString(tr("DLC already installed:") + "\n" + addonDirPath +
                                           "\n\n" + tr("Would you like to overwrite?")));
                    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    msgBox.setDefaultButton(QMessageBox::No);
                    int result = msgBox.exec();
                    if (result == QMessageBox::Yes) {
                        game_update_path = addon_extract_path;
                    } else {
                        return;
                    }
                }
            } else {
                msgBox.setText(QString(tr("Game already installed") + "\n" + gameDirPath + "\n" +
                                       tr("Would you like to overwrite?")));
                msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                msgBox.setDefaultButton(QMessageBox::No);
                int result = msgBox.exec();
                if (result == QMessageBox::Yes) {
                    // Do nothing.
                } else {
                    return;
                }
            }
        } else {
            // Do nothing;
            if (pkgType.contains("PATCH") || category == "ac") {
                QMessageBox::information(
                    this, tr("PKG Extraction"),
                    tr("PKG is a patch or DLC, please install the game first!"));
                return;
            }
            // what else?
        }
        if (!pkg.Extract(file, game_update_path, failreason)) {
            ShowCritical(this, tr("PKG ERROR"), QString::fromStdString(failreason));
        } else {
            int nfiles = pkg.GetNumberOfFiles();

            if (nfiles > 0) {
                QVector<int> indices;
                for (int i = 0; i < nfiles; i++) {
                    indices.append(i);
                }

                QProgressDialog dialog;
                dialog.setWindowTitle(tr("PKG Extraction"));
                dialog.setWindowModality(Qt::WindowModal);
                QString extractmsg = QString(tr("Extracting PKG %1/%2")).arg(pkgNum).arg(nPkg);
                dialog.setLabelText(extractmsg);
                dialog.setAutoClose(true);
                dialog.setRange(0, nfiles);

                dialog.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                                                       dialog.size(), this->geometry()));

                QFutureWatcher<void> futureWatcher;
                connect(&futureWatcher, &QFutureWatcher<void>::finished, this, [=, this]() {
                    if (pkgNum == nPkg) {
                        QString path;

                        // We want to show the parent path instead of the full path
                        Common::FS::PathToQString(path, game_folder_path.parent_path());
                        QIcon windowIcon(
                            Common::FS::PathToUTF8String(game_folder_path / "sce_sys/icon0.png")
                                .c_str());

                        QMessageBox extractMsgBox(this);
                        extractMsgBox.setWindowTitle(tr("Extraction Finished"));
                        if (!windowIcon.isNull()) {
                            extractMsgBox.setWindowIcon(windowIcon);
                        }
                        extractMsgBox.setText(
                            QString(tr("Game successfully installed at %1")).arg(path));
                        extractMsgBox.addButton(QMessageBox::Ok);
                        extractMsgBox.setDefaultButton(QMessageBox::Ok);
                        connect(&extractMsgBox, &QMessageBox::buttonClicked, this,
                                [&](QAbstractButton* button) {
                                    if (extractMsgBox.button(QMessageBox::Ok) == button) {
                                        extractMsgBox.close();
                                        emit ExtractionFinished();
                                    }
                                });
                        extractMsgBox.exec();
                    }
                    if (delete_file_on_install) {
                        std::filesystem::remove(file);
                    }
                });
                connect(&dialog, &QProgressDialog::canceled, [&]() { futureWatcher.cancel(); });
                connect(&futureWatcher, &QFutureWatcher<void>::progressValueChanged, &dialog,
                        &QProgressDialog::setValue);
                futureWatcher.setFuture(
                    QtConcurrent::map(indices, [&](int index) { pkg.ExtractFiles(index); }));
                dialog.exec();
            }
        }
    } else {
        ShowCritical(this, tr("PKG ERROR"), tr("File doesn't appear to be a valid PKG file"));
    }
}

void MainWindow::InstallDirectory() {
    GameInstallDialog dlg;
    dlg.exec();
    RefreshGameTable();
}

void MainWindow::SetLastUsedTheme() {
    Theme lastTheme = static_cast<Theme>(m_gui_settings->GetValue(gui::gen_theme).toInt());
    m_current_theme = lastTheme;
    m_window_themes.SetWindowTheme(lastTheme, ui->mw_searchbar);

    switch (lastTheme) {
    case Theme::Light:
        ui->setThemeLight->setChecked(true);
        isIconBlack = true;
        break;
    case Theme::Dark:
        ui->setThemeDark->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::Green:
        ui->setThemeGreen->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::Blue:
        ui->setThemeBlue->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::Violet:
        ui->setThemeViolet->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::Gruvbox:
        ui->setThemeGruvbox->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::TokyoNight:
        ui->setThemeTokyoNight->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    case Theme::Oled:
        ui->setThemeOled->setChecked(true);
        isIconBlack = false;
        SetUiIcons(false);
        break;
    }

    if (m_background_widget) {
        m_background_widget->SetThemeBackground(
            m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
    }
}

void MainWindow::SetLastIconSizeBullet() {
    // set QAction bullet point if applicable
    int lastSize = m_gui_settings->GetValue(gui::gl_icon_size).toInt();
    int lastSizeGrid = m_gui_settings->GetValue(gui::gg_icon_size).toInt();
    if (isTableList) {
        switch (lastSize) {
        case 36:
            ui->setIconSizeTinyAct->setChecked(true);
            break;
        case 64:
            ui->setIconSizeSmallAct->setChecked(true);
            break;
        case 128:
            ui->setIconSizeMediumAct->setChecked(true);
            break;
        case 256:
            ui->setIconSizeLargeAct->setChecked(true);
            break;
        }
    } else {
        switch (lastSizeGrid) {
        case 69:
            ui->setIconSizeTinyAct->setChecked(true);
            break;
        case 97:
            ui->setIconSizeSmallAct->setChecked(true);
            break;
        case 161:
            ui->setIconSizeMediumAct->setChecked(true);
            break;
        case 256:
            ui->setIconSizeLargeAct->setChecked(true);
            break;
        }
    }
}

QIcon MainWindow::RecolorIcon(const QIcon& icon, bool isWhite) {
    QPixmap pixmap(icon.pixmap(icon.actualSize(QSize(120, 120))));
    QColor clr(isWhite ? Qt::white : Qt::black);
    QBitmap mask = pixmap.createMaskFromColor(clr, Qt::MaskOutColor);
    pixmap.fill(QColor(isWhite ? Qt::black : Qt::white));
    pixmap.setMask(mask);
    return QIcon(pixmap);
}

void MainWindow::SetUiIcons(bool isWhite) {
    ui->bootInstallPkgAct->setIcon(RecolorIcon(ui->bootInstallPkgAct->icon(), isWhite));
    ui->bootGameAct->setIcon(RecolorIcon(ui->bootGameAct->icon(), isWhite));
    ui->installFirmwareAct->setIcon(RecolorIcon(ui->installFirmwareAct->icon(), isWhite));
    ui->shadFolderAct->setIcon(RecolorIcon(ui->shadFolderAct->icon(), isWhite));
    ui->exitAct->setIcon(RecolorIcon(ui->exitAct->icon(), isWhite));
#ifdef ENABLE_UPDATER
    ui->updaterAct->setIcon(RecolorIcon(ui->updaterAct->icon(), isWhite));
#endif
    ui->downloadCheatsPatchesAct->setIcon(
        RecolorIcon(ui->downloadCheatsPatchesAct->icon(), isWhite));
    ui->dumpGameListAct->setIcon(RecolorIcon(ui->dumpGameListAct->icon(), isWhite));
    ui->aboutAct->setIcon(RecolorIcon(ui->aboutAct->icon(), isWhite));
    ui->setlistModeListAct->setIcon(RecolorIcon(ui->setlistModeListAct->icon(), isWhite));
    ui->setlistModeGridAct->setIcon(RecolorIcon(ui->setlistModeGridAct->icon(), isWhite));
    ui->gameInstallPathAct->setIcon(RecolorIcon(ui->gameInstallPathAct->icon(), isWhite));
    ui->menuThemes->setIcon(RecolorIcon(ui->menuThemes->icon(), isWhite));
    ui->menuGame_List_Icons->setIcon(RecolorIcon(ui->menuGame_List_Icons->icon(), isWhite));
    ui->menuUtils->setIcon(RecolorIcon(ui->menuUtils->icon(), isWhite));
    ui->playButton->setIcon(RecolorIcon(ui->playButton->icon(), isWhite));
    ui->pauseButton->setIcon(RecolorIcon(ui->pauseButton->icon(), isWhite));
    ui->stopButton->setIcon(RecolorIcon(ui->stopButton->icon(), isWhite));
    ui->refreshButton->setIcon(RecolorIcon(ui->refreshButton->icon(), isWhite));
    ui->restartButton->setIcon(RecolorIcon(ui->restartButton->icon(), isWhite));
    ui->settingsButton->setIcon(RecolorIcon(ui->settingsButton->icon(), isWhite));
    ui->fullscreenButton->setIcon(RecolorIcon(ui->fullscreenButton->icon(), isWhite));
    ui->controllerButton->setIcon(RecolorIcon(ui->controllerButton->icon(), isWhite));
    ui->keyboardButton->setIcon(RecolorIcon(ui->keyboardButton->icon(), isWhite));
    ui->refreshGameListAct->setIcon(RecolorIcon(ui->refreshGameListAct->icon(), isWhite));
    ui->menuGame_List_Mode->setIcon(RecolorIcon(ui->menuGame_List_Mode->icon(), isWhite));
    ui->pkgViewerAct->setIcon(RecolorIcon(ui->pkgViewerAct->icon(), isWhite));
    ui->trophyViewerAct->setIcon(RecolorIcon(ui->trophyViewerAct->icon(), isWhite));
    ui->configureAct->setIcon(RecolorIcon(ui->configureAct->icon(), isWhite));
    ui->addElfFolderAct->setIcon(RecolorIcon(ui->addElfFolderAct->icon(), isWhite));
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    emit WindowResized(event);
    QMainWindow::resizeEvent(event);
}

void MainWindow::HandleResize(QResizeEvent* event) {
    if (m_background_view) {
        if (auto* parent = m_background_view->parentWidget()) {
            m_background_view->setGeometry(parent->rect());
        }
    }
    if (m_background_widget) {
        m_background_widget->setGeometry(this->rect());
        m_background_widget->SetThemeBackground(
            m_window_themes.CreateThemeBackground(m_current_theme, this->size()));
    }

    if (isTableList) {
        m_game_list_frame->RefreshListBackgroundImage();
    } else {
        m_game_grid_frame->windowWidth = this->width();
        m_game_grid_frame->PopulateGameGrid(m_game_info->m_games, false);
        m_game_grid_frame->RefreshGridBackgroundImage();
    }
}

void MainWindow::AddRecentFiles(QString filePath) {
    QList<QString> list = gui_settings::Var2List(m_gui_settings->GetValue(gui::gen_recentFiles));
    if (!list.empty()) {
        if (filePath == list.at(0)) {
            return;
        }
        auto it = std::find(list.begin(), list.end(), filePath);
        if (it != list.end()) {
            list.erase(it);
        }
    }
    list.insert(list.begin(), filePath);
    if (list.size() > 6) {
        list.pop_back();
    }
    m_gui_settings->SetValue(gui::gen_recentFiles, gui_settings::List2Var(list));
    CreateRecentGameActions(); // Refresh the QActions.
}

void MainWindow::CreateRecentGameActions() {
    m_recent_files_group = new QActionGroup(this);
    ui->menuRecent->clear();
    QList<QString> list = gui_settings::Var2List(m_gui_settings->GetValue(gui::gen_recentFiles));

    for (int i = 0; i < list.size(); i++) {
        QAction* recentFileAct = new QAction(this);
        recentFileAct->setText(list.at(i));
        ui->menuRecent->addAction(recentFileAct);
        m_recent_files_group->addAction(recentFileAct);
    }

    connect(m_recent_files_group, &QActionGroup::triggered, this, [this](QAction* action) {
        auto gamePath = Common::FS::PathFromQString(action->text());
        AddRecentFiles(action->text()); // Update the list.
        if (!std::filesystem::exists(gamePath)) {
            ShowCritical(nullptr, tr("Run Game"), QString(tr("Eboot.bin file not found")));
            return;
        }
        if (m_preview_audio_player) {
            m_preview_audio_player->stop();
        }
        StartEmulator(gamePath);
    });
}

void MainWindow::LoadTranslation() {
    auto language = m_gui_settings->GetValue(gui::gen_guiLanguage).toString();

    const QString base_dir = QStringLiteral(":/translations");
    QString base_path = QStringLiteral("%1/%2.qm").arg(base_dir).arg(language);

    if (QFile::exists(base_path)) {
        if (translator != nullptr) {
            qApp->removeTranslator(translator);
        }

        translator = new QTranslator(qApp);
        if (!translator->load(base_path)) {
            QMessageBox::warning(
                nullptr, QStringLiteral("Translation Error"),
                QStringLiteral("Failed to find load translation file for '%1':\n%2")
                    .arg(language)
                    .arg(base_path));
            delete translator;
        } else {
            qApp->installTranslator(translator);
            ui->retranslateUi(this);
        }
    }
}

void MainWindow::OnLanguageChanged(const QString& locale) {
    m_gui_settings->SetValue(gui::gen_guiLanguage, locale);

    LoadTranslation();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            auto tblMode = m_gui_settings->GetValue(gui::gl_mode).toInt();
            if (tblMode != 2 && (tblMode != 1 || m_game_grid_frame->IsValidCellSelected())) {
                StartGame();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::StartEmulator(std::filesystem::path path) {
    if (isGameRunning) {
        ShowCritical(nullptr, tr("Run Game"), QString(tr("Game is already running!")));
        return;
    }
    if (m_preview_audio_player) {
        m_preview_audio_player->stop();
    }
    QString gamePath;
    Common::FS::PathToQString(gamePath, path);
    AppendToGameLog(tr("Starting emulator: %1").arg(gamePath), Common::Log::Level::Info);
    isGameRunning = true;
    showMinimized();
#ifdef __APPLE__
    // SDL on macOS requires main thread.
    Core::Emulator* emulator = Common::Singleton<Core::Emulator>::Instance();
    emulator->Run(path);
#else
    std::thread emulator_thread([=] {
        Core::Emulator* emulator = Common::Singleton<Core::Emulator>::Instance();
        emulator->Run(path);
    });
    emulator_thread.detach();
#endif
}
