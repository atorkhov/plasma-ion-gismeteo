/***************************************************************************
 *   Copyright (C) 2012 by Alexey Torkhov <atorkhov@gmail.com>             *
 *                                                                         *
 *   Based on Environment Canada Ion by Shawn Starr                        *
 *   Copyright (C) 2007-2011 by Shawn Starr <shawn.starr@rogers.com>       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA          *
 ***************************************************************************/

/* Ion for Gismeteo data */

#include "config.h"

#include "ion_gismeteo.h"

#include <QXmlQuery>
#include <QXmlResultItems>
#include <QXmlFormatter>
#include <QFile>
#include <QBuffer>

#include <KIO/Job>
#include <KUnitConversion/Converter>
#include <Solid/Networking>
#include <Plasma/DataContainer>

#include <qlibxmlnodemodel.h>

// ctor, dtor
EnvGismeteoIon::EnvGismeteoIon(QObject *parent, const QVariantList &args)
        : IonInterface(parent, args)
{
}

void EnvGismeteoIon::deleteForecasts()
{
    QMutableHashIterator<QString, WeatherData> it(m_weatherData);
    while (it.hasNext()) {
        it.next();
        WeatherData &item = it.value();
        qDeleteAll(item.warnings);
        item.warnings.clear();

        qDeleteAll(item.watches);
        item.watches.clear();

        qDeleteAll(item.forecasts);
        item.forecasts.clear();
    }
}

void EnvGismeteoIon::reset()
{
    deleteForecasts();
}

EnvGismeteoIon::~EnvGismeteoIon()
{
    // Destroy each watch/warning stored in a QVector
    deleteForecasts();
}

// Get the master list of locations to be parsed
void EnvGismeteoIon::init()
{
    kDebug() << "init()";

    setInitialized(true);
}

QMap<QString, IonInterface::ConditionIcons> EnvGismeteoIon::setupConditionIconMappings(void) const
{
    QMap<QString, ConditionIcons> conditionList;

    return conditionList;
}


QMap<QString, IonInterface::ConditionIcons> EnvGismeteoIon::setupForecastIconMappings(void) const
{
    QMap<QString, ConditionIcons> forecastList;

    return forecastList;
}

QMap<QString, IonInterface::ConditionIcons> const& EnvGismeteoIon::conditionIcons(void) const
{
    static QMap<QString, ConditionIcons> const condval = setupConditionIconMappings();
    return condval;
}

QMap<QString, IonInterface::ConditionIcons> const& EnvGismeteoIon::forecastIcons(void) const
{
    static QMap<QString, ConditionIcons> const foreval = setupForecastIconMappings();
    return foreval;
}

QStringList EnvGismeteoIon::validate(const QString& source) const
{
    kDebug() << "validate()";
    QStringList placeList;
    return placeList;
}

// Get a specific Ion's data
bool EnvGismeteoIon::updateIonSource(const QString& source)
{
    kDebug() << "updateIonSource()" << source;

    // We expect the applet to send the source in the following tokenization:
    // ionname|validate|place_name - Triggers validation of place
    // ionname|weather|place_name - Triggers receiving weather of place

    QStringList sourceAction = source.split('|');

    // Guard: if the size of array is not 2 then we have bad data, return an error
    if (sourceAction.size() < 2) {
        setData(source, "validate", "gismeteo|malformed");
        return true;
    }

    if (sourceAction[1] == "validate" && sourceAction.size() > 2) {
        QStringList result = validate(sourceAction[2]);

        if (result.size() == 1) {
            setData(source, "validate", QString("gismeteo|valid|single|").append(result.join("|")));
            return true;
        } else if (result.size() > 1) {
            setData(source, "validate", QString("gismeteo|valid|multiple|").append(result.join("|")));
            return true;
        } else if (result.size() == 0) {
            setData(source, "validate", QString("gismeteo|invalid|single|").append(sourceAction[2]));
            return true;
        }

    } else if (sourceAction[1] == "weather" && sourceAction.size() > 2) {
        getXMLData(source);
        return true;
    } else {
        setData(source, "validate", "gismeteo|malformed");
        return true;
    }
    return false;
}

// Gets specific city XML data
void EnvGismeteoIon::getXMLData(const QString& source)
{
    foreach (const QString &fetching, m_jobList) {
        if (fetching == source) {
            // already getting this source and awaiting the data
            return;
        }
    }

    kDebug() << source;

    // Demunge source name for key only.
    QString dataKey = source;
    dataKey.remove("gismeteo|weather|");

    KUrl url = QString("http://www.gismeteo.ru/city/hourly/" + dataKey + "/");
    url = "file:///home/alex/Develop/kde/plasma-ion-gismeteo/4368.xml";
    kDebug() << "Will Try URL: " << url;

    KIO::TransferJob* const newJob  = KIO::get(url.url(), KIO::Reload, KIO::HideProgressInfo);

    m_jobXml.insert(newJob, QByteArray());
    m_jobList.insert(newJob, source);

    connect(newJob, SIGNAL(data(KIO::Job*,QByteArray)), this,
            SLOT(slotDataArrived(KIO::Job*,QByteArray)));
    connect(newJob, SIGNAL(result(KJob*)), this, SLOT(slotJobFinished(KJob*)));
}

void EnvGismeteoIon::slotDataArrived(KIO::Job *job, const QByteArray &data)
{

    if (data.isEmpty() || !m_jobXml.contains(job)) {
        return;
    }

    // Send to xml.
    m_jobXml[job].append(data);
}

void EnvGismeteoIon::slotJobFinished(KJob *job)
{
    // Dual use method, if we're fetching location data to parse we need to do this first
    const QString source = m_jobList.value(job);
    setData(source, Data());

    const QByteArray &data = m_jobXml.value(job);
    readXMLData(m_jobList[job], data);

    m_jobList.remove(job);
    m_jobXml.remove(job);
}

// Parse Weather data main loop, from here we have to decend into each tag pair
bool EnvGismeteoIon::readXMLData(const QString& source, const QByteArray& xml)
{
    QXmlQuery query;

    kDebug() << "readXMLData()";

    // Setup model
    QLibXmlNodeModel model(query.namePool(), xml, QUrl());
    query.setFocus(model.dom());

    // Setup query
    QFile queryFile;
    queryFile.setFileName(PLASMA_ION_GISMETEO_SHARE_PATH "/gismeteo.xq");
    if (!queryFile.open(QIODevice::ReadOnly)) {
        kDebug() << "Can't open XQuery file" << PLASMA_ION_GISMETEO_SHARE_PATH "/gismeteo.xq";
        return false;
    }
    query.setQuery(&queryFile, QUrl::fromLocalFile(PLASMA_ION_GISMETEO_SHARE_PATH "/gismeteo.xq"));

    if (!query.isValid()) {
        kDebug() << "query is not valid";
        return false;
    }

    // Setup a formatter
    QBuffer out;
    out.open(QIODevice::WriteOnly);
    QXmlFormatter formatter(query, &out);

    // Evaluate query
    query.evaluateTo(&formatter);

    kDebug() << out.data();

    /*
    QXmlResultItems results;

    query.evaluateTo(&results);

    QXmlItem item(results.next());
    while (!item.isNull()) {
        kDebug() << qPrintable(item.toString());
        item = results.next();
    };

    if (results.hasError())
        kDebug() << "runtime error";
    */

    return false;
}

void EnvGismeteoIon::updateWeather(const QString& source)
{
    kDebug() << "updateWeather()";
}

#include "ion_gismeteo.moc"
