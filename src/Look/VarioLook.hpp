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

#ifndef XCSOAR_VARIO_LOOK_HPP
#define XCSOAR_VARIO_LOOK_HPP

#include "ui/canvas/Color.hpp"
#include "ui/canvas/Brush.hpp"
#include "ui/canvas/Pen.hpp"
#include "ui/canvas/Bitmap.hpp"
#include "ui/canvas/Font.hpp"
#include "ui/canvas/Canvas.hpp"

class Font;

struct VarioLook {
  bool inverse, colors;

  Color background_color, text_color, dimmed_text_color;

  Color sink_color, lift_color;

  Brush sink_brush, lift_brush, ave_brush;

  Pen thick_background_pen, thick_sink_pen, thick_lift_pen, ave_pen, th_ave_pen;

  Pen markings_pen, border_pen;

  Font text_font, value_font, corner_font;

  bool fonts_valid;
  PixelRect info_box, old_rc;
  unsigned num_info_box, info_height;

  void Initialise(bool inverse, bool colors,
                  const Font &text_font);

  void Resize(Canvas &canvas, PixelRect rc);
  bool HasChanged(PixelRect rc);
};

#endif
