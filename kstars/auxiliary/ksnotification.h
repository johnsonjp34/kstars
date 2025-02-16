/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <KLocalizedString>
#include <QString>

/**
 * @namespace KSNotification
 * General KStars Notifications for desktop and lite version
 *
 * @author Jasem Mutlaq
 */
namespace KSNotification
{

typedef enum
{
    EVENT_DBG,
    EVENT_INFO,
    EVENT_WARN,
    EVENT_ALERT
} EventType;

void error(const QString &message, const QString &title = i18n("Error"), uint32_t timeout = 0);
void sorry(const QString &message, const QString &title = i18n("Sorry"), uint32_t timeout = 0);
void info(const QString &message, const QString &title = i18n("Info"), uint32_t timeout = 0);
/**
 * @brief transient Non modal message box that gets deleted on close.
 * @param message message content
 * @param title message title
 */
void transient(const QString &message, const QString &title);

void event(const QLatin1String &name, const QString &message, EventType type = EVENT_INFO);
}
