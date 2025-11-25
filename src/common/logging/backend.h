#// SPDX-FileCopyrightText: Copyright 2014 Citra Emulator Project
#// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string_view>
#include "common/logging/filter.h"
#include "common/logging/log_entry.h"

namespace Common::Log {

class Filter;
using LogCallback = std::function<void(const Entry&)>;

/// Initializes the logging system. This should be the first thing called in main.
void Initialize(std::string_view log_file = "");

bool IsActive();

/// Starts the logging threads.
void Start();

/// Explictily stops the logger thread and flushes the buffers
void Stop();

/// Closes log files and stops the logger
void Denitializer();

/// The global filter will prevent any messages from even being processed if they are filtered.
void SetGlobalFilter(const Filter& filter);

void SetColorConsoleBackendEnabled(bool enabled);

void SetAppend();

void RegisterCallbackBackend(LogCallback callback);

} // namespace Common::Log
