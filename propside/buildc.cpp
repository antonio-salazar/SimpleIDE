/*
 * This file is part of the Parallax Propeller SimpleIDE development environment.
 *
 * Copyright (C) 2014 Parallax Incorporated
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qtversion.h"

#include "buildc.h"
#include "Sleeper.h"
#include "properties.h"
#include "asideconfig.h"
#include "hintdialog.h"
#include "directory.h"

BuildC::BuildC(ProjectOptions *projopts, QPlainTextEdit *compstat, QLabel *stat, QLabel *progsize, QProgressBar *progbar, QComboBox *cb, Properties *p)
    : Build(projopts, compstat, stat, progsize, progbar, cb, p)
{
}

int  BuildC::runBuild(QString option, QString projfile, QString compiler)
{
    int rc = 0;

    incHash.clear();

    projectFile = projfile;
    aSideCompiler = compiler;
    aSideCompilerPath = sourcePath(compiler);

    setMemModel(projectOptions->getMemModel());

    projName = shortFileName(projectFile).replace(".side", ".c");
    exeName = projName.mid(0, projName.lastIndexOf(".")) + ".elf";
    if (option.indexOf(BUILDALL_MEMTYPE) == 0) {
        model = option.mid(option.indexOf("=")+1);
        setMemModel(model);
    }
    exePath = outputPath + exeName;

    if (ensureOutputDirectory() != 0)
        return -1;

    QStringList clist;
    QFile file(projectFile);
    QString proj = "";
    if(file.open(QFile::ReadOnly | QFile::Text)) {
        proj = file.readAll();
        file.close();
    }

    proj = proj.trimmed(); // kill extra white space
    QStringList list = proj.split("\n");

    /* add option to build */
    if(option.length() > 0)
        clist.append(option);

    /* Calculate the number of compile steps for progress.
     * Skip empty lines and don't count ">" parameters.
     */
    int maxprogress = list.length();

    /* If we don't have a list we can't compile!
     */
    if(maxprogress < 1)
        return -1;

    //checkAndSaveFiles();

    progress->hide();
    programSize->setText("");

    compileStatus->setPlainText(tr("Project Directory: ")+sourcePath(projectFile)+"\n");
    compileStatus->moveCursor(QTextCursor::End);

    QString version = QString("%1 Version %2.%3.%4").arg(ASideGuiKey)
            .arg(IDEVERSION).arg(MINVERSION).arg(FIXVERSION);
    compileStatus->appendPlainText(version);

    QString library = properties->getGccLibraryStr();
    compileStatus->appendPlainText(library);

    QString workspace = properties->getGccWorkspaceStr();
    if(workspace.length() > 0) {
        QString wrkup(workspace+"Updated.txt");
        if(QFile::exists(wrkup)) {
            QFile upd(wrkup);
            if(upd.open(QFile::ReadOnly)) {
                QString sd = upd.readAll();
                if(sd.contains("_")) {
                    sd = sd.left(sd.indexOf("_"));
                }
                workspace += " Updated on: "+sd;
                upd.close();
            }
        }
        compileStatus->appendPlainText(workspace+"\n");
    }

    bool rebuild = true;

    //if (projectOptions->getDependentBuild().length() == 0)
    //    rebuild = true;

    Qt::KeyboardModifiers keymods = QApplication::keyboardModifiers();
    if ((keymods & Qt::AltModifier) != 0)
        rebuild = false;

    QStringList localList = getLocalSourceList(list);

    if (!rebuild) {
        rebuild = isOutdated(localList, sourcePath(projectFile), sourcePath(projectFile)+exePath);
        status->setText(tr("Build not needed."));
    }

    /* if a local source is out of date, rebuild all */
    if (rebuild)
    {
        status->setText(tr("Building ..."));

        QString memmod = projectOptions->getMemModel();
        if (option.indexOf(BUILDALL_MEMTYPE) == 0) {
            memmod = option.mid(option.indexOf("=")+1);
            memmod = ">memtype="+memmod;
            option = "";
            for (int n = 0; n < list.length(); n++) {
                if(list[n].contains(">memtype"))  {
                    list[n] = memmod;
                    break;
                }
            }
        }

        showCompilerVersion();

        foreach(QString item, list) {
            if(item.length() == 0) {
                maxprogress--;
                continue;
            }

            if(item.at(0) == '>')
                maxprogress--;
        }
        maxprogress++;

        /* choose some global options
        if(this->projectOptions->getEECOG().length() > 0)
            eecog = true;
        */

        /* remove a.out before build
         */
        QFile aout(exePath);
        if(aout.exists()) {
            if(aout.remove() == false) {
                rc = QMessageBox::question(0,
                    tr("Can't Remove File"),
                    tr("Can't Remove output file before build.\n"\
                       "Please close any program using the file \"") + exeName + tr("\".\n"\
                       "Continue?"),
                    QMessageBox::No, QMessageBox::Yes);
                if(rc == QMessageBox::No)
                    return -1;
            }
        }

        /* remove projectFile.pex before build
         */
        QString pexFile = projectFile;
        pexFile = pexFile.mid(0,pexFile.lastIndexOf("."))+".pex";
        QFile pex(pexFile);
        if(pex.exists()) {
            if(pex.remove() == false) {
                int rc = QMessageBox::question(0,
                    tr("Can't Remove File"),
                    tr("Can't Remove output file before build.\n"\
                       "Please close any program using the file \"")+pexFile+"\".\n" \
                       "Continue?",
                    QMessageBox::No, QMessageBox::Yes);
                if(rc == QMessageBox::No)
                    return -1;
            }
        }

        /* Run through file list and compile according to extension.
         * Add main file after going through the list. i.e start at list[1]
         */
        QStringList inclist;
        for(int n = 1; rc == 0 && n < list.length(); n++) {
            QApplication::processEvents();
            progress->setValue(100*n/maxprogress);
            QString name = list[n];
            if(name.length() == 0)
                continue;
            if(name.at(0) == '>')
                continue;
            QString base = shortFileName(name.mid(0,name.lastIndexOf(".")));
            if(name.contains(FILELINK)) {
                name = name.mid(name.indexOf(FILELINK)+QString(FILELINK).length());
                base = name.mid(0,name.lastIndexOf("."));
                QString inc = base.mid(0,base.lastIndexOf("/"));
                if(inclist.count() > 0) {
                    if(inclist.contains(inc,Qt::CaseInsensitive) == false)
                        inclist.append(inc);
                }
                else {
                    inclist.append(inc);
                }
            }

            QString suffix = name.mid(name.lastIndexOf("."));
            suffix = suffix.toLower();

            if(suffix.compare(".spin") == 0) {
                if(runBstc(name))
                    rc = -1;
                if(proj.toLower().lastIndexOf(".dat") < 0) // intermediate
                    list.append(outputPath+shortFileName(name.mid(0,name.lastIndexOf(".spin")))+".dat");
            }
    #if 1 // this needs to be updated for the memory model directories
            else if(suffix.compare(".espin") == 0) {
                QString basepath = sourcePath(projectFile);
                this->compileStatus->appendPlainText("Copying "+name+" to tmp.spin for spin compiler.");
                if(QFile::exists(basepath+"tmp.spin"))
                    QFile::remove(basepath+"tmp.spin");
                if(QFile::copy(basepath+base+".espin",basepath+"tmp.spin") != true) {
                    rc = -1;
                    continue;
                }
                if(runBstc("tmp.spin")) {
                    rc = -1;
                    continue;
                }
                if(QFile::exists(basepath+outputPath+base+".edat"))
                    QFile::remove(basepath+outputPath+base+".edat");
                if(QFile::copy(basepath+outputPath+"tmp.dat",basepath+outputPath+base+".edat") != true) {
                    rc = -1;
                    continue;
                }
                if(proj.toLower().lastIndexOf(".edat") < 0) // intermediate
                    list.append(outputPath+name.mid(0,name.lastIndexOf(".espin"))+".edat");
            }
    #endif
            else if(suffix.compare(".dat") == 0) {
                if(runObjCopy(name))
                    rc = -1;
                if(proj.toLower().lastIndexOf("_firmware.o") < 0)
                    clist.append(outputPath+shortFileName(name.mid(0,name.lastIndexOf(".dat")))+"_firmware.o");
            }

    #if 1 // this needs to be updated for the memory model directories
            else if(suffix.compare(".edat") == 0) {
                if(runObjCopy(name))
                    rc = -1;
                if(runCogObjCopy(base+"_firmware.ecog", base+"_firmware.o", outputPath))
                    rc = -1;
                if(runObjCopyRedefineSym("_binary_"+base+"_edat_start", "_load_start_"+base+"_firmware_ecog",outputPath+base+"_firmware.o"))
                    rc = -1;
                if(runObjCopyRedefineSym("_binary_"+base+"_edat_end",   "_load_stop_"+base+"_firmware_ecog",outputPath+base+"_firmware.o"))
                    rc = -1;
                if(proj.toLower().lastIndexOf("_firmware.o") < 0)
                    clist.append(outputPath+base+"_firmware.o");
            }
    #endif
            else if(suffix.compare(".s") == 0) {
                if(runGAS(name))
                    rc = -1;
                if(proj.toLower().lastIndexOf(".o") < 0)
                    clist.append(outputPath+name.mid(0,name.lastIndexOf(".s"))+".o");
            }
            else if(suffix.compare(".S") == 0) {
                if(runGAS(name))
                    rc = -1;
                if(proj.toLower().lastIndexOf(".o") < 0)
                    clist.append(outputPath+name.mid(0,name.lastIndexOf(".S"))+".o");
            }
            /* .cogc also does COG specific objcopy */
            else if(suffix.compare(".cogc") == 0) {
                if(runCOGC(name,".cog"))
                    rc = -1;
                clist.append(outputPath+shortFileName(base)+".cog");
            }
            /* .cogc also does COG specific objcopy */
            else if(suffix.compare(".ecogc") == 0) {
                if(runCOGC(name,".ecog"))
                    rc = -1;
                clist.append(outputPath+shortFileName(base)+".ecog");
            }
            /* dont add .a yet */
            else if(suffix.compare(".a") == 0) {
            }
            /* add all others */
            else {
                clist.append(name);
            }

        }

        /* add inclist if it exists
         */
        for(int n = 0; n < inclist.length(); n++) {
            clist.append("-I");
            clist.append(inclist.at(n));
        }

        /* add main file */
        clist.append(list[0]);

        QStringList liblist;

        /* add library .a files to the end of the list
         */
        for(int n = 0; n < list.length(); n++) {
            QApplication::processEvents();
            progress->setValue(100*n/maxprogress);
            QString name = list[n];
            if(name.length() == 0)
                continue;
            if(name.at(0) == '>')
                continue;
            if(name.contains(FILELINK))
                name = name.mid(name.indexOf(FILELINK)+QString(FILELINK).length());

            /* add .a */
            if(name.toLower().lastIndexOf(".a") > 0) {
                liblist.append(name);
                clist.append(name);
            }
        }

        QString loadtype;
        QTextCursor cur;
        bool runpex = false;

        if(rc != 0) {
            QVariant vname = process->property("Name");
            QString name = "Build";
            if(vname.canConvert(QVariant::String)) {
                name = vname.toString();
            }
            buildResult(rc, rc, name, name);
        }
        else {
            rc = runCompiler(clist);

            cur = compileStatus->textCursor();

            loadtype = cbBoard->currentText();
            if(loadtype.contains(ASideConfig::UserDelimiter+ASideConfig::SdRun, Qt::CaseInsensitive)) {
                runpex = true;
            }
            else
            if(loadtype.contains(ASideConfig::UserDelimiter+ASideConfig::SdLoad, Qt::CaseInsensitive)) {
                runpex = true;
            }
            if(runpex) {
                rc = runPexMake(exePath);
                if(rc != 0)
                    compileStatus->appendPlainText("Could not make AUTORUN.PEX\n");
            }

            if(rc == 0) {
                compileStatus->appendPlainText("Done. Build Succeeded!\n");
                cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
                compileStatus->setTextCursor(cur);
            }
            else {
                compileStatus->appendPlainText("Done. Build Failed!\n");
                if(compileStatus->toPlainText().indexOf("error:",0, Qt::CaseInsensitive) > 0) {
                    compileStatus->appendPlainText("Click error or warning messages above to debug.\n");
                }
                if(compileStatus->toPlainText().indexOf("undefined reference",0, Qt::CaseInsensitive) > 0) {
                    QStringList ssplit = compileStatus->toPlainText().split("undefined reference to ", QString::SkipEmptyParts, Qt::CaseInsensitive);
                    if(ssplit.count() > 1) {
                        QString msg = ssplit.at(1);
                        QStringList msplit = msg.split("collect");
                        if(msplit.count() > 0) {
                            QString mstr = msplit.at(0);
                            if(mstr.indexOf("`__") == 0) {
                                mstr = mstr.mid(2);
                                mstr = mstr.trimmed();
                                mstr = mstr.mid(0,mstr.length()-1);
                            }
                            compileStatus->appendPlainText("Check source for bad function call or global variable name "+mstr+"\n");
                        }
                        else {
                            compileStatus->appendPlainText("Check source for bad function call or global variable name "+ssplit.at(1)+"\n");
                        }
                        cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
                        compileStatus->setTextCursor(cur);
                        return rc;
                    }
                }
                if(compileStatus->toPlainText().indexOf("overflowed by", 0, Qt::CaseInsensitive) > 0) {
                    compileStatus->appendPlainText("Your program is too big for the memory model selected in the project.");
                    cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
                    compileStatus->setTextCursor(cur);
                    return rc;
                }
                if(compileStatus->toPlainText().indexOf("Error: Relocation overflows", 0, Qt::CaseInsensitive) > 0) {
                    compileStatus->appendPlainText("Your program is too big for the memory model selected in the project.");
                    cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
                    compileStatus->setTextCursor(cur);
                    return rc;
                }
            }
        }

        Sleeper::ms(25);
        progress->hide();

        cur = compileStatus->textCursor();
        cur.movePosition(QTextCursor::End,QTextCursor::MoveAnchor);
        compileStatus->setTextCursor(cur);
    }
    return rc;
}

