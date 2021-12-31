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

#include "Gauge/GaugeGlide.hpp"
#include "Look/GlideLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"

GaugeGlide::GaugeGlide(const FullBlackboard &_blackboard,
                       ContainerWindow &parent,  GlideLook &_look,
                       PixelRect rc, const WindowStyle style) noexcept
  :blackboard(_blackboard), look(_look)
{
  Create(parent, rc, style);
}

void
GaugeGlide::OnPaintBuffer(Canvas &canvas)
{
  const PixelRect rc = GetClientRect();
  spacing = rc.GetHeight() / SEGMENTS;
  const unsigned width = rc.GetWidth();

  canvas.DrawFilledRectangle(rc, look.background_color);
  canvas.Select(look.border);
  canvas.DrawRectangle(rc);
  canvas.SetBackgroundColor(look.background_color);

  auto calc = Calculated();
  int y;
  double root_val;
  const unsigned arrow_span = width / 8;

  bool on_task = calc.task_stats.task_valid && calc.task_stats.current_leg.location_remaining.IsValid();

  // Draw Arrow for GR since last thermal
  if (calc.last_thermal.IsDefined())
  {
    root_val = GetGlideRoot(calc.cruise_gr);
    y = VertPos(rc, root_val);

    BulkPixelPoint gr_t[3];
    gr_t[0].x = rc.left + (width / 2);
    gr_t[0].y = y;
    gr_t[1].x = rc.left;
    gr_t[1].y = y - arrow_span;
    gr_t[2].x = rc.left;
    gr_t[2].y = y + arrow_span;

    canvas.Select(!on_task ? look.border_brush 
                  : IsGood(calc.task_stats.glide_required, calc.cruise_gr) ? look.good_brush 
                  : look.bad_brush);
    canvas.Select(!on_task ? look.border 
                  : IsGood(calc.task_stats.glide_required, calc.cruise_gr) ? look.good_pen
                  : look.bad_pen);

    canvas.DrawTriangleFan(gr_t, 3);
  }

  // Only if cruising, show current GR
  if (!calc.circling && calc.flight.IsGliding())
  {
    root_val = GetGlideRoot(calc.gr);
    y = VertPos(rc, root_val);

    BulkPixelPoint gr_c[3];
    gr_c[0].x = rc.right - (width / 2);
    gr_c[0].y = y;
    gr_c[1].x = rc.right;
    gr_c[1].y = y - arrow_span;
    gr_c[2].x = rc.right;
    gr_c[2].y = y + arrow_span;

    canvas.Select(!on_task ? look.border_brush 
                  : IsGood(calc.task_stats.glide_required, calc.gr) ? look.good_brush 
                  : look.bad_brush);
    canvas.Select(!on_task ? look.border 
                  : IsGood(calc.task_stats.glide_required, calc.gr) ? look.good_pen
                  : look.bad_pen);

    canvas.DrawTriangleFan(gr_c, 3);
  }

  // Draw GR Required for the task
  if (on_task)
  {
    root_val = GetGlideRoot(calc.task_stats.glide_required);
    y = VertPos(rc, root_val);
    canvas.Select(look.border);
    canvas.DrawLine({rc.left, y}, {rc.right, y});
  }

  // Draw the GR numbers
  unsigned i;
  TCHAR buffer[3];
  for (i = MINGR_ROOT; i < MAXGR_ROOT; i++) {
    _stprintf(buffer, _T("%d"), i * i);
    RenderScale(canvas, rc, i, buffer);
  }
  RenderScale(canvas, rc, i, _T("∞"));
  RenderScale(canvas, rc, ++i, _T("++"));
}

bool 
GaugeGlide::IsGood(double required, double actual)
{
  if (required > 0 && actual < 0)
    return true;
  else if (required < 0 && actual > 0)
    return false;
  if (required < actual && required > 0)
    return true;
  else if (required >= actual && required < 0)
    return true;
  else
    return false;
}

double 
GaugeGlide::GetGlideRoot(double val)
{
  return (val < 0) ? -1.0 : sqrt(val);
}

void
GaugeGlide::RenderScale(Canvas &canvas, PixelRect rc, unsigned root_value, const TCHAR *label) noexcept
{
  PixelSize text_size = canvas.CalcTextSize(label);
  const unsigned width = text_size.width;
  const int y = VertPos(rc, (root_value <= MAXGR_ROOT) ? root_value : -1.0 ) - (text_size.height / 2);

  canvas.SetBackgroundColor(look.background_color);
  canvas.SetTextColor(look.text_color);
  canvas.Select(look.text_font);

  const int left = (rc.GetWidth() - width) / 2;

  const PixelPoint text_position{left, y};
  canvas.DrawText(text_position, label);
}

unsigned GaugeGlide::VertPos(PixelRect rc, double root_value)
{
  double val = root_value;
  if (val < 0)
    val = MAXGR_ROOT + 1.0;
  else if (val >= MAXGR_ROOT)
    val = MAXGR_ROOT;
  else if (val < MINGR_ROOT)
    val = MINGR_ROOT;
  return rc.bottom - (spacing * (val - MINGR_ROOT)) - (spacing / 2);
}

void
GaugeGlide::OnResize(PixelSize new_size)
{
  AntiFlickerWindow::OnResize(new_size);
}
