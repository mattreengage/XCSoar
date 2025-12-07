// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "InfoBoxSettings.hpp"
#include "ui/dim/Rect.hpp"

namespace InfoBoxLayout {

struct Layout {
  InfoBoxSettings::Geometry geometry;

  bool landscape;

  PixelSize control_size;

  unsigned count;
  PixelRect positions[InfoBoxSettings::Panel::MAX_CONTENTS];

  PixelRect vario;

  PixelRect remaining;

    bool HasVario() const {
      return vario.right > vario.left && vario.bottom > vario.top;
    }

    void ClearVario() {
      vario.left = vario.top = vario.right = vario.bottom = 0;
    }

    PixelRect nav;
    bool HasNav() const {
      return nav.right > nav.left && nav.bottom > nav.top;
    }

    void ClearNav() {
      nav.left = nav.top = nav.right = nav.bottom = 0;
    }

    PixelRect glide;
    bool HasGlide() const {
      return glide.right > glide.left && glide.bottom > glide.top;
    }

    void ClearGlide() {
      glide.left = glide.top = glide.right = glide.bottom = 0;
    }

  };

[[gnu::pure]]
Layout
Calculate(PixelRect rc, InfoBoxSettings::Geometry geometry) noexcept;

[[gnu::const]]
int
GetBorder(InfoBoxSettings::Geometry geometry, bool landscape,
          unsigned i) noexcept;

} // namespace InfoBoxLayout