int  BuildC::showCompilerVersion()
{
    int rc = 0;
    QStringList args;
    args.append("-v");
    QString compstr;
#if defined(Q_OS_WIN32)
    compstr = shortFileName(aSideCompiler);
#else
    compstr = aSideCompiler;
#endif
    if(checkCompilerInfo()) {
        return -1;
    }
    rc = startProgram(compstr, sourcePath(projectFile), args);
    return rc;
}

int  BuildC::runCOGC(QString name, QString outext)
{
    int rc = 0; // return code

    if(checkCompilerInfo()) {
        return -1;
    }

    QString compstr = shortFileName(aSideCompiler);
    QString objcopy = "propeller-elf-objcopy";

    QString base = shortFileName(name.mid(0,name.lastIndexOf(".")));
    // copy .cog to .c
    // QFile::copy(sourcePath(projectFile)+name,sourcePath(projectFile)+base+".c");
    // run C compiler on new file
    QStringList args;
    args.append("-r");  // relocatable ?
    args.append("-Os"); // default optimization for -mcog
    args.append("-mcog"); // compile for cog
    args.append("-o"); // create a .cog object
    args.append(outputPath+base+outext);
    args.append("-xc"); // code to compile is C code
    //args.append("-c");
    args.append(name);

    /* run compiler */
    rc = startProgram(compstr, sourcePath(projectFile), args);
    if(rc) return rc;

    /* now do objcopy */
    args.clear();
    args.append("--localize-text");
    args.append("--rename-section");
    args.append(".text="+base+outext);
    args.append(outputPath+base+outext);

    /* run object copy to localize fix up .cog object */
    rc = startProgram(objcopy, sourcePath(projectFile), args);

    return rc;
}

