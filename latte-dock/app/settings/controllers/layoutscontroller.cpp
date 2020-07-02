/*
 * Copyright 2020  Michail Vourlakos <mvourlakos@gmail.com>
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

#include "layoutscontroller.h"

// local
#include "ui_settingsdialog.h"
#include "../universalsettings.h"
#include "../dialogs/settingsdialog.h"
#include "../data/uniqueidinfo.h"
#include "../delegates/activitiesdelegate.h"
#include "../delegates/backgroundcmbdelegate.h"
#include "../delegates/checkboxdelegate.h"
#include "../delegates/layoutnamedelegate.h"
#include "../delegates/shareddelegate.h"
#include "../handlers/tablayoutshandler.h"
#include "../tools/settingstools.h"
#include "../../layout/genericlayout.h"
#include "../../layout/centrallayout.h"
#include "../../layout/sharedlayout.h"
#include "../../layouts/importer.h"
#include "../../layouts/manager.h"
#include "../../layouts/synchronizer.h"

// Qt
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QItemSelection>
#include <QStringList>
#include <QTemporaryDir>

// KDE
#include <KArchive/KTar>
#include <KArchive/KArchiveEntry>
#include <KArchive/KArchiveDirectory>
#include <KMessageWidget>

namespace Latte {
namespace Settings {
namespace Controller {

Layouts::Layouts(Settings::Handler::TabLayouts *parent)
    : QObject(parent),
      m_handler(parent),
      m_model(new Model::Layouts(this, m_handler->corona())),
      m_proxyModel(new QSortFilterProxyModel(this)),
      m_view(m_handler->ui()->layoutsView),
      m_headerView(new Settings::Layouts::HeaderView(Qt::Horizontal, m_handler->dialog())),
      m_storage(KConfigGroup(KSharedConfig::openConfig(),"LatteSettingsDialog").group("TabLayouts"))
{   
    loadConfig();
    m_proxyModel->setSourceModel(m_model);

    connect(m_model, &Model::Layouts::inMultipleModeChanged, this, &Layouts::updateLastColumnWidth);

    connect(m_model, &QAbstractItemModel::dataChanged, this, &Layouts::dataChanged);
    connect(m_model, &Model::Layouts::rowsInserted, this, &Layouts::dataChanged);
    connect(m_model, &Model::Layouts::rowsRemoved, this, &Layouts::dataChanged);

    connect(m_model, &Model::Layouts::nameDuplicated, this, &Layouts::on_nameDuplicatedFrom);

    connect(m_headerView, &QObject::destroyed, this, [&]() {
        m_viewSortColumn = m_headerView->sortIndicatorSection();
        m_viewSortOrder = m_headerView->sortIndicatorOrder();
    });

    initView();
    loadLayouts();
}

Layouts::~Layouts()
{
    saveConfig();

    //! remove
    qDeleteAll(m_layouts);

    for (const auto &tempDir : m_tempDirectories) {
        QDir tDir(tempDir);

        if (tDir.exists() && tempDir.startsWith("/tmp/")) {
            tDir.removeRecursively();
        }
    }
}

QAbstractItemModel *Layouts::proxyModel() const
{
    return m_proxyModel;
}

QAbstractItemModel *Layouts::baseModel() const
{
    return m_model;
}

QTableView *Layouts::view() const
{
    return m_view;
}

void Layouts::initView()
{
    m_view->setModel(m_proxyModel);
    m_view->setHorizontalHeader(m_headerView);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSortingEnabled(true);

    m_proxyModel->setSortRole(Model::Layouts::SORTINGROLE);
    m_proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

    m_view->sortByColumn(m_viewSortColumn, m_viewSortOrder);

    //!find the available colors
    m_iconsPath = m_handler->corona()->kPackage().path() + "../../plasmoids/org.kde.latte.containment/contents/icons/";
    m_model->setIconsPath(m_iconsPath);

    QDir layoutDir(m_iconsPath);
    QStringList filter;
    filter.append(QString("*print.jpg"));
    QStringList files = layoutDir.entryList(filter, QDir::Files | QDir::NoSymLinks);
    QStringList colors;

    for (auto &file : files) {
        int colorEnd = file.lastIndexOf("print.jpg");
        QString color = file.remove(colorEnd, 9);
        colors.append(color);
    }

    m_view->setItemDelegateForColumn(Model::Layouts::NAMECOLUMN, new Settings::Layout::Delegate::LayoutName(this));
    m_view->setItemDelegateForColumn(Model::Layouts::BACKGROUNDCOLUMN, new Settings::Layout::Delegate::BackgroundCmbBox(this, m_iconsPath, colors));
    m_view->setItemDelegateForColumn(Model::Layouts::MENUCOLUMN, new Settings::Layout::Delegate::CheckBox(this));
    m_view->setItemDelegateForColumn(Model::Layouts::BORDERSCOLUMN, new Settings::Layout::Delegate::CheckBox(this));
    m_view->setItemDelegateForColumn(Model::Layouts::ACTIVITYCOLUMN, new Settings::Layout::Delegate::Activities(this));
    m_view->setItemDelegateForColumn(Model::Layouts::SHAREDCOLUMN, new Settings::Layout::Delegate::Shared(this));

    connect(m_view, &QObject::destroyed, this, &Controller::Layouts::storeColumnWidths);
}

bool Layouts::dataAreChanged() const
{
    return m_model->dataAreChanged();
}

bool Layouts::hasSelectedLayout() const
{
    int selectedRow = m_view->currentIndex().row();

    return (selectedRow >= 0);
}

bool Layouts::selectedLayoutIsCurrentActive() const
{
    Settings::Data::Layout selectedLayoutCurrent = selectedLayoutCurrentData();
    Settings::Data::Layout selectedLayoutOriginal = selectedLayoutOriginalData();
    selectedLayoutOriginal = selectedLayoutOriginal.isEmpty() ? selectedLayoutCurrent : selectedLayoutOriginal;

    return (selectedLayoutCurrent.isActive && (selectedLayoutOriginal.name == m_handler->corona()->layoutsManager()->currentLayoutName()));
}

QString Layouts::colorPath(const QString color) const
{
    QString path = m_iconsPath + color + "print.jpg";

    if (!QFileInfo(path).exists()) {
        return m_iconsPath + "blueprint.jpg";
    }

    return path;
}

QString Layouts::iconsPath() const
{
    return m_iconsPath;
}

const Data::Layout Layouts::selectedLayoutCurrentData() const
{
    int selectedRow = m_view->currentIndex().row();
    if (selectedRow >= 0) {
        QString selectedId = m_proxyModel->data(m_proxyModel->index(selectedRow, Model::Layouts::IDCOLUMN), Qt::UserRole).toString();

        return m_model->currentData(selectedId);
    } else {
        return Data::Layout();
    }
}

const Data::Layout Layouts::selectedLayoutOriginalData() const
{
    int selectedRow = m_view->currentIndex().row();
    QString selectedId = m_proxyModel->data(m_proxyModel->index(selectedRow, Model::Layouts::IDCOLUMN), Qt::UserRole).toString();

    return m_model->originalData(selectedId);;
}

bool Layouts::inMultipleMode() const
{
    return m_model->inMultipleMode();
}

void Layouts::setInMultipleMode(bool inMultiple)
{
    m_model->setInMultipleMode(inMultiple);
}

void Layouts::updateLastColumnWidth()
{
    if (m_model->inMultipleMode()) {
        m_view->setColumnHidden(Model::Layouts::SHAREDCOLUMN, false);

        //! column widths
        if (m_viewColumnWidths.count()>=5) {
            m_view->setColumnWidth(Model::Layouts::ACTIVITYCOLUMN, m_viewColumnWidths[4].toInt());
        }
    } else {
        m_view->setColumnHidden(Model::Layouts::SHAREDCOLUMN, true);
    }
}

int Layouts::rowForId(QString id) const
{
    for (int i = 0; i < m_proxyModel->rowCount(); ++i) {
        QString rowId = m_proxyModel->data(m_proxyModel->index(i, Model::Layouts::IDCOLUMN), Qt::UserRole).toString();

        if (rowId == id) {
            return i;
        }
    }

    return -1;
}

int Layouts::rowForName(QString layoutName) const
{
    for (int i = 0; i < m_proxyModel->rowCount(); ++i) {
        QString rowName = m_proxyModel->data(m_proxyModel->index(i, Model::Layouts::NAMECOLUMN), Qt::UserRole).toString();

        if (rowName == layoutName) {
            return i;
        }
    }

    return -1;
}

QString Layouts::uniqueTempDirectory()
{
    QTemporaryDir tempDir;
    tempDir.setAutoRemove(false);
    m_tempDirectories.append(tempDir.path());

    return tempDir.path();
}

QString Layouts::uniqueLayoutName(QString name)
{
    int pos_ = name.lastIndexOf(QRegExp(QString(" - [0-9]+")));

    if (m_model->containsCurrentName(name) && pos_ > 0) {
        name = name.left(pos_);
    }

    int i = 2;

    QString namePart = name;

    while (m_model->containsCurrentName(name)) {
        name = namePart + " - " + QString::number(i);
        i++;
    }

    return name;
}

void Layouts::removeSelected()
{
    if (m_view->currentIndex().row() < 0) {
        return;
    }

    Data::Layout selectedOriginal = selectedLayoutOriginalData();

    if (m_handler->corona()->layoutsManager()->synchronizer()->layout(selectedOriginal.name)) {
        return;
    }

    int row = m_view->currentIndex().row();
    row = qMin(row, m_proxyModel->rowCount() - 1);
    m_view->selectRow(row);

    Data::Layout selected = selectedLayoutCurrentData();
    m_model->removeLayout(selected.id);
}

void Layouts::toggleLockedForSelected()
{
    if (!hasSelectedLayout()) {
        return;
    }

    Data::Layout selected = selectedLayoutCurrentData();

    m_proxyModel->setData(m_proxyModel->index(m_view->currentIndex().row(), Model::Layouts::NAMECOLUMN), !selected.isLocked, Settings::Model::Layouts::ISLOCKEDROLE);
}

void Layouts::toggleSharedForSelected()
{  
    if (!hasSelectedLayout()) {
        return;
    }

    int row = m_view->currentIndex().row();

    Data::Layout selected = selectedLayoutCurrentData();

    if (selected.isShared()) {
        m_proxyModel->setData(m_proxyModel->index(row, Model::Layouts::SHAREDCOLUMN), QStringList(), Qt::UserRole);
    } else {
        QStringList assignedIds;
        QStringList availableShareIds = m_model->availableShareIdsFor(selected.id);

        for (const auto &id : availableShareIds) {
            Data::Layout iLayoutCurrent = m_model->currentData(id);
            Data::Layout iLayoutOriginal = m_model->originalData(id);
            iLayoutOriginal = iLayoutOriginal.isEmpty() ? iLayoutCurrent : iLayoutOriginal;

            if (m_handler->corona()->layoutsManager()->synchronizer()->layout(iLayoutOriginal.name)) {
                assignedIds << id;
                m_proxyModel->setData(m_proxyModel->index(row, Model::Layouts::SHAREDCOLUMN), assignedIds, Qt::UserRole);
                break;
            }
        }

        if (assignedIds.isEmpty() && availableShareIds.count()>0) {
            assignedIds << availableShareIds[0];
            m_proxyModel->setData(m_proxyModel->index(row, Model::Layouts::SHAREDCOLUMN), assignedIds, Qt::UserRole);
        }
    }
}

void Layouts::selectRow(const QString &id)
{    
    m_view->selectRow(rowForId(id));
}

void Layouts::setLayoutProperties(const Data::Layout &layout)
{
    m_model->setLayoutProperties(layout);
}

QString Layouts::layoutNameForFreeActivities() const
{
    return m_model->layoutNameForFreeActivities();
}

void Layouts::setOriginalLayoutForFreeActivities(const QString &id)
{
    m_model->setOriginalLayoutForFreeActivities(id);
    emit dataChanged();
}

void Layouts::loadLayouts()
{
    m_model->clear();
    bool inMultiple{m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts};
    setInMultipleMode(inMultiple);

    //! The shares map needs to be constructed for start/scratch.
    //! We start feeding information with layout_names and during the process
    //! we update them to valid layout_ids
    Latte::Layouts::SharesMap sharesMap;

    int i = 0;
    QStringList brokenLayouts;

    if (m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts) {
        m_handler->corona()->layoutsManager()->synchronizer()->syncActiveLayoutsToOriginalFiles();
    }

    Settings::Data::LayoutsTable layoutsBuffer;

    for (const auto layout : m_handler->corona()->layoutsManager()->layouts()) {
        Settings::Data::Layout original;
        original.id = QDir::homePath() + "/.config/latte/" + layout + ".layout.latte";

        CentralLayout *central = new CentralLayout(this, original.id);

        original.name = central->name();
        original.backgroundStyle = central->backgroundStyle();
        original.color = central->color();
        original.background = central->customBackground();
        original.textColor = central->customTextColor();
        original.isActive = (m_handler->corona()->layoutsManager()->synchronizer()->layout(original.name) != nullptr);
        original.isLocked = !central->isWritable();
        original.isShownInMenu = central->showInMenu();
        original.hasDisabledBorders = central->disableBordersForMaximizedWindows();
        original.activities = central->activities();

        m_layouts[original.id] = central;

        //! create initial SHARES maps
        QString shared = central->sharedLayoutName();
        if (!shared.isEmpty()) {
            sharesMap[shared].append(original.id);
        }

        layoutsBuffer << original;

        qDebug() << "counter:" << i << " total:" << m_model->rowCount();

        i++;

        Latte::Layout::GenericLayout *generic = m_handler->corona()->layoutsManager()->synchronizer()->layout(central->name());

        if ((generic && generic->layoutIsBroken()) || (!generic && central->layoutIsBroken())) {
            brokenLayouts.append(central->name());
        }
    }

    //! update SHARES map keys in order to use the #settingsid(s)
    QStringList tempSharedNames;

    //! remove these records after updating
    for (QHash<const QString, QStringList>::iterator i=sharesMap.begin(); i!=sharesMap.end(); ++i) {
        tempSharedNames << i.key();
    }

    //! update keys
    for (QHash<const QString, QStringList>::iterator i=sharesMap.begin(); i!=sharesMap.end(); ++i) {
        QString shareid = layoutsBuffer.idForName(i.key());
        if (!shareid.isEmpty()) {
            sharesMap[shareid] = i.value();
        }
    }

    //! remove deprecated keys
    for (const auto &key : tempSharedNames) {
        sharesMap.remove(key);
    }

    qDebug() << "SHARES MAP ::: " << sharesMap;

    for (QHash<const QString, QStringList>::iterator i=sharesMap.begin(); i!=sharesMap.end(); ++i) {
        layoutsBuffer[i.key()].shares = i.value();
    }

    //! Send original loaded data to model
    m_model->setOriginalData(layoutsBuffer, inMultiple);
    m_model->setOriginalLayoutForFreeActivities(layoutsBuffer.idForName(m_handler->corona()->universalSettings()->lastNonAssignedLayoutName()));

    m_view->selectRow(rowForName(m_handler->corona()->layoutsManager()->currentLayoutName()));

    //! this line should be commented for debugging layouts window functionality
    m_view->setColumnHidden(Model::Layouts::IDCOLUMN, true);
    m_view->setColumnHidden(Model::Layouts::HIDDENTEXTCOLUMN, true);

    if (m_handler->corona()->universalSettings()->canDisableBorders()) {
        m_view->setColumnHidden(Model::Layouts::BORDERSCOLUMN, false);
    } else {
        m_view->setColumnHidden(Model::Layouts::BORDERSCOLUMN, true);
    }

    m_view->resizeColumnsToContents();


    if (!m_viewColumnWidths.isEmpty()) {
        int lastColumn = inMultiple ? 5 : 4;

        for (int i=0; i<qMin(m_viewColumnWidths.count(),lastColumn); ++i) {
            m_view->setColumnWidth(Model::Layouts::BACKGROUNDCOLUMN+i, m_viewColumnWidths[i].toInt());
        }
    }

    updateLastColumnWidth();

    //! there are broken layouts and the user must be informed!
    if (brokenLayouts.count() > 0) {
        if (brokenLayouts.count() == 1) {
            m_handler->showInlineMessage(i18nc("settings:broken layout", "Layout <b>%0</b> <i>is broken</i>! Please <b>remove</b> it to improve stability...").arg(brokenLayouts.join(", ")),
                                         KMessageWidget::Error,
                                         true);
        } else {
            m_handler->showInlineMessage(i18nc("settings:broken layouts", "Layouts <b>%0</b> <i>are broken</i>! Please <b>remove</b> them to improve stability...").arg(brokenLayouts.join(", ")),
                                         KMessageWidget::Error,
                                         true);
        }
    }
}

const Data::Layout Layouts::addLayoutForFile(QString file, QString layoutName, bool newTempDirectory)
{
    if (layoutName.isEmpty()) {
        layoutName = CentralLayout::layoutName(file);
    }

    layoutName = uniqueLayoutName(layoutName);

    Data::Layout copied;

    if (newTempDirectory) {
        copied.id = uniqueTempDirectory() + "/" + layoutName + ".layout.latte";
        QFile(file).copy(copied.id);
    } else {
        copied.id = file;
    }

    QFileInfo newFileInfo(copied.id);

    if (newFileInfo.exists() && !newFileInfo.isWritable()) {
        QFile(copied.id).setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    if (m_layouts.contains(copied.id)) {
        CentralLayout *oldSettings = m_layouts.take(copied.id);
        delete oldSettings;
    }

    CentralLayout *settings = new CentralLayout(this, copied.id);
    m_layouts[copied.id] = settings;

    copied.name = uniqueLayoutName(layoutName);
    copied.backgroundStyle = settings->backgroundStyle();
    copied.color = settings->color();
    copied.textColor = settings->customTextColor();
    copied.background = settings->customBackground();
    copied.isLocked = !settings->isWritable();
    copied.isShownInMenu = settings->showInMenu();
    copied.hasDisabledBorders = settings->disableBordersForMaximizedWindows();

    m_model->appendLayout(copied);

    m_view->selectRow(rowForId(copied.id));

    return copied;
}

void Layouts::copySelectedLayout()
{
    int row = m_view->currentIndex().row();

    if (row < 0) {
        return;
    }

    Settings::Data::Layout selectedLayoutCurrent = selectedLayoutCurrentData();
    Settings::Data::Layout selectedLayoutOriginal = selectedLayoutOriginalData();
    selectedLayoutOriginal = selectedLayoutOriginal.isEmpty() ? selectedLayoutCurrent : selectedLayoutOriginal;


    //! Update original layout before copying if this layout is active
    if (m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts) {
        Latte::Layout::GenericLayout *generic = m_handler->corona()->layoutsManager()->synchronizer()->layout(selectedLayoutOriginal.name);
        if (generic) {
            generic->syncToLayoutFile();
        }
    }

    Settings::Data::Layout copied = selectedLayoutCurrent;

    copied.name = uniqueLayoutName(selectedLayoutCurrent.name);
    copied.id = uniqueTempDirectory() + "/" + copied.name + ".layout.latte";;
    copied.isActive = false;
    copied.isLocked = false;
    copied.activities = QStringList();
    copied.shares = QStringList();

    QFile(selectedLayoutCurrent.id).copy(copied.id);
    QFileInfo newFileInfo(copied.id);

    if (newFileInfo.exists() && !newFileInfo.isWritable()) {
        QFile(copied.id).setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    CentralLayout *settings = new CentralLayout(this, copied.id);
    settings->clearLastUsedActivity();

    m_layouts[copied.id] = settings;
    m_model->appendLayout(copied);

    m_view->selectRow(rowForId(copied.id));
}

bool Layouts::importLayoutsFromV1ConfigFile(QString file)
{
    KTar archive(file, QStringLiteral("application/x-tar"));
    archive.open(QIODevice::ReadOnly);

    //! if the file isnt a tar archive
    if (archive.isOpen()) {
        QDir tempDir{uniqueTempDirectory()};

        const auto archiveRootDir = archive.directory();

        for (const auto &name : archiveRootDir->entries()) {
            auto fileEntry = archiveRootDir->file(name);
            fileEntry->copyTo(tempDir.absolutePath());
        }

        QString name = Latte::Layouts::Importer::nameOfConfigFile(file);

        QString applets(tempDir.absolutePath() + "/" + "lattedock-appletsrc");

        if (QFile(applets).exists()) {
            QStringList importedlayouts;

            if (m_handler->corona()->layoutsManager()->importer()->importOldLayout(applets, name, false, tempDir.absolutePath())) {
                Settings::Data::Layout imported = addLayoutForFile(tempDir.absolutePath() + "/" + name + ".layout.latte", name);
                importedlayouts << imported.name;
            }

            QString alternativeName = name + "-" + i18nc("layout", "Alternative");

            if (m_handler->corona()->layoutsManager()->importer()->importOldLayout(applets, alternativeName, false, tempDir.absolutePath())) {
                Settings::Data::Layout imported = addLayoutForFile(tempDir.absolutePath() + "/" + alternativeName + ".layout.latte", alternativeName, false);
                importedlayouts << imported.name;
            }

            if (importedlayouts.count() > 0) {
                if (importedlayouts.count() == 1) {
                    m_handler->showInlineMessage(i18n("Layout <b>%0</b> imported successfully...").arg(importedlayouts[0]),
                            KMessageWidget::Information);
                } else {
                    m_handler->showInlineMessage(i18n("Layouts <b>%0</b> imported successfully...").arg(importedlayouts.join(",")),
                                                 KMessageWidget::Information);
                }

                return true;
            }
        }
    }

    return false;
}

void Layouts::on_sharedToInEditChanged(const QString &id, const bool &inEdit)
{
    int row = m_model->rowForId(id);

    if (row >= 0) {
        m_model->setData(m_model->index(row, Model::Layouts::SHAREDCOLUMN), inEdit, Model::Layouts::SHAREDTOINEDITROLE);
    }
}

void Layouts::reset()
{
    m_model->resetData();
    m_view->selectRow(rowForName(m_handler->corona()->layoutsManager()->currentLayoutName()));
}

void Layouts::save()
{
    //! Update Layouts
    QStringList knownActivities = m_handler->corona()->layoutsManager()->synchronizer()->activities();

    QTemporaryDir layoutTempDir;

    qDebug() << "Temporary Directory ::: " << layoutTempDir.path();

    QString switchToLayout;

    QHash<QString, Latte::Layout::GenericLayout *> activeLayoutsToRename;

    Settings::Data::LayoutsTable originalLayouts = m_model->originalLayoutsData();
    Settings::Data::LayoutsTable currentLayouts = m_model->currentLayoutsData();
    Settings::Data::LayoutsTable removedLayouts = originalLayouts.subtracted(currentLayouts);

    //! remove layouts that have been removed from the user
    for (int i=0; i<removedLayouts.rowCount(); ++i) {
        QFile(removedLayouts[i].id).remove();

        if (m_layouts.contains(removedLayouts[i].id)) {
            CentralLayout *removedLayout = m_layouts.take(removedLayouts[i].id);
            delete removedLayout;
        }
    }

    QList<Data::UniqueIdInfo> alteredIdsInfo;

    QList<Data::Layout> alteredLayouts = m_model->alteredLayouts();

    for (int i = 0; i < alteredLayouts.count(); ++i) {
        Data::Layout iLayoutCurrentData = alteredLayouts[i];
        Data::Layout iLayoutOriginalData = m_model->originalData(iLayoutCurrentData.id);
        iLayoutOriginalData = iLayoutOriginalData.isEmpty() ? iLayoutCurrentData : iLayoutOriginalData;

        QStringList cleanedActivities;

        //!update only activities that are valid
        for (const auto &activity : iLayoutCurrentData.activities) {
            if (knownActivities.contains(activity) && activity != Settings::Data::Layout::FREEACTIVITIESID) {
                cleanedActivities.append(activity);
            }
        }

        //qDebug() << i << ". " << id << " - " << color << " - " << name << " - " << menu << " - " << lActivities;
        //! update the generic parts of the layouts
        bool isOriginalLayout = m_model->originalLayoutsData().containsId(iLayoutCurrentData.id);
        Latte::Layout::GenericLayout *genericActive= isOriginalLayout ? m_handler->corona()->layoutsManager()->synchronizer()->layout(iLayoutOriginalData.name) : nullptr;
        Latte::Layout::GenericLayout *generic = genericActive ? genericActive : m_layouts[iLayoutCurrentData.id];

        //! unlock read-only layout
        if (!generic->isWritable()) {
            generic->unlock();
        }

      /*  if (iLayoutCurrentData.color.startsWith("/")) {
            //it is image file in such case
            if (iLayoutCurrentData.color != generic->background()) {
                generic->setBackground(iLayoutCurrentData.color);
            }

            if (generic->textColor() != iLayoutCurrentData.textColor) {
                generic->setTextColor(iLayoutCurrentData.textColor);
            }
        } else {
            if (iLayoutCurrentData.color != generic->color()) {
                generic->setColor(iLayoutCurrentData.color);
                generic->setBackground(QString());
                generic->setTextColor(QString());
            }
        }*/

        //! update only the Central-specific layout parts
        CentralLayout *centralActive = isOriginalLayout ? m_handler->corona()->layoutsManager()->synchronizer()->centralLayout(iLayoutOriginalData.name) : nullptr;
        CentralLayout *central = centralActive ? centralActive : m_layouts[iLayoutCurrentData.id];

        if (central->showInMenu() != iLayoutCurrentData.isShownInMenu) {
            central->setShowInMenu(iLayoutCurrentData.isShownInMenu);
        }

        if (central->disableBordersForMaximizedWindows() != iLayoutCurrentData.hasDisabledBorders) {
            central->setDisableBordersForMaximizedWindows(iLayoutCurrentData.hasDisabledBorders);
        }

        if (central->activities() != cleanedActivities) {
            central->setActivities(cleanedActivities);
        }

        //! If the layout name changed OR the layout path is a temporary one
        if ((iLayoutCurrentData.name != iLayoutOriginalData.name) || iLayoutCurrentData.isTemporary()) {
            //! If the layout is Active in MultipleLayouts
            if (m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts && generic->isActive()) {
                qDebug() << " Active Layout Should Be Renamed From : " << generic->name() << " TO :: " << iLayoutCurrentData.name;
                activeLayoutsToRename[iLayoutCurrentData.name] = generic;
            }

            QString tempFile = layoutTempDir.path() + "/" + QString(generic->name() + ".layout.latte");
            qDebug() << "new temp file ::: " << tempFile;

            if ((m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::SingleLayout) && (generic->name() == m_handler->corona()->layoutsManager()->currentLayoutName())) {
                switchToLayout = iLayoutCurrentData.name;
            }

            generic = m_layouts.take(iLayoutCurrentData.id);
            delete generic;

            QFile(iLayoutCurrentData.id).rename(tempFile);

            Data::UniqueIdInfo idInfo;

            idInfo.oldId = iLayoutCurrentData.id;
            idInfo.newId = tempFile;
            idInfo.newName = iLayoutCurrentData.name;

            alteredIdsInfo << idInfo;
        }
    }

    //! this is necessary in case two layouts have to swap names
    //! so we copy first the layouts in a temp directory and afterwards all
    //! together we move them in the official layout directory
    for (int i = 0; i < alteredIdsInfo.count(); ++i) {
        Data::UniqueIdInfo idInfo = alteredIdsInfo[i];

        QString newFile = QDir::homePath() + "/.config/latte/" + idInfo.newName + ".layout.latte";
        QFile(idInfo.newId).rename(newFile);

        CentralLayout *nLayout = new CentralLayout(this, newFile);
        m_layouts[newFile] = nLayout;

        //! updating the #SETTINGSID in the model for the layout that was renamed
        for (int j = 0; j < m_model->rowCount(); ++j) {
            Data::Layout jLayout = m_model->at(j);

            if (jLayout.id == idInfo.oldId) {
                m_model->setData(m_model->index(j, Model::Layouts::IDCOLUMN), newFile, Qt::UserRole);
            }
        }
    }

    if (m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts) {
        for (const auto &newLayoutName : activeLayoutsToRename.keys()) {
            Latte::Layout::GenericLayout *layoutPtr = activeLayoutsToRename[newLayoutName];
            qDebug() << " Active Layout of Type: " << layoutPtr->type() << " Is Renamed From : " << activeLayoutsToRename[newLayoutName]->name() << " TO :: " << newLayoutName;
            layoutPtr->renameLayout(newLayoutName);
        }
    }

    //! lock layouts in the end when the user has chosen it
    for (int i = 0; i < alteredLayouts.count(); ++i) {
        Data::Layout layoutCurrentData = alteredLayouts[i];
        Data::Layout layoutOriginalData = m_model->originalData(layoutCurrentData.id);
        layoutOriginalData = layoutOriginalData.isEmpty() ? layoutCurrentData : layoutOriginalData;

        Latte::Layout::GenericLayout *layoutPtr = m_handler->corona()->layoutsManager()->synchronizer()->layout(layoutOriginalData.name);

        if (!layoutPtr && m_layouts.contains(layoutCurrentData.id)) {
            layoutPtr = m_layouts[layoutCurrentData.id];
        }

        if (layoutCurrentData.isLocked && layoutPtr && layoutPtr->isWritable()) {
            layoutPtr->lock();
        }
    }

    //! update SharedLayouts that are Active
    syncActiveShares();

    //! reload layouts in layoutsmanager
    m_handler->corona()->layoutsManager()->synchronizer()->loadLayouts();

    if (!m_model->layoutNameForFreeActivities().isEmpty()) {
        //! make sure that there is a layout for free activities
        //! send to layout manager in which layout to switch
        MemoryUsage::LayoutsMemory inMemoryOption = Latte::MemoryUsage::SingleLayout;

        if (inMultipleMode()) {
            inMemoryOption = Latte::MemoryUsage::MultipleLayouts;
        }

        if (m_handler->corona()->layoutsManager()->memoryUsage() != inMemoryOption) {
            MemoryUsage::LayoutsMemory previousMemoryUsage = m_handler->corona()->layoutsManager()->memoryUsage();
            m_handler->corona()->layoutsManager()->setMemoryUsage(inMemoryOption);

            m_handler->corona()->layoutsManager()->switchToLayout(m_model->layoutNameForFreeActivities(), previousMemoryUsage);
        } else {
            if (m_handler->corona()->layoutsManager()->memoryUsage() == MemoryUsage::MultipleLayouts) {
                m_handler->corona()->layoutsManager()->synchronizer()->syncMultipleLayoutsToActivities(m_model->layoutNameForFreeActivities());
            } else {
                m_handler->corona()->layoutsManager()->switchToLayout(m_model->layoutNameForFreeActivities());
            }
        }
    }

    m_model->applyData();

    emit dataChanged();
}

