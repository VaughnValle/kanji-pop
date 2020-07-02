/*
 * Copyright 2017  Smith AR <audoban@openmailbox.org>
 *                 Michail Vourlakos <mvourlakos@gmail.com>
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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

// local
#include <coretypes.h>
#include "genericdialog.h"
#include "../controllers/layoutscontroller.h"
#include "../handlers/tablayoutshandler.h"
#include "../handlers/tabpreferenceshandler.h"

// Qt
#include <QObject>
#include <QButtonGroup>
#include <QDialog>
#include <QDebug>
#include <QMenuBar>
#include <QStandardItemModel>
#include <QTimer>

// KDE
#include <KHelpMenu>

namespace Ui {
class SettingsDialog;
}

namespace KActivities {
class Controller;
}

namespace Latte {
class Corona;
class CentralLayout;
}

namespace Latte {
namespace Settings {
namespace Dialog {

enum ConfigurationPage
{
    LayoutPage = 0,
    PreferencesPage
};

class SettingsDialog : public GenericDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent, Latte::Corona *corona);
    ~SettingsDialog();

    Latte::Corona *corona() const;
    Ui::SettingsDialog *ui() const;

    QMenuBar *appMenuBar() const;
    QMenu *fileMenu() const;
    QMenu *helpMenu() const;

    void setStoredWindowSize(const QSize &size);

    QSize downloadWindowSize() const;
    void setDownloadWindowSize(const QSize &size);

    ConfigurationPage currentPage();
    void setCurrentPage(int page);
    void toggleCurrentPage();

    void requestImagesDialog(int row);
    void requestColorsDialog(int row);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void on_import_fullconfiguration();
    void on_export_fullconfiguration();

    void accept() override;
    void reject() override;

    void apply();
    void reset();
    void restoreDefaults();
    void showLayoutInformation();
    void showScreensInformation();
    void updateApplyButtonsState();
    void updateWindowActivities();    

    void loadConfig();
    void saveConfig();

    void on_currentTabChanged(int index);

private:
    void initGlobalMenu();
    void initLayoutMenu();
    void initFileMenu();
    void initHelpMenu();

    void save();
    void setCurrentFreeActivitiesLayout(const int &row);

    int saveChangesConfirmation();
    bool saveChanges();

    QSize storedWindowSize() const;

private:
    Latte::Corona *m_corona{nullptr};
    Ui::SettingsDialog *m_ui;

    //! Handlers for UI
    Settings::Handler::TabLayouts *m_tabLayoutsHandler{nullptr};
    Settings::Handler::TabPreferences *m_tabPreferencesHandler{nullptr};

    //! properties
    QSize m_windowSize;
    QSize m_downloadWindowSize;

    //! Global menu
    QMenuBar *m_globalMenuBar{nullptr};

    //! are used for confirmation purposes, the user can choose to save or discard settings and
    //! change its current tab also
    int m_acceptedPage{-1};
    int m_nextPage{-1};

    //! File menu actions
    QMenu *m_fileMenu{nullptr};
    QAction *m_importFullAction{nullptr};
    QAction *m_exportFullAction{nullptr};

    //! Help menu actions
    KHelpMenu *m_helpMenu{nullptr};

    //! storage
    KConfigGroup m_deprecatedStorage;
    KConfigGroup m_storage;

    //! workaround to assign ALLACTIVITIES during startup
    QTimer m_activitiesTimer;
};

}
}
}

#endif // SETTINGSDIALOG_H
