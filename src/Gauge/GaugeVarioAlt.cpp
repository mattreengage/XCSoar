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

#include "Gauge/GaugeVarioAlt.hpp"
#include "Look/VarioAltLook.hpp"
#include "ui/canvas/Canvas.hpp"
#include "Screen/Layout.hpp"
#include "Renderer/UnitSymbolRenderer.hpp"
#include "Math/FastRotation.hpp"
#include "Units/Units.hpp"
#include "util/Clamp.hpp"
#include "util/Macros.hpp"
#include "Formatter/Units.hpp"
#include "Units/System.hpp"
#include "Units/Descriptor.hpp"
#include "Interface.hpp"

#define DELTA_V_STEP 4.
#define DELTA_V_LIMIT 16.
#define TEXT_BUG _T("Bugs")

inline
GaugeVarioAlt::BallastGeometry::BallastGeometry( VarioAltLook &look,
                                             const PixelRect &rc) noexcept
{
  // position of ballast value
  value_pos.x = 4;
  value_pos.y = rc.top + 4
    + look.corner_font.GetCapitalHeight()
    - look.corner_font.GetAscentHeight();
}

inline
GaugeVarioAlt::BugsGeometry::BugsGeometry( VarioAltLook &look,
                                       const PixelRect &rc) noexcept
{
  value_pos.x = 4;
  value_pos.y = rc.bottom - 4
    - look.corner_font.GetHeight();

  label_pos.x = 4;
  label_pos.y = value_pos.y
    - look.corner_font.GetAscentHeight();
}

inline
GaugeVarioAlt::LabelValueGeometry::LabelValueGeometry( VarioAltLook &look,
                                                   PixelPoint position) noexcept
  :label_right(position.x - Layout::Scale(2)),
   label_top(position.y + Layout::Scale(2)),
   label_bottom(label_top + look.text_font.GetCapitalHeight()),
   label_y(label_top + look.text_font.GetCapitalHeight()
           - look.text_font.GetAscentHeight()),
   // TODO: update after units got reconfigured?
   value_right(position.x - Layout::Scale(2)),
   value_top(label_bottom + Layout::Scale(2)),
   value_bottom(value_top + look.value_font.GetCapitalHeight()),
   value_y(value_top + look.value_font.GetCapitalHeight()
           - look.value_font.GetAscentHeight())
{
}

inline unsigned
GaugeVarioAlt::LabelValueGeometry::GetHeight( VarioAltLook &look) noexcept
{
  return look.value_font.GetCapitalHeight() + (look.text_font.GetCapitalHeight() * 3);
}

inline
GaugeVarioAlt::Geometry::Geometry( VarioAltLook &look, const PixelRect &rc) noexcept
  :ballast(look, rc), bugs(look, rc)
{
  nlength0 = Layout::Scale(17);
  nlength1 = Layout::Scale(1);
  nwidth = Layout::Scale(4);
  nline = Layout::Scale(16);

  v_width = rc.GetWidth();
  v_height = rc.GetHeight();

  offset = rc.GetMiddleRight();

  num_values = look.num_info_box;

  const int info_height = LabelValueGeometry::GetHeight(look);
  int pos = look.info_box.top;
  value_a_pos = {look, {look.info_box.right, pos}};
  pos += info_height;
  value_b_pos = {look, {look.info_box.right, pos}};
  pos += info_height;
  value_c_pos = {look, {look.info_box.right, pos}};
  pos += info_height;
  value_d_pos = {look, {look.info_box.right, pos}};
  pos += info_height;
  value_e_pos = {look, {look.info_box.right, pos}};
}

GaugeVarioAlt::GaugeVarioAlt(const FullBlackboard &_blackboard,
                       ContainerWindow &parent,  VarioAltLook &_look,
                       PixelRect rc, const WindowStyle style) noexcept
  :blackboard(_blackboard), look(_look)
{
  Create(parent, rc, style);

  // Initialise from the settings
  ComputerSettings &comp = CommonInterface::SetComputerSettings();
  comp.vario_range = Settings().vario_range;
}

