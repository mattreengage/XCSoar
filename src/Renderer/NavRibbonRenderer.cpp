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

#include "NavRibbonRenderer.hpp"
#include "Screen/Layout.hpp"
#include "Math/Screen.hpp"
#include "util/Macros.hpp"

#include "Interface.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Engine/Task/TaskManager.hpp"
#include "Components.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Units/Units.hpp"
#include <tchar.h>

#include "Formatter/UserUnits.hpp"
#include "util/StaticString.hxx"

#ifdef ENABLE_OPENGL
#include "ui/canvas/opengl/Scope.hpp"
#endif


void NavRibbonRenderer::Initialise(const PixelRect rc)
{
  font.Load(FontDescription((rc.bottom - rc.top) / 26, true));
  is_dirty = false;
}

void NavRibbonRenderer::MakeDirty()
{
  is_dirty = true;
}

void
NavRibbonRenderer::Draw(Canvas &canvas, const PixelRect rc)
{
  if (is_dirty)
    this->Initialise(rc);

  const NMEAInfo &basic = CommonInterface::Basic();
  const TaskStats &task_stats = CommonInterface::Calculated().task_stats;
  const ElementStat &current_leg = task_stats.current_leg;
  if (!task_stats.task_valid || !current_leg.location_remaining.IsValid())
    return;

  const auto way_point = protected_task_manager != nullptr
    ? protected_task_manager->GetActiveWaypoint()
    : nullptr;

  if (!way_point)
    return;

  const TCHAR *name = way_point->name.c_str();
  StaticString<32> buffer;
  auto const leg = current_leg.vector_remaining.distance;
  auto const task = task_stats.task_finished
                     ? task_stats.current_leg.vector_remaining.distance
                     : task_stats.total.remaining.GetDistance();

  // Create the ribbon
  PixelRect border = PixelRect(rc.left, rc.top, rc.right, rc.top + Layout::Scale(19));
  canvas.DrawFilledRectangle(border, COLOR_WHITE);
  canvas.SetBackgroundColor(COLOR_WHITE);
  canvas.SetTextColor(COLOR_BLACK);
  canvas.Select(font);

  // Add the waypoint name
  const unsigned padding = Layout::GetTextPadding();
  PixelSize tsize = canvas.CalcTextSize(name);
  PixelPoint p = PixelPoint(border.left + padding, border.bottom - padding - tsize.height);
  canvas.DrawText(p, name);

  // Add the leg distance remaining
  FormatUserDistanceSmart(leg, buffer.buffer(), true);
  p = PixelPoint(border.left + (padding * 10) + tsize.width, border.bottom - padding - tsize.height);
  canvas.DrawText(p, buffer);

  // Add the task distance remaining
  FormatUserDistanceSmart(task, buffer.buffer(), true);
  tsize = canvas.CalcTextSize(buffer);
  p = PixelPoint(border.right - padding - tsize.width, border.bottom - padding - tsize.height);
  canvas.DrawText(p, buffer);

  // Add the track arrow - centre bottom
  int middle = (border.right - border.left) / 2;
  canvas.Select(Brush(COLOR_BLACK));
  BulkPixelPoint black_triangle[4] = { { 0, -10 }, { -4, 0}, { 4, 0}, { 0, -10 } };
  PixelPoint track_pos = PixelPoint(middle, border.bottom);

  PolygonRotateShift(black_triangle, ARRAY_SIZE(black_triangle),
                     track_pos, Angle::Zero());
  canvas.DrawPolygon(black_triangle, ARRAY_SIZE(black_triangle));

  // Add the target bearing arrow (max 90 degrees)
  const GeoVector &vector_remaining = task_stats.current_leg.vector_remaining;
  if (!basic.track_available || !task_stats.task_valid ||
      !vector_remaining.IsValid() || vector_remaining.distance <= 10)
    return;

  // Determine what the difference between track and bearing is and calculate a display offset
  // Maximum shown is at 90 degrees either side.
  Angle angle = vector_remaining.bearing - basic.track;
  double degrees = angle.Degrees();
  if (degrees > 180.0)
    degrees -= 360.0;
  else if (degrees < -180)
    degrees += 360;
  if (degrees < -90)
    degrees = -90.0;
  else if ( degrees > 90)
    degrees = 90;
  int offset = (int)(degrees / 90 * middle);

  canvas.Select(Brush(COLOR_BLUE));
  BulkPixelPoint blue_triangle[4] = { { 0, 10 }, { -4, 0}, { 4, 0}, { 0, 10 } };
  PixelPoint target_pos = PixelPoint(middle + offset, border.top);

  PolygonRotateShift(blue_triangle, ARRAY_SIZE(blue_triangle),
                     target_pos, Angle::Zero());
  canvas.DrawPolygon(blue_triangle, ARRAY_SIZE(blue_triangle));
}