int  BuildC::runBstc(QString spinfile)
{
    int rc = 0;

    if (ensureOutputDirectory() != 0)
        return -1;

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QStringList args;
    args.append("-c");

    /* run the bstc program */
    QString spin = properties->getSpinCompilerStr();
    QString comp = spin.mid(spin.lastIndexOf("/")+1);

    QDir libdir;

    QString binaryfile = spinfile.mid(0,spinfile.lastIndexOf("."));
    binaryfile = binaryfile.mid(binaryfile.lastIndexOf("/")+1);
    binaryfile = outputPath+binaryfile;

    if((comp.compare("openspin",Qt::CaseInsensitive) == 0) ||
       (comp.compare("openspin.exe",Qt::CaseInsensitive) == 0)) {
        // Roy's compiler always makes a .binary
        if(libdir.exists(properties->getSpinLibraryStr())) {
            args.append("-I");
            args.append(properties->getSpinLibraryStr());
        }
        binaryfile += +".dat";
    }
    else {
        /* other compiler options */
        if(projectOptions->getSpinCompOptions().length()) {
            QStringList complist = projectOptions->getSpinCompOptions().split(" ",QString::SkipEmptyParts);
            foreach(QString compopt, complist) {
                args.append(compopt);
            }
        }

        // BSTC needs to be told to make a .binary
        if(libdir.exists(properties->getSpinLibraryStr())) {
            args.append("-L");
            args.append(properties->getSpinLibraryStr());
        }
    }

    args.append("-o");
    args.append(binaryfile);
    args.append(spinfile); // using shortname limits us to files in the project directory.
    rc = startProgram(comp, sourcePath(projectFile), args);

    return rc;
}

int  BuildC::runCogObjCopy(QString datfile, QString tarfile, QString outpath)
{
    int rc = 0;

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QStringList args;

    //args.append("--localize-text");
    args.append("--rename-section");
    args.append(".data="+datfile);
    args.append(tarfile);

    /* run objcopy */
    QString objcopy = "propeller-elf-objcopy";
    rc = startProgram(objcopy, sourcePath(projectFile)+outpath, args);

    return rc;
}

int  BuildC::runObjCopyRedefineSym(QString oldsym, QString newsym, QString file)
{
    int rc = 0;

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QStringList args;

    args.append("--redefine-sym");
    args.append(oldsym+"="+newsym);
    args.append(file);

    /* run objcopy */
    QString objcopy = "propeller-elf-objcopy";
    rc = startProgram(objcopy, sourcePath(projectFile), args);

    return rc;
}

