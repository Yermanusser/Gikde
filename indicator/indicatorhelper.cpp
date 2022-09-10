/*
 * SPDX-FileCopyrightText: 2019 Weixuan XIAO <veyx.shaw@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "indicatorhelper.h"

#include <QApplication>
#include <QIcon>

IndicatorHelper::IndicatorHelper()
{
}
IndicatorHelper::~IndicatorHelper()
{
}

void IndicatorHelper::preInit()
{
}

void IndicatorHelper::postInit()
{
}

void IndicatorHelper::iconPathHook()
{
}

int IndicatorHelper::daemonHook(QProcess &kdeconnectd)
{
    Q_UNUSED(kdeconnectd);

    return 0;
}

#ifdef QSYSTRAY
void IndicatorHelper::systrayIconHook(QSystemTrayIcon &systray)
{
    systray.setIcon(QIcon::fromTheme(QStringLiteral("kdeconnectindicatordark")));
}
#else
void IndicatorHelper::systrayIconHook(KStatusNotifierItem &systray)
{
    systray.setIconByName(QStringLiteral("kdeconnectindicatordark"));
}
#endif
