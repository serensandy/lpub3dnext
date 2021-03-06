/**************************************************************************** 
**
** Copyright (C) 2007-2009 Kevin Clague. All rights reserved.
** Copyright (C) 2015 - 2020 Trevor SANDY. All rights reserved.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <QFileDialog>
#include <QDir>
#include <QComboBox>
#include <QLineEdit>
#include <QUndoStack>
#include <QSettings>

#include "lpub.h"
#include "lpub_preferences.h"
#include "editwindow.h"
#include "paths.h"
#include "threadworkers.h"
#include "messageboxresizable.h"
#include "metagui.h"

#include <LDVQt/LDVImageMatte.h>

void Gui::open()
{  
  if (maybeSave()) {
    QSettings Settings;
    QString modelDir;
    if (Settings.contains(QString("%1/%2").arg(SETTINGS,"ProjectsPath"))) {
      modelDir = Settings.value(QString("%1/%2").arg(SETTINGS,"ProjectsPath")).toString();
    } else {
      modelDir = Preferences::ldrawLibPath + "/models";
    }

    QString fileName = QFileDialog::getOpenFileName(
      this,
      tr("Open LDraw File"),
      modelDir,
      tr("LDraw Files (*.dat *.ldr *.mpd)"));

    timer.start();

    QFileInfo fileInfo(fileName);
    if (fileInfo.exists()) {
      Settings.setValue(QString("%1/%2").arg(SETTINGS,"ProjectsPath"),fileInfo.path());
      if (!openFile(fileName)) {
          emit messageSig(LOG_STATUS, QString("Load LDraw model file %1 aborted.").arg(fileName));
          return;
      }
      displayPage();
      enableActions();
      ldrawFile.showLoadMessages();
      emit messageSig(LOG_STATUS, gui->loadAborted() ?
                       QString("Load LDraw model file %1 aborted.").arg(fileName) :
                       QString("File loaded (%1 parts). %2")
                               .arg(ldrawFile.getPartCount())
                               .arg(elapsedTime(timer.elapsed())));
      return;
    }
  }
  return;
}

void Gui::openDropFile(QString &fileName){

  if (maybeSave()) {
      timer.start();
      QFileInfo fileInfo(fileName);
      QString extension = fileInfo.suffix().toLower();
      bool ldr = false, mpd = false, dat= false;
      ldr = extension == "ldr";
      mpd = extension == "mpd";
      dat = extension == "dat";
      if (fileInfo.exists() && (ldr || mpd || dat)) {
          QSettings Settings;
          Settings.setValue(QString("%1/%2").arg(SETTINGS,"ProjectsPath"),fileInfo.path());
          if (!openFile(fileName)) {
              emit messageSig(LOG_STATUS, QString("Load LDraw model file %1 aborted.").arg(fileName));
              return;
          }
          displayPage();
          enableActions();
          ldrawFile.showLoadMessages();
          emit messageSig(LOG_STATUS, gui->loadAborted() ?
                              QString("Load LDraw model file %1 aborted.").arg(fileName) :
                              QString("File loaded (%1 parts). %2")
                                      .arg(ldrawFile.getPartCount())
                                      .arg(elapsedTime(timer.elapsed())));
        } else {
          QString noExtension;
          if (extension.isEmpty())
              noExtension = QString("<br>No file exension specified. Set the file extension to .mpd,.ldr, or .dat.");
          emit messageSig(LOG_ERROR, QString("File not supported!<br>%1%2")
                          .arg(fileName).arg(noExtension));
        }
    }
}

void Gui::openFolder(const QString &folder)
{
    QString CommandPath = folder;
    QProcess *Process = new QProcess(this);
    Process->setWorkingDirectory(QDir::currentPath() + QDir::separator());
#ifdef Q_OS_WIN
    Process->setNativeArguments(CommandPath);
    QDesktopServices::openUrl((QUrl("file:///"+CommandPath, QUrl::TolerantMode)));
#else
    Process->execute(CommandPath);
    Process->waitForFinished();

    QProcess::ExitStatus Status = Process->exitStatus();

    if (Status != 0) {  // look for error
        QErrorMessage *m = new QErrorMessage(this);
        m->showMessage(QString("%1\n%2").arg("Failed to open image folder!").arg(CommandPath));
      }
#endif
}

void Gui::openWorkingFolder() {
    if (!getCurFile().isEmpty())
        openFolder(QFileInfo(getCurFile()).absolutePath());
}

void Gui::updateOpenWithActions()
{
    QSettings Settings;
    QString const openWithProgramListKey("OpenWithProgramList");
    if (Settings.contains(QString("%1/%2").arg(SETTINGS,openWithProgramListKey))) {

      QStringList programEntries = Settings.value(QString("%1/%2").arg(SETTINGS,openWithProgramListKey)).toStringList();

      numPrograms = qMin(programEntries.size(), Preferences::maxOpenWithPrograms);

      // filter programPaths that don't exist
      QString programName, programPath;
      for (int i = 0; i < numPrograms; ) {
        programPath = QDir::toNativeSeparators(programEntries.at(i).split("|").last());
        QFileInfo fileInfo(programPath);
        if (fileInfo.exists()) {
          i++;
        } else {
          programEntries.removeOne(programEntries.at(i));
          --numPrograms;
        }
      }

      auto getProgramIcon = [&programPath] ()
      {
          QStringList pathList   = QStandardPaths::standardLocations(QStandardPaths::TempLocation);
          QString iconPath       = pathList.first();
          const QString iconFile = QString("%1/%2icon.png").arg(iconPath).arg(QFileInfo(programPath).baseName());
          if (!QFileInfo(iconFile).exists()) {
              QFileInfo programInfo(programPath);
              QFileSystemModel *fsModel = new QFileSystemModel;
              fsModel->setRootPath(programInfo.path());
              QIcon fileIcon = fsModel->fileIcon(fsModel->index(programInfo.filePath()));
              QPixmap iconPixmap = fileIcon.pixmap(16,16);
              if (!iconPixmap.save(iconFile))
                  emit gui->messageSig(LOG_INFO,QString("Could not save program file icon: %1").arg(iconFile));
              return fileIcon;
          }
          return QIcon(iconFile);
      };

      for (int i = 0; i < numPrograms; i++) {
        programName = programEntries.at(i).split("|").first();
        programPath = programEntries.at(i).split("|").last();
        QString text = programName;
        QFileInfo fileInfo(programPath);
        if (text.isEmpty())
            text = tr("&%1 %2").arg(i + 1).arg(fileInfo.fileName());
        openWithActList[i]->setText(text);
        openWithActList[i]->setData(programPath);
        openWithActList[i]->setIcon(getProgramIcon());
        openWithActList[i]->setStatusTip(QString("Open %1 with program: %2")
                                      .arg(curFile.isEmpty() ? "current file" : QFileInfo(curFile).fileName())
                                      .arg(fileInfo.absoluteFilePath()));
        openWithActList[i]->setVisible(true);
      }

      for (int j = numPrograms; j < Preferences::maxOpenWithPrograms; j++) {
        openWithActList[j]->setVisible(false);
      }
      openWithMenu->setEnabled(numPrograms > 0);
    }
}

void Gui::openWithSetup()
{
    OpenWithProgramDialogGui *openWithProgramDialogGui =
                              new OpenWithProgramDialogGui();
    openWithProgramDialogGui->setOpenWithProgram();
    updateOpenWithActions();
}

void Gui::openWith()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString program = action->data().toString();
        QStringList arguments = QStringList() << curFile;

//        QProcess *Process = new QProcess(this);
//        Process->setWorkingDirectory(QDir::currentPath() + QDir::separator());
//        Process->start(program, arguments);

        qint64 pid;
        QString workingDirectory = QDir::currentPath() + QDir::separator();
        QProcess::startDetached(program, {arguments}, workingDirectory, &pid);
        emit messageSig(LOG_INFO, QString("Launched external applicatin %1...")
                        .arg(QFileInfo(program).fileName()));
    }
}

void Gui::openRecentFile()
{
  QAction *action = qobject_cast<QAction *>(sender());
  if (action) {
    timer.start();
    QString fileName = action->data().toString();
    QFileInfo fileInfo(fileName);
    QDir::setCurrent(fileInfo.absolutePath());
    if (!openFile(fileName)) {
        emit messageSig(LOG_STATUS, QString("Load LDraw model file %1 aborted.").arg(fileName));
        return;
    }
    Paths::mkDirs();
    displayPage();
    enableActions();
    ldrawFile.showLoadMessages();
    emit messageSig(LOG_STATUS, gui->loadAborted() ?
                        QString("Load LDraw model file %1 aborted.").arg(fileName) :
                        QString("File loaded (%1 parts). %2")
                                .arg(ldrawFile.getPartCount())
                                .arg(elapsedTime(timer.elapsed())));
  }
}



void Gui::clearRecentFiles()
{
  QSettings Settings;
  if (Settings.contains(QString("%1/%2").arg(SETTINGS,"LPRecentFileList"))) {
    QStringList files = Settings.value(QString("%1/%2").arg(SETTINGS,"LPRecentFileList")).toStringList();
    files.clear();
     Settings.setValue(QString("%1/%2").arg(SETTINGS,"LPRecentFileList"), files);
   }
  updateRecentFileActions();
}

bool Gui::loadFile(const QString &file)
{
    currentStep = nullptr;

    QString fileName = file;
    QFileInfo info(fileName);
    if (info.exists()) {
        timer.start();
        QDir::setCurrent(info.absolutePath());
        if (!openFile(fileName)) {
            emit messageSig(LOG_STATUS, QString("Load LDraw model file %1 aborted.").arg(fileName));
            return false;
        }
        // check if possible to load page number
        QSettings Settings;
        if (Settings.contains(QString("%1/%2").arg(DEFAULTS,SAVE_DISPLAY_PAGE_NUM_KEY))) {
            displayPageNum = Settings.value(QString("%1/%2").arg(DEFAULTS,SAVE_DISPLAY_PAGE_NUM_KEY)).toInt();
            Settings.remove(QString("%1/%2").arg(DEFAULTS,SAVE_DISPLAY_PAGE_NUM_KEY));
          }
        Paths::mkDirs();
        displayPage();
        enableActions();
        ldrawFile.showLoadMessages();
        emit messageSig(LOG_STATUS, gui->loadAborted() ?
                            QString("Load LDraw model file %1 aborted.").arg(fileName) :
                            QString("File loaded (%1 parts). %2")
                                    .arg(ldrawFile.getPartCount())
                                    .arg(elapsedTime(timer.elapsed())));
        return true;
    } else {
        emit messageSig(LOG_ERROR,QString("Unable to load file %1.").arg(fileName));
    }
    return false;
}

void Gui::enableWatcher()
{
#ifdef WATCHER
    if (curFile != "") {
      if (isMpd()) {
        watcher.addPath(curFile);
      }
      QStringList filePaths = ldrawFile.getSubFilePaths();
      filePaths.removeDuplicates();
      if (filePaths.size()) {
        for (QString filePath : filePaths) {
          watcher.addPath(filePath);
        }
      }
    }
#endif
}

void Gui::disableWatcher()
{
#ifdef WATCHER
    if (curFile != "") {
      if (isMpd()) {
        watcher.removePath(curFile);
      }
      QStringList filePaths = ldrawFile.getSubFilePaths();
      filePaths.removeDuplicates();
      if (filePaths.size()) {
        for (QString filePath : filePaths) {
          watcher.removePath(filePath);
        }
      }
    }
#endif
}

void Gui::save()
{
  disableWatcher();

  if (curFile.isEmpty()) {
    saveAs();
  } else {
    saveFile(curFile);
  }

 enableWatcher();
}

void Gui::saveAs()
{
  disableWatcher();

  QString fileName = QFileDialog::getSaveFileName(this,tr("Save As"),curFile,tr("LDraw (*.mpd *.ldr *.dat)"));
  if (fileName.isEmpty()) {
    return;
  }
  QFileInfo fileInfo(fileName);
  QString extension = fileInfo.suffix().toLower();

  if (extension == "mpd" ||
      extension == "ldr" ||
      extension == "dat") {
    saveFile(fileName);
    closeFile();
    openFile(fileName);
    displayPage();
  } else {
    QMessageBox::warning(nullptr,QMessageBox::tr(VER_PRODUCTNAME_STR),
                              QMessageBox::tr("Invalid LDraw extension %1 specified.  File not saved.")
                                .arg(extension));

  }
  enableWatcher();
} 

void Gui::saveCopy()
{
  QString fileName = QFileDialog::getSaveFileName(this,tr("Save As"),curFile,tr("LDraw (*.mpd *.ldr *.dat)"));
  if (fileName.isEmpty()) {
    return;
  }

  QFileInfo fileInfo(fileName);
  QString extension = fileInfo.suffix().toLower();

  if (extension == "mpd" ||
      extension == "ldr" ||
      extension == "dat") {
    saveFile(fileName);
  } else {
    QMessageBox::warning(nullptr,QMessageBox::tr(VER_PRODUCTNAME_STR),
                              QMessageBox::tr("Invalid LDraw extension %1 specified.  File not saved.")
                                .arg(extension));
  }
}

bool Gui::maybeSave(bool prompt, int sender /*SaveOnNone=0*/)
{
  QString senderLabel;
  bool proceed = true;
  SaveOnSender saveSender = SaveOnSender(sender);
  if (saveSender ==  SaveOnRedraw) {
      senderLabel = "redraw";
      proceed = Preferences::showSaveOnRedraw;
  } else
  if (saveSender ==  SaveOnUpdate) {
     senderLabel = "update";
     proceed = Preferences::showSaveOnUpdate;
  }

  if ( ! undoStack->isClean() && proceed) {
    if (Preferences::modeGUI && prompt) {
      // Get the application icon as a pixmap
      QPixmap _icon = QPixmap(":/icons/lpub96.png");
      if (_icon.isNull())
          _icon = QPixmap (":/icons/update.png");

      QMessageBoxResizable box;
      box.setWindowIcon(QIcon());
      box.setIconPixmap (_icon);
      box.setTextFormat (Qt::RichText);
      box.setWindowTitle(tr ("%1 Document").arg(VER_PRODUCTNAME_STR));
      box.setWindowFlags (Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
      QString title = "<b>" + tr ("Document changes detected&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;") + "</b>";
      QString text = tr("The document has been modified.<br>"
                        "Do you want to save your changes?");
      box.setText (title);
      box.setInformativeText (text);
      box.setStandardButtons (QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
      box.setDefaultButton   (QMessageBox::Save);

      if (saveSender){
          QCheckBox *cb = new QCheckBox(tr("Do not show save changes on %1 message again.").arg(senderLabel));
          box.setCheckBox(cb);
          QObject::connect(cb, &QCheckBox::stateChanged, [&saveSender](int state) {
              bool checked = true;
              if (static_cast<Qt::CheckState>(state) == Qt::CheckState::Checked) {
                  checked = false;
              }
              if (saveSender == SaveOnRedraw) {
                  Preferences::setShowSaveOnRedrawPreference(checked);
              } else {
                  Preferences::setShowSaveOnUpdatePreference(checked);
              }
          });
      }

      int ExecReturn = box.exec();
      if (ExecReturn == QMessageBox::Save) {
        save();
      } else
      if (ExecReturn == QMessageBox::Cancel) {
        return false;
      }
    } else {
      save();
      emit messageSig(LOG_INFO,tr("Open document has been saved!"));
    }
  }
  return true;
}

bool Gui::saveFile(const QString &fileName)
{
  bool rc;
  rc = ldrawFile.saveFile(fileName);
  setCurrentFile(fileName);
  undoStack->setClean();
  if (rc) {
    statusBar()->showMessage(tr("File saved"), 2000);
  }
  return rc;
}

void Gui::closeFile()
{
  ldrawFile.empty();
  editWindow->textEdit()->document()->clear();
  editWindow->textEdit()->document()->setModified(false);
  mpdCombo->setMaxCount(0);
  mpdCombo->setMaxCount(1000);
  setGoToPageCombo->setMaxCount(0);
  setGoToPageCombo->setMaxCount(1000);
  setPageLineEdit->clear();
  pageSizes.clear();
  undoStack->clear();
  if (Preferences::enableFadeSteps || Preferences::enableHighlightStep)
      ldrawColourParts.clearGeneratedColorParts();
  submodelIconsLoaded = gMainWindow->mSubmodelIconsLoaded = false;
  if (!curFile.isEmpty())
      emit messageSig(LOG_DEBUG, QString("File closed - %1.").arg(curFile));
}

void Gui::closeModelFile(){
  disableWatcher();
  QString topModel = ldrawFile.topLevelFile();
  //3D Viewer
  emit clearViewerWindowSig();
  emit updateAllViewsSig();
  emit disable3DActionsSig();
  // Editor
  emit disableEditorActionsSig();
  // Gui
  clearPage(KpageView,KpageScene,true);
  disableActions();
  disableActions2();
  closeFile();
  editModeWindow->close();
  editModelFileAct->setText(tr("Edit current model file"));
  editModelFileAct->setStatusTip(tr("Edit loaded LDraw model file"));
  emit messageSig(LOG_INFO, QString("Model %1 unloaded.").arg(topModel));
  curFile.clear();

  QString windowName = VER_FILEDESCRIPTION_STR;
  QString windowVersion;
#if defined LP3D_CONTINUOUS_BUILD || defined LP3D_DEVOPS_BUILD || defined LP3D_NEXT_BUILD
  windowVersion = QString("v%1 r%2 (%3)")
          .arg(VER_PRODUCTVERSION_STR)
          .arg(VER_REVISION_STR)
          .arg(VER_BUILD_TYPE_STR);
#else
  windowVersion = QString("v%1%2")
          .arg(VER_PRODUCTVERSION_STR)
          .arg(QString(VER_REVISION_STR).toInt() ?
                   QString(" r%1").arg(VER_REVISION_STR) :
                   QString());
#endif

setWindowTitle(tr("%1[*] - %2").arg(windowName).arg(windowVersion));
}
/***************************************************************************
 * File opening closing stuff
 **************************************************************************/

bool Gui::openFile(QString &fileName)
{
  disableWatcher();

  parsedMessages.clear();
  clearPage(KpageView,KpageScene,true);
  closeFile();
  if (lcGetPreferences().mViewPieceIcons)
      mPliIconsPath.clear();
  emit messageSig(LOG_INFO_STATUS, QString("Loading LDraw model file [%1]...").arg(fileName));
  if (ldrawFile.loadFile(fileName) != 0) {
      closeModelFile();
      return false;
  }
  setFadeStepsFromCommandMeta();
  setHighlightStepFromCommandMeta();
  if (Preferences::enableFadeSteps && Preferences::enableImageMatting)
    LDVImageMatte::clearMatteCSIImages();
  displayPageNum = 1;
  QFileInfo info(fileName);
  QDir::setCurrent(info.absolutePath());
  Paths::mkDirs();
  editModelFileAct->setText(tr("Edit %1").arg(info.fileName()));
  editModelFileAct->setStatusTip(tr("Edit loaded LDraw model file %1").arg(info.fileName()));
  if (Preferences::enableFadeSteps || Preferences::enableHighlightStep)
      writeGeneratedColorPartsToTemp();
  bool overwriteCustomParts = false;
  emit messageSig(LOG_INFO, "Loading fade color parts...");
  processFadeColourParts(overwriteCustomParts);
  emit messageSig(LOG_INFO, "Loading highlight color parts...");
  processHighlightColourParts(overwriteCustomParts);
  emit messageSig(LOG_INFO, "Loading user interface items...");
  attitudeAdjustment();
  mpdCombo->setMaxCount(0);
  mpdCombo->setMaxCount(1000);
  mpdCombo->addItems(ldrawFile.subFileOrder());
  mpdCombo->setToolTip(tr("Current Submodel: %1").arg(mpdCombo->currentText()));
  setCurrentFile(fileName);
  emit messageSig(LOG_STATUS, "Loading LDraw Editor display...");
  displayFile(&ldrawFile,ldrawFile.topLevelFile());
  undoStack->setClean();
  curFile = fileName;
  insertFinalModel();    //insert final fully coloured model if fadeStep turned on
  generateCoverPages();  //auto-generate cover page

  enableWatcher();

  defaultResolutionType(Preferences::preferCentimeters);
  emit messageSig(LOG_DEBUG, QString("File opened - %1.").arg(fileName));
  return true;
}

void Gui::updateRecentFileActions()
{
  QSettings Settings;
  if (Settings.contains(QString("%1/%2").arg(SETTINGS,"LPRecentFileList"))) {
    QStringList files = Settings.value(QString("%1/%2").arg(SETTINGS,"LPRecentFileList")).toStringList();

    int numRecentFiles = qMin(files.size(), int(MaxRecentFiles));

    // filter filest that don't exist

    for (int i = 0; i < numRecentFiles; ) {
      QFileInfo fileInfo(files[i]);
      if (fileInfo.exists()) {
        i++;
      } else {
        files.removeOne(files[i]);
        --numRecentFiles;
      }
    }
    Settings.setValue(QString("%1/%2").arg(SETTINGS,"LPRecentFileList"), files);

    for (int i = 0; i < numRecentFiles; i++) {
      QFileInfo fileInfo(files[i]);
      QString text = tr("&%1 %2").arg(i + 1).arg(fileInfo.fileName());
      recentFilesActs[i]->setText(text);
      recentFilesActs[i]->setData(files[i]);
      recentFilesActs[i]->setStatusTip(fileInfo.absoluteFilePath());
      recentFilesActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; j++) {
      recentFilesActs[j]->setVisible(false);
    }
    separatorAct->setVisible(numRecentFiles > 0);
  }
}

void Gui::setCurrentFile(const QString &fileName)
{
  QString windowName;
  if (fileName.size() == 0) {
    windowName = VER_FILEDESCRIPTION_STR;
  } else {
    QFileInfo fileInfo(fileName);
    windowName = fileInfo.fileName();
  }
  QString windowVersion;
#if defined LP3D_CONTINUOUS_BUILD || defined LP3D_DEVOPS_BUILD || defined LP3D_NEXT_BUILD
  windowVersion = QString("%1 v%2 r%3 (%4)")
                          .arg(VER_PRODUCTNAME_STR)
                          .arg(VER_PRODUCTVERSION_STR)
                          .arg(VER_REVISION_STR)
                          .arg(VER_BUILD_TYPE_STR);
#else
  windowVersion = QString("%1 v%2%3")
                          .arg(VER_PRODUCTNAME_STR)
                          .arg(VER_PRODUCTVERSION_STR)
                          .arg(QString(VER_REVISION_STR).toInt() ?
                                   QString(" r%1").arg(VER_REVISION_STR) :
                                   QString());
#endif

  setWindowTitle(tr("%1[*] - %2").arg(windowName).arg(windowVersion));

  if (fileName.size() > 0) {
    QSettings Settings;
    QStringList files = Settings.value(QString("%1/%2").arg(SETTINGS,"LPRecentFileList")).toStringList();
    files.removeAll("");
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles) {
      files.removeLast();
    }
    Settings.setValue(QString("%1/%2").arg(SETTINGS,"LPRecentFileList"), files);
  }
  updateRecentFileActions();
}

void Gui::loadLastOpenedFile(){
    updateRecentFileActions();
    if (recentFilesActs[0]) {
        loadFile(recentFilesActs[0]->data().toString());
    }
}

void Gui::fileChanged(const QString &path)
{
  if (! changeAccepted)
    return;

  changeAccepted = false;

  // Get the application icon as a pixmap
  QPixmap _icon = QPixmap(":/icons/lpub96.png");
  if (_icon.isNull())
      _icon = QPixmap (":/icons/update.png");

  QMessageBoxResizable box;
  box.setWindowIcon(QIcon());
  box.setIconPixmap (_icon);
  box.setTextFormat (Qt::RichText);
  box.setWindowTitle(tr ("%1 File Change").arg(VER_PRODUCTNAME_STR));
  box.setWindowFlags (Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
  QString title = "<b>" + tr ("External change detected") + "</b>";
  QString text = tr("\"%1\" contents were changed by an external source. Reload?").arg(path);
  box.setText (title);
  box.setInformativeText (text);
  box.setStandardButtons (QMessageBox::Yes | QMessageBox::No);
  box.setDefaultButton   (QMessageBox::Yes);

  if (box.exec() == QMessageBox::Yes) {
    changeAccepted = true;
    int goToPage = displayPageNum;
    QString fileName = path;
    if (!openFile(fileName)) {
        emit messageSig(LOG_STATUS, QString("Load LDraw model file %1 aborted.").arg(fileName));
        return;
    }
    displayPageNum = goToPage;
    displayPage();
  }
}

void Gui::writeGeneratedColorPartsToTemp(){
  for (int i = 0; i < ldrawFile._subFileOrder.size(); i++) {
    QString fileName = ldrawFile._subFileOrder[i].toLower();
    if (ldrawColourParts.isLDrawColourPart(fileName)) {
      Where here(ldrawFile._subFileOrder[i],0);
      normalizeHeader(here);
      QString fileName = ldrawFile._subFileOrder[i].toLower();
      QStringList content = ldrawFile.contents(fileName);
      emit messageSig(LOG_INFO, "Writing generated part to temp directory: " + fileName + "...");
      writeToTmp(fileName,content);
    }
  }
}

void Gui::setFadeStepsFromCommandMeta()
{
    if (!Preferences::enableFadeSteps){
        Where fileHeader(gui->topLevelFile(),0);
        QRegExp lineRx = QRegExp("\\bFADE TRUE\\b");
        Preferences::enableFadeSteps = stepContains(fileHeader,lineRx);
        if (Preferences::enableFadeSteps){
            messageSig(LOG_INFO,QString("Fade Previous Steps is %1.").arg(Preferences::enableFadeSteps ? "ON" : "OFF"));
            ldrawColorPartsLoad();
            QString result;
            if (!Preferences::fadeStepsUseColour) {
                lineRx.setPattern("\\bUSE_FADE_COLOR TRUE\\b");
                Preferences::fadeStepsUseColour = stepContains(fileHeader,lineRx);
                messageSig(LOG_INFO,QString("Use Global Fade Color is %1").arg(Preferences::fadeStepsUseColour ? "ON" : "OFF"));
            }

            int fadeStepsOpacityCompare  = Preferences::fadeStepsOpacity;
            result.clear();
            lineRx.setPattern("FADE_OPACITY\\s(\\d+)");
            stepContains(fileHeader,lineRx,result,1);
            bool ok = result.toInt(&ok);
            Preferences::fadeStepsOpacity = ok ? result.toInt() : FADE_OPACITY_DEFAULT;
            bool fadeStepsOpacityChanged  = Preferences::fadeStepsOpacity != fadeStepsOpacityCompare;
            if (fadeStepsOpacityChanged)
                messageSig(LOG_INFO,QString("Fade Step Transparency changed from %1 to %2 percent")
                           .arg(fadeStepsOpacityCompare)
                           .arg(Preferences::fadeStepsOpacity));

            QString fadeStepsColourCompare  = Preferences::validFadeStepsColour;
            result.clear();
            lineRx.setPattern("\\bFADE_COLOR\\b\\s\"(\\w+)\"");
            stepContains(fileHeader,lineRx,result,1);
            Preferences::validFadeStepsColour = !result.isEmpty() ? result : Preferences::validFadeStepsColour;
            bool fadeStepsColourChanged       = QString(Preferences::validFadeStepsColour).toLower() != fadeStepsColourCompare.toLower();
            if (fadeStepsColourChanged)
                messageSig(LOG_INFO,QString("Fade Step Color preference changed from %1 to %2")
                                            .arg(fadeStepsColourCompare.replace("_"," "))
                                            .arg(QString(Preferences::validFadeStepsColour).replace("_"," ")));

            gui->partWorkerLDSearchDirs.setDoFadeStep(true);
            gui->partWorkerLDSearchDirs.addCustomDirs();
        }
    }
}

void Gui::setHighlightStepFromCommandMeta()
{
    if (!Preferences::enableHighlightStep) {
        Where fileHeader(gui->topLevelFile(),0);
        QRegExp lineRx = QRegExp("\\bHIGHLIGHT TRUE\\b");
        Preferences::enableHighlightStep = stepContains(fileHeader,lineRx);
        if (Preferences::enableHighlightStep) {
            messageSig(LOG_INFO,QString("Highlight Current Step is %1.").arg(Preferences::enableHighlightStep ? "ON" : "OFF"));
            ldrawColorPartsLoad();
            QString highlightStepColourCompare  = Preferences::highlightStepColour;
            QString result;
            lineRx.setPattern("\\HIGHLIGHT_COLOR\\b\\s\"(#[A-Fa-f0-9]{6}|\\w+)\"");
            stepContains(fileHeader,lineRx,result,1);
            Preferences::highlightStepColour = !result.isEmpty() ? result : Preferences::validFadeStepsColour;
            bool highlightStepColorChanged   = QString(Preferences::highlightStepColour).toLower() != highlightStepColourCompare.toLower();
            if (highlightStepColorChanged)
                messageSig(LOG_INFO,QString("Highlight Step Color preference changed from %1 to %2")
                                            .arg(highlightStepColourCompare)
                                            .arg(Preferences::highlightStepColour));

            gui->partWorkerLDSearchDirs.setDoHighlightStep(true);
            gui->partWorkerLDSearchDirs.addCustomDirs();
        }
    }
}

//void Gui::dropEvent(QDropEvent* event)
//{
//  const QMimeData* mimeData = event->mimeData();

//  if (mimeData->hasUrls()) {

//      QList<QUrl> urlList = mimeData->urls();

//      // load only the first file in the list;
//      QString fileName = urlList.at(0).toLocalFile();

//      if (urlList.size() > 1) {
//          QMessageBox::warning(nullptr,
//                               QMessageBox::tr(VER_PRODUCTNAME_STR),
//                               QMessageBox::tr("%1 files selected.\nOnly file %2 will be opened.")
//                               .arg(urlList.size())
//                               .arg(fileName));
//        }

//      openDropFile(fileName);
//      event->acceptProposedAction();
//    }
//}

//void Gui::dragEnterEvent(QDragEnterEvent* event)
//{
//  if (event->mimeData()->hasUrls()) {
//      event->acceptProposedAction();
//    }
//}

//void Gui::dragMoveEvent(QDragMoveEvent* event)
//{
//  if (event->mimeData()->hasUrls()) {
//      event->acceptProposedAction();
//    }
//}

//void Gui::dragLeaveEvent(QDragLeaveEvent* event)
//{
//  event->accept();
//}

QString Gui::elapsedTime(const qint64 &duration){

    qint64 elapsed = duration;
    int milliseconds = int(elapsed % 1000);
    elapsed /= 1000;
    int seconds = int(elapsed % 60);
    elapsed /= 60;
    int minutes = int(elapsed % 60);
    elapsed /= 60;
    int hours = int(elapsed % 24);

    return QString("Elapsed time: %1%2%3")
                   .arg(hours >   0 ?
                                  QString("%1 %2 ")
                                          .arg(hours)
                                          .arg(hours > 1 ? "hours" : "hour")
                                  : QString())
                   .arg(minutes > 0 ?
                                  QString("%1 %2 ")
                                          .arg(minutes)
                                          .arg(minutes > 1 ? "minutes" : "minute")
                                  : QString())
                   .arg(QString("%1.%2 %3")
                                .arg(seconds)
                                .arg(milliseconds,3,10,QLatin1Char('0'))
                                .arg(seconds > 1 ? "seconds" : "second"));
}
