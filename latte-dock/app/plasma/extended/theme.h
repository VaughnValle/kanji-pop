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

#ifndef PLASMATHEMEEXTENDED_H
#define PLASMATHEMEEXTENDED_H

// C++
#include <array>

// Qt
#include <QObject>
#include <QTemporaryDir>

// KDE
#include <KConfigGroup>
#include <KSharedConfig>

// Plasma
#include <Plasma/FrameSvg>
#include <Plasma/Theme>

namespace Latte {
class Corona;
namespace WindowSystem {
class SchemeColors;
}
}

namespace Latte {
namespace PlasmaExtended {

class Theme: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasShadow READ hasShadow NOTIFY hasShadowChanged)
    Q_PROPERTY(bool isLightTheme READ isLightTheme NOTIFY themeChanged)
    Q_PROPERTY(bool isDarkTheme READ isDarkTheme NOTIFY themeChanged)

    Q_PROPERTY(int bottomEdgeRoundness READ bottomEdgeRoundness NOTIFY roundnessChanged)
    Q_PROPERTY(int leftEdgeRoundness READ leftEdgeRoundness NOTIFY roundnessChanged)
    Q_PROPERTY(int topEdgeRoundness READ topEdgeRoundness NOTIFY roundnessChanged)
    Q_PROPERTY(int rightEdgeRoundness READ rightEdgeRoundness NOTIFY roundnessChanged)

    Q_PROPERTY(int outlineWidth READ outlineWidth NOTIFY outlineWidthChanged)

    Q_PROPERTY(float bottomEdgeMaxOpacity READ bottomEdgeMaxOpacity NOTIFY maxOpacityChanged)
    Q_PROPERTY(float leftEdgeMaxOpacity READ leftEdgeMaxOpacity NOTIFY maxOpacityChanged)
    Q_PROPERTY(float topEdgeMaxOpacity READ topEdgeMaxOpacity NOTIFY maxOpacityChanged)
    Q_PROPERTY(float rightEdgeMaxOpacity READ rightEdgeMaxOpacity NOTIFY maxOpacityChanged)

    Q_PROPERTY(Latte::WindowSystem::SchemeColors *defaultTheme READ defaultTheme NOTIFY themeChanged)
    Q_PROPERTY(Latte::WindowSystem::SchemeColors *lightTheme READ lightTheme NOTIFY themeChanged)
    Q_PROPERTY(Latte::WindowSystem::SchemeColors *darkTheme READ darkTheme NOTIFY themeChanged)

public:
    Theme(KSharedConfig::Ptr config, QObject *parent);
    ~Theme() override;;

    bool hasShadow() const;
    bool isLightTheme() const;
    bool isDarkTheme() const;

    int bottomEdgeRoundness() const;
    int leftEdgeRoundness() const;
    int topEdgeRoundness() const;
    int rightEdgeRoundness() const;

    int outlineWidth() const;
    void setOutlineWidth(int width);

    float bottomEdgeMaxOpacity() const;
    float leftEdgeMaxOpacity() const;
    float topEdgeMaxOpacity() const;
    float rightEdgeMaxOpacity() const;

    WindowSystem::SchemeColors *defaultTheme() const;
    WindowSystem::SchemeColors *lightTheme() const;
    WindowSystem::SchemeColors *darkTheme() const;

    void load();

signals:
    void compositingChanged();
    void hasShadowChanged();
    void maxOpacityChanged();
    void outlineWidthChanged();
    void roundnessChanged();
    void themeChanged();

private slots:
    void loadConfig();
    void saveConfig();
    void loadThemeLightness();

private:
    void loadThemePaths();
    void loadRoundness();
    void loadCompositingRoundness();

    void setOriginalSchemeFile(const QString &file);
    void parseThemeSvgFiles();
    void updateDefaultScheme();
    void updateDefaultSchemeValues();
    void updateReversedScheme();
    void updateReversedSchemeValues();

    int roundness(const QImage &svgImage, Plasma::Types::Location edge);

private:
    bool m_isLightTheme{false};
    bool m_compositing{true};

    int m_bottomEdgeRoundness{0};
    int m_leftEdgeRoundness{0};
    int m_topEdgeRoundness{0};
    int m_rightEdgeRoundness{0};

    int m_outlineWidth{1};

    float m_bottomEdgeMaxOpacity{1};
    float m_leftEdgeMaxOpacity{1};
    float m_topEdgeMaxOpacity{1};
    float m_rightEdgeMaxOpacity{1};

    QString m_themePath;
    QString m_themeWidgetsPath;
    QString m_defaultSchemePath;
    QString m_originalSchemePath;
    QString m_reversedSchemePath;

    std::array<QMetaObject::Connection, 2> m_kdeConnections;

    QTemporaryDir m_extendedThemeDir;
    KConfigGroup m_themeGroup;
    Plasma::Theme m_theme;

    Latte::Corona *m_corona{nullptr};
    WindowSystem::SchemeColors *m_defaultScheme{nullptr};
    WindowSystem::SchemeColors *m_reversedScheme{nullptr};
};

}
}

#endif
