#include <QCoreApplication>
#include <QCommandLineParser>
#include <QObject>
#include <QDebug>
#include <QRegularExpression>
#include <QFileInfo>

#include <stdio.h>
#include <stdlib.h>

#include "propellermanager.h"
#include "propellerloader.h"
#include "propellerterminal.h"
#include "propellerimage.h"

#ifndef VERSION
#define VERSION "0.0.0"
#endif

PropellerImage load_image(QCommandLineParser &parser);
void open_loader(QCommandLineParser &parser, QStringList device_list);
void terminal(const QString & device);
void info(PropellerImage image);
void list();
void error(const QString & text);
void message(const QString & text);

int reset_pin = -1;

PropellerManager manager;
QStringList device_list = manager.listPorts();

QCommandLineOption argList      (QStringList() << "l" << "list",    QObject::tr("List available devices"));
QCommandLineOption argWrite     (QStringList() << "w" << "write",   QObject::tr("Write program to EEPROM"));
QCommandLineOption argDevice    (QStringList() << "d" << "device",  QObject::tr("Device to program (default: first system device)"), "DEV");
QCommandLineOption argPin       (QStringList() << "p" << "pin",     QObject::tr("Pin for GPIO reset"), "PIN");
QCommandLineOption argTerm      (QStringList() << "t" << "terminal",QObject::tr("Drop into terminal after download"));
QCommandLineOption argIdentify  (QStringList() << "i" << "identify",QObject::tr("Identify device connected at port"));
QCommandLineOption argInfo      (QStringList() << "image",          QObject::tr("Print info about downloadable image"));
QCommandLineOption argClkMode   (QStringList() << "clkmode",        QObject::tr("Change clock mode before download"), "MODE");
QCommandLineOption argClkFreq   (QStringList() << "clkfreq",        QObject::tr("Change clock frequency before download"), "FREQ");
QCommandLineOption argHighSpeed (QStringList() << "ultrafast",      QObject::tr("Enable two-stage high-speed mode (experimental)"));


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCoreApplication::setOrganizationName("Parallax Inc.");
    QCoreApplication::setOrganizationDomain("www.parallax.com");
    QCoreApplication::setApplicationVersion(VERSION);
    QCoreApplication::setApplicationName(QObject::tr("PropellerManager CLI"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.setApplicationDescription(
            QObject::tr("\nA command-line wrapper for the PropellerManager API"
                "\nCopyright 2015 by %1").arg(QCoreApplication::organizationName()));

    parser.addOption(argList);
    parser.addOption(argWrite);
    parser.addOption(argDevice);
    parser.addOption(argPin);
    parser.addOption(argTerm);
    parser.addOption(argIdentify);
    parser.addOption(argInfo);
    parser.addOption(argClkMode);
    parser.addOption(argClkFreq);
    parser.addOption(argHighSpeed);

    parser.addPositionalArgument("file",  QObject::tr("Binary file to download"), "FILE");

    parser.process(app);

    if (parser.isSet(argList))
    {
        list();
        return 0;
    }


    if (!parser.value(argPin).isEmpty())
    {
        bool ok;
        reset_pin = parser.value(argPin).toInt(&ok);
        if (!ok)
            error("Invalid GPIO pin passed");
    }


    if (reset_pin > -1)
        message("Using GPIO pin "+QString::number(reset_pin)+" for hardware reset");


    if (parser.isSet(argIdentify))
    {
        if (! device_list.length() > 0)
            error("No devices attached!");

        foreach (QString d, device_list)
        {
            PropellerLoader loader(&manager, d);

            switch (loader.version())
            {
                case 1:
                    printf("%s %s\n", qPrintable(d), "Propeller P8X32A");
                    break;
                case 0:
                default:
                    break;
            }

            fflush(stdout);
        }
    }
    else if (parser.isSet(argInfo))
    {
        info(load_image(parser));
    }
    else
    {
        open_loader(parser, device_list);
    }

    return 0;
}

void open_loader(QCommandLineParser &parser, QStringList device_list)
{
    if (device_list.isEmpty())
        error("No device available for download!");

    QString device = device_list[0];
    if (!parser.value(argDevice).isEmpty())
    {
        device = parser.value(argDevice);
    }

    if (parser.positionalArguments().isEmpty())
    {
        if (parser.isSet(argTerm))
        {
            terminal(device);
            return;
        }
        else
        {
            error("Must provide name of binary");
        }
    }

    PropellerLoader loader(&manager, device);
    PropellerImage image = load_image(parser);

    if (parser.isSet(argClkFreq))
    {
        bool ok;
        int freq = parser.value(argClkFreq).toInt(&ok);
        if (!ok)
            error("Invalid clock frequency: "+parser.value(argClkFreq));

        image.setClockFrequency(freq);
        image.recalculateChecksum();
    }

    if (parser.isSet(argClkMode))
    {
        bool ok;
        int mode = parser.value(argClkMode).toUInt(&ok, 16);
        if (!image.setClockMode(mode) || !ok)
            error("Clock mode setting "+QString::number(mode, 16)+"is invalid!");
        image.recalculateChecksum();
    }

    if (!image.isValid())
        error("Image is invalid!");


    if (parser.isSet(argHighSpeed))
    {
        if (loader.highSpeedUpload(image, parser.isSet(argWrite)))
            exit(1);
    }
    else
    {
        if (loader.upload(image, parser.isSet(argWrite)))
            exit(1);
    }

    if (parser.isSet(argTerm))
        terminal(device);
}

void list()
{
    for (int i = 0; i < device_list.size(); i++)
    {
        printf("%s\n",qPrintable(device_list[i]));
    }
}

void terminal(const QString & device)
{
    message("--------------------------------------");
    message("Opening terminal: ");
    message("  (Ctrl+C to exit)");
    message("--------------------------------------");

    PropellerTerminal t(&manager, device);
    QEventLoop loop;
    loop.exec();
}

void info(PropellerImage image)
{
    printf("           Image: %s\n",qPrintable(image.fileName()));
    printf("            Type: %s\n",qPrintable(image.imageTypeText()));
    printf("            Size: %u\n",image.imageSize());
    printf("        Checksum: %u (%s)\n",image.checksum(), image.checksumIsValid() ? "VALID" : "INVALID");
    printf("    Program size: %u\n",image.programSize());
    printf("   Variable size: %u\n",image.variableSize());
    printf(" Free/stack size: %u\n",image.stackSize());
    printf("      Clock mode: %s\n",qPrintable(image.clockModeText()));

    if (image.clockMode() != 0x00 && image.clockMode() != 0x01)
        printf(" Clock frequency: %i\n",image.clockFrequency());
}

PropellerImage load_image(QCommandLineParser &parser)
{
    QString filename = parser.positionalArguments()[0];
    QRegularExpression re_binary(".*\\.binary$");
    QRegularExpression re_eeprom(".*\\.eeprom$");

    if (!QFileInfo(filename).exists())
        error("File does not exist!");

    if (!filename.contains(re_binary) && !filename.contains(re_eeprom))
        error("Invalid file specified!");

    QFile file(filename);

    if (!file.open(QIODevice::ReadOnly))
        return PropellerImage();

    return PropellerImage(file.readAll(),filename);
}

void message(const QString & text)
{
    fprintf(stderr, "%s\n", qPrintable(text));
    fflush(stderr);
}


void error(const QString & text)
{
    message("ERROR: " + text);
    exit(1);
}