int  BuildC::runObjCopy(QString datfile, QString outpath)
{
    int rc = 0;

    QString oldsym = datfile.replace("-","_");//.mid(datfile.lastIndexOf("/")+1);
    oldsym = "_binary_" + oldsym.replace(separator, "_").replace(".", "_");
    QString newsym = datfile.replace("-","_");//.mid(datfile.lastIndexOf("/")+1);
    newsym = "_binary_" + newsym.mid(newsym.lastIndexOf(separator)+1).replace(".", "_");

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QString objfile = outputPath+shortFileName(datfile.mid(0,datfile.lastIndexOf(".")))+"_firmware.o";
    QStringList args;
    args.append("-I");
    args.append("binary");
    args.append("-B");
    args.append("propeller");
    args.append("-O");
    args.append("propeller-elf-gcc");

    // with the memory model directories objcopy will generate symbols like "_binary_lmm_toggle_start"
    // but the user will expect "_binary_toggle_start" so we need to rename the generated symbols
    args.append("--redefine-sym");
    args.append(oldsym+"_start"+"="+newsym+"_start");
    args.append("--redefine-sym");
    args.append(oldsym+"_end"+"="+newsym+"_end");
    args.append("--redefine-sym");
    args.append(oldsym+"_size"+"="+newsym+"_size");
    args.append(datfile);
    args.append(objfile);

    /* run objcopy to make a spin .dat file into an object file */
    QString objcopy = "propeller-elf-objcopy";
    rc = startProgram(objcopy, sourcePath(projectFile)+outpath, args);

    return rc;
}

int  BuildC::runGAS(QString gasfile)
{
    int rc = 0;

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QString objfile = outputPath+shortFileName(gasfile.mid(0,gasfile.lastIndexOf(".")))+".o";
    QStringList args;
    args.append("-o");
    args.append(objfile);
    args.append(gasfile);

    /* run the as program */
    QString gas = "propeller-elf-as";
    rc = startProgram(gas, sourcePath(projectFile), args);

    return rc;
}

int  BuildC::runPexMake(QString fileName)
{
    int rc = 0;

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QString pexFile = projectFile;
    pexFile = sourcePath(projectFile) + "a.pex";
    QString autoexec = sourcePath(projectFile) + "AUTORUN.PEX";
    QFile pex(pexFile);
    if(pex.exists()) {
        pex.remove();
    }
    QFile apex(autoexec);
    if(apex.exists()) {
        apex.remove();
    }

    QStringList args;
    args.append("-x");
    args.append(fileName);

    /* run the as program */
    QString prog = "propeller-load";
    rc = startProgram(prog, sourcePath(projectFile), args);

    if(rc == 0) {
        QFile npex(pexFile);
        if(npex.exists()) {
            npex.copy(autoexec);
            npex.remove();
        }
    }
    return rc;
}

int  BuildC::runAR(QStringList copts, QString libname)
{
    int rc;
    QString compstr;
    QStringList args;
    args.append("rs");
    args.append(libname);

    foreach(QString s, copts) {
        if(s.contains(".out",Qt::CaseInsensitive))
            continue;
        if(s.contains(".elf",Qt::CaseInsensitive))
            continue;
        if(s.contains(".o",Qt::CaseInsensitive))
            args.append(s);
        if(s.contains(".cog",Qt::CaseInsensitive))
            args.append(s);
        if(s.contains(".ecog",Qt::CaseInsensitive))
            args.append(s);
    }

#if defined(Q_OS_WIN32)
    compstr = shortFileName(aSideCompiler);
#else
    compstr = aSideCompiler;
#endif
    QString arpath = sourcePath(compstr);
    QString ar = shortFileName(compstr);
    ar = ar.replace("gcc","ar");
    compstr = arpath+ar;

    /* remove old archive */
    if(QFile::exists(sourcePath(projectFile)+libname))
        QFile::remove(sourcePath(projectFile)+libname);

    /* this runs the archiver */
    rc = startProgram(ar,sourcePath(projectFile),args);
    return rc;
}

