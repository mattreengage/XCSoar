// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include <cstdint>

enum class VarioRange : uint8_t {
  RANGE_LOW,
  RANGE_NORMAL,
  RANGE_HIGH,
};

struct VarioSettings {
  VarioRange vario_range;
  bool show_alt_vario;
  bool show_average;
  bool show_mc;
  bool show_speed_to_fly;
  bool show_ballast;
  bool show_bugs;
  bool show_gross;
  bool show_average_needle;
  bool show_thermal_average_needle;

  void SetDefaults() noexcept;
};
