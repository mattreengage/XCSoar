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

#ifndef GAUGE_VARIO_H
#define GAUGE_VARIO_H

#include "ui/window/AntiFlickerWindow.hpp"
#include "ui/dim/BulkPoint.hpp"
#include "Blackboard/FullBlackboard.hpp"
#include "Math/Point2D.hpp"

struct VarioLook;
class ContainerWindow;

class GaugeVario : public AntiFlickerWindow
{
  static constexpr unsigned NARROWS = 3;
  static constexpr int YOFFSET = 36;

  /** 5 m/s */
  static constexpr int GAUGEVARIORANGE = 5;

  /** degrees total sweep */
  static constexpr int GAUGEVARIOSWEEP = 90;

  /** Max should be less than total to keep needles visible at all times */
  static constexpr int gmax = GAUGEVARIOSWEEP;
  static constexpr int gmin = -gmax;

  struct BallastGeometry {
    PixelPoint value_pos;

    BallastGeometry() = default;
    BallastGeometry( VarioLook &look, const PixelRect &rc) noexcept;
  };

  struct BugsGeometry {
    PixelPoint value_pos, label_pos;

    BugsGeometry() = default;
    BugsGeometry( VarioLook &look, const PixelRect &rc) noexcept;
  };

  struct LabelValueGeometry {
    int label_right, label_top, label_bottom, label_y;
    int value_right, value_top, value_bottom, value_y;

    LabelValueGeometry() = default;
    LabelValueGeometry( VarioLook &look, PixelPoint position) noexcept;

    static unsigned GetHeight( VarioLook &look) noexcept;
  };

  struct Geometry {
    int nlength0, nlength1, nwidth, nline;
    int v_width, v_height;

    PixelPoint offset;

    unsigned num_values;

    LabelValueGeometry value_a_pos, value_b_pos, value_c_pos, value_d_pos, value_e_pos;

    BallastGeometry ballast;
    BugsGeometry bugs;

    Geometry() = default;
    Geometry( VarioLook &look, const PixelRect &rc) noexcept;
  } geometry;

  struct DrawInfo {
    unsigned last_width;
    double last_value;
    TCHAR last_text[32];
    Unit last_unit;

    void Reset() noexcept {
      last_width = 0;
      last_value = -9999;
      last_text[0] = '\0';
      last_unit = Unit::UNDEFINED;
    }
  };

  struct LabelValueDrawInfo {
    DrawInfo label;
    DrawInfo value;

    void Reset() noexcept {
      label.Reset();
      value.Reset();
    }
  };

  const FullBlackboard &blackboard;

  VarioLook &look;

  bool needle_initialised = false;

  LabelValueDrawInfo value_a, value_b, value_c, value_d, value_e;

  BulkPixelPoint polys[(gmax * 2 + 1) * 3];
  BulkPixelPoint ave_polys[(gmax * 2 + 1) * 4];
  BulkPixelPoint th_ave_polys[(gmax * 2 + 1) * 4];
  BulkPixelPoint lines[gmax * 2 + 1], lines1[gmax * 2 + 1];

public:
  GaugeVario(const FullBlackboard &blackboard,
             ContainerWindow &parent, VarioLook &look,
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
  void RenderBase(Canvas &canvas, PixelRect rc) noexcept;
  void RenderValue(Canvas &canvas, const LabelValueGeometry &g,
                   LabelValueDrawInfo &di,
                   double Value, const TCHAR *Label, 
                   bool frac, int infinity) noexcept;
  void RenderBallast(Canvas &canvas) noexcept;
  void RenderBugs(Canvas &canvas) noexcept;
  int  ValueToNeedlePos(double Value) noexcept;
  void RenderNeedles(Canvas &canvas, int var, int avg, int th) noexcept;

  void MakePolygon(const int i) noexcept;
  void MakeAvePolygon(const int i) noexcept;
  void MakeAllPolygons() noexcept;
  BulkPixelPoint *getPolygon(const int i) noexcept;
  BulkPixelPoint *getAvePolygon(const int i) noexcept;
  BulkPixelPoint *getThAvePolygon(const int i) noexcept;
};

#endif