void
GaugeVarioAlt::OnPaintBuffer(Canvas &canvas)
{
  const PixelRect rc = GetClientRect();
  if (!look.fonts_valid || look.HasChanged(rc))
  {
    look.Resize(canvas, rc);
    geometry = {look, rc};
  }

  RenderBase(canvas, rc);

  auto calc = Calculated();
  auto basic = Basic();

  if (geometry.num_values > 0) {
    // JMW averager now displays netto average if not circling
    RenderValue(canvas, geometry.value_a_pos, value_a,
                Units::ToUserVSpeed(calc.circling ? calc.average : calc.netto_average),
                calc.circling ? _T("Avg") : _T("NetAvg"), 
                true,
                0);


    RenderValue(canvas, geometry.value_b_pos, value_b,
                Units::ToUserVSpeed(calc.current_thermal.lift_rate),
                _T("Avg Th."), 
                true,
                0);

    auto alt = (basic.baro_altitude_available) 
              ? basic.baro_altitude 
              : basic.gps_altitude;
    auto alt_fl = (basic.pressure_altitude_available)
              ? basic.pressure_altitude
              : GetComputerSettings().pressure.QNHAltitudeToPressureAltitude(basic.gps_altitude);

    /** 
      TODO: Make Transition Altitude a setting
    */
    auto transition = Units::ToSysAltitude(10000);

    RenderValue(canvas, geometry.value_c_pos, value_c,
                Units::ToUserAltitude((alt > transition) ? alt_fl : alt),
                (alt > transition) ? _T("Alt FL") : _T("Alt QNH"), 
                false,
                0);


    const TaskStats &task_stats = calc.task_stats;
    if (task_stats.task_valid && task_stats.total.travelled.IsDefined()) 
    {
      RenderValue(canvas, geometry.value_d_pos, value_d,
                  Units::ToUserTaskSpeed(task_stats.total.travelled.GetSpeed()),
                  _T("V Task Avg"), 
                  true,
                  0);
      if (geometry.num_values > 4)
      {
        RenderValue(canvas, geometry.value_e_pos, value_e,
                  Units::ToUserTaskSpeed(task_stats.last_hour.speed),
                  _T("V Task H"), 
                  true,
                  0);
      }
    }
  }

  if (Settings().show_ballast && look.num_info_box > 0)
    RenderBallast(canvas);

  if (Settings().show_bugs && look.num_info_box > 0)
    RenderBugs(canvas);

  int ival, ival_av = 0;
  int ival_av_thermal = 0;
  ival_av_thermal = ValueToNeedlePos(calc.current_thermal.lift_rate);

  auto vval = basic.brutto_vario;
  ival = ValueToNeedlePos(vval);
  if (!calc.circling)
    ival_av = ValueToNeedlePos(calc.netto_average);
  else
    ival_av = ValueToNeedlePos(calc.average);

  RenderNeedles(canvas, ival, ival_av, ival_av_thermal);
}

static constexpr PixelPoint
TransformRotatedPoint(IntPoint2D pt, IntPoint2D offset, int ratio) noexcept
{
  return { pt.x + offset.x, (pt.y * ratio / 66) + offset.y + 1 };
}

void
GaugeVarioAlt::MakePolygon(const int i) noexcept
{
  auto *bit = getPolygon(i);
  auto *bline = &lines[i + gmax];
  auto *bline1 = &lines1[i + gmax];

  const FastIntegerRotation r(Angle::Degrees(i));

  bit[0] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0, geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit[1] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength1, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit[2] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0, -geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);

  *bline = TransformRotatedPoint(r.Rotate({-geometry.offset.x, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  *bline1 = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0 - geometry.nlength1 + Layout::Scale(3), 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
}

void
GaugeVarioAlt::MakeAvePolygon(const int i) noexcept
{
  auto *bit = getAvePolygon(i);

  const FastIntegerRotation r(Angle::Degrees(i));
  int mid = (geometry.nlength0 - geometry.nlength1) / 2 + geometry.nlength1;

  bit[0] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + mid, geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit[1] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength1, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit[2] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + mid, -geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit[3] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);

  auto *bit_th = getThAvePolygon(i);
  bit_th[0] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit_th[1] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength1, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit_th[2] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength1, -geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
  bit_th[3] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength1, geometry.nwidth}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
}

inline BulkPixelPoint *
GaugeVarioAlt::getPolygon(int i) noexcept
{
  return polys + (i + gmax) * 3;
}