int  BuildC::runCompiler(QStringList copts)
{
    int rc = 0;

    if(projectFile.isNull()) {
        QMessageBox mbox(QMessageBox::Critical, "Error No Project",
            "Please select a tab and press F4 to set main project file.", QMessageBox::Ok);
        mbox.exec();
        return -1;
    }

    int prog = 0;
    progress->show();
    progress->setValue(prog);

    //getApplicationSettings();
    if(checkCompilerInfo()) {
        return -1;
    }

    QStringList args = getCompilerParameters(copts);
    QString compstr;

#if defined(Q_OS_WIN32)
    compstr = shortFileName(aSideCompiler);
#else
    compstr = aSideCompiler;
#endif

    if(projectOptions->getCompiler().indexOf("++") > -1) {
        compstr = compstr.mid(0,compstr.lastIndexOf("-")+1);
        compstr+="c++";
    }

    /* this is intermediate compile */
    QStringList tlist;
    int inc = 0;
    int lib = 0;
    QStringList libs;

    // remove and save libs
    foreach (QString s, args) {
        if(s.contains("-l")) {
            args.removeOne(s);
            if(libs.contains(s) == false)
                libs.append(s);
            continue;
        }
    }

    // remove main file. put back after library build
    QString mainProjectFile = args.at(args.count()-1);
    args.removeLast();

    /*
     * move -I and -L entries to beginning of args list
     * 1) make an ILlist and remove entries.
     * 2) add entries back from ILlist.
     */
    QStringList ILlist;
    foreach (QString s, args) {
        if(inc) {
            inc = 0;
            args.removeOne(s);
            ILlist.append(s);
        }
        else if(lib) {
            lib = 0;
            args.removeOne(s);
            ILlist.append(s);
        }
        else if(s.indexOf("-I") == 0) {
            inc++;
            args.removeOne(s);
            ILlist.append(s);
        }
        else if(s.indexOf("-L") == 0) {
            lib++;
            args.removeOne(s);
            ILlist.append(s);
        }
    }

    /**
     * Here is most likely the best place to automatically add -I and -L paths and -lnames.
     *
     * Some folks just don't get the idea that they have to add a library to use it.
     * Often they will just add an include.
     * So, we have to babysit them and try to find the include in our library and add links.
     *
     * A potential problem is with include name collisions.
     * If an include header is found and we don't already have a path, add paths and -lnames.
     * -I -L are in the ILlist stringlist. -lnames are in the libs stringlist
     *
     * Another potential problem is with Zip project.
     * This means we want to add links to the project.
     */

#ifdef ENABLE_AUTOLIB
    if(this->properties->getAutoLib()) {
        QStringList libadd;
        QStringList newList;
        // With autolib only use Simple Libraries newList is empty
        // This means we don't search the existing folder for libraries.
        // If we search the existing folder for libraries and autolib
        // is enabled, then we can end up with the wrong library.

        libadd = getLibraryList(newList,this->projectFile);
        foreach(QString s, libadd) {
            QApplication::processEvents();
            bool contains = false;
            QString ms = s.mid(s.lastIndexOf("/")+1);

            foreach(QString ils, ILlist) {
                if(ils.endsWith(ms)) {
                    contains = true;
                    break;
                }
            }

            if(contains == false) {
                ILlist.append("-I");
                ILlist.append(s);
                ILlist.append("-L");
                ILlist.append(s+"/"+this->outputPath);
            }
            s = s.mid(s.lastIndexOf("/")+1);

            if(s.indexOf("lib") == 0) {
                s = s.mid(3);
                s = "-l"+s;
                if(libs.contains(s) == false)
                    libs.append(s);
            }
        }
    }
#endif

    /* add back in reverse order */
    for(int n = ILlist.length()-1; n >= 0; n--) {
        QString s = ILlist.at(n);
        args.insert(0,s);
    }

    // just use inc for this next round.
    // we must be concerned with multiple field args like -I -L -D
    inc = 0;
    lib = 0;

    int maxprogress = args.length();
    foreach (QString s, args) {

        if( s.contains(".spin",Qt::CaseInsensitive) ||
            s.contains(".a",Qt::CaseInsensitive) ||
            s.contains(".o",Qt::CaseInsensitive) ||
            s.contains(".cog",Qt::CaseInsensitive) ||
            s.contains(".ecog",Qt::CaseInsensitive) ||
            s.contains(".out",Qt::CaseInsensitive) ||
            s.contains(".elf",Qt::CaseInsensitive) ||
            s.contains("-o")
            )
            continue;

        // *.spin should never happen because of C build rules
        // remove .h files
        if(s.endsWith(".h")) {
            args.removeOne(s);
            continue;
        }

        if(inc) {
            inc = 0;
            tlist.append(s);
        }
        else if((s.indexOf("-D") == 0)) {
            tlist.append(s);
        }
        else if((s.indexOf("-I") == 0) ||
                (s.indexOf("-L") == 0) ||
                (s.indexOf("-B") == 0) ||
                (s.indexOf("-b") == 0) ||
                (s.indexOf("-V") == 0) ||
                (s.indexOf("-x") == 0) ||
                (s.indexOf("-X") == 0)) { // any -X
            inc++;
            tlist.append(s);
        }
        else if(s.indexOf("-") == 0) {
            tlist.append(s);
        }
        else if(s.compare(".") != 0) {
            if(!tlist.contains("-c"))
                tlist.append("-c");
            tlist.append(s);
            args.removeOne(s);
            QString objPath = outputPath + shortFileName(s);
            objPath = objPath.replace(".c", ".o");
            args.append(objPath);
            tlist.append("-o");
            tlist.append(objPath);
            rc = startProgram(compstr,sourcePath(projectFile),tlist);
            tlist.removeLast(); // objPath
            tlist.removeLast(); // "-o"
            tlist.removeLast(); // srcFile
            if(rc != 0)
                return rc;
        }
        progress->setValue((100*prog++)/maxprogress);
    }

    /* let's make a library after compiling the program so we can use .o from save-temps
     */
    QString libbase = projName.mid(0, projName.lastIndexOf("."));
    QString libname = outputPath + libbase + ".a";
    if(projectOptions->getMakeLibrary().isEmpty() != true)
    {
        QStringList objs;

        QString projobj = exePath;
        projobj.replace(".elf",".o");
        if(args.contains(projobj)) {
            args.removeOne(projobj);
        }

        foreach(QString s, args) {
            if(s.contains(".out",Qt::CaseInsensitive))
                continue;
            if(s.contains(".elf",Qt::CaseInsensitive))
                continue;
            if(s.contains(".o",Qt::CaseInsensitive) ||
               s.contains(".cog",Qt::CaseInsensitive) ||
               s.contains(".ecog",Qt::CaseInsensitive)) {
                objs.append(s);
                args.removeOne(s);
             }
        }

        this->runAR(objs, libname);
        progress->setValue((100*prog++)/maxprogress);
    }

    // add GC stuff
    if(projectOptions->getEnableGcSections().length() != 0) {
        args.append("-ffunction-sections");
        args.append("-fdata-sections");
        args.append("-Wl,--gc-sections");
    }

    // add main file back
    args.append(mainProjectFile);

    // add the project library if necessary
    if(projectOptions->getMakeLibrary().isEmpty() != true)
        args.append(libname);

    /* disable tiny lib if inappropriate */
    if(projectOptions->getTinyLib().length()) {
        if(model.contains("cog",Qt::CaseInsensitive) == true) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"-ltiny\""+tr(" flag in COG mode programs.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else  if(projectOptions->getMathLib().length()) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"-ltiny\""+tr(" flag in -lm floating point programs.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else {
            libs.append(projectOptions->getTinyLib());
        }
    }

    /* append libs lib count times */
    for(int n = libs.count(); n > 0; n--) {
        int m = 0;
        for(; m < libs.count(); m++) {
            //foreach(QString s, libs) {
            args.append(libs[m]);
        }
        libs.removeAt(m-1); // optimize library add
    }

    // this is the final compile/link
    rc = startProgram(compstr,sourcePath(projectFile),args);
    progress->setValue((100*prog++)/maxprogress);
    if(rc != 0)
        return rc;

    if(exePath.contains("xmm", Qt::CaseInsensitive) == false) {
        args.clear();
        args.append("-s");
        args.append(exePath);
        rc = startProgram("propeller-load",sourcePath(projectFile),args,this->DumpNormal);
    }

    args.clear();
    args.append("-h");
    args.append(exePath);
    rc = startProgram("propeller-elf-objdump",sourcePath(projectFile),args,this->DumpReadSizes);
    progress->setValue((100*prog++)/maxprogress);

    /*
     * Report program size
     * Use the projectFile instead of the current tab file
     */
    if(codeSize == 0) codeSize = memorySize;
    QString ssize = QString(tr("Code Size")+" %L1 "+tr("bytes")+" (%L2 "+tr("total")+")").arg(codeSize).arg(memorySize);
    programSize->setText(ssize);
    progress->setValue(100);
    status->setText(status->text()+" done.");

    return rc;
}

/*
 * make debug info for a .c file
 */
int BuildC::makeDebugFiles(QString fileName, QString projfile, QString compiler)
{
    projectFile = projfile;
    aSideCompiler = compiler;
    aSideCompilerPath = sourcePath(compiler);

    if (ensureOutputDirectory() != 0)
        return -1;

    if(fileName.length() == 0)
        return -1;

    if(
       fileName.contains(".h",Qt::CaseInsensitive) ||
       fileName.contains(".spin",Qt::CaseInsensitive) ||
       fileName.contains("-I",Qt::CaseInsensitive) ||
       fileName.contains("-L",Qt::CaseInsensitive)
       ) {
        QMessageBox mbox(QMessageBox::Information, "Can't Show That",
            "Can't show assembly file info on this entry.", QMessageBox::Ok);
        mbox.exec();
        return -1;
    }

    if(projectFile.isNull()) {
        QMessageBox mbox(QMessageBox::Critical, "Error. No Project",
            "Please select a tab and press F4 to set main project file.", QMessageBox::Ok);
        mbox.exec();
        return -1;
    }

    QString name = fileName.mid(0,fileName.lastIndexOf('.'));
    if(fileName.contains(FILELINK)) {
        fileName = fileName.mid(fileName.indexOf(FILELINK)+QString(FILELINK).length());
        name = fileName.mid(0,fileName.lastIndexOf('.'));
        if(name.contains("/")) {
            name = name.mid(name.lastIndexOf("/")+1);
        }
    }


    //getApplicationSettings();
    if(checkCompilerInfo()) {
        QMessageBox mbox(QMessageBox::Critical, "Error. No Compiler",
            "Please open propertes and set the compiler, loader path, and workspace.", QMessageBox::Ok);
        mbox.exec();
        return -1;
    }

    QStringList copts;
    if(fileName.contains(".cogc",Qt::CaseInsensitive)) {
        copts.append("-xc");
    }
    copts.append("-S");

    copts.append(fileName);

    QFile file(projectFile);
    QString proj = "";
    if(file.open(QFile::ReadOnly | QFile::Text)) {
        proj = file.readAll();
        file.close();
    }

    QStringList ILlist;
    proj = proj.trimmed(); // kill extra white space
    QStringList list = proj.split("\n");
    foreach(QString iname, list) {
        if(iname.contains("-I")) {
            copts.append(iname);
            ILlist.append(iname);
        }
    }

    QStringList args = getCompilerParameters(copts);
    QString compstr;

    if(fileName.contains(".cogc",Qt::CaseInsensitive)) {
        for(int n = 0; n < args.length(); n++) {
            QString item = args.at(n);
            if(item.indexOf("-m") == 0) {
                args[n] = QString("-mcog");
                break;
            }
        }
    }

#if defined(Q_OS_WIN32)
    compstr = shortFileName(aSideCompiler);
#else
    compstr = aSideCompiler;
#endif

    if(projectOptions->getCompiler().indexOf("++") > -1) {
        compstr = compstr.mid(0,compstr.lastIndexOf("-")+1);
        compstr+="c++";
    }

    removeArg(args,"-S"); // peward++ Thanks! C & asm
    removeArg(args,"-o");
    removeArg(args,exePath);
    args.append("-o");
    args.append(outputPath+name+".o");
    args.insert(0,"-Wa,-ahdlnsg="+outputPath+name+SHOW_ASM_EXTENTION); // peward++
    args.insert(0,"-c"); // peward++
    args.insert(0,"-g"); // peward++

#ifdef ENABLE_AUTOLIB
    if(this->properties->getAutoLib()) {
        QStringList libadd;
        QStringList newList;
        // With autolib only use Simple Libraries newList is empty
        // This means we don't search the existing folder for libraries.
        // If we search the existing folder for libraries and autolib
        // is enabled, then we can end up with the wrong library.
        libadd = getLibraryList(newList,this->projectFile);
        foreach(QString s, libadd) {
            if(ILlist.contains(s) == false) {
                ILlist.append("-I");
                ILlist.append(s);
            }
            s = s.mid(s.lastIndexOf("/")+1);
        }
    }

    /* add back in reverse order */
    for(int n = ILlist.length()-1; n >= 0; n--) {
        QString s = ILlist.at(n);
        args.insert(0,s);
    }
#endif

    /* this is the final compile/link */
    compileStatus->setPlainText("");
    int rc = startProgram(compstr,sourcePath(projectFile),args);
    if(rc) {
        QMessageBox mbox(QMessageBox::Critical, tr("Compile Error"),
                         tr("Please check the compiler, loader path, and workspace."), QMessageBox::Ok);
        mbox.exec();
        compileStatus->appendPlainText("Compile Debug Error.");
        return -1;
    }
    compileStatus->appendPlainText("Done. Compile Debug Ok.");

    return 0;
}

QString BuildC::getOutputPath(QString projfile)
{
    projectFile = projfile;
    if (ensureOutputDirectory() != 0)
        return "";
    return outputPath;
}

QStringList BuildC::getCompilerParameters(QStringList copts)
{
    QStringList args;
    getCompilerParameters(copts, &args);
    return args;
}

int BuildC::getCompilerParameters(QStringList copts, QStringList *args)
{
    // use the projectFile instead of the current tab file
    //QString srcpath = sourcePath(projectFile);

    //portName = cbPort->itemText(cbPort->currentIndex());
    //boardName = cbBoard->itemText(cbBoard->currentIndex());

    //model = projectOptions->getMemModel();
    QString newmodel;
    if (copts.at(0).contains(BUILDALL_MEMTYPE)) {
        QString mopt = copts.at(0);
        newmodel = mopt.mid(mopt.indexOf("=")+1);
        model = newmodel;
        copts.removeAt(0);
    }
    model = model.mid(0,model.indexOf(" ")); // anything after the first word is just description

    if(copts.length() > 0) {
        QString s = copts.at(0);
        if(s.compare("-g") == 0)
            args->append(s);
    }
    args->append("-o");
    args->append(exePath);

    args->append(projectOptions->getOptimization());
    args->append("-m"+model);

    args->append("-I");
    args->append(".");
    args->append("-L");
    args->append(".");

    if(projectOptions->getWarnAll().length())
        args->append(projectOptions->getWarnAll());
    if(projectOptions->get32bitDoubles().length())
        args->append(projectOptions->get32bitDoubles());
    if(projectOptions->getExceptions().length())
        args->append(projectOptions->getExceptions());
    if(projectOptions->getNoFcache().length())
        args->append(projectOptions->getNoFcache());

    if(projectOptions->getSimplePrintf().length() > 0) {
        /* don't use simple printf flag for COG model programs. */
        if(model.contains("cog",Qt::CaseInsensitive) == true) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"Simple printf\""+tr(" flag in COG mode program.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else if(projectOptions->getTinyLib().length() > 0) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"Simple printf\""+tr(" flag in a program using -ltiny.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else {
            args->append(projectOptions->getSimplePrintf());
        }
    }

    if(projectOptions->getCompiler().indexOf("++") > -1)
        args->append("-fno-rtti");

    /* other compiler options */
    if(projectOptions->getCompOptions().length()) {
        QStringList complist = projectOptions->getCompOptions().split(" ",QString::SkipEmptyParts);
        foreach(QString compopt, complist) {
            args->append(compopt);
        }
    }

    /* files */
    for(int n = 0; n < copts.length(); n++) {
        QString parm = copts[n];
        if(parm.length() == 0)
            continue;
        if(parm.indexOf(" ") > 0 && parm[0] == '-') {
            // handle stuff like -I path
            QStringList sp = parm.split(" ");
            args->append(sp.at(0));
            QString join = "";
            int m;
            for(m = 1; m < sp.length()-1; m++)
                join += sp.at(m) + " ";
            join += sp.at(m);

            QString jpath = join;

            // add the memory model subdirectory for library paths
            if (sp[0] == "-L")
                join += separator + model + separator;
            args->append(join);

            // add includes for library paths ... can remove duplicates later
            // project listings are sorted so -I should come before -L
            if (sp[0] == "-L") {
                bool gotit = false;
                for(int m = 0; m < args->count(); m++) {
                    QString arg(args->at(m));
                    if(arg.compare("-I") == 0) {
                        if(m+1 < args->count()) {
                            QString arg2(args->at(m+1));
                            if(arg2.compare(jpath) == 0) {
                                gotit = true;
                                break;
                            }
                        }
                    }
                }
                if(gotit == false) {
                    args->append("-I");
                    args->append(jpath);
                }
            }

        }
        else if (parm.indexOf(".cfg",0, Qt::CaseInsensitive) > -1){
            // don't append .cfg parameter
        }
        else {
            args->append(parm);
        }
    }

#ifndef AUTOLIB
    /*
     * libraries - use libs to make copy, then add it twice to args.
     * we do this because there may be some library interdependencies.
     */
    QStringList libs;

    if(projectOptions->getTinyLib().length()) {
        if(model.contains("cog",Qt::CaseInsensitive) == true) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"-ltiny\""+tr(" flag in COG mode programs.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else  if(projectOptions->getMathLib().length()) {
            this->compileStatus->insertPlainText(tr("Ignoring")+" \"-ltiny\""+tr(" flag in -lm floating point programs.")+"\n");
            this->compileStatus->moveCursor(QTextCursor::End);
        }
        else {
            libs.append(projectOptions->getTinyLib());
        }
    }
    if(projectOptions->getMathLib().length()) {
        libs.append(projectOptions->getMathLib());
    }
    if(projectOptions->getPthreadLib().length()) {
        libs.append(projectOptions->getPthreadLib());
    }

    /* other linker options */
    if(projectOptions->getLinkOptions().length()) {
        QStringList linklist = projectOptions->getLinkOptions().split(" ",QString::SkipEmptyParts);
        foreach(QString linkopt, linklist) {
            libs.append(linkopt);
        }
    }

    /* check for changes to linker libs */
    foreach(QString s, copts) {
        if(s.left(2).compare("-L")==0) {
            QString libname = s.mid(s.lastIndexOf("lib"));
            if(libname.length() > 0) {
                libname = libname.mid(3);
                if(libname.length() > 0) {
                    libname = "-l"+libname;
                    if(libs.contains(libname) == false) {
                        libs.append(libname);
                    }
                }
            }
        }
    }

    /* append libs lib count times */
    for(int n = libs.count(); n > 0; n--) {
        foreach(QString s, libs) {
            args->append(s);
        }
    }
#endif

    /* strip */
    if(projectOptions->getStripElf().length())
        args->append(projectOptions->getStripElf());

    if (newmodel.length() > 0) {
        for (int n = 0; n < args->length(); n++) {
            QString item = args->at(n);
            if (item.indexOf(model) == 0) {
                item.replace(model,newmodel);
                args->removeAt(n);
                args->insert(n,item);
            }
        }
    }

    return args->length();
}

void BuildC::setMemModel(QString model)
{
    memModel = model;
}

QString BuildC::getMemModel()
{
    return memModel;
}

int BuildC::ensureOutputDirectory()
{
    //QString modelOption = projectOptions->getMemModel();
    QString modelOption = getMemModel();
    model = modelOption.mid(0, modelOption.indexOf(" "));
    model = model.replace("-","_");
    outputPath = model + separator;

    int rc = 0;

    QDir projectDir(sourcePath(projectFile));
    if (!projectDir.exists(model)) {
        if (!projectDir.mkdir(model)) {
            QMessageBox::critical(0,
                tr("Can't Create Output Directory"),
                tr("Can't create output directory for build."));
            rc = -1;
        }
    }

    return rc;
}

void BuildC::appendLoaderParameters(QString copts, QString file, QStringList *args)
{
    /* if propeller-load parameters -l or -z in copts, don't append a.out */
    if((copts.indexOf("-l") > -1 || copts.indexOf("-z") > -1 || copts.indexOf("-R") > -1) == false)
        args->append(exePath);

    QStringList olist = copts.split(" ",QString::SkipEmptyParts);

    for (int n = 0; n < olist.length(); n++) {
        args->append(olist[n]);
    }
    if(file.isEmpty() == false)
        args->append(file);

    //qDebug() << args;
}

bool BuildC::isOutdated(QStringList srclist, QString srcpath, QString target)
{
    QFile outfile(target);
    if (!QFileInfo(outfile).exists())
        return true;
    QDateTime outtime = QFileInfo(outfile).lastModified();
    foreach(QString item, srclist) {
        QFile sourcefile(srcpath+item);
        QDateTime sourcetime = QFileInfo(sourcefile).lastModified();
        if (outtime.secsTo(sourcetime) > 0) {
            return true;
        }
    }
    return false;
}

QStringList BuildC::getLocalSourceList(QStringList &LLlist)
{
    QStringList list;
    foreach(QString item, LLlist) {
        if(item.at(0) == '>')
            continue;
        list.append(item);
    }
    return list;
}

QStringList BuildC::getLibraryList(QStringList &ILlist, QString projFile)
{
    QSettings settings(publisherKey,ASideGuiKey);
    QStringList newList;
    //QString projFile = this->projectFile;

    if(QFile::exists(projFile) == false)
        return newList;

    QVariant libv = settings.value(gccLibraryKey);
    QString libdir;
    if(libv.canConvert(QVariant::String)) {
        libdir = libv.toString();
    }
    if(libdir.isEmpty())
        return newList;

    QStringList files;
    QString file;
    QFile proj(projFile);
    if(proj.open(QFile::ReadOnly | QFile::Text)) {
        file = proj.readAll();
        proj.close();
    }
    files = file.split("\n",QString::SkipEmptyParts);

    QStringList srcList;
    for(int n = files.count()-1; n > -1; n--) {
        QApplication::processEvents();
        QString s = files.at(n);
        if(s.indexOf("-I") == 0 ||
           s.indexOf("-L") == 0 ||
           s.indexOf(">")  == 0 ) {
            continue;
        }
        else if(s.indexOf("->") > 0) {
            srcList.append(s.mid(s.indexOf("->")+2).trimmed());
        }
        else {
            srcList.append(s.trimmed());
        }
    }

    QStringList ilist;
    for(int n = 0; n < ILlist.count(); n+=2) {
        QApplication::processEvents();
        ilist.append(ILlist.at(n)+" "+ILlist.at(n+1));
    }
    QString projectPath = projFile;
    if(projectFile.indexOf("/") > -1)
        projectPath = projFile.left(projFile.lastIndexOf("/"));
    else
        projectPath = ".";

    /* invalidate cache each time we build */
    filesHash.clear();

    foreach(QString srcFile, srcList) {
        autoAddLib(projectPath, srcFile, libdir, ilist, &newList);
    }

    newList.removeDuplicates();
    return newList;
}

int  BuildC::autoAddLib(QString projectPath, QString srcFile, QString libdir, QStringList incList, QStringList *newList)
{
    QApplication::processEvents();
    QString include("#include ");

    QString includedStr = projectPath+"/"+srcFile;
    if(filesHash.contains(includedStr)) return newList->count();

    QStringList findlist = Directory::findCSourceList(projectPath+"/"+srcFile, include);
    filesHash[includedStr] = includedStr;

    foreach(QString inc, findlist) {
        QApplication::processEvents();
        inc = inc.mid(inc.indexOf(include)+include.length());
        inc = inc.trimmed();
        //if(inc.at(0) == '<') continue;
        //if(inc.endsWith('>'))continue;
        if(inc.at(0) == '"' || inc.at(0) == '<') inc = inc.mid(1);
        if(inc.endsWith('"') || inc.at(0) == '>')inc = inc.left(inc.count()-1);
        inc = inc.trimmed();
        inc = "lib"+inc;
        inc = inc.mid(0,inc.indexOf(".h"));
        QString lib = findInclude(projectPath,libdir,inc);
        if(lib.length() == 0) {
            //continue;
        }
        /* If this is in a library project, don't include it
           otherwise an infinite directory can be made */
        if(lib.compare(projectPath) == 0)
            continue;
        /*
         With autolib, we only want to add full paths.
         These paths will never be "save as" but will be zipped though differently.
         */
        QString libpath = "-L "+lib;
        if(lib.isEmpty() == false && incList.contains(libpath) == false) {
            incList.append(libpath);
            newList->append(lib);
            QStringList ilist;
            ilist.append(lib);
            if(libdir.endsWith("/") == false)
                libdir += "/";
            QString incFile = inc.mid(3) + ".h";
            QString mydir = findInclude(projectPath, libdir, incFile);
            mydir = mydir.mid(0,mydir.lastIndexOf("/"));
            autoAddLib(mydir, incFile, libdir, incList, newList);
        }
    }
    return newList->count();
}

/*
 * findInclude is cached and uses a hash table to speed up entries.
 * protects against changes in the file-system.
 */
QString BuildC::findInclude(QString projdir, QString libdir, QString include)
{
    QString ms;
    QString s;
    if(incHash.contains(include) == false) {
        // library path is not cached yet - cache it
        ms = s = findIncludePath(projdir,libdir,include);
        if(s.length() > 0)
            incHash.insert(include, s);
    }
    else {
        // we have a cache match.
        ms = s = incHash[include];
        // if the file is missing for some reason, find it again.
        if(QFile::exists(s) == false) {
            // entry is invalid. remove it.
            incHash.remove(include);
            ms = s = findIncludePath(projdir,libdir,include);
            if(s.length() > 0)
                incHash.insert(include, s);
        }
    }
    return s;
}

/*
 * find and cache the include path.
 */
QString BuildC::findIncludePath(QString projdir, QString libdir, QString include)
{
    QString s;
    // look in project first
    s = Directory::recursiveFindFile(projdir, include);
    if(s.length() > 0) {
        incHash.insert(include, s);
        return s;
    }
    // if we get here, not project code was found - look in global library
    s = Directory::recursiveFindFile(libdir,include);
    if(s.length() > 0) {
        s = Directory::recursiveFindFile(libdir, include);
        incHash.insert(include, s);
        return s;
    }
    return s;
}


