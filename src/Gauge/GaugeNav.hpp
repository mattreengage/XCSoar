/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2021 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#ifndef GAUGE_NAV_H
#define GAUGE_NAV_H

#include "ui/window/AntiFlickerWindow.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Blackboard/FullBlackboard.hpp"
#include "Math/Point2D.hpp"

struct NavLook;
class ContainerWindow;

class GaugeNav : public AntiFlickerWindow
{
  static constexpr unsigned MINGR_ROOT = 3;
  static constexpr unsigned MAXGR_ROOT = 10;
  static constexpr unsigned SEGMENTS = MAXGR_ROOT - MINGR_ROOT + 2;

  const FullBlackboard &blackboard;

  NavLook &look;

  unsigned spacing;

public:
  GaugeNav(const FullBlackboard &blackboard,
             ContainerWindow &parent, NavLook &look,
             PixelRect rc, const WindowStyle style=WindowStyle()) noexcept;

protected:
  const MoreData &Basic() const noexcept {
    return blackboard.Basic();
  }

  const DerivedInfo &Calculated() const noexcept {
    return blackboard.Calculated();
  }

  const ComputerSettings &GetComputerSettings() const noexcept {
    return blackboard.GetComputerSettings();
  }

  const GlidePolar &GetGlidePolar() const noexcept {
    return GetComputerSettings().polar.glide_polar_task;
  }

  const VarioSettings &Settings() const noexcept {
    return blackboard.GetUISettings().vario;
  }

protected:
  /* virtual methods from class Window */
  virtual void OnResize(PixelSize new_size) override;

  /* virtual methods from class AntiFlickerWindow */
  virtual void OnPaintBuffer(Canvas &canvas) override;

private:
  void NoTarget(Canvas &canvas, PixelRect rc);
  int GetOffset(Angle angle);
};

#endif