inline BulkPixelPoint *
GaugeVarioAlt::getAvePolygon(int i) noexcept
{
  return ave_polys + (i + gmax) * 4;
}

inline BulkPixelPoint *
GaugeVarioAlt::getThAvePolygon(int i) noexcept
{
  return th_ave_polys + (i + gmax) * 4;
}

inline void
GaugeVarioAlt::MakeAllPolygons() noexcept
{
  for (int i = gmin; i <= gmax; i++)
  {
    MakePolygon(i);
    MakeAvePolygon(i);
  }
}

inline void
GaugeVarioAlt::RenderBase(Canvas &canvas, PixelRect rc) noexcept
{
  canvas.DrawFilledRectangle(rc, look.background_color);
  canvas.SetBackgroundColor(look.background_color);

  // The ring
  canvas.Select(look.markings_pen);
  canvas.DrawPolyline(lines, ARRAY_SIZE(lines));
  canvas.DrawPolyline(lines1, ARRAY_SIZE(lines1));

  // The marks on the ring
  double range = GAUGEVARIORANGE[(int)GetComputerSettings().vario_range];

  canvas.Select(look.border_pen);
  for (int i = -4; i <= 4; i++)
  {
    /*  Markings need to be in whole units that are close to whole m/s
        To do that, convert the m/s to user units and round, then convert
        back to system units (m/s) not rounded and then get the arc position
        for that. This results in accurate placement of the marks */
    const double internal = range * i / 5;
    const int val = iround(Units::ToUserVSpeed(internal));
    const double act = Units::ToSysVSpeed(val);
    const int pos = ValueToNeedlePos(act);
    const FastIntegerRotation r(Angle::Degrees(pos));

    BulkPixelPoint dash[2];
    dash[0] = TransformRotatedPoint(r.Rotate({-geometry.offset.x, 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);
    dash[1] = TransformRotatedPoint(r.Rotate({-geometry.offset.x + geometry.nlength0 - geometry.nlength1 + Layout::Scale(3), 0}),
                                 geometry.offset, 
                                 geometry.v_width * 100 / geometry.v_height);

    canvas.DrawPolyline(dash, ARRAY_SIZE(dash));

    // Render the numbers
    if (look.num_info_box > 0)
    {
      TCHAR buffer[3];
      int disp = abs(val);
      if (Units::GetUserVerticalSpeedUnit() == Unit::FEET_PER_MINUTE)
        disp = (disp + 50) / 100;
      _stprintf(buffer, _T("%d"), disp);
      canvas.Select(look.corner_font);
      PixelSize ts = canvas.CalcTextSize(buffer);
      PixelPoint local_offset = geometry.offset;
      local_offset.x -= ts.width / 2;
      local_offset.y -= ts.height / 2;

      PixelPoint pos = TransformRotatedPoint(r.Rotate({-geometry.offset.x + (geometry.nlength0 - geometry.nlength1 + Layout::Scale(3)) / 2, 0}),
                                 local_offset, 
                                 geometry.v_width * 100 / geometry.v_height);

      canvas.DrawText(pos, buffer);
    }
  }

  if (look.num_info_box > 0)
  {
    // Render the scale
    const FastIntegerRotation r(Angle::Degrees(90));
    auto *scale = Units::GetUnitName(Units::GetUserVerticalSpeedUnit());
    canvas.Select(look.corner_font);
    PixelSize ts = canvas.CalcTextSize(scale);
    PixelPoint local_offset = geometry.offset;
    local_offset.x -= (ts.width + Layout::Scale(2));
    local_offset.y -= ts.height / 2;

    PixelPoint pos = TransformRotatedPoint(r.Rotate({-geometry.offset.x + (geometry.nlength0 - geometry.nlength1 + Layout::Scale(3)) / 2, 0}),
                                local_offset, 
                                geometry.v_width * 100 / geometry.v_height);

    canvas.DrawText(pos, scale);
  }
}

int
GaugeVarioAlt::ValueToNeedlePos(double Value) noexcept
{
  double degrees_per_unit =
    double(GAUGEVARIOSWEEP) / GAUGEVARIORANGE[(int)GetComputerSettings().vario_range];

  int i;

  if (!needle_initialised){
    MakeAllPolygons();
    needle_initialised = true;
  }

  i = iround(Value * degrees_per_unit);
  i = Clamp(i, int(gmin), int(gmax));
  return i;
}

void
GaugeVarioAlt::RenderNeedles(Canvas &canvas, int var, int avg, int th) noexcept
{
  if (Settings().show_average_needle)
  {
    canvas.Select(look.ave_brush);
    canvas.Select(look.ave_pen);
    canvas.DrawPolygon(getAvePolygon(Clamp(avg, int(gmin) + 2, int(gmax) - 2)), 4);
  }

  if (Settings().show_thermal_average_needle)
  {
    canvas.Select(look.th_ave_pen);
    canvas.DrawPolyline(getThAvePolygon(Clamp(th, int(gmin) + 2, int(gmax) - 2)), 4);
  }

  canvas.SelectNullPen();
  // legacy behaviour
  if (look.inverse) {
    canvas.SelectWhiteBrush();
    canvas.SelectWhitePen();
  } else {
    canvas.SelectBlackBrush();
    canvas.SelectBlackPen();
  }
  canvas.DrawPolygon(getPolygon(Clamp(var, int(gmin) + 2, int(gmax) - 2)), 3);
}

// TODO code: Optimise vario rendering, this is slow
void
GaugeVarioAlt::RenderValue(Canvas &canvas, const LabelValueGeometry &g,
                        LabelValueDrawInfo &di,
                        double value, const TCHAR *label, 
                        bool frac, int infinity) noexcept
{
  value = (double)iround(value * 10) / 10; // prevent the -0.0 case
  bool is_infinite = false;
  bool is_climb = false;
  if (infinity > 0) {
    is_infinite = abs(value) >= infinity;
    is_climb = value < 0;
  }

  TCHAR buffer[18];
  canvas.SetBackgroundColor(look.background_color);
  canvas.SetTextColor(look.text_color);
  if (is_infinite)
    _stprintf(buffer, _T("∞ "));
  else if (is_climb)
    _stprintf(buffer, _T("+++"));
  else
    _stprintf(buffer, frac ? _T("%.1f") : _T("%.0f"), (double)value);
  canvas.Select(look.value_font);
  unsigned width = canvas.CalcTextSize(buffer).width;
  PixelPoint text_position{g.value_right - (int)width, g.value_y};
  canvas.DrawText(text_position, buffer);

  canvas.SetTextColor(look.dimmed_text_color);
  canvas.Select(look.text_font);
  width = canvas.CalcTextSize(label).width;
  text_position = {g.label_right - (int)width, g.label_y};
  canvas.DrawText(text_position, label);
}


inline void
GaugeVarioAlt::RenderBallast(Canvas &canvas) noexcept
{
  int ballast = iround(GetGlidePolar().GetWingLoading());

  canvas.Select(look.corner_font);
  canvas.SetBackgroundColor(look.background_color);
  
  const auto &g = geometry.ballast;

  // new ballast 0, hide value
  TCHAR buffer[18];
  _stprintf(buffer, _T("%u kg/m2"), ballast);
  canvas.SetTextColor(look.text_color);
  canvas.DrawText(g.value_pos, buffer);
}

inline void
GaugeVarioAlt::RenderBugs(Canvas &canvas) noexcept
{
  int bugs = iround((1 - GetComputerSettings().polar.bugs) * 100);

  canvas.Select(look.corner_font);
  canvas.SetBackgroundColor(look.background_color);
  const auto &g = geometry.bugs;

  // new ballast 0, hide value
  TCHAR buffer[5];
  _stprintf(buffer, _T("%u%%"), bugs);
  canvas.SetTextColor(look.text_color);
  canvas.DrawText(g.value_pos, buffer);

  canvas.Select(look.text_font);
  canvas.SetTextColor(look.dimmed_text_color);
  canvas.DrawText(g.label_pos, TEXT_BUG);


}

void
GaugeVarioAlt::OnResize(PixelSize new_size)
{
  AntiFlickerWindow::OnResize(new_size);

  geometry = {look, GetClientRect()};

  /* trigger reinitialisation */
  needle_initialised = false;

  value_a.Reset();
  value_b.Reset();
  value_c.Reset();
  value_d.Reset();
  value_e.Reset();
}