void Layouts::syncActiveShares()
{
    if (m_handler->corona()->layoutsManager()->memoryUsage() != MemoryUsage::MultipleLayouts) {
        return;
    }

    Settings::Data::LayoutsTable currentLayoutsData = m_model->currentLayoutsData();
    Settings::Data::LayoutsTable originalLayoutsData = m_model->originalLayoutsData();

    Latte::Layouts::SharesMap  currentSharesNamesMap = currentLayoutsData.sharesMap();
    QStringList originalSharesIds = originalLayoutsData.allSharesIds();
    QStringList currentSharesIds = currentLayoutsData.allSharesIds();

    QStringList deprecatedSharesIds = Latte::subtracted(originalSharesIds, currentSharesIds);
    QStringList deprecatedSharesNames;

    for(int i=0; i<deprecatedSharesIds.count(); ++i) {
        QString shareId = deprecatedSharesIds[i];

        if (currentLayoutsData.containsId(shareId)) {
            deprecatedSharesNames << currentLayoutsData[shareId].name;
        } else if (originalLayoutsData.containsId(shareId)) {
            deprecatedSharesNames << originalLayoutsData[shareId].name;
        }
    }

    qDebug() << " CURRENT SHARES NAMES MAP  :: " << currentSharesNamesMap;
    qDebug() << " DEPRECATED SHARES ::";

    m_handler->corona()->layoutsManager()->synchronizer()->syncActiveShares(currentSharesNamesMap, deprecatedSharesNames);
}

