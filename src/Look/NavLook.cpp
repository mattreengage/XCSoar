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

#include "NavLook.hpp"
#include "FontDescription.hpp"
#include "Screen/Layout.hpp"
#include "Units/Units.hpp"
#include "Resources.hpp"

#include <algorithm>

void
NavLook::Initialise(bool _inverse, bool _colors,
                      const Font &_text_font)
{
  inverse = _inverse;
  colors = _colors;

  if (inverse) {
    background_color = COLOR_BLACK;
    text_color = COLOR_WHITE;
  } else {
    background_color = COLOR_WHITE;
    text_color = COLOR_BLACK;
  }

  goal_brush.Create(Color(0x20, 0x20, 0xff));
  track_brush.Create(text_color);

  goal_pen.Create(Layout::Scale(1), Color(0x20, 0x20, 0xff));
  track_pen.Create(3, text_color);

  border_pen.Create(1, text_color);

  fonts_valid = false;
}

void
NavLook::Resize(Canvas &canvas, PixelRect rc)
{
  const unsigned height = rc.GetHeight() - 9;
  text_font.Load(FontDescription(height * 2 / 3, true, false, false));

  // First attempt is to make error text the full height available
  error_font.Load(FontDescription(height, false, false, false));

  // If the text is wider than available, scale down accordingly
  canvas.Select(error_font);
  PixelSize text_size = canvas.CalcTextSize(no_target_msg);
  if (text_size.width > rc.GetWidth())
    error_font.Load(FontDescription(height * rc.GetWidth() / text_size.width, false, false, false));

  middle = rc.GetWidth() / 2 + rc.left;
  fonts_valid = true;
  old_rc = rc;
}

bool
NavLook::HasChanged(PixelRect rc)
{
  return (rc.right != old_rc.right || 
    rc.bottom !=old_rc.bottom ||
    rc.top != old_rc.top ||
    rc.left != old_rc.left);
}
