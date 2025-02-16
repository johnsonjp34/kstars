/*  Progress and status of capture preparation and execution
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_capturestatuswidget.h"
#include "ekos/ekos.h"

#include <KLed>

namespace Ekos
{

class CaptureStatusWidget : public QWidget, Ui::CaptureStatusWidget
{
    Q_OBJECT

public:
    CaptureStatusWidget(QWidget * parent = nullptr);

    /**
     * @brief Change the status text and LED color
     */
    void setStatus(QString text, Qt::GlobalColor color);

    /**
     * @brief Currently displayed status text
     */
    QString getStatusText() { return statusText->text(); }

public slots:
    /**
     * @brief Handle new capture state
     */
    void setCaptureState(CaptureState status);

    /**
     * @brief Handle new filter state
     */
    void setFilterState(FilterState status);

private:
    KLed *statusLed {nullptr};

    FilterState lastFilterState = FILTER_IDLE;
};

} // namespace
