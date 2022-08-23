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

#include "Gauge/GaugeNav.hpp"
#include "Look/NavLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Task/ProtectedTaskManager.hpp"
#include "Engine/Task/TaskManager.hpp"
#include "Components.hpp"
#include "Engine/Waypoint/Waypoint.hpp"
#include "Math/Screen.hpp"
#include "util/Macros.hpp"
#include "Formatter/UserUnits.hpp"

GaugeNav::GaugeNav(const FullBlackboard &_blackboard,
                       ContainerWindow &parent,  NavLook &_look,
                       PixelRect rc, const WindowStyle style) noexcept
  :blackboard(_blackboard), look(_look)
{
  Create(parent, rc, style);
}

void
GaugeNav::OnPaintBuffer(Canvas &canvas)
{
  const PixelRect rc = GetClientRect();

  if (!look.fonts_valid || look.HasChanged(rc))
    look.Resize(canvas, rc);

  canvas.DrawFilledRectangle(rc, look.background_color);
  canvas.Select(look.border_pen);
  canvas.DrawRectangle(rc);
  canvas.SetBackgroundColor(look.background_color);

  const TaskStats task_stats = Calculated().task_stats;
  const ElementStat current_leg = task_stats.current_leg;
  if (!task_stats.task_valid || !current_leg.location_remaining.IsValid())
  {
    NoTarget(canvas, rc);
    return;
  }

  const auto way_point = protected_task_manager != nullptr
    ? protected_task_manager->GetActiveWaypoint()
    : nullptr;

  if (!way_point)
  {
    NoTarget(canvas, rc);
    return;
  }

  // We have something real to display.
  const TCHAR *name = way_point->name.c_str();
  StaticString<32> buffer;
  auto const leg = current_leg.vector_remaining.distance;
  auto const task = task_stats.task_finished
                     ? task_stats.current_leg.vector_remaining.distance
                     : task_stats.total.remaining.GetDistance();
  auto const basic = Basic();

  // Create the ribbon
  canvas.SetBackgroundColor(look.background_color);


  canvas.SetTextColor(look.text_color);
  canvas.Select(look.text_font);

  // Add the waypoint name
  const unsigned padding = Layout::GetTextPadding();
  PixelSize tsize = canvas.CalcTextSize(name);
  PixelPoint p = PixelPoint(rc.left + padding, rc.bottom - padding - tsize.height);
  canvas.DrawText(p, name);

  // Add the leg distance remaining
  FormatUserDistanceSmart(leg, buffer.buffer(), true);
  p = PixelPoint(rc.left + (padding * 10) + tsize.width, rc.bottom - padding - tsize.height);
  canvas.DrawText(p, buffer);

  // Add the task distance remaining
  FormatUserDistanceSmart(task, buffer.buffer(), true);
  tsize = canvas.CalcTextSize(buffer);
  p = PixelPoint(rc.right - padding - tsize.width, rc.bottom - padding - tsize.height);
  canvas.DrawText(p, buffer);

  // Add the target bearing arrow (max 90 degrees)
  const GeoVector &vector_remaining = task_stats.current_leg.vector_remaining;
  if (basic.track_available && task_stats.task_valid &&
      vector_remaining.IsValid() && vector_remaining.distance > 10)
  {
    canvas.Select(look.goal_brush);
    canvas.Select(look.goal_pen);
    int icon_height = rc.GetHeight() * 2 / 3;
    int icon_half_width = icon_height / 4;

    int offset = GetOffset(vector_remaining.bearing - basic.track);
    BulkPixelPoint blue_triangle[4] = { 
      { (int)look.middle + offset - icon_half_width, rc.top}, 
      { (int)look.middle + offset, rc.top + icon_height }, 
      { (int)look.middle + offset + icon_half_width, rc.top}
    };

    canvas.DrawTriangleFan(blue_triangle, ARRAY_SIZE(blue_triangle));
  }

  // Add the track arrow - centre bottom
  canvas.Select(look.track_pen);
  canvas.DrawLine({(int)look.middle, rc.bottom}, {(int)look.middle, rc.top});

}

int
GaugeNav::GetOffset(Angle angle)
{
  double degrees = angle.Degrees();
  if (degrees > 180.0)
    degrees -= 360.0;
  else if (degrees < -180)
    degrees += 360;
  if (degrees < -90)
    degrees = -90.0;
  else if ( degrees > 90)
    degrees = 90;
  return (int)(degrees / 90 * look.middle);
}

void
GaugeNav::NoTarget(Canvas &canvas, PixelRect rc)
{
  canvas.Select(look.error_font);
  PixelSize text_size = canvas.CalcTextSize(look.no_target_msg);
  canvas.SetTextColor(look.text_color);

  const int left = look.middle - (text_size.width / 2);

  const PixelPoint text_position{left, rc.top + 1};
  canvas.DrawText(text_position, look.no_target_msg);

}

void
GaugeNav::OnResize(PixelSize new_size)
{
  AntiFlickerWindow::OnResize(new_size);
}
