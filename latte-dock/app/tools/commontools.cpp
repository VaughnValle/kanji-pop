/*
 * Copyright 2018  Michail Vourlakos <mvourlakos@gmail.com>
 *
 * This file is part of Latte-Dock
 *
 * Latte-Dock is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Latte-Dock is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "commontools.h"

// Qt
#include <QFileInfo>
#include <QStandardPaths>
#include <QtMath>

namespace Latte {

float colorBrightness(QColor color)
{
    return colorBrightness(color.red(), color.green(), color.blue());
}

float colorBrightness(QRgb rgb)
{
    return colorBrightness(qRed(rgb), qGreen(rgb), qBlue(rgb));
}

float colorBrightness(float r, float g, float b)
{
    float brightness = (r * 299 + g * 587 + b * 114) / 1000;

    return brightness;
}


float colorLumina(QRgb rgb)
{
    float r = (float)(qRed(rgb)) / 255;
    float g = (float)(qGreen(rgb)) / 255;
    float b = (float)(qBlue(rgb)) / 255;

    return colorLumina(r, g, b);
}

float colorLumina(QColor color)
{
    return colorLumina(color.redF(), color.greenF(), color.blueF());
}

float colorLumina(float r, float g, float b)
{
    // formula for luminance according to:
    // https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef

    float rS = (r <= 0.03928 ? r / 12.92 : qPow(((r + 0.055) / 1.055), 2.4));
    float gS = (g <= 0.03928 ? g / 12.92 : qPow(((g + 0.055) / 1.055), 2.4));
    float bS = (b <= 0.03928 ? b / 12.92 : qPow(((b + 0.055) / 1.055), 2.4));

    float luminosity = 0.2126 * rS + 0.7152 * gS + 0.0722 * bS;

    return luminosity;
}

QString standardPath(QString subPath, bool localfirst)
{
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    if (localfirst) {
        for (const auto &pt : paths) {
            QString ptF = pt + "/" +subPath;
            if (QFileInfo(ptF).exists()) {
                return ptF;
            }
        }
    } else {
        for (int i=paths.count()-1; i>=0; i--) {
            QString ptF = paths[i] + "/" +subPath;
            if (QFileInfo(ptF).exists()) {
                return ptF;
            }
        }
    }

    //! in any case that above fails
    if (QFileInfo("/usr/share/"+subPath).exists()) {
        return "/usr/share/"+subPath;
    }

    return "";
}

}