void Layouts::storeColumnWidths()
{
    //! save column widths
    m_viewColumnWidths.clear();

    m_viewColumnWidths << QString::number(m_view->columnWidth(Model::Layouts::BACKGROUNDCOLUMN));
    m_viewColumnWidths << QString::number(m_view->columnWidth(Model::Layouts::NAMECOLUMN));
    m_viewColumnWidths << QString::number(m_view->columnWidth(Model::Layouts::MENUCOLUMN));
    m_viewColumnWidths << QString::number(m_view->columnWidth(Model::Layouts::BORDERSCOLUMN));

    if (inMultipleMode()) {
        m_viewColumnWidths << QString::number(m_view->columnWidth(Model::Layouts::ACTIVITYCOLUMN));
    } else {
        //! In Single Mode, keed recorded value for ACTIVITYCOLUMN
        if (m_viewColumnWidths.count()>=5) {
            m_viewColumnWidths << m_viewColumnWidths[4];
        }
    }
}

void Layouts::on_nameDuplicatedFrom(const QString &provenId, const QString &trialId)
{
    //! duplicated layout name
    int pRow = rowForId(provenId);
    int tRow = rowForId(trialId);

    int originalRow = m_model->rowForId(provenId);
    Data::Layout provenLayout = m_model->at(originalRow);

    m_handler->showInlineMessage(i18nc("settings: layout name used","Layout <b>%0</b> is already used, please provide a different name...").arg(provenLayout.name),
                                 KMessageWidget::Error);

    QModelIndex tIndex = m_proxyModel->index(tRow, Model::Layouts::NAMECOLUMN);

    //! avoid losing focuse
    QTimer::singleShot(0, [this, tIndex]() {
        m_view->edit(tIndex);
    });
}

void Layouts::loadConfig()
{
    //! remove old unneeded oprtions
    KConfigGroup deprecatedStorage(KConfigGroup(KSharedConfig::openConfig(), "UniversalSettings"));
    QStringList columnWidths = deprecatedStorage.readEntry("layoutsColumnWidths", QStringList());

    if (!columnWidths.isEmpty()) {
        //! migrating
        m_viewColumnWidths = columnWidths;
    } else {
        //! new storage
        m_viewColumnWidths = m_storage.readEntry("columnWidths", QStringList());
        m_viewSortColumn = m_storage.readEntry("sortColumn", (int)Model::Layouts::NAMECOLUMN);
        m_viewSortOrder = static_cast<Qt::SortOrder>(m_storage.readEntry("sortOrder", (int)Qt::AscendingOrder));
    }
}

void Layouts::saveConfig()
{
    m_storage.writeEntry("columnWidths", m_viewColumnWidths);
    m_storage.writeEntry("sortColumn", m_viewSortColumn);
    m_storage.writeEntry("sortOrder", (int)m_viewSortOrder);
}

}
}
}
